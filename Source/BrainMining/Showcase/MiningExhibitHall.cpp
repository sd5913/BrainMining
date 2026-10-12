#include "Showcase/MiningExhibitHall.h"
#include "Showcase/MiningTreasure.h"
#include "MiningCollection.h"
#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningCellUniverse.h"
#include "MiningRoomEscape.h"
#include "MiningGiantRoomEntrance.h"
#include "MiningGUIWidgets.h"
#include "MiningWalletSubsystem.h"
#include "PopupClose/PopupCloseStage.h"
#include "WenSi/WenSiPuzzle.h"
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
}

bool AMiningExhibitController::InputKey(const FInputKeyEventArgs& Params)
{
    auto* Hall=AMiningExhibitHall::Find(GetWorld());
    if (Hall && Hall->bReady)
    {
        if (Params.Key==EKeys::I) { if(Params.Event==IE_Pressed) Hall->ToggleCatalogue(); return true; }
        if (Params.Key==EKeys::F6) { if(Params.Event==IE_Pressed) Hall->ReturnToLobby(); return true; }
        if (Params.Key==EKeys::E && Params.Event==IE_Pressed && Hall->ActiveArea==1 && !Hall->IsInteractionOpen() && Hall->Treasure && Hall->Treasure->CollectTarget(this)) return true;
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
}

bool AMiningExhibitHall::InitializeAreas()
{
    Controller=Cast<AMiningExhibitController>(GetWorld()->GetFirstPlayerController());
    if(!Controller.IsValid() || !Controller->GetPawn() || Areas.Num()!=6 || !Collection || Collection->Items.Num()!=5 || !Treasure || Treasure->Items.IsEmpty()) return false;
    auto* Mining=Controller->GetPawn()->FindComponentByClass<UMiningComponent>();
    if(!Mining) return false;
    BaseRadius=Mining->Radius; BaseInterval=Mining->Interval;
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
    ReturnToLobby();
    return true;
}

void AMiningExhibitHall::RegisterActor(AActor* Actor)
{
    if(!IsValid(Actor) || Actor==this || Actor==Collection || !Actor->GetRootComponent() || KnownActors.Contains(Actor) || Actor->ActorHasTag(TEXT("BM_PlayerEffect")) || Actor->IsA<APawn>() || Actor->IsA<AController>() || Actor->IsA<AHUD>() || Actor->IsA<APlayerCameraManager>() || (Collection && Actor==Collection->Preview)) return;
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
            Actor->SetActorHiddenInGame(true); Actor->SetActorTickEnabled(false); Entry.bSuspended=true;
        }
        else if(bActive && Entry.bSuspended)
        {
            Actor->SetActorHiddenInGame(Entry.bHidden); Actor->SetActorTickEnabled(Entry.bTick);
            for(auto& Component:Entry.PostProcesses) if(Component.Key.IsValid()) Component.Key->bEnabled=Component.Value;
            if(auto* Volume=Cast<APostProcessVolume>(Actor)) Volume->bEnabled=Entry.bVolumeEnabled;
            Entry.bSuspended=false;
        }
    }
    Collection->SetActorTickEnabled(true);
}

void AMiningExhibitHall::CloseInteraction()
{
    CloseMenu();
    if(!Controller.IsValid()) return;
    Controller->SetUpgradeMenuHeld(false);
    if(Collection && Collection->bCatalogueOpen) Collection->ToggleCatalogue();
    if(Office) Office->CancelSession();
    if(Museum) Museum->SetAreaActive(false);
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

bool AMiningExhibitHall::EnterArea(int32 Index)
{
    if(!bReady || !Controller.IsValid() || !Controller->GetPawn() || (Index!=INDEX_NONE && !Areas.IsValidIndex(Index))) return false;
    if(Areas.IsValidIndex(ActiveArea) && (!Controller->IsMoveInputIgnored() || Menu.IsValid() || Collection->bCatalogueOpen || Controller->bUpgradeMenuHeld)) ResumePoints[ActiveArea]=Controller->GetPawn()->GetActorTransform();
    CloseInteraction();
    ActiveArea=Index;
    UpdateActorVisibility();
    Collection->SetAreaActive(Index==1); Treasure->SetAreaActive(Index==1);
    if(Office) Office->SetAreaActive(Index==3);
    if(Museum) Museum->SetAreaActive(Index==4);
    if(Ending) Ending->SetAreaActive(Index==5);
    const FTransform Destination=Index==INDEX_NONE?LobbyEntry:ResumePoints[Index];
    Controller->GetPawn()->SetActorTransform(Destination,false,nullptr,ETeleportType::TeleportPhysics);
    Controller->SetControlRotation(Destination.Rotator());
    auto* Mining=Controller->GetPawn()->FindComponentByClass<UMiningComponent>();
    Mining->FallResetDistance=0; Mining->bScopedReset=true;
    const FVector Center=Index==INDEX_NONE?LobbyEntry.GetLocation():Areas[Index].Origin;
    Mining->ResetBounds=FBox(Center-FVector(30000,30000,50000),Center+FVector(Index==5?200000:60000,30000,50000));
    ApplyUpgrades();
    if(Index==2)
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

void AMiningExhibitHall::ReturnToLobby() { EnterArea(INDEX_NONE); }

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
            GUI->PickaxeUpgrade->bPreviewUpgrades=false;
            GUI->VortexUpgrade->bPreviewUpgrades=false; GUI->BlastUpgrade->bPreviewUpgrades=false;
            GUI->SetItemUnlocked(0,true); GUI->SetItemUnlocked(1,false); GUI->SetItemUnlocked(2,false);
            GUI->SetAbilityAvailable(TEXT("G"),true);
            ApplyUpgrades();
        }
        auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
        GUI->SetCoinDisplay(FText::AsNumber(Wallet->Balance));
        const int32 Level=Wallet->PickaxeLevel;
        GUI->PickaxeUpgrade->SetPriceDisplay(UpgradePrices.IsValidIndex(Level)?FText::AsNumber(UpgradePrices[Level]):FText::FromString(TEXT("MAX")));
        Treasure->Coins=Wallet->Balance;
    }
}

