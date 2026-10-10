#include "Showcase/MiningShowcase.h"
#include "MiningEncounterPlayer.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningCollection.h"
#include "MiningComponent.h"
#include "MiningHUD.h"
#include "PopupClose/PopupCloseStage.h"
#include "WenSi/WenSiPuzzle.h"
#include "Subtitles/SubtitleSubsystem.h"
#include "EchoPassagePlayer.h"
#include "EchoPassageStage.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "UObject/ConstructorHelpers.h"

namespace MiningShowcase
{
    class FShortcuts : public IInputProcessor
    {
    public:
        explicit FShortcuts(AMiningShowcaseController* InController) : Controller(InController) {}
        virtual void Tick(float DeltaTime, FSlateApplication& Application, TSharedRef<ICursor> Cursor) override {}
        virtual bool HandleKeyDownEvent(FSlateApplication& Application, const FKeyEvent& Event) override
        {
            if (!Controller.IsValid() || Event.IsRepeat()) return false;
            if (Event.GetKey() == EKeys::F2) { Controller->ToggleShowcaseMenu(); return true; }
            if (Event.GetKey() == EKeys::F5) { Controller->RestartExhibit(); return true; }
            if (Event.GetKey() == EKeys::F8) { Controller->ReturnToShowcase(); return true; }
            return false;
        }
    private:
        TWeakObjectPtr<AMiningShowcaseController> Controller;
    };
}

AMiningShowcaseController::AMiningShowcaseController()
{
    static ConstructorHelpers::FClassFinder<ABrainMiningPlayerController> Defaults(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController"));
    if (Defaults.Succeeded()) UseInputDefaultsFrom(Defaults.Class->GetDefaultObject<ABrainMiningPlayerController>());
    PrimaryActorTick.bTickEvenWhenPaused = true;
    bShouldPerformFullTickWhenPaused = true;
}
AMiningShowcaseDirector* AMiningShowcaseController::GetDirector() const
{
    for (TActorIterator<AMiningShowcaseDirector> Director(GetWorld()); Director; ++Director) return *Director;
    return nullptr;
}
void AMiningShowcaseController::BeginPlay()
{
    Super::BeginPlay();
    if (IsLocalController() && FSlateApplication::IsInitialized())
    {
        ShortcutProcessor = MakeShared<MiningShowcase::FShortcuts>(this);
        FSlateApplication::Get().RegisterInputPreProcessor(ShortcutProcessor, 0);
    }
}
void AMiningShowcaseController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (ShortcutProcessor && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(ShortcutProcessor);
    ShortcutProcessor.Reset();
    Super::EndPlay(Reason);
}
void AMiningShowcaseController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::F2, IE_Pressed, this, &AMiningShowcaseController::ToggleShowcaseMenu).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::F8, IE_Pressed, this, &AMiningShowcaseController::ReturnToShowcase).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::F5, IE_Pressed, this, &AMiningShowcaseController::RestartExhibit).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AMiningShowcaseController::InteractExhibit).bConsumeInput = false;
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AMiningShowcaseController::KnockExhibit).bConsumeInput = false;
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AMiningShowcaseController::ResetEcho).bConsumeInput = false;
    InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AMiningShowcaseController::LeaveCards).bConsumeInput = false;
    InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AMiningShowcaseController::ConfirmCards).bConsumeInput = false;
}
void AMiningShowcaseController::ToggleShowcaseMenu() { if (auto* Director = GetDirector()) Director->ToggleMenu(); }
void AMiningShowcaseController::ReturnToShowcase() { if (auto* Director = GetDirector()) Director->ReturnToLobby(); }
void AMiningShowcaseController::RestartExhibit() { if (auto* Director = GetDirector()) Director->RestartCurrent(); }
void AMiningShowcaseController::InteractExhibit()
{
    if (auto* Director = GetDirector(); Director && !Director->bMenuOpen && !Director->bBusy)
    {
        if (Director->CardInteraction) Director->CardInteraction->Interact();
        else Director->InteractPortal();
    }
}
void AMiningShowcaseController::KnockExhibit() { if (auto* Director = GetDirector(); Director && !Director->bMenuOpen && Director->EchoInteraction) Director->EchoInteraction->Knock(); }
void AMiningShowcaseController::ResetEcho() { if (auto* Director = GetDirector(); Director && !Director->bMenuOpen && Director->EchoInteraction) Director->EchoInteraction->Restart(); }
void AMiningShowcaseController::LeaveCards() { if (auto* Director = GetDirector(); Director && !Director->bMenuOpen && Director->CardInteraction) Director->CardInteraction->LeaveSeat(); }
void AMiningShowcaseController::ConfirmCards() { if (auto* Director = GetDirector(); Director && !Director->bMenuOpen && Director->CardInteraction) Director->CardInteraction->ConfirmCards(); }

