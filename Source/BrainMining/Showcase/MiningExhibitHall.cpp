#include "Showcase/MiningExhibitHall.h"
#include "MiningAudioComponent.h"
#include "Showcase/SMiningCampaignMap.h"
#include "MiningProgressionComponent.h"
#include "Showcase/MiningTreasure.h"
#include "MiningCollection.h"
#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "GeneratedMiningRock.h"
#include "MiningCellUniverse.h"
#include "MiningRoomEscape.h"
#include "MiningGiantRoomEntrance.h"
#include "MiningGUIWidgets.h"
#include "MiningWalletSubsystem.h"
#include "StartRoom/MiningStartRoom.h"
#include "PopupClose/PopupCloseStage.h"
#include "WenSi/WenSiPuzzle.h"
#include "WenSi/WenSiMirrorPuzzle.h"
#include "EchoPassagePlayer.h"
#include "CardRoomPlayer.h"
#include "EchoPassageStage.h"
#include "CardRoomStage.h"
#include "Components/PostProcessComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/HUD.h"
#include "Camera/PlayerCameraManager.h"
#include "InputKeyEventArgs.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/ConstructorHelpers.h"

AMiningExhibitGameMode::AMiningExhibitGameMode()
{
    static ConstructorHelpers::FClassFinder<APawn> Pawn(TEXT("/Game/Showcase/Blueprints/BP_SharedMiningCharacter"));
    if (Pawn.Succeeded()) DefaultPawnClass=Pawn.Class;
    PlayerControllerClass=AMiningExhibitController::StaticClass();
    HUDClass=AHUD::StaticClass();
}

void AMiningExhibitController::BeginPlay()
{
    Super::BeginPlay();
    EncounterPolicy->Release();
    EchoInteraction->SetExplorationActive(false);
    CardInteraction->SetRoomActive(false);
    SetMiningGUIEnabled(true);
    if (auto* Room = AMiningStartRoom::Find(GetWorld())) Room->InitializePlayer(GetPawn());
}

void AMiningExhibitController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if (auto* Room = AMiningStartRoom::Find(GetWorld())) Room->InitializePlayer(InPawn);
}

bool AMiningExhibitController::InputKey(const FInputKeyEventArgs& Params)
{
    if (IsAudioSettingsOpen() || Params.Key == EKeys::F8) return Super::InputKey(Params);
    if (auto* Room = AMiningStartRoom::Find(GetWorld()))
    {
        if (Params.Key == EKeys::E && Params.Event == IE_Pressed && !bUpgradeMenuHeld)
            if (Room->Interact(this)) return true;
        // Preserve the introductory pickups; the main game's tools/UI resume once both are acquired.
        if ((!Room->bPickaxeUnlocked || !Room->bJetpackUnlocked) &&
            (Params.Key == EKeys::G || Params.Key == EKeys::V || Params.Key == EKeys::Tab)) return true;
    }
    auto* Hall=AMiningExhibitHall::Find(GetWorld());
    if (Hall && Hall->bReady)
    {
        if (Params.Key==EKeys::M) { if(Params.Event==IE_Pressed) Hall->ToggleMap(); return true; }
        if (Hall->IsMapOpen()) { if(Params.Key==EKeys::Escape && Params.Event==IE_Pressed) Hall->CloseMap(); return true; }
        if (Params.Key==EKeys::I) { if(Params.Event==IE_Pressed) Hall->ToggleCatalogue(); return true; }
        if (Params.Key==EKeys::F6) { if(Params.Event==IE_Pressed) Hall->ReturnToLobby(); return true; }
        if (Params.Key==EKeys::F7) { if(Params.Event==IE_Pressed) Hall->SkipCurrentRoom(); return true; }
        if (Params.Key==EKeys::E && Params.Event==IE_Pressed && !Hall->IsInteractionOpen() && Hall->Treasure && Hall->Treasure->IsAreaActive() && Hall->Treasure->CollectTarget(this)) return true;
        if (Params.Key==EKeys::E && Hall->ActiveArea==INDEX_NONE && !Hall->IsInteractionOpen())
        { if(Params.Event==IE_Pressed) Hall->InteractPortal(); return true; }
    }
    return Super::InputKey(Params);
}

