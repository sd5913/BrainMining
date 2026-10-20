#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "BrainMiningPlayerController.h"
#include "MiningEncounterPlayer.h"
#include "MobileTouchControls.h"
#include "WenSi/WenSiPuzzle.h"
#include "WenSi/WenSiMirrorPuzzle.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Input/HittestGrid.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SVirtualWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWenSiMobileInputTest, "BrainMining.Mobile.WenSiTouchRouting", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWenSiMobileInputTest::RunTest(const FString&)
{
    auto* Instance = NewObject<UGameInstance>(GEngine);
    Instance->InitializeStandalone();
    auto* World = Instance->GetWorld();
    auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
    auto* Viewport = NewObject<UGameViewportClient>(GEngine);
    Instance->GetWorldContext()->GameViewport = Viewport;
    Viewport->Init(*Instance->GetWorldContext(), Instance, false);
    Viewport->SetViewportOverlayWidget(nullptr, SNew(SOverlay));
    LocalPlayer->PlayerAdded(Viewport, 0);
    auto* Controller = World->SpawnActor<AMiningEncounterPlayerController>();
    World->AddController(Controller);
    Controller->SetPlayer(LocalPlayer);
    Controller->Possess(World->SpawnActor<ACharacter>());
    ON_SCOPE_EXIT
    {
        Controller->ResetMobileInput();
        LocalPlayer->PlayerRemoved();
        Controller->UnPossess();
        LocalPlayer->PlayerController = nullptr;
        Instance->Shutdown();
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
    };
    auto Controls = SNew(SMobileTouchControls).OwnerController(Controller);
    const auto Verify = [&](auto* Widget, auto* Puzzle, auto* Thought)
    {
        Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        auto Root = Widget->TakeWidget();
        auto Window = SNew(SVirtualWindow).Size(FVector2D(1600,900));
        Window->SetContent(SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
            + SOverlay::Slot()[Controls]
            + SOverlay::Slot()[Root]);
        const auto Paint = [&](const FVector2D& Size)
        {
            Window->Resize(Size);
            Window->SlatePrepass(1.f);
            auto& Grid = Window->GetHittestGrid();
            Grid.SetHittestArea(FVector2D::ZeroVector, Size);
            Grid.Clear();
            FPaintArgs Args(nullptr, Grid, FVector2D::ZeroVector, FPlatformTime::Seconds(), .016f);
            FSlateWindowElementList Elements(Window);
            Window->Paint(Args, FGeometry::MakeRoot(Size, FSlateLayoutTransform()), FSlateRect(0,0,Size.X,Size.Y), Elements, 0, FWidgetStyle(), true);
        };
        for (const bool HasThought : {false, true})
        {
            Puzzle->Thoughts[0] = HasThought ? Thought : nullptr;
            Widget->Refresh();
            for (const FVector2D Size : {FVector2D(1600,900), FVector2D(2400,1080)})
            {
                Paint(Size);
                Widget->NativeTick(FGeometry::MakeRoot(Size, FSlateLayoutTransform()), 1.f);
                Paint(Size);
                for (const FVector2D Fraction : {FVector2D(.7,.3), FVector2D(.9,.5), FVector2D(.2,.8), FVector2D(.9,.8)})
                {
                    const FVector2D Position = Size * Fraction;
                    const auto Path = Window->GetHittestGrid().GetBubblePath(Position, 0.f, false);
                    TestTrue(TEXT("Exploration HUD passes empty space and thought cards to touch controls"), !Path.IsEmpty() && Path.Last().Widget == Controls);
                }
                const FVector2D Position = Size * FVector2D(.7,.3);
                const FGeometry Geometry = FGeometry::MakeRoot(Size, FSlateLayoutTransform());
                const FPointerEvent Start(0, Position, Position, TSet<FKey>(), EKeys::LeftMouseButton, 0.f, FModifierKeysState());
                Controls->OnTouchStarted(Geometry, Start);
                const float Yaw = Controller->RotationInput.Yaw;
                const FPointerEvent Move(0, Position + FVector2D(40,0), Position, TSet<FKey>(), EKeys::LeftMouseButton, 0.f, FModifierKeysState());
                Controls->OnTouchMoved(Geometry, Move);
                TestTrue(TEXT("Touch drag changes camera yaw with WenSi HUD visible"), Controller->RotationInput.Yaw != Yaw);
                Controls->OnTouchEnded(Geometry, Move);
            }
        }
        Puzzle->ToggleEdit();
        TestTrue(TEXT("Thought editing still locks the camera"), Controller->IsLookInputIgnored());
        Paint(FVector2D(1600,900));
        const auto CardPath = Window->GetHittestGrid().GetBubblePath(FVector2D(160,450), 0.f, false);
        TestTrue(TEXT("Editing thought card still receives pointer hits"), !CardPath.IsEmpty() && CardPath.Last().Widget != Controls);
        const auto EmptyPath = Window->GetHittestGrid().GetBubblePath(FVector2D(1120,270), 0.f, false);
        TestTrue(TEXT("Editing leaves empty space available to mobile buttons"), !EmptyPath.IsEmpty() && EmptyPath.Last().Widget == Controls);
        Puzzle->ToggleEdit();
        TestTrue(TEXT("Leaving thought editing restores touch look"), Controller->IsMobileLookEnabled());
        Window->SetContent(SNullWidget::NullWidget);
    };
    auto* Puzzle = World->SpawnActor<AWenSiPuzzle>();
    Puzzle->UI = CreateWidget<UWenSiPuzzleWidget>(Controller);
    Puzzle->UI->Puzzle = Puzzle;
    Verify(Puzzle->UI.Get(), Puzzle, World->SpawnActor<AWenSiObject>());
    auto* Mirror = World->SpawnActor<AWenSiMirrorPuzzle>();
    Mirror->UI = CreateWidget<UWenSiMirrorWidget>(Controller);
    Mirror->UI->Puzzle = Mirror;
    Verify(Mirror->UI.Get(), Mirror, World->SpawnActor<AWenSiMirrorExhibit>());
    Puzzle->bAreaManaged = true;
    Puzzle->SetAreaActive(false);
    Mirror->CloseThoughts();
    auto* LeftThought = World->SpawnActor<AWenSiMirrorExhibit>(FVector(1000,0,0), FRotator::ZeroRotator);
    auto* RightThought = World->SpawnActor<AWenSiMirrorExhibit>(FVector(1400,0,0), FRotator::ZeroRotator);
    LeftThought->Kind = RightThought->Kind = 2;
    LeftThought->Word = EWenSiMirrorWord::Heavy;
    RightThought->Word = EWenSiMirrorWord::Conductive;
    using Button = SMobileTouchControls::EButton;
    const FVector2D Size(2400,1080);
    const FGeometry Geometry = FGeometry::MakeRoot(Size, FSlateLayoutTransform());
    const auto Press = [&](Button Action)
    {
        const FVector2D Position = Controls->GetButtonCenter(Action, Size);
        TestTrue(TEXT("WenSi action button accepts touch"), Controls->HandlePointerStarted(Geometry, 4, Position).IsEventHandled());
        Controls->HandlePointerEnded(4);
    };
    Mirror->Aim = LeftThought;
    Press(Button::Interact);
    TestTrue(TEXT("Touch interaction selects first thought immediately"), Mirror->Thoughts[0] == LeftThought);
    TestFalse(TEXT("Exchange is unavailable with only one thought"), Controls->IsButtonVisible(Button::Edit));
    Mirror->Aim = RightThought;
    Press(Button::Interact);
    TestTrue(TEXT("Touch interaction selects second thought"), Mirror->Thoughts[1] == RightThought);
    TestTrue(TEXT("Two thoughts expose the exchange button"), Controls->IsButtonVisible(Button::Edit));
    Press(Button::Edit);
    TestTrue(TEXT("Mobile edit action starts the current mirror exchange"), Mirror->IsSwapping());
    TestFalse(TEXT("Exchange does not lock camera in obsolete drag mode"), Mirror->bEditing || Controller->IsLookInputIgnored());
    Mirror->AdvanceSwap(AWenSiMirrorPuzzle::SwapTravelSeconds);
    TestTrue(TEXT("Mobile exchange commits both words"), LeftThought->Word == EWenSiMirrorWord::Conductive && RightThought->Word == EWenSiMirrorWord::Heavy);
    Mirror->AdvanceSwap(AWenSiMirrorPuzzle::SwapTotalSeconds);
    TestFalse(TEXT("Exchange completes and releases thoughts"), Mirror->IsSwapping() || Mirror->Thoughts[0].IsValid() || Mirror->Thoughts[1].IsValid());
    Mirror->Thoughts[0] = LeftThought;
    Mirror->Thoughts[1] = RightThought;
    Press(Button::ThoughtRight);
    TestTrue(TEXT("Right dismissal preserves left thought and never swaps"), Mirror->Thoughts[0] == LeftThought && !Mirror->Thoughts[1].IsValid() && !Mirror->IsSwapping());
    Press(Button::ThoughtLeft);
    TestFalse(TEXT("Left dismissal clears its thought"), Mirror->Thoughts[0].IsValid());
    Mirror->SetAreaActive(false);
    Puzzle->SetAreaActive(true);
    Puzzle->Thoughts[0] = World->SpawnActor<AWenSiObject>();
    Press(Button::Edit);
    TestTrue(TEXT("Legacy puzzle still enters word editing"), Puzzle->bEditing && Controller->IsLookInputIgnored());
    TestTrue(TEXT("Word editing exposes mobile back"), Controller->IsMobileBackVisible());
    Controller->HandleMobileBack();
    TestTrue(TEXT("Mobile back leaves word editing and restores view"), !Puzzle->bEditing && Controller->IsMobileLookEnabled());
    return true;
}
#endif