AMiningShowcaseGameMode::AMiningShowcaseGameMode()
{
    static ConstructorHelpers::FClassFinder<APawn> PawnClass(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
    if (PawnClass.Succeeded()) DefaultPawnClass = PawnClass.Class;
    PlayerControllerClass = AMiningShowcaseController::StaticClass();
    HUDClass = AHUD::StaticClass();
}
AMiningShowcaseDirector::AMiningShowcaseDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    LobbyCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("LobbyCamera"));
    LobbyCamera->SetupAttachment(RootComponent);
    LobbyCamera->SetRelativeLocation(FVector(-2400, 0, 1500));
    LobbyCamera->SetRelativeRotation(FRotator(-22, 0, 0));
    Policy = CreateDefaultSubobject<UMiningEncounterPlayerPolicy>(TEXT("ExhibitPolicy"));
    auto* LobbyBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("LobbyExposureBounds"));
    LobbyBounds->SetupAttachment(RootComponent);
    LobbyBounds->SetRelativeLocation(FVector(800, 0, 450));
    LobbyBounds->SetBoxExtent(FVector(4000, 2400, 1000));
    LobbyBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Exposure = CreateDefaultSubobject<UPostProcessComponent>(TEXT("LobbyExposure"));
    Exposure->SetupAttachment(LobbyBounds);
    Exposure->bUnbound = false;
    Exposure->Priority = 5;
    Exposure->Settings.bOverride_AutoExposureMethod = true;
    Exposure->Settings.AutoExposureMethod = AEM_Manual;
    Exposure->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Exposure->Settings.AutoExposureApplyPhysicalCameraExposure = false;
    Exposure->Settings.bOverride_AutoExposureBias = true;
    Exposure->Settings.AutoExposureBias = 0;
}
AMiningShowcaseController* AMiningShowcaseDirector::Player() const { return Cast<AMiningShowcaseController>(GetWorld()->GetFirstPlayerController()); }
void AMiningShowcaseDirector::BeginPlay()
{
    Super::BeginPlay();
    Status = FText::FromString(TEXT("选择一个展区，或关闭目录后走近展台按 E。"));
    if (Player())
    {
        Widget = CreateWidget<UMiningShowcaseWidget>(Player());
        Widget->Director = this;
        Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        Widget->AddToViewport(200);
        if (!bContinuousLayout) SetMenuOpen(true);
    }
}
EMiningShowcaseKind AMiningShowcaseDirector::GetActiveKind() const
{
    return Exhibits.IsValidIndex(ActiveIndex) ? Exhibits[ActiveIndex].Kind : EMiningShowcaseKind::Mining;
}
bool AMiningShowcaseDirector::IsModalActive() const
{
    if (ActiveIndex == INDEX_NONE || bBusy) return false;
    if (UGameplayStatics::IsGamePaused(this)) return true;
    if (CardInteraction && CardInteraction->Stage && CardInteraction->Stage->Run.Phase != ECardRoomPhase::Exploring) return true;
    if (GetActiveKind() == EMiningShowcaseKind::WenSi)
        for (TActorIterator<AWenSiPuzzle> Puzzle(GetWorld()); Puzzle; ++Puzzle) if (Puzzle->bEditing) return true;
    if (GetActiveKind() == EMiningShowcaseKind::Popup)
        for (TActorIterator<APopupCloseStage> Stage(GetWorld()); Stage; ++Stage) if (Stage->IsSessionActive()) return true;
    return false;
}
FVector AMiningShowcaseDirector::GetPortalPosition(int32 Index) const
{
    return GetActorLocation() + FVector((Index / 3) * 850, ((Index % 3) - 1) * 1000, 0);
}
int32 AMiningShowcaseDirector::GetNearbyExhibit() const
{
    if (ActiveIndex != INDEX_NONE || bBusy || !Player() || !Player()->GetPawn()) return INDEX_NONE;
    for (int32 Index = 0; Index < Exhibits.Num(); ++Index)
        if (FVector::Dist2D(Player()->GetPawn()->GetActorLocation(), GetPortalPosition(Index)) < 300) return Index;
    return INDEX_NONE;
}
void AMiningShowcaseDirector::InteractPortal() { EnterExhibit(GetNearbyExhibit()); }
bool AMiningShowcaseDirector::EnterExhibit(int32 Index)
{
    if (bContinuousLayout) return false;
    if (bBusy || !Exhibits.IsValidIndex(Index)) return false;
    const FString Package = Exhibits[Index].Level.ToSoftObjectPath().GetLongPackageName();
    if (Package.IsEmpty() || !FPackageName::DoesPackageExist(Package))
    {
        Status = FText::FromString(FString::Printf(TEXT("地图缺失：%s。请检查 Git LFS 资源。"), *Package));
        return false;
    }
    BeginTransition(Index);
    return true;
}
void AMiningShowcaseDirector::ReturnToLobby()
{
    if (bContinuousLayout) { CloseContinuousInteraction(); return; }
    if (bBusy) return;
    if (ActiveIndex == INDEX_NONE) { SetMenuOpen(true); return; }
    BeginTransition(INDEX_NONE);
}
void AMiningShowcaseDirector::RestartCurrent() { if (bContinuousLayout) { RestartContinuousRegion(); return; } if (!bBusy && ActiveIndex != INDEX_NONE) EnterExhibit(ActiveIndex); }
void AMiningShowcaseDirector::ToggleMenu()
{
    if (bContinuousLayout) { bGuideVisible = !bGuideVisible; return; }
    if (bBusy) return;
    if (ActiveIndex != INDEX_NONE)
    {
        Status = FText::FromString(TEXT("F8 返回大厅选择其他展区；F5 重新开始当前展区。"));
        return;
    }
    SetMenuOpen(!bMenuOpen);
}
void AMiningShowcaseDirector::SetMenuOpen(bool bOpen)
{
    if (bContinuousLayout) return;
    bMenuOpen = bOpen;
    auto* Controller = Player();
    if (!Controller) return;
    if (bMenuLocked != bOpen)
    {
        Controller->SetIgnoreMoveInput(bOpen);
        Controller->SetIgnoreLookInput(bOpen);
        bMenuLocked = bOpen;
    }
    Controller->bShowMouseCursor = bOpen;
    if (bOpen)
    {
        if (APawn* Pawn = Controller->GetPawn()) if (auto* Mining = Pawn->FindComponentByClass<UMiningComponent>()) Mining->CancelMining();
        FInputModeGameAndUI Mode;
        Mode.SetHideCursorDuringCapture(false);
        Controller->SetInputMode(Mode);
    }
    else
    {
        Controller->SetInputMode(FInputModeGameOnly());
        if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
    }
}
void AMiningShowcaseDirector::ResetController()
{
    if (auto* Controller = Player())
    {
        Controller->ResetIgnoreMoveInput();
        Controller->ResetIgnoreLookInput();
        Controller->bShowMouseCursor = false;
        Controller->SetInputMode(FInputModeGameOnly());
        Controller->ClientSetHUD(AHUD::StaticClass());
        if (Controller->PlayerCameraManager) Controller->PlayerCameraManager->StopCameraFade();
    }
    bMenuLocked = bMenuOpen = false;
}
void AMiningShowcaseDirector::ReleaseExhibit()
{
    if (bContinuousLayout) { SwitchContinuousRegion(INDEX_NONE); return; }
    UGameplayStatics::SetGamePaused(this, false);
    if (auto* Subtitles = USubtitleSubsystem::Get(this)) Subtitles->StopAll();
    if (EchoInteraction) { EchoInteraction->DestroyComponent(); EchoInteraction = nullptr; }
    if (CardInteraction) { CardInteraction->DestroyComponent(); CardInteraction = nullptr; }
    Policy->Release();
    for (TActorIterator<APopupCloseStage> Stage(GetWorld()); Stage; ++Stage) Stage->CancelSession();
    for (TActorIterator<AWenSiPuzzle> Puzzle(GetWorld()); Puzzle; ++Puzzle) if (Puzzle->bEditing) Puzzle->ToggleEdit();
    for (TActorIterator<AMiningCollectionField> Field(GetWorld()); Field; ++Field) if (Field->bCatalogueOpen) Field->ToggleCatalogue();
    if (auto* Controller = Player())
    {
        Controller->SetViewTarget(this);
        if (APawn* Pawn = Controller->GetPawn()) { Controller->UnPossess(); Pawn->Destroy(); }
    }
    ResetController();
    bEchoTransferPending = false;
    bModalWasActive = false;
}
void AMiningShowcaseDirector::BeginTransition(int32 Index)
{
    SetMenuOpen(false);
    ReleaseExhibit();
    PendingIndex = Index;
    ActiveIndex = INDEX_NONE;
    bBusy = true;
    TransitionStarted = FPlatformTime::Seconds();
    Status = FText::FromString(TEXT("正在整理展区，请稍候……"));
    if (Streaming)
    {
        Streaming->SetShouldBeVisible(false);
        Streaming->SetShouldBeLoaded(false);
        Streaming->SetIsRequestingUnloadAndRemoval(true);
        Transition = ETransition::Unloading;
    }
    else { CleanupSessionActors(); BeginLoad(); }
}
void AMiningShowcaseDirector::BeginLoad()
{
    if (PendingIndex == INDEX_NONE) { FinishLobby(); return; }
    SpawnHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &AMiningShowcaseDirector::OnSessionActorSpawned));
    bool bSuccess = false;
    Streaming = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(this, Exhibits[PendingIndex].Level, FVector::ZeroVector, FRotator::ZeroRotator, bSuccess,
        FString::Printf(TEXT("Showcase_%d"), ++InstanceSerial));
    if (!bSuccess || !Streaming)
    {
        CleanupSessionActors();
        FinishLobby();
        Status = FText::FromString(TEXT("展区加载失败，已返回大厅。"));
        return;
    }
    Transition = ETransition::Loading;
    TransitionStarted = FPlatformTime::Seconds();
    Status = FText::Format(FText::FromString(TEXT("正在加载：{0}")), Exhibits[PendingIndex].Title);
}
bool AMiningShowcaseDirector::SpawnPlayer(const FTransform& Transform)
{
    auto* Controller = Player();
    auto* Mode = GetWorld()->GetAuthGameMode();
    if (!Controller || !Mode) return false;
    auto* Pawn = Mode->SpawnDefaultPawnAtTransform(Controller, Transform);
    if (!Pawn) return false;
    Controller->Possess(Pawn);
    Controller->SetViewTarget(Pawn);
    Controller->SetControlRotation(Transform.Rotator());
    return true;
}
void AMiningShowcaseDirector::ActivateLoadedExhibit()
{
    ULevel* Level = Streaming ? Streaming->GetLoadedLevel() : nullptr;
    if (!Level || !Player()) return;
    ActiveIndex = PendingIndex;
    FTransform Entry(FRotator::ZeroRotator, FVector(-430, 0, 110));
    for (AActor* Actor : Level->Actors) if (auto* Start = Cast<APlayerStart>(Actor)) { Entry = Start->GetActorTransform(); break; }
    for (AActor* Actor : Level->Actors) if (auto* Popup = Cast<APopupCloseStage>(Actor)) Entry = Popup->GetPlayerSpawnTransform();
    ResetController();
    if (!SpawnPlayer(Entry))
    {
        bBusy = false;
        BeginTransition(INDEX_NONE);
        return;
    }
    const EMiningShowcaseKind Kind = GetActiveKind();
    if (Kind == EMiningShowcaseKind::Mining) Player()->ClientSetHUD(AMiningHUD::StaticClass());
    else if (Kind != EMiningShowcaseKind::Collection) Policy->Apply(Player()->GetPawn());
    if (Kind == EMiningShowcaseKind::Echo)
    {
        for (AActor* Actor : Level->Actors) if (auto* Stage = Cast<AEchoPassageStage>(Actor))
        {
            Stage->DestinationLevel.Reset();
            Stage->OnPassageCompleted.AddDynamic(this, &AMiningShowcaseDirector::OnEchoCompleted);
        }
        EchoInteraction = NewObject<UEchoPassageInteractionComponent>(Player());
        EchoInteraction->RegisterComponent();
        EchoInteraction->Restart();
    }
    if (Kind == EMiningShowcaseKind::Cards)
    {
        CardInteraction = NewObject<UCardRoomInteractionComponent>(Player());
        CardInteraction->RegisterComponent();
        if (CardInteraction->Stage)
        {
            CardInteraction->Stage->OnRoomCompleted.AddDynamic(this, &AMiningShowcaseDirector::OnCardCompleted);
            CardInteraction->Stage->OnCollectibleClaimed.AddDynamic(this, &AMiningShowcaseDirector::OnCardReward);
        }
    }
    if (Kind == EMiningShowcaseKind::WenSi)
    {
        if (auto* Character = Cast<ACharacter>(Player()->GetPawn()))
        {
            Character->GetCapsuleComponent()->SetCapsuleSize(32, 88);
            Character->GetCharacterMovement()->MaxWalkSpeed = 260;
            Character->GetCharacterMovement()->MaxStepHeight = 20;
        }
    }
    Transition = ETransition::None;
    bBusy = false;
    Visited.Add(Exhibits[ActiveIndex].Id);
    Status = FText::FromString(TEXT("F5 重新体验  ·  F8 返回大厅  ·  各展区进度独立"));
    if (auto* Subtitles = USubtitleSubsystem::Get(this)) Subtitles->PlaySegment(Exhibits[ActiveIndex].SubtitleId);
    UE_LOG(LogTemp, Display, TEXT("SHOWCASE_ACTIVE %s"), *Exhibits[ActiveIndex].Id.ToString());
}
void AMiningShowcaseDirector::FinishLobby()
{
    Streaming = nullptr;
    ActiveIndex = PendingIndex = INDEX_NONE;
    Transition = ETransition::None;
    bBusy = false;
    ResetController();
    SpawnPlayer(FTransform(FRotator::ZeroRotator, GetActorLocation() + FVector(-1300, 0, 110)));
    Status = FText::FromString(TEXT("已返回大厅。展区会重新开始，参观记录保留到本次试玩结束。"));
    SetMenuOpen(true);
}
void AMiningShowcaseDirector::OnSessionActorSpawned(AActor* Actor)
{
    if (Actor && Actor->GetLevel() == GetWorld()->PersistentLevel && !Actor->IsA<APawn>() && !Actor->IsA<AController>() && !Actor->IsA<AHUD>()) SessionActors.Add(Actor);
}
void AMiningShowcaseDirector::CleanupSessionActors()
{
    if (SpawnHandle.IsValid()) GetWorld()->RemoveOnActorSpawnedHandler(SpawnHandle);
    SpawnHandle.Reset();
    for (auto Actor : SessionActors) if (Actor.IsValid()) Actor->Destroy();
    SessionActors.Reset();
}
void AMiningShowcaseDirector::ReportCompletion(FName Id)
{
    Completed.Add(Id);
    if (bContinuousLayout) { Status = FText::FromString(TEXT("本区已完成。沿步道继续探索，进度会保留。")); return; }
    Status = FText::FromString(TEXT("本展区已完成！F5 可重新体验，F8 返回大厅。"));
}
void AMiningShowcaseDirector::OnEchoCompleted(FName Id)
{
    if (Exhibits.IsValidIndex(ActiveIndex)) ReportCompletion(Exhibits[ActiveIndex].Id);
    bEchoTransferPending = !bContinuousLayout;
}
void AMiningShowcaseDirector::OnCardCompleted(FName Id) { if (Exhibits.IsValidIndex(ActiveIndex)) ReportCompletion(Exhibits[ActiveIndex].Id); }
void AMiningShowcaseDirector::OnCardReward(FName Id) { Status = FText::FromString(bContinuousLayout ? TEXT("已领取卡牌演示奖励。按 Q 离座，沿步道继续探索。") : TEXT("已领取卡牌房的演示奖励（不计入收集图鉴）。F8 返回大厅。")); }
void AMiningShowcaseDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bContinuousLayout) UpdateContinuousRegion();
    if (Transition == ETransition::Unloading && (!Streaming || !Streaming->GetLoadedLevel()))
    {
        Streaming = nullptr;
        CleanupSessionActors();
        BeginLoad();
    }
    else if (Transition == ETransition::Loading && Streaming && Streaming->IsLevelVisible()) ActivateLoadedExhibit();
    if (Transition == ETransition::Loading && Streaming && Streaming->GetLevelStreamingState() == ELevelStreamingState::FailedToLoad)
    {
        bBusy = false;
        BeginTransition(INDEX_NONE);
        Status = FText::FromString(TEXT("展区加载失败，正在返回大厅。"));
    }
    if (bBusy && FPlatformTime::Seconds() - TransitionStarted > 60)
    {
        Status = FText::FromString(TEXT("加载耗时较长，正在等待引擎完成资源处理……"));
    }
    if (bEchoTransferPending && !bBusy)
    {
        bEchoTransferPending = false;
        for (int32 Index = 0; Index < Exhibits.Num(); ++Index) if (Exhibits[Index].Kind == EMiningShowcaseKind::Cards) { EnterExhibit(Index); break; }
    }
    if (bBusy || ActiveIndex == INDEX_NONE || !Exhibits.IsValidIndex(ActiveIndex)) return;
    const bool bModalNow = IsModalActive();
    if (bModalNow && !bModalWasActive) if (auto* Subtitles = USubtitleSubsystem::Get(this)) Subtitles->StopAll();
    bModalWasActive = bModalNow;
    ProgressPoll += DeltaSeconds;
    if (ProgressPoll < 0.5f) return;
    ProgressPoll = 0;
    if (!Completed.Contains(Exhibits[ActiveIndex].Id))
    {
        if (GetActiveKind() == EMiningShowcaseKind::Collection)
            for (TActorIterator<AMiningCollectionField> Field(GetWorld()); Field; ++Field) if (Field->UniqueCount() == 4) ReportCompletion(Exhibits[ActiveIndex].Id);
        if (GetActiveKind() == EMiningShowcaseKind::WenSi)
            for (TActorIterator<AWenSiPuzzle> Puzzle(GetWorld()); Puzzle; ++Puzzle) if (Puzzle->bComplete) ReportCompletion(Exhibits[ActiveIndex].Id);
        if (GetActiveKind() == EMiningShowcaseKind::Popup)
            for (TActorIterator<APopupCloseStage> Stage(GetWorld()); Stage; ++Stage) if (Stage->GetCompletedSessions() > 0) ReportCompletion(Exhibits[ActiveIndex].Id);
    }
}
void AMiningShowcaseDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    ReleaseExhibit();
    CleanupSessionActors();
    if (Widget) Widget->RemoveFromParent();
    Widget = nullptr;
    Super::EndPlay(Reason);
}