AMiningExhibitHall::AMiningExhibitHall()
{
    PrimaryActorTick.bCanEverTick=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

AMiningExhibitHall* AMiningExhibitHall::Find(UWorld* World)
{
    if (World) for(TActorIterator<AMiningExhibitHall> Hall(World); Hall; ++Hall) return *Hall;
    return nullptr;
}

void AMiningExhibitHall::BeginPlay()
{
    Super::BeginPlay();
    SpawnHandle=GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateWeakLambda(this,[this](AActor* Actor){ PendingActors.Add(Actor); }));
    for(TActorIterator<AMiningCollectionField> Actor(GetWorld()); Actor; ++Actor) Collection=*Actor;
    for(TActorIterator<AMiningTreasureField> Actor(GetWorld()); Actor; ++Actor) Treasure=*Actor;
    for(TActorIterator<AMiningRoomEscape> Actor(GetWorld()); Actor; ++Actor) Journey=*Actor;
    for(TActorIterator<AMiningGiantRoomEntrance> Actor(GetWorld()); Actor; ++Actor) Entrance=*Actor;
    for(TActorIterator<AMiningCellUniverse> Actor(GetWorld()); Actor; ++Actor) Ending=*Actor;
    for(TActorIterator<APopupCloseStage> Actor(GetWorld()); Actor; ++Actor) Office=*Actor;
    for(TActorIterator<AWenSiPuzzle> Actor(GetWorld()); Actor; ++Actor) Museum=*Actor;
    for(TActorIterator<AWenSiMirrorPuzzle> Actor(GetWorld()); Actor; ++Actor) MirrorMuseum=*Actor;
}

bool AMiningExhibitHall::InitializeAreas()
{
    Controller=Cast<AMiningExhibitController>(GetWorld()->GetFirstPlayerController());
    if(!Controller.IsValid() || !Controller->GetPawn() || Areas.IsEmpty() || !Collection) return false;
    if(bLinearCampaign ? Collection->Items.Num()!=Collection->AuthoredPlacements.Num() : (Areas.Num()!=6 || Collection->Items.Num()!=5 || !Treasure || Treasure->Items.IsEmpty())) return false;
    auto* Mining=Controller->GetPawn()->FindComponentByClass<UMiningComponent>();
    if(!Mining) return false;
    auto* Progression=Controller->GetPawn()->FindComponentByClass<UMiningProgressionComponent>();
    if(!Progression || !Progression->InitializeProgression()) return false;
    ResumePoints.SetNum(Areas.Num());
    for(int32 Index=0; Index<Areas.Num(); ++Index) ResumePoints[Index]=Areas[Index].Entry;
    for(TActorIterator<AActor> Actor(GetWorld()); Actor; ++Actor) RegisterActor(*Actor);
    PendingActors.Reset();
    Guide=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.012f,.024f,.032f,.88f))
        .Padding(12).HAlign(HAlign_Left).VAlign(VAlign_Bottom)
        .Visibility_Lambda([this](){return IsInteractionOpen()?EVisibility::Collapsed:EVisibility::HitTestInvisible;})
        [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",16)).ColorAndOpacity(FLinearColor::White)
            .Text_Lambda([this](){return FText::FromString(StatusText());}).WrapTextAt(690)];
    auto Wrapper=SNew(SVerticalBox)
        +SVerticalBox::Slot().FillHeight(1)[SNullWidget::NullWidget]
        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(FMargin(18,0,18,220))[Guide.ToSharedRef()];
    Guide=Wrapper;
    GetGameInstance()->GetGameViewportClient()->AddViewportWidgetContent(Guide.ToSharedRef(),90);
    bReady=true;
    if(bLinearCampaign) EnterArea(0); else ReturnToLobby();
    return true;
}