void AMiningExhibitHall::ApplyUpgrades()
{
    if(!Controller.IsValid() || !Controller->GetPawn()) return;
    const int32 Level=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>()->PickaxeLevel;
    if(auto* Mining=Controller->GetPawn()->FindComponentByClass<UMiningComponent>())
    { Mining->Radius=BaseRadius*(1.f+.05f*Level); Mining->Interval=BaseInterval*(1.f-.1f*Level); }
    if(Controller->MiningGUI) Controller->MiningGUI->SetItemLevel(0,Level,false);
}

bool AMiningExhibitHall::PurchaseUpgrade(int32 ItemIndex, int32 RequestedLevel)
{
    auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
    if(ItemIndex!=0 || RequestedLevel!=Wallet->PickaxeLevel+1 || RequestedLevel>3 || !UpgradePrices.IsValidIndex(Wallet->PickaxeLevel)) return false;
    if(!Wallet->Spend(UpgradePrices[Wallet->PickaxeLevel])) { Notice=TEXT("金币不足：在收集与宝物区挖取矿物后再升级。"); return false; }
    Wallet->PickaxeLevel=RequestedLevel; ApplyUpgrades();
    Notice=TEXT("升级完成：镐子切割范围增加，挖掘间隔缩短。所有体验区共用。");
    return true;
}
void AMiningExhibitHall::OnUpgradeRequested(int32 ItemIndex, int32 RequestedLevel) { PurchaseUpgrade(ItemIndex,RequestedLevel); }

bool AMiningExhibitHall::IsInteractionOpen() const
{
    return Menu.IsValid() || !Controller.IsValid() || Controller->IsPaused() || Controller->bUpgradeMenuHeld || Controller->bShowMouseCursor || Controller->IsLookInputIgnored();
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
        [SNew(SButton).ContentPadding(16).OnClicked_Lambda([this,Index](){EnterArea(Index);return FReply::Handled();})
            [SNew(STextBlock).Text(Areas[Index].Title).Font(FCoreStyle::GetDefaultFontStyle("Bold",22)).ColorAndOpacity(FLinearColor::White)]];
    Choices->AddSlot().AutoHeight().Padding(0,5)
        [SNew(SButton).ContentPadding(16).Text(FText::FromString(TEXT("返回体验大厅"))).OnClicked_Lambda([this](){ReturnToLobby();return FReply::Handled();})];
    Menu=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.01f,.024f,.035f,.97f))
        .Padding(24).HAlign(HAlign_Center)
        [SNew(SBox).WidthOverride(800)
            [SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(10)[SNew(STextBlock).Text(FText::FromString(TEXT("同一地图 · 六个体验区"))).Font(FCoreStyle::GetDefaultFontStyle("Bold",28)).ColorAndOpacity(FLinearColor::White)]
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
    CloseMenu();
    if(Guide && GetGameInstance()) if(auto* Viewport=GetGameInstance()->GetGameViewportClient()) Viewport->RemoveViewportWidgetContent(Guide.ToSharedRef());
    if(BoundGUI.IsValid()) BoundGUI->OnUpgradeRequested.RemoveAll(this);
    GetWorld()->RemoveOnActorSpawnedHandler(SpawnHandle);
    Guide.Reset();
    Super::EndPlay(Reason);
}
