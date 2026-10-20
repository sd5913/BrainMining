#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "MobileTouchControls.h"
#include "MobileUIWidgets.h"
#include "MiningComponent.h"
#include "MiningEncounterPlayer.h"
#include "MiningExperienceSubsystem.h"
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
    Menu->CloseMenu();
    TestFalse(TEXT("Resume restores input suspension"), Mining->IsInputSuspended());
    TestFalse(TEXT("Resume restores movement and camera input"), Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored());
    TestFalse(TEXT("Resume never restarts a released tool"), Mining->IsMiningActive());
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