void AMiningExhibitHall::RegisterActor(AActor* Actor)
{
    if(!IsValid(Actor) || Actor==this || Actor==Collection || Actor==Treasure || !Actor->GetRootComponent() || KnownActors.Contains(Actor) || Actor->ActorHasTag(TEXT("BM_PlayerEffect")) || Actor->IsA<APawn>() || Actor->IsA<AController>() || Actor->IsA<AHUD>() || Actor->IsA<APlayerCameraManager>() || (Collection && Actor==Collection->Preview)) return;
    int32 AreaIndex=INDEX_NONE;
    bool bExplicit=Actor->ActorHasTag(TEXT("BM_Hub"));
    for(int32 Index=0; Index<Areas.Num(); ++Index) if(Actor->ActorHasTag(Areas[Index].Tag)) { AreaIndex=Index; bExplicit=true; break; }
    for(AActor* OwningActor=Actor->GetOwner(); !bExplicit && OwningActor && OwningActor!=Actor; OwningActor=OwningActor->GetOwner())
        for(int32 Index=0; Index<Areas.Num(); ++Index) if(OwningActor->ActorHasTag(Areas[Index].Tag)) { AreaIndex=Index; bExplicit=true; break; }
    if(!bExplicit)
    {
        double Distance=FVector::DistSquared(Actor->GetActorLocation(),LobbyEntry.GetLocation());
        for(int32 Index=0; Index<Areas.Num(); ++Index)
        {
            const double Candidate=FVector::DistSquared(Actor->GetActorLocation(),Areas[Index].Origin);
            if(Candidate<Distance) { Distance=Candidate; AreaIndex=Index; }
        }
    }
    FAreaActor Entry;
    Entry.Actor=Actor; Entry.Area=AreaIndex;
    ManagedActors.Add(MoveTemp(Entry)); KnownActors.Add(Actor);
}

void AMiningExhibitHall::UpdateActorVisibility()
{
    for(auto& Entry:ManagedActors)
    {
        auto* Actor=Entry.Actor.Get(); if(!Actor) continue;
        const bool bActive=Entry.Area==ActiveArea;
        if(!bActive && !Entry.bSuspended)
        {
            Entry.bHidden=Actor->IsHidden(); Entry.bTick=Actor->IsActorTickEnabled();
            TArray<UPostProcessComponent*> Components; Actor->GetComponents(Components);
            Entry.PostProcesses.Reset();
            for(auto* Component:Components) { Entry.PostProcesses.Emplace(Component,bool(Component->bEnabled)); Component->bEnabled=false; }
            if(auto* Volume=Cast<APostProcessVolume>(Actor)) { Entry.bVolumeEnabled=Volume->bEnabled; Volume->bEnabled=false; }
            if(!bContinuousCampaign) Actor->SetActorHiddenInGame(true);
            Actor->SetActorTickEnabled(false); Entry.bSuspended=true;
        }
        else if(bActive && Entry.bSuspended)
        {
            if(!bContinuousCampaign) Actor->SetActorHiddenInGame(Entry.bHidden);
            Actor->SetActorTickEnabled(Entry.bTick);
            for(auto& Component:Entry.PostProcesses) if(Component.Key.IsValid()) Component.Key->bEnabled=Component.Value;
            if(auto* Volume=Cast<APostProcessVolume>(Actor)) Volume->bEnabled=Entry.bVolumeEnabled;
            Entry.bSuspended=false;
        }
    }
    Collection->SetActorTickEnabled(true);
}

void AMiningExhibitHall::CloseInteraction()
{
    CloseMap();
    CloseMenu();
    if(!Controller.IsValid()) return;
    Controller->SetUpgradeMenuHeld(false);
    if(Collection && Collection->bCatalogueOpen) Collection->ToggleCatalogue();
    if(Office) Office->CancelSession();
    if(Museum) Museum->SetAreaActive(false);
    if(MirrorMuseum) MirrorMuseum->SetAreaActive(false);
    Controller->CardInteraction->LeaveSeat();
    Controller->CardInteraction->ReleaseForExploration();
    Controller->EchoInteraction->SetExplorationActive(false);
    Controller->EncounterPolicy->Release();
    if(Journey) Journey->SetAreaActive(false);
    Controller->SetPause(false); Controller->ResetIgnoreInputFlags();
    Controller->bShowMouseCursor=false; Controller->SetInputMode(FInputModeGameOnly());
    if(APawn* Pawn=Controller->GetPawn())
    {
        Pawn->EnableInput(Controller.Get()); Pawn->SetActorEnableCollision(true);
        if(auto* Mining=Pawn->FindComponentByClass<UMiningComponent>()) { Mining->CancelMining(); Mining->SetInputSuspended(false); Mining->SetComponentTickEnabled(true); Mining->SetInputContextEnabled(true); }
        if(auto* Move=Pawn->FindComponentByClass<UMiningCharacterMovementComponent>()) { Move->SetFlightInputHeld(false); Move->StopMovementImmediately(); Move->SetMovementMode(MOVE_Falling); }
    }
}

