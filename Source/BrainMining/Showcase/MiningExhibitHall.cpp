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
#include "StartRoom/MiningOpening.h"
#include "PopupClose/PopupCloseStage.h"
#include "WenSi/WenSiPuzzle.h"
#include "WenSi/WenSiMirrorPuzzle.h"
#include "WenSi/MouseMuseumPortal.h"
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
#include "InputCoreTypes.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

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
    if(AMiningOpening::IsOpening(GetWorld()))return true;
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
        // I is part of the museum's hidden SKIP chord. Consume that press so
        // it cannot open the catalogue while the three other keys are held.
        if (Params.Key==EKeys::I && Params.Event==IE_Pressed && Hall->IsMuseumSkipChordCandidate()) return true;
        if (Params.Key==EKeys::I) { if(Params.Event==IE_Pressed) Hall->ToggleCatalogue(); return true; }
        if (!IsPaused() && Hall->HandleMirrorInput(Params)) return true;
        if (Params.Key==EKeys::F6) { if(Params.Event==IE_Pressed) Hall->ReturnToLobby(); return true; }
        if (Params.Key==EKeys::F7) { if(Params.Event==IE_Pressed) Hall->SkipCurrentRoom(); return true; }
        if (Params.Key==EKeys::E && Params.Event==IE_Pressed && !Hall->IsInteractionOpen() && Hall->Treasure && Hall->Treasure->IsAreaActive() && Hall->Treasure->CollectTarget(this)) return true;
        if (Params.Key==EKeys::E && Hall->ActiveArea==INDEX_NONE && !Hall->IsInteractionOpen())
        { if(Params.Event==IE_Pressed) Hall->InteractPortal(); return true; }
    }
    return Super::InputKey(Params);
}

bool AMiningExhibitHall::HandleMirrorInput(const FInputKeyEventArgs& Params)
{
    if (!MirrorMuseum || !MirrorMuseum->bAreaActive || IsInteractionOpen()) return false;
    if (Params.Key==EKeys::E && MirrorMuseum->MousePortal)
    {
        auto* Portal=MirrorMuseum->MousePortal.Get();
        if(Portal->IsEntering())return true;
        Portal->RefreshFocus();
        if(!Portal->IsFocused())return false;
        if(Params.Event==IE_Pressed)Portal->TryEnter();
        return true;
    }
    if (Params.Key!=EKeys::R && Params.Key!=EKeys::Q) return false;
    if (Params.Event==IE_Pressed)
    {
        if (Params.Key==EKeys::R) MirrorMuseum->SwapThoughts();
        else MirrorMuseum->CloseThoughts();
    }
    return true;
}

bool AMiningExhibitHall::IsMuseumSkipChordCandidate() const
{
    return bLinearCampaign && Areas.IsValidIndex(ActiveArea) &&
        AreaKind(ActiveArea)==EMiningExperienceKind::Museum && MirrorMuseum &&
        MirrorMuseum->bAreaActive && Controller.IsValid() &&
        // S and K are enough to reserve I for the chord while the user is
        // still pressing the four keys in sequence; TickMuseumSkipChord
        // requires all four keys for the actual three-second activation.
        Controller->IsInputKeyDown(EKeys::S) && Controller->IsInputKeyDown(EKeys::K);
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
    bReady=true;
    if(bLinearCampaign) EnterArea(0); else ReturnToLobby();
    AMiningOpening::EnsureMainGameOpening(this);
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
        if(auto* Move=Pawn->FindComponentByClass<UMiningCharacterMovementComponent>()) { Move->ResetFlightInput(); Move->StopMovementImmediately(); Move->SetMovementMode(MOVE_Falling); }
    }
}

void AMiningExhibitHall::BeginTerrainWarmup(const FVector& Center)
{
    if (!GetWorld()) return;
    TerrainWarmupCenter=Center;
    TerrainWarmupStep=0;
    GetWorld()->GetTimerManager().ClearTimer(TerrainWarmupTimer);
    // Keep the same destination queued while budgeted activation catches up.
    GetWorld()->GetTimerManager().SetTimer(TerrainWarmupTimer,
        FTimerDelegate::CreateWeakLambda(this,[this]()
        {
            AGeneratedMiningRock::PrewarmTerrainAt(GetWorld(),TerrainWarmupCenter,5000.f);
            if(++TerrainWarmupStep>=300)GetWorld()->GetTimerManager().ClearTimer(TerrainWarmupTimer);
        }),.016f,true);
}

