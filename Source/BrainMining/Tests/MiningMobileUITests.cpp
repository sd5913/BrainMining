#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "MobileTouchControls.h"
#include "MobileUIWidgets.h"
#include "MiningComponent.h"
#include "MiningEncounterPlayer.h"
#include "MiningGiantRoomEntrance.h"
#include "MiningRoomEscape.h"
#include "EchoPassagePlayer.h"
#include "EchoPassageStage.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "MiningExperienceSubsystem.h"
#include "MiningCollection.h"
#include "EnhancedInputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/SOverlay.h"
#include "StartRoom/MiningOpening.h"
#include "Containers/Ticker.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "UnrealClient.h"
#include "EndingEmergence.h"
#include "MiningGamepadSubsystem.h"
#include "GameFramework/PlayerInput.h"
#include "MediaPlayer.h"
#include "FileMediaSource.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMobileLayoutTest, "BrainMining.Mobile.IconLayout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningMobileLayoutTest::RunTest(const FString&)
{
    auto Controls = SNew(SMobileTouchControls);
    using Button = SMobileTouchControls::EButton;
    const Button Active[] = {Button::Mine, Button::Bomb, Button::Drill, Button::Jetpack, Button::Interact, Button::ThoughtLeft, Button::ThoughtRight, Button::Edit};
    for (const FVector2D Size : {FVector2D(1280,720), FVector2D(1600,720), FVector2D(2400,1080), FVector2D(800,360)})
    {
        TestTrue(TEXT("Primary mining target is enlarged"), Controls->GetButtonHitRadius(Button::Mine, Size) > Controls->GetButtonHitRadius(Button::Drill, Size));
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Active); ++Index)
        {
            const FVector2D Center = Controls->GetButtonCenter(Active[Index], Size);
            const float Radius = Controls->GetButtonHitRadius(Active[Index], Size);
            TestTrue(TEXT("Touch target remains on screen"), Center.X - Radius >= 0 && Center.Y - Radius >= 0 && Center.X + Radius <= Size.X && Center.Y + Radius <= Size.Y);
            for (int32 Other = Index + 1; Other < UE_ARRAY_COUNT(Active); ++Other)
                TestTrue(TEXT("Adjacent action targets do not overlap"), FVector2D::Distance(Center, Controls->GetButtonCenter(Active[Other], Size)) > Radius + Controls->GetButtonHitRadius(Active[Other], Size));
        }
    }
    for (const FName Name : {FName(TEXT("pick")), FName(TEXT("drill")), FName(TEXT("bomb")), FName(TEXT("jet")), FName(TEXT("hand")), FName(TEXT("pause")), FName(TEXT("up")), FName(TEXT("plate")), FName(TEXT("rim")), FName(TEXT("knock")), FName(TEXT("cursor")), FName(TEXT("left")), FName(TEXT("right")), FName(TEXT("panel"))})
    {
        FString Source;
        TestTrue(TEXT("Runtime SVG is packaged from Content"), FFileHelper::LoadFileToString(Source, *(FPaths::ProjectContentDir() / TEXT("UI/Mobile") / (Name.ToString() + TEXT(".svg")))));
        TestFalse(TEXT("Controls have no visible text labels"), Source.Contains(TEXT("<text")));
        TestNotNull(TEXT("Slate icon brush is available"), MiningMobileUI::Brush(Name));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMobileEndingTest, "BrainMining.Mobile.EndingProgression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningMobileEndingTest::RunTest(const FString&)
{
    auto* Instance = NewObject<UGameInstance>(GEngine);
    Instance->InitializeStandalone();
    auto* World = Instance->GetWorld();
    auto& Context = *Instance->GetWorldContext();
    auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
    auto* Viewport = NewObject<UGameViewportClient>(GEngine);
    Context.GameViewport = Viewport;
    Viewport->Init(Context, Instance, false);
    Viewport->SetViewportOverlayWidget(nullptr, SNew(SOverlay));
    LocalPlayer->PlayerAdded(Viewport, 0);
    auto* Controller = World->SpawnActor<AMiningEncounterPlayerController>();
    World->AddController(Controller);
    Controller->SetPlayer(LocalPlayer);
    auto* Pawn = World->SpawnActor<ACharacter>();
    auto* Mining = NewObject<UMiningComponent>(Pawn);
    Pawn->AddInstanceComponent(Mining);
    Mining->RegisterComponent();
    Controller->Possess(Pawn);
    Controller->EncounterPolicy->SetMiningAllowed(true);
    Controller->bForceTouchControls = true;
    Controller->SetMiningGUIEnabled(false);
    auto* Ending = World->SpawnActor<AEndingEmergence>();
    Ending->Player = Controller;
    Ending->bReady = true;
    auto* Menu = Instance->GetSubsystem<UMiningExperienceSubsystem>();
    ON_SCOPE_EXIT
    {
        Menu->CloseMenu();
        Controller->ResetMobileInput();
        LocalPlayer->PlayerRemoved();
        Controller->UnPossess();
        LocalPlayer->PlayerController = nullptr;
        Instance->Shutdown();
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
    };
    auto Controls = SNew(SMobileTouchControls).OwnerController(Controller);
    using Button = SMobileTouchControls::EButton;
    const FVector2D Size(1600,720);
    const FGeometry Geometry = FGeometry::MakeRoot(Size, FSlateLayoutTransform());
    const FVector2D MineCenter = Controls->GetButtonCenter(Button::Mine, Size);
    TestTrue(TEXT("Cell mining stays visible while desktop inventory is hidden"), Controls->IsButtonVisible(Button::Mine));
    TestTrue(TEXT("Cell touch begins actual mining"), Controls->HandlePointerStarted(Geometry, 1, MineCenter).IsEventHandled() && Mining->IsMiningActive());
    const float InitialYaw = Controller->RotationInput.Yaw;
    Controls->HandlePointerMoved(Geometry, 1, MineCenter + FVector2D(40,0));
    TestTrue(TEXT("Cell mining finger also turns the camera"), Controller->RotationInput.Yaw != InitialYaw);
    TestFalse(TEXT("Cell does not expose unrelated weapons"), Controls->IsButtonVisible(Button::Bomb) || Controls->IsButtonVisible(Button::Drill));
    TestFalse(TEXT("Floating cell room does not expose a misleading jetpack button"), Controls->IsButtonVisible(Button::Jetpack));
    Controller->HandleMobileMove(FVector2D(.12, .16));
    TestEqual(TEXT("Cell floating retains gentle analog joystick input"), UMiningGamepadSubsystem::ReadMove(Controller), FVector2D(.12, .16));
    Controller->HandleMobileMove(FVector2D::ZeroVector);
    Pawn->ConsumeMovementInputVector();
    Ending->GateIndex = 1;
    Mining->SetInputSuspended(true);
    Controller->SetIgnoreMoveInput(true);
    Controls->Tick(Geometry, 0, .016f);
    TestTrue(TEXT("Blood entry releases the hidden tool"), Controls->MinePointer == -1 && !Mining->IsMiningActive());
    TestTrue(TEXT("Blood keeps steering and look despite pawn movement lock"), Controller->IsMobileMoveEnabled() && Controller->IsMobileLookEnabled());
    TestTrue(TEXT("Blood accepts joystick touch"), Controls->HandlePointerStarted(Geometry, 2, FVector2D(160,570)).IsEventHandled());
    Controls->HandlePointerMoved(Geometry, 2, FVector2D(200,480));
    Controls->Tick(Geometry, 0, .016f);
    TArray<UInputComponent*> InputStack;
    Controller->PlayerInput->ProcessInputStack(InputStack, .016f, false);
    TestEqual(TEXT("Blood receives analog forward and lateral movement through ending reader"), UMiningGamepadSubsystem::ReadMove(Controller), FVector2D(Controls->JoystickAxis.X, -Controls->JoystickAxis.Y));
    TestTrue(TEXT("Blood joystick contains forward and lateral input"), UMiningGamepadSubsystem::ReadMove(Controller).Y > .2 && UMiningGamepadSubsystem::ReadMove(Controller).X > .2);
    TestFalse(TEXT("Scripted camera movement never moves hidden pawn"), Pawn->GetPendingMovementInputVector().SizeSquared() > 0);
    TestTrue(TEXT("Blood look accepts a second touch"), Controls->HandlePointerStarted(Geometry, 3, FVector2D(1020,300)).IsEventHandled());
    Controls->Tick(Geometry, 0, .016f);
    TestEqual(TEXT("Blood tick retains look while pawn movement is locked"), Controls->LookPointer, 3);
    Ending->bEnteringBrain = true;
    Controller->SetIgnoreLookInput(true);
    Controls->Tick(Geometry, 0, .016f);
    TestTrue(TEXT("Brain arrival releases both touch axes"), Controls->LookPointer == -1 && Controls->JoystickPointer == -1);
    Ending->bEnteringBrain = false;
    Ending->GateIndex = 2;
    Controller->ResetIgnoreLookInput();
    TestTrue(TEXT("Neurons expose jump and look only"), Controls->IsButtonVisible(Button::Jetpack) && Controller->IsMobileLookEnabled() && !Controller->IsMobileMoveEnabled() && !Controls->IsButtonVisible(Button::Mine));
    const FVector2D JumpCenter = Controls->GetButtonCenter(Button::Jetpack, Size);
    Controls->HandlePointerStarted(Geometry, 4, JumpCenter);
    Controller->PlayerInput->ProcessInputStack(InputStack, .016f, false);
    TestTrue(TEXT("Neuron touch reaches the actual Space edge reader"), UMiningGamepadSubsystem::ReadJumpPressed(Controller));
    const float NeuronYaw = Controller->RotationInput.Yaw;
    Controls->HandlePointerMoved(Geometry, 4, JumpCenter + FVector2D(35,0));
    TestTrue(TEXT("Neuron jump finger can aim during transit"), Controller->RotationInput.Yaw != NeuronYaw);
    Controls->HandlePointerEnded(4);
    Controller->PlayerInput->ProcessInputStack(InputStack, .016f, false);
    TestFalse(TEXT("Neuron jump release is not sticky"), UMiningGamepadSubsystem::ReadJumpHeld(Controller));
    TestTrue(TEXT("Pause opens during locked-pawn ending"), Menu->ToggleMobileMenu());
    TestFalse(TEXT("Pause blocks cinematic touch input"), Controller->IsMobileLookEnabled() || Controller->IsMobileJumpEnabled());
    Menu->CloseMenu();
    TestTrue(TEXT("Resume preserves mining suspension and hidden pawn lock"), Mining->IsInputSuspended() && Controller->IsMoveInputIgnored());
    TestTrue(TEXT("Resume restores neuron aim and jump"), Controller->IsMobileLookEnabled() && Controller->IsMobileJumpEnabled());
    for (int32 Shot = 0; Shot <= 13; ++Shot)
    {
        Ending->Shot = Shot;
        Controller->SetIgnoreLookInput(true);
        const bool UsesJoystick = Shot <= 6 || Shot == 9;
        TestEqual(TEXT("Scale pullback and automatic drill have stage-appropriate steering"), Controller->IsMobileMoveEnabled(), UsesJoystick);
        TestFalse(TEXT("Film cannot manually aim, jump or mine"), Controller->IsMobileLookEnabled() || Controller->IsMobileJumpEnabled() || Controls->IsButtonVisible(Button::Mine));
        Controller->ResetIgnoreLookInput();
    }
    Ending->FinaleVideoSource = NewObject<UFileMediaSource>(Ending);
    Ending->FinaleVideoPlayer = NewObject<UMediaPlayer>(Ending);
    Ending->bFinaleVideoRequested = true;
    Ending->LastFinaleVideoTime = 0;
    Ending->UpdateFinaleVideoProgress(19.f);
    TestFalse(TEXT("Slow video startup is given a grace period"), Ending->bComplete);
    Ending->UpdateFinaleVideoProgress(1.1f);
    TestTrue(TEXT("Stalled decoder cannot trap the player forever"), Ending->bComplete);
    Ending->bComplete = false;
    Ending->FinaleVideoIdleTime = 0;
    Ending->FinaleVideoFailed(TEXT("test"));
    Ending->UpdateFinaleVideoProgress(0);
    TestTrue(TEXT("Asynchronous video failure advances to credits"), Ending->bComplete);
    Ending->ResetFinaleVideo();
    TestTrue(TEXT("Restart clears failure and playback watchdog state"), !Ending->bFinaleVideoFailed && !Ending->bFinaleVideoRequested && Ending->FinaleVideoIdleTime == 0);
    TestFalse(TEXT("Credits expose no gameplay touch controls"), Controller->IsMobileMoveEnabled() || Controller->IsMobileLookEnabled() || Controller->IsMobileJumpEnabled() || Controls->IsButtonVisible(Button::Mine));
    TestTrue(TEXT("Credits keep the touch collection entry available"), MiningMobileUI::CanOpenCollection(Controller));
    Controller->HandleMobilePause();
    TestTrue(TEXT("Credits pause opens the touch menu instead of quitting"), Menu->IsMenuOpen());
    Menu->CloseMenu();
    TestTrue(TEXT("Credits resume preserves the scripted movement lock"), Controller->IsMoveInputIgnored());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMobileLifecycleTest, "BrainMining.Mobile.InputAndMenu", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningMobileLifecycleTest::RunTest(const FString&)
{
    auto* Instance = NewObject<UGameInstance>(GEngine);
    Instance->InitializeStandalone();
    auto* World = Instance->GetWorld();
    auto& Context = *Instance->GetWorldContext();
    auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
    auto* Viewport = NewObject<UGameViewportClient>(GEngine);
    Context.GameViewport = Viewport;
    Viewport->Init(Context, Instance, false);
    Viewport->SetViewportOverlayWidget(nullptr, SNew(SOverlay));
    LocalPlayer->PlayerAdded(Viewport, 0);
    auto* Controller = World->SpawnActor<AMiningEncounterPlayerController>();
    World->AddController(Controller);
    Controller->SetPlayer(LocalPlayer);
    auto* Pawn = World->SpawnActor<ACharacter>();
    auto* Mining = NewObject<UMiningComponent>(Pawn);
    Pawn->AddInstanceComponent(Mining);
    Mining->RegisterComponent();
    Controller->Possess(Pawn);
    Controller->EncounterPolicy->SetMiningAllowed(true);
    Controller->bForceTouchControls = true;
    ON_SCOPE_EXIT
    {
        Instance->GetSubsystem<UMiningExperienceSubsystem>()->CloseMenu();
        Controller->ResetMobileInput();
        Controller->MobileTouchControls.Reset();
        LocalPlayer->PlayerRemoved();
        Controller->UnPossess();
        LocalPlayer->PlayerController = nullptr;
        Instance->Shutdown();
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
    };
    auto Controls = SNew(SMobileTouchControls).OwnerController(Controller);
    Controller->MobileTouchControls = Controls;
    const FVector2D Size(1600,720);
    const FGeometry Geometry = FGeometry::MakeRoot(Size, FSlateLayoutTransform());
    using Button = SMobileTouchControls::EButton;
    const FVector2D MineCenter = Controls->GetButtonCenter(Button::Mine, Size);
    TestTrue(TEXT("Mining press is handled immediately"), Controls->HandlePointerStarted(Geometry, 1, MineCenter).IsEventHandled());
    const FRotator BeforeDrag = Controller->RotationInput;
    TestTrue(TEXT("Same finger can drag while holding a tool"), Controls->HandlePointerMoved(Geometry, 1, MineCenter + FVector2D(30,10)).IsEventHandled());
    TestTrue(TEXT("Tool drag delivers camera yaw and pitch"), !FMath::IsNearlyEqual(Controller->RotationInput.Yaw, BeforeDrag.Yaw) && !FMath::IsNearlyEqual(Controller->RotationInput.Pitch, BeforeDrag.Pitch));
    TestEqual(TEXT("Drag retains the active mining finger"), Controls->MinePointer, 1);
    TestTrue(TEXT("Independent second look finger is accepted"), Controls->HandlePointerStarted(Geometry, 2, FVector2D(1020,300)).IsEventHandled());
    TestEqual(TEXT("Second finger owns look without replacing mining"), Controls->LookPointer, 2);
    TestTrue(TEXT("Third finger can move independently"), Controls->HandlePointerStarted(Geometry, 3, FVector2D(160,570)).IsEventHandled());
    auto* Menu = Instance->GetSubsystem<UMiningExperienceSubsystem>();
    TestTrue(TEXT("Mobile pause opens from the actual subsystem"), Menu->ToggleMobileMenu());
    TestTrue(TEXT("Menu suspends mining"), Mining->IsInputSuspended());
    TestEqual(TEXT("Menu releases the mining pointer"), Controls->MinePointer, -1);
    TestEqual(TEXT("Menu releases look pointer"), Controls->LookPointer, -1);
    TestEqual(TEXT("Menu releases movement pointer"), Controls->JoystickPointer, -1);
    TFunction<bool(const TSharedRef<SWidget>&, FName)> HasTag;
    HasTag = [&HasTag](const TSharedRef<SWidget>& Widget, FName Tag)
    {
        if (Widget->GetTag() == Tag) return true;
        auto* Children = Widget->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
            if (HasTag(Children->GetChildAt(Index), Tag)) return true;
        return false;
    };
    TestTrue(TEXT("Pause shows its collection action"), HasTag(Menu->Menu.ToSharedRef(), TEXT("gem")));
    Menu->RequestQuitConfirmation();
    Menu->Menu->Tick(Geometry, 0, .016f);
    TestTrue(TEXT("External quit request shows the actual touch confirmation"), HasTag(Menu->Menu.ToSharedRef(), TEXT("check")));
    TestFalse(TEXT("Quit confirmation replaces pause actions"), HasTag(Menu->Menu.ToSharedRef(), TEXT("gem")));
    Menu->CancelQuitConfirmation();
    Menu->Menu->Tick(Geometry, 0, .016f);
    TestTrue(TEXT("Cancelling quit restores the actual touch pause actions"), HasTag(Menu->Menu.ToSharedRef(), TEXT("gem")));
    TestFalse(TEXT("Cancelling quit removes the exit confirmation action"), HasTag(Menu->Menu.ToSharedRef(), TEXT("check")));
    Menu->CloseMenu();
    TestFalse(TEXT("Resume restores input suspension"), Mining->IsInputSuspended());
    TestFalse(TEXT("Resume restores movement and camera input"), Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored());
    TestFalse(TEXT("Resume never restarts a released tool"), Mining->IsMiningActive());
    Pawn->InputComponent = NewObject<UEnhancedInputComponent>(Pawn);
    Pawn->InputComponent->RegisterComponent();
    auto* Collection = World->SpawnActorDeferred<AMiningCollectionField>(AMiningCollectionField::StaticClass(), FTransform::Identity);
    Collection->bCatalogueOnly = true;
    Collection->bSharedAbilities = true;
    Collection->bUseAuthoredPlacements = true;
    Collection->bShowExplorationHUD = false;
    Collection->FinishSpawning(FTransform::Identity);
    TestTrue(TEXT("Pause opens before the collection action"), Menu->ToggleMobileMenu());
    Menu->OpenPauseCollection();
    TestTrue(TEXT("Pause collection action opens the catalogue"), Collection->bCatalogueOpen);
    TestFalse(TEXT("Collection replaces the pause menu"), Menu->IsMenuOpen());
    TestTrue(TEXT("Catalogue exposes the mobile back action"), Controller->IsMobileBackVisible());
    Controller->HandleMobileBack();
    TestFalse(TEXT("Mobile back closes the catalogue without a widget key event"), Collection->bCatalogueOpen);
    TestFalse(TEXT("Catalogue back restores gameplay input"), Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored() || Controller->bShowMouseCursor || Controller->IsPaused());
    TestTrue(TEXT("Catalogue back restores the mining tick"), Mining->IsComponentTickEnabled());
    if (!Collection->HasActorBegunPlay()) Collection->EndPlay(EEndPlayReason::Destroyed);
    Collection->Destroy();
    Controller->SetIgnoreMoveInput(true);
    Controller->SetIgnoreLookInput(true);
    Controller->bShowMouseCursor = true;
    TestTrue(TEXT("Pause can overlay an existing modal room"), Menu->ToggleMobileMenu());
    Menu->CloseMenu();
    TestTrue(TEXT("Modal room retains its original input locks"), Controller->IsMoveInputIgnored() && Controller->IsLookInputIgnored() && Controller->bShowMouseCursor);
    Controller->ResetIgnoreInputFlags();
    Controller->bShowMouseCursor = false;
    Controls->HandlePointerStarted(Geometry, 5, MineCenter);
    Controller->SetIgnoreLookInput(true);
    Controls->Tick(Geometry, 0, .016f);
    TestEqual(TEXT("Entering a room modal releases hidden mining input"), Controls->MinePointer, -1);
    TestFalse(TEXT("Modal transition stops the tool"), Mining->IsMiningActive());
    Controller->ResetIgnoreInputFlags();
    Controls->HandlePointerStarted(Geometry, 4, MineCenter);
    Controller->OnApplicationActivationChanged(false);
    TestEqual(TEXT("Focus loss releases the held tool"), Controls->MinePointer, -1);
    TestTrue(TEXT("No removed reset, book or map buttons"), !Controls->IsButtonVisible(Button::Reset) && !Controls->IsButtonVisible(Button::Catalogue) && !Controls->IsButtonVisible(Button::Map));
    auto* Echo = World->SpawnActor<AEchoPassageStage>();
    auto* Entrance = World->SpawnActor<AMiningGiantRoomEntrance>();
    Controller->EchoInteraction->Stage = Echo;
    Echo->bLanded = true;
    Echo->bCompleted = false;
    Pawn->SetActorLocation(FVector(-Entrance->MiningZoneDepth - 100, 0, 0));
    Entrance->Tick(0);
    TestTrue(TEXT("Echo exploration exposes knock"), Controls->IsButtonVisible(Button::Primary));
    TestFalse(TEXT("Echo exploration hides mining"), Controls->IsButtonVisible(Button::Mine));
    Pawn->SetActorLocation(FVector(-Entrance->MiningZoneDepth + 100, 0, 0));
    Entrance->Tick(0);
    TestFalse(TEXT("Exit wall is mineable before completing the passage"), Echo->bCompleted || Mining->IsEncounterBlocked());
    TestFalse(TEXT("Inactive echo no longer masks the mining button"), Controls->IsButtonVisible(Button::Primary));
    TestTrue(TEXT("Exit wall exposes the actual mining button"), Controls->IsButtonVisible(Button::Mine));
    Controls->HandlePointerStarted(Geometry, 6, MineCenter);
    TestTrue(TEXT("Exit wall touch reaches mining input"), Mining->IsMiningActive());
    Controller->ResetMobileInput();
    Entrance->Destroy();
    Echo->Destroy();
    auto* CardStage = World->SpawnActor<ACardRoomStage>();
    Controller->CardInteraction->Stage = CardStage;
    auto& CardRun = CardStage->Run;
    CardRun.Phase = ECardRoomPhase::Choosing;
    CardRun.Round = 1;
    CardRun.Hand = {ERoomCard::Laser};
    CardRun.Stage(0);
    Controller->CardInteraction->bRoomActive = false;
    TestFalse(TEXT("Inactive card rooms cannot replace mining controls"),Controller->IsMobileConfirmVisible());
    Controller->HandleMobileConfirm();
    TestTrue(TEXT("Mobile confirm ignores inactive rooms"),CardRun.Phase == ECardRoomPhase::Choosing);
    Controller->CardInteraction->bRoomActive = true;
    TestTrue(TEXT("Active card room owns confirmation"),Controller->IsMobileConfirmVisible());
    TestFalse(TEXT("No unreachable confirm underneath card overlay"),Controls->IsButtonVisible(Button::Confirm));
    Controller->HandleMobileConfirm();
    TestTrue(TEXT("Mobile confirm dispatches without a keyboard event"),CardRun.Phase == ECardRoomPhase::Resolving);
    auto* RewardEntrance = World->SpawnActor<AMiningGiantRoomEntrance>();
    auto* Escape = World->SpawnActor<AMiningRoomEscape>();
    Escape->bBuildStudyLayout = false;
    Escape->CardStage = CardStage;
    Escape->Entrance = RewardEntrance;
    World->InitializeActorsForPlay(FURL());
    Escape->DispatchBeginPlay();
    Escape->Tick(0.f);
    TestTrue(TEXT("Escape reward callback is initialized"),CardStage->OnCollectibleClaimed.IsBound());
    TestFalse(TEXT("Drill remains locked before explicit pickup"),Mining->IsDrillUnlocked());
    CardRun.Phase = ECardRoomPhase::Victory;
    TestFalse(TEXT("Victory does not unlock the drill automatically"),Mining->IsDrillUnlocked());
    TestFalse(TEXT("No unreachable pickup button underneath card overlay"),Controls->IsButtonVisible(Button::Interact));
    Controller->HandleMobileInteract();
    TestTrue(TEXT("Reward pickup unlocks and equips drill"),Mining->IsDrillUnlocked() && Mining->bDrillMode);
    TestTrue(TEXT("Reward pickup starts the escape warning"),Escape->Phase == EMiningEscapePhase::Warning);
    TestTrue(TEXT("Reward is collected and card input is released"),CardRun.Phase == ECardRoomPhase::Collected && !Controller->CardInteraction->bRoomActive);
    TestTrue(TEXT("Exploration interaction returns after pickup"),Controls->IsButtonVisible(Button::Interact));
    TestFalse(TEXT("Reward cannot be claimed twice"),CardStage->CollectReward());
    TestTrue(TEXT("Duplicate pickup preserves escape phase"),Escape->Phase == EMiningEscapePhase::Warning);
    Escape->Destroy();
    RewardEntrance->Destroy();
    CardStage->Destroy();
    return true;
}

static FAutoConsoleCommandWithWorld MobileCaptureCommand(TEXT("bm.Mobile.Capture"), TEXT("Capture runtime mobile HUD and menus, then exit. Use -MobileUI."), FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
    if (!World || !World->IsGameWorld()) return;
    TWeakObjectPtr<UWorld> WeakWorld(World);
    auto Step = MakeShared<int32>(0);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld, Step](float)
    {
        if (!WeakWorld.IsValid()) return false;
        auto* Player = Cast<ABrainMiningPlayerController>(WeakWorld->GetFirstPlayerController());
        if (!Player || !Player->GetPawn()) return true;
        auto* Menu = Player->GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>();
        if (*Step == 0)
        {
            for (TActorIterator<AMiningOpening> Opening(WeakWorld.Get()); Opening; ++Opening) Opening->StartGame();
        }
        else if (*Step == 1)
        {
            if (AMiningOpening::IsOpening(WeakWorld.Get())) return true;
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Validation/MobileUI/hud.png"), true, false);
        }
        else if (*Step == 2) Menu->ToggleMobileMenu();
        else if (*Step == 3) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Validation/MobileUI/pause.png"), true, false);
        else if (*Step == 4) { Menu->CloseMenu(); Menu->ToggleMobileMenu(true); }
        else if (*Step == 5) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Validation/MobileUI/upgrade.png"), true, false);
        else { Menu->CloseMenu(); UE_LOG(LogTemp, Display, TEXT("MOBILE_UI_CAPTURE_COMPLETE")); FPlatformMisc::RequestExit(false); return false; }
        ++*Step;
        return true;
    }), 4.f);
}));
#endif