bool AMiningExhibitHall::EnterArea(int32 Index, bool bMovePlayer)
{
    if(!bReady || !Controller.IsValid() || !Controller->GetPawn() || (Index!=INDEX_NONE && !Areas.IsValidIndex(Index))) return false;
    if(bLinearCampaign && (Index<0 || Index>UnlockedArea)) return false;
    if(Areas.IsValidIndex(Index) && AreaKind(Index)==EMiningExperienceKind::Office && Office && Office->IsOfficeEscaped()) return false;
    if(Areas.IsValidIndex(ActiveArea) && (!Controller->IsMoveInputIgnored() || Menu.IsValid() || Collection->bCatalogueOpen || Controller->bUpgradeMenuHeld)) ResumePoints[ActiveArea]=Controller->GetPawn()->GetActorTransform();
    auto* Movement=Controller->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>();
    const FVector Velocity=Movement?Movement->Velocity:FVector::ZeroVector;
    const EMovementMode Mode=Movement?Movement->MovementMode.GetValue():MOVE_Falling;
    CloseInteraction();
    ActiveArea=Index;
    CompletionDelay=0;
    const auto Kind=AreaKind(Index);
    UpdateActorVisibility();
    Collection->SetAreaActive(Kind==EMiningExperienceKind::Collection || (bLinearCampaign && Kind==EMiningExperienceKind::Mining));
    if(Treasure)
    {
        Treasure->ActiveAreaTag=Areas.IsValidIndex(Index)?Areas[Index].Tag:NAME_None;
        Treasure->SetActorTickEnabled(true);
        Treasure->SetAreaActive(Kind==EMiningExperienceKind::Collection || (bContinuousCampaign && Kind==EMiningExperienceKind::Mining));
    }
    if(Office) Office->SetAreaActive(Kind==EMiningExperienceKind::Office);
    if(Museum) Museum->SetAreaActive(Kind==EMiningExperienceKind::Museum);
    if(Ending) Ending->SetAreaActive(Kind==EMiningExperienceKind::Ending);
    const FTransform Destination=Index==INDEX_NONE?LobbyEntry:(bContinuousCampaign?Areas[Index].Entry:ResumePoints[Index]);
    if(bMovePlayer)
    {
        AGeneratedMiningRock::PrepareTerrainAt(GetWorld(), Destination.GetLocation());
        Controller->GetPawn()->SetActorTransform(Destination,false,nullptr,ETeleportType::TeleportPhysics);
        Controller->SetControlRotation(Destination.Rotator());
    }
    else if(Movement) { Movement->Velocity=Velocity; Movement->SetMovementMode(Mode); }
    auto* Mining=Controller->GetPawn()->FindComponentByClass<UMiningComponent>();
    Mining->FallResetDistance=0; Mining->bScopedReset=true;
    Mining->ResetAreaTag=bContinuousCampaign && Areas.IsValidIndex(Index)?Areas[Index].Tag:NAME_None;
    const FVector Center=Index==INDEX_NONE?LobbyEntry.GetLocation():Areas[Index].Origin;
    Mining->ResetBounds=FBox(Center-FVector(30000,30000,50000),Center+FVector(Kind==EMiningExperienceKind::Ending?200000:60000,30000,50000));
    if(bContinuousCampaign && Areas.IsValidIndex(Index) && Areas[Index].MiningBounds.IsValid) Mining->ResetBounds=Areas[Index].MiningBounds;
    ApplyUpgrades();
    if(MirrorMuseum) MirrorMuseum->SetAreaActive(Kind==EMiningExperienceKind::Museum);
    if(Kind==EMiningExperienceKind::Journey)
    {
        Controller->EncounterPolicy->Apply(Controller->GetPawn());
        Controller->EchoInteraction->SetExplorationActive(true);
        if(Journey) Journey->SetAreaActive(true);
    }
    Controller->SetMiningGUIEnabled(true);
    if(Index!=INDEX_NONE) Visited.Add(Index);
    Notice.Empty();
    return true;
}