bool AMiningExhibitHall::EnterArea(int32 Index, bool bMovePlayer)
{
    if(!bReady || !Controller.IsValid() || !Controller->GetPawn() || (Index!=INDEX_NONE && !Areas.IsValidIndex(Index))) return false;
    if(bLinearCampaign && (Index<0 || Index>UnlockedArea)) return false;
    if(Areas.IsValidIndex(Index) && AreaKind(Index)==EMiningExperienceKind::Office && Office && Office->IsOfficeEscaped()) return false;
    if(Areas.IsValidIndex(ActiveArea) && (!Controller->IsMoveInputIgnored() || Collection->bCatalogueOpen || Controller->bUpgradeMenuHeld)) ResumePoints[ActiveArea]=Controller->GetPawn()->GetActorTransform();
    auto* Movement=Controller->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>();
    const FVector Velocity=Movement?Movement->Velocity:FVector::ZeroVector;
    const EMovementMode Mode=Movement?Movement->MovementMode.GetValue():MOVE_Falling;
    CloseInteraction();
    ActiveArea=Index;
    CompletionDelay=0;
    const auto Kind=AreaKind(Index);
    UpdateActorVisibility();
    const bool bAreaHasCollectibles=Areas.IsValidIndex(Index) && Collection->AuthoredPlacements.ContainsByPredicate([&](const FMiningCollectionPlacement& P){return P.AreaTag==Areas[Index].Tag;});
    Collection->SetAreaActive(Kind==EMiningExperienceKind::Collection || (bLinearCampaign && (Kind==EMiningExperienceKind::Mining || bAreaHasCollectibles)));
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
        // Teleports require immediate collision only around the landing point.
        AGeneratedMiningRock::PrepareTerrainAt(GetWorld(),Destination.GetLocation(),1400.f);
        Controller->GetPawn()->SetActorTransform(Destination,false,nullptr,ETeleportType::TeleportPhysics);
        Controller->SetControlRotation(Destination.Rotator());
        BeginTerrainWarmup(Destination.GetLocation());
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
    return true;
}

bool AMiningExhibitHall::EnterDeveloperArea(int32 Index, FString& Error)
{
    if (!bReady || !Controller.IsValid() || !Controller->GetPawn() || !Areas.IsValidIndex(Index))
    {
        Error = TEXT("房间尚未就绪，或目标区域不存在。");
        return false;
    }
    if (AreaKind(Index) == EMiningExperienceKind::Office && Office && Office->IsOfficeEscaped())
    {
        Error = TEXT("办公室已完成撤离，请选择下方的独立办公室重新测试。");
        return false;
    }
    FVector Destination = Areas[Index].Entry.GetLocation();
    const FRotator Rotation = Areas[Index].Entry.Rotator();
    AGeneratedMiningRock::PrepareTerrainAt(GetWorld(), Destination);
    bool bFoundDestination = false;
    for (const float HeightOffset : {0.f, 25.f, 50.f, 100.f, 150.f, 225.f, 300.f})
    {
        FVector Candidate = Areas[Index].Entry.GetLocation() + FVector(0, 0, HeightOffset);
        if (GetWorld()->FindTeleportSpot(Controller->GetPawn(), Candidate, Rotation))
        {
            Destination = Candidate;
            bFoundDestination = true;
            break;
        }
    }
    if (!bFoundDestination)
    {
        Error = TEXT("房间入口被碰撞阻挡，未移动角色。可以使用下方的独立关卡。");
        return false;
    }
    const int32 PreviousUnlockedArea = UnlockedArea;
    UnlockedArea = FMath::Max(UnlockedArea, Index);
    if (!EnterArea(Index, false))
    {
        UnlockedArea = PreviousUnlockedArea;
        Error = TEXT("房间激活失败，未移动角色。");
        return false;
    }
    Controller->GetPawn()->SetActorLocationAndRotation(Destination, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    Controller->SetControlRotation(Rotation);
    if (auto* Movement = Controller->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>())
    {
        Movement->ResetFlightInput();
        Movement->StopMovementImmediately();
        Movement->SetMovementMode(MOVE_Falling);
    }
    if (auto* StartRoom = AMiningStartRoom::Find(GetWorld())) StartRoom->UnlockDeveloperEquipment();
    Error.Empty();
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
        return (bContinuousCampaign && !(ActorHasTag(TEXT("BM_MainGame_RoomsLinked")) && ActiveArea==Areas.Num()-1)) ||
            FBox(Areas[ActiveArea].Exit-Areas[ActiveArea].ExitExtent,Areas[ActiveArea].Exit+Areas[ActiveArea].ExitExtent).IsInsideOrOn(Controller->GetPawn()->GetActorLocation());
    case EMiningExperienceKind::Office: return Office && (bContinuousCampaign ? Office->GetCompletedSessions()>0 : Office->IsOfficeEscaped()) && !Office->IsSessionActive();
    case EMiningExperienceKind::Journey: return Journey && Journey->Phase==EMiningEscapePhase::Escaped;
    case EMiningExperienceKind::Museum: return (MirrorMuseum && MirrorMuseum->bComplete) || (Museum && Museum->bComplete);
    case EMiningExperienceKind::Ending: return Ending && Ending->bComplete;
    default: return false;
    }
}