void AMiningExhibitHall::ReturnToLobby()
{
    if(!bLinearCampaign) { EnterArea(INDEX_NONE); return; }
    if(!Areas.IsValidIndex(ActiveArea) || !Controller.IsValid()) return;
    // A rescue returns to this section's entrance without resetting the run.
    const int32 Index=ActiveArea;
    if(AreaKind(Index)==EMiningExperienceKind::Office && Office && Office->IsOfficeEscaped() && Areas.IsValidIndex(Index+1))
    {
        UnlockedArea=FMath::Max(UnlockedArea,Index+1);EnterArea(Index+1);return;
    }
    ActiveArea=INDEX_NONE;
    ResumePoints[Index]=Areas[Index].Entry;
    EnterArea(Index);
}

bool AMiningExhibitHall::SkipCurrentRoom()
{
    const auto Kind=AreaKind(ActiveArea);
    if(!bLinearCampaign || !Areas.IsValidIndex(ActiveArea+1) ||
       (Kind!=EMiningExperienceKind::Office && Kind!=EMiningExperienceKind::Journey && Kind!=EMiningExperienceKind::Museum)) return false;
    const int32 Skipped=ActiveArea;
    CompletedAreas.Add(Skipped);
    if(Areas[Skipped].ProgressGate)
    {Areas[Skipped].ProgressGate->SetActorHiddenInGame(true);Areas[Skipped].ProgressGate->SetActorEnableCollision(false);}
    UnlockedArea=FMath::Max(UnlockedArea,Skipped+1);
    return EnterArea(Skipped+1);
}

EMiningExperienceKind AMiningExhibitHall::AreaKind(int32 Index) const
{
    if(!Areas.IsValidIndex(Index)) return EMiningExperienceKind::Legacy;
    if(Areas[Index].Kind!=EMiningExperienceKind::Legacy) return Areas[Index].Kind;
    const EMiningExperienceKind Legacy[]={EMiningExperienceKind::Mining,EMiningExperienceKind::Collection,EMiningExperienceKind::Journey,EMiningExperienceKind::Office,EMiningExperienceKind::Museum,EMiningExperienceKind::Ending};
    return Index<UE_ARRAY_COUNT(Legacy)?Legacy[Index]:EMiningExperienceKind::Legacy;
}

bool AMiningExhibitHall::CanAdvanceCampaign() const
{
    if(!bLinearCampaign || !Areas.IsValidIndex(ActiveArea) || !Controller.IsValid() || !Controller->GetPawn() || IsInteractionOpen()) return false;
    switch(AreaKind(ActiveArea))
    {
    case EMiningExperienceKind::Mining:
        return bContinuousCampaign || FBox(Areas[ActiveArea].Exit-Areas[ActiveArea].ExitExtent,Areas[ActiveArea].Exit+Areas[ActiveArea].ExitExtent).IsInsideOrOn(Controller->GetPawn()->GetActorLocation());
    case EMiningExperienceKind::Office: return Office && (bContinuousCampaign ? Office->GetCompletedSessions()>0 : Office->IsOfficeEscaped()) && !Office->IsSessionActive();
    case EMiningExperienceKind::Journey: return Journey && Journey->Phase==EMiningEscapePhase::Escaped;
    case EMiningExperienceKind::Museum: return (MirrorMuseum && MirrorMuseum->bComplete) || (Museum && Museum->bComplete);
    case EMiningExperienceKind::Ending: return Ending && Ending->bComplete;
    default: return false;
    }
}

void AMiningExhibitHall::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!bReady) { InitializeAreas(); return; }
    if(!PendingActors.IsEmpty())
    {
        auto Pending=MoveTemp(PendingActors); PendingActors.Reset();
        for(auto& Actor:Pending) if(Actor.IsValid()) RegisterActor(Actor.Get());
        UpdateActorVisibility();
    }
    auto* GUI=Controller.IsValid()?Controller->MiningGUI.Get():nullptr;
    if(GUI)
    {
        if(BoundGUI.Get()!=GUI)
        {
            GUI->OnUpgradeRequested.AddUniqueDynamic(this,&AMiningExhibitHall::OnUpgradeRequested); BoundGUI=GUI;
            GUI->EnableProgressionPanel();
            GUI->SetAbilityAvailable(TEXT("G"),true);
            ApplyUpgrades();
        }
        auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
        GUI->SetCoinDisplay(FText::AsNumber(Wallet->Balance));
        if(auto* Progression=Controller->GetPawn()->FindComponentByClass<UMiningProgressionComponent>())
            for(int32 Index=0; Index<4 && Progression->Tracks.IsValidIndex(Index); ++Index)
            {
                GUI->SetItemLevel(Index, Progression->GetLevel(Index), false);
                GUI->SetUpgradeDetails(Index, Progression->Tracks[Index].Name, Progression->GetNextDescription(Index),
                    Progression->GetNextPrice(Index), Wallet->Balance>=Progression->GetNextPrice(Index));
            }
        if(Treasure) Treasure->Coins=Wallet->Balance;
    }
    if(bContinuousCampaign && Areas.IsValidIndex(ActiveArea) && !IsInteractionOpen())
    {
        if(CanAdvanceCampaign()) CompletedAreas.Add(ActiveArea);
        for(int32 I:CompletedAreas) if(Areas[I].ProgressGate)
        { Areas[I].ProgressGate->SetActorHiddenInGame(true); Areas[I].ProgressGate->SetActorEnableCollision(false); }
        const FVector P=Controller->GetPawn()->GetActorLocation();
        const auto NearThreshold=[&](int32 I){return FMath::Abs(P.Y-Areas[I].Exit.Y)<Areas[I].ExitExtent.Y && FMath::Abs(P.Z-Areas[I].Exit.Z)<Areas[I].ExitExtent.Z;};
        if(ActiveArea>0 && P.X<Areas[ActiveArea-1].Exit.X-100 && NearThreshold(ActiveArea-1)) EnterArea(ActiveArea-1,false);
        else if(CompletedAreas.Contains(ActiveArea))
        {
            if(ActiveArea==Areas.Num()-1) bCampaignComplete=true;
            else if(P.X>Areas[ActiveArea].Exit.X && (NearThreshold(ActiveArea) ||
                (Areas[ActiveArea+1].ActivationBounds.IsValid && Areas[ActiveArea+1].ActivationBounds.IsInsideOrOn(P))))
            { UnlockedArea=FMath::Max(UnlockedArea,ActiveArea+1); EnterArea(ActiveArea+1,false); }
        }
    }
    else if(bLinearCampaign && !bContinuousCampaign && !bCampaignComplete)
    {
        CompletionDelay=CanAdvanceCampaign()?CompletionDelay+DeltaSeconds:0;
        const float Delay=AreaKind(ActiveArea)==EMiningExperienceKind::Mining?.25f:3.f;
        if(CompletionDelay>=Delay)
        {
            if(Areas.IsValidIndex(ActiveArea+1))
            {
                UnlockedArea=FMath::Max(UnlockedArea,ActiveArea+1);
                EnterArea(ActiveArea+1);
                if(Controller->PlayerCameraManager) Controller->PlayerCameraManager->StartCameraFade(1,0,.6f,FLinearColor::Black,false,false);
            }
            else bCampaignComplete=true;
        }
    }
}

void AMiningExhibitHall::ApplyUpgrades()
{
    if(!Controller.IsValid() || !Controller->GetPawn()) return;
    if(auto* Progression=Controller->GetPawn()->FindComponentByClass<UMiningProgressionComponent>()) Progression->ApplyUpgrades();
}