void AMiningExhibitHall::TickMuseumSkipChord(float DeltaSeconds)
{
    if(!bLinearCampaign || !Areas.IsValidIndex(ActiveArea) ||
       AreaKind(ActiveArea)!=EMiningExperienceKind::Museum || !MirrorMuseum ||
       !MirrorMuseum->bAreaActive || IsInteractionOpen() || !Controller.IsValid() ||
       Controller->IsPaused())
    {
        MuseumSkipHoldSeconds=0.f;
        bMuseumSkipTriggered=false;
        return;
    }

    const bool bKeyboardChord=Controller->IsInputKeyDown(EKeys::S) &&
        Controller->IsInputKeyDown(EKeys::K) && Controller->IsInputKeyDown(EKeys::I) &&
        Controller->IsInputKeyDown(EKeys::P);
    bool bCornerTouchChord=false;
    if(SVirtualJoystick::ShouldDisplayTouchInterface())
    {
        int32 Width=0,Height=0;Controller->GetViewportSize(Width,Height);
        if(Width>0 && Height>0)
        {
            bool Corners[4]={false,false,false,false};
            for(int32 Index=0;Index<10;++Index)
            {
                float X=0.f,Y=0.f;bool bPressed=false;
                Controller->GetInputTouchState(static_cast<ETouchIndex::Type>(Index),X,Y,bPressed);
                if(!bPressed)continue;
                const bool bLeft=X<Width*.22f,bRight=X>Width*.78f;
                const bool bTop=Y<Height*.22f,bBottom=Y>Height*.78f;
                if(bLeft&&bTop)Corners[0]=true;
                else if(bRight&&bTop)Corners[1]=true;
                else if(bLeft&&bBottom)Corners[2]=true;
                else if(bRight&&bBottom)Corners[3]=true;
            }
            bCornerTouchChord=Corners[0]&&Corners[1]&&Corners[2]&&Corners[3];
        }
    }

    if(!(bKeyboardChord||bCornerTouchChord))
    {
        MuseumSkipHoldSeconds=0.f;
        bMuseumSkipTriggered=false;
        return;
    }
    MuseumSkipHoldSeconds+=FMath::Max(0.f,DeltaSeconds);
    if(!bMuseumSkipTriggered && MuseumSkipHoldSeconds>=3.f)
        bMuseumSkipTriggered=MirrorMuseum->SkipToReward();
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
    // The next mine is prepared throughout the card room, well before its exit.
    if(bContinuousCampaign && AreaKind(ActiveArea)==EMiningExperienceKind::Journey && GetWorld()->GetTimeSeconds()-LastJourneyPrewarm>.25)
        for(int32 Next=ActiveArea+1;Next<Areas.Num();++Next)
            if(AreaKind(Next)==EMiningExperienceKind::Mining)
            { LastJourneyPrewarm=GetWorld()->GetTimeSeconds(); AGeneratedMiningRock::PrewarmTerrainAt(GetWorld(),Areas[Next].Entry.GetLocation(),5000.f); break; }
    TickMuseumSkipChord(DeltaSeconds);
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
        if (!Wallet->UsesMineralInventory()) GUI->SetCoinDisplay(FText::AsNumber(Wallet->Balance));
        if(auto* Progression=Controller->GetPawn()->FindComponentByClass<UMiningProgressionComponent>())
            for(int32 Index=0; Index<4 && Progression->Tracks.IsValidIndex(Index); ++Index)
            {
                GUI->SetItemLevel(Index, Progression->GetLevel(Index), false);
                GUI->SetUpgradeDetails(Index, Progression->Tracks[Index].Name, Progression->GetNextDescription(Index),
                    Progression->GetNextPrice(Index), Wallet->Balance>=Progression->GetNextPrice(Index));
            }
        if(Treasure) Treasure->Coins=Wallet->UsesMineralInventory() ? 0 : Wallet->Balance;
    }
    if(bContinuousCampaign && Areas.IsValidIndex(ActiveArea) && !IsInteractionOpen())
    {
        if(CanAdvanceCampaign()) CompletedAreas.Add(ActiveArea);
        for(int32 I:CompletedAreas) if(Areas[I].ProgressGate)
        { Areas[I].ProgressGate->SetActorHiddenInGame(true); Areas[I].ProgressGate->SetActorEnableCollision(false); }
        const FVector P=Controller->GetPawn()->GetActorLocation();
        const auto NearThreshold=[&](int32 I){return FMath::Abs(P.Y-Areas[I].Exit.Y)<Areas[I].ExitExtent.Y && FMath::Abs(P.Z-Areas[I].Exit.Z)<Areas[I].ExitExtent.Z;};
        const bool bMuseumDrop = ActorHasTag(TEXT("BM_MuseumCeilingDrop"));
        const auto IsDrop = [&](int32 I){return bMuseumDrop && Areas.IsValidIndex(I) && Areas[I].Tag==TEXT("BM_Campaign_MainGame03");};
        if(ActiveArea>0 && !IsDrop(ActiveArea-1) && P.X<Areas[ActiveArea-1].Exit.X-100 && NearThreshold(ActiveArea-1)) EnterArea(ActiveArea-1,false);
        else if(CompletedAreas.Contains(ActiveArea))
        {
            if(ActiveArea==Areas.Num()-1) bCampaignComplete=true;
            else if(IsDrop(ActiveArea)
                ? (P.Z<Areas[ActiveArea].Exit.Z && FMath::Abs(P.X-Areas[ActiveArea].Exit.X)<Areas[ActiveArea].ExitExtent.X && FMath::Abs(P.Y-Areas[ActiveArea].Exit.Y)<Areas[ActiveArea].ExitExtent.Y)
                : (P.X>Areas[ActiveArea].Exit.X && (NearThreshold(ActiveArea) ||
                (Areas[ActiveArea+1].ActivationBounds.IsValid && Areas[ActiveArea+1].ActivationBounds.IsInsideOrOn(P)))))
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
    if(!bPurchased) return false;
    return true;
}
void AMiningExhibitHall::OnUpgradeRequested(int32 ItemIndex, int32 RequestedLevel) { PurchaseUpgrade(ItemIndex,RequestedLevel); }

bool AMiningExhibitHall::IsInteractionOpen() const
{
    return Map.IsValid() || !Controller.IsValid() || Controller->IsPaused() || Controller->bUpgradeMenuHeld || Controller->bShowMouseCursor || Controller->IsLookInputIgnored();
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

void AMiningExhibitHall::EndPlay(const EEndPlayReason::Type Reason)
{
    CloseMap();
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(TerrainWarmupTimer);
    if(BoundGUI.IsValid()) BoundGUI->OnUpgradeRequested.RemoveAll(this);
    GetWorld()->RemoveOnActorSpawnedHandler(SpawnHandle);
    Super::EndPlay(Reason);
}