bool AMiningExhibitHall::PurchaseUpgrade(int32 ItemIndex, int32 RequestedLevel)
{
    if(!bReady || !Controller.IsValid() || !Controller->GetPawn()) return false;
    auto* Progression=Controller->GetPawn()->FindComponentByClass<UMiningProgressionComponent>();
    const bool bPurchased = Progression && Progression->Purchase(ItemIndex,RequestedLevel);
    if(auto* Audio=Controller->GetPawn()->FindComponentByClass<UMiningAudioComponent>()) Audio->PlayPurchase(bPurchased);
    if(!bPurchased) { Notice=TEXT("无法升级：检查金币余额、等级顺序和配置。"); return false; }
    Notice=TEXT("升级完成，能力与金币在所有体验区共用。");
    return true;
}
void AMiningExhibitHall::OnUpgradeRequested(int32 ItemIndex, int32 RequestedLevel) { PurchaseUpgrade(ItemIndex,RequestedLevel); }

bool AMiningExhibitHall::IsInteractionOpen() const
{
    return Map.IsValid() || Menu.IsValid() || !Controller.IsValid() || Controller->IsPaused() || Controller->bUpgradeMenuHeld || Controller->bShowMouseCursor || Controller->IsLookInputIgnored();
}

void AMiningExhibitHall::ToggleCatalogue()
{
    if(Collection && (Collection->bCatalogueOpen || !IsInteractionOpen())) Collection->ToggleCatalogue();
}

void AMiningExhibitHall::InteractPortal()
{
    if(!Controller.IsValid()) return;
    const FVector Position=Controller->GetPawn()->GetActorLocation();
    for(int32 Index=0; Index<Areas.Num(); ++Index)
    {
        const FVector Portal=LobbyEntry.GetLocation()+FVector(550+(Index/3)*650,((Index%3)-1)*650,-90);
        if(FVector::Dist2D(Position,Portal)<280) { EnterArea(Index); return; }
    }
}

FString AMiningExhibitHall::StatusText() const
{
    if(bLinearCampaign)
    {
        FString Hint=Areas.IsValidIndex(ActiveArea)?Areas[ActiveArea].Instructions.ToString():FString();
        if(Collection && Collection->Target) Hint=AMiningCollectible::ItemName(Collection->Target->ItemId)+TEXT(" · E 收集");
        else if(Collection && Collection->ToastRemaining>0 && Collection->IsAreaActive()) Hint=Collection->Toast;
        if(bContinuousCampaign && CompletedAreas.Contains(ActiveArea) && AreaKind(ActiveArea)!=EMiningExperienceKind::Mining) Hint=TEXT("本段完成，出口已打开；沿通道步行前往下一段。");
        else if(CompletionDelay>0 && AreaKind(ActiveArea)!=EMiningExperienceKind::Mining) Hint=TEXT("本段完成，正在前往下一段……");
        if(AreaKind(ActiveArea)==EMiningExperienceKind::Ending && Ending) Hint=Ending->Hint();
        if(Treasure && Treasure->IsAreaActive() && Treasure->Target) Hint=Treasure->GetPrompt();
        if(bCampaignComplete) Hint=TEXT("全流程完成。可按 F4 回访已到达的区域，补齐藏品。");
        return FString::Printf(TEXT("%d / %d · %s    藏品 %d / 4\n%s\nM 地图   I 图鉴   Tab 升级   F4 已到达区域   F6 本段入口\nF7 跳过当前小游戏（保留藏品）"),ActiveArea+1,Areas.Num(),Areas.IsValidIndex(ActiveArea)?*Areas[ActiveArea].Title.ToString():TEXT(""),Collection?Collection->UniqueCount():0,*Hint);
    }
    const FString Title=Areas.IsValidIndex(ActiveArea)?Areas[ActiveArea].Title.ToString():TEXT("六区体验大厅 · 靠近入口按 E，或按 F4 选择体验区");
    FString Hint=Areas.IsValidIndex(ActiveArea)?Areas[ActiveArea].Instructions.ToString():TEXT("六个体验区共存于同一 Level；切区保留角色、金币和收藏。");
    if(ActiveArea==1 && Treasure && Treasure->Target) Hint=Treasure->GetPrompt();
    else if(ActiveArea==1 && Collection && Collection->ToastRemaining>0) Hint=Collection->Toast;
    else if(ActiveArea==5 && Ending) Hint=Ending->Hint();
    return Title+TEXT("\n")+Hint+TEXT("\nTab 升级   I 图鉴   F4 体验区   F6 返回大厅")+(Notice.IsEmpty()?FString():TEXT("\n")+Notice);
}

bool AMiningExhibitHall::ToggleMenu()
{
    if(Menu) { CloseMenu(); return true; }
    if(!bReady || IsInteractionOpen()) return false;
    auto Choices=SNew(SVerticalBox);
    for(int32 Index=0; Index<Areas.Num(); ++Index)
        Choices->AddSlot().AutoHeight().Padding(0,5)
        [SNew(SButton).IsEnabled(!bLinearCampaign || Index<=UnlockedArea).ContentPadding(16).OnClicked_Lambda([this,Index](){EnterArea(Index);return FReply::Handled();})
            [SNew(STextBlock).Text(Areas[Index].Title).Font(FCoreStyle::GetDefaultFontStyle("Bold",22)).ColorAndOpacity(FLinearColor::White)]];
    Choices->AddSlot().AutoHeight().Padding(0,5)
        [SNew(SButton).ContentPadding(16).Text(FText::FromString(bLinearCampaign?TEXT("返回本段入口"):TEXT("返回体验大厅"))).OnClicked_Lambda([this](){ReturnToLobby();return FReply::Handled();})];
    Menu=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.01f,.024f,.035f,.97f))
        .Padding(24).HAlign(HAlign_Center)
        [SNew(SBox).WidthOverride(800)
            [SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(10)[SNew(STextBlock).Text(FText::FromString(bLinearCampaign?TEXT("全流程试玩 · 已到达的区域"):TEXT("同一地图 · 六个体验区"))).Font(FCoreStyle::GetDefaultFontStyle("Bold",28)).ColorAndOpacity(FLinearColor::White)]
                +SVerticalBox::Slot().AutoHeight().Padding(10)[SNew(STextBlock).Text(FText::FromString(TEXT("只移动当前角色，不切换地图。金币、收藏与升级跨区保留。"))).AutoWrapText(true).ColorAndOpacity(FLinearColor(.7f,.85f,.85f))]
                +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[Choices]]
                +SVerticalBox::Slot().AutoHeight().Padding(10)[SNew(SButton).ContentPadding(12).Text(FText::FromString(TEXT("继续当前体验 · F4 / Esc"))).OnClicked_Lambda([this](){CloseMenu();return FReply::Handled();})]]];
    Controller->SetIgnoreLookInput(true); Controller->SetIgnoreMoveInput(true); Controller->SetPause(true);
    Controller->GetPawn()->FindComponentByClass<UMiningComponent>()->SetInputSuspended(true);
    if(auto* Move=Controller->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>()) Move->SetFlightInputHeld(false);
    GetGameInstance()->GetGameViewportClient()->AddViewportWidgetContent(Menu.ToSharedRef(),1100);
    FInputModeUIOnly Mode; Mode.SetWidgetToFocus(Choices->GetChildren()->GetChildAt(0)); Controller->SetInputMode(Mode); Controller->bShowMouseCursor=true;
    return true;
}

void AMiningExhibitHall::CloseMenu()
{
    if(!Menu) return;
    if(auto* Viewport=GetGameInstance()->GetGameViewportClient()) Viewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
    Menu.Reset();
    if(Controller.IsValid())
    {
        Controller->SetPause(false); Controller->SetIgnoreLookInput(false); Controller->SetIgnoreMoveInput(false);
        Controller->SetInputMode(FInputModeGameOnly()); Controller->bShowMouseCursor=false;
        if(Controller->GetPawn()) Controller->GetPawn()->FindComponentByClass<UMiningComponent>()->SetInputSuspended(false);
    }
}

void AMiningExhibitHall::EndPlay(const EEndPlayReason::Type Reason)
{
    CloseMap();
    CloseMenu();
    if(Guide && GetGameInstance()) if(auto* Viewport=GetGameInstance()->GetGameViewportClient()) Viewport->RemoveViewportWidgetContent(Guide.ToSharedRef());
    if(BoundGUI.IsValid()) BoundGUI->OnUpgradeRequested.RemoveAll(this);
    GetWorld()->RemoveOnActorSpawnedHandler(SpawnHandle);
    Guide.Reset();
    Super::EndPlay(Reason);
}
