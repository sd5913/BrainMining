#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "MiningGamepadSubsystem.h"
#include "MiningHapticsSubsystem.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "BrainMiningPlayerController.h"
#include "MiningExperienceSubsystem.h"
#include "StartRoom/MiningOpening.h"
#include "PopupClose/PopupCloseStage.h"
#include "PopupClose/PopupOfficeDevice.h"
#include "PopupClose/PopupCloseWidgets.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "Widgets/SWindow.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Widgets/SViewport.h"
#include "Widgets/Text/STextBlock.h"

namespace MiningGamepadViewportTests
{
FString WidgetText(const TSharedRef<SWidget>& Widget)
{
    if (Widget->GetTypeAsString() == TEXT("STextBlock")) return StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString();
    FString Text;
    auto* Children = Widget->GetChildren();
    for (int32 Index = 0; Index < Children->Num(); ++Index) Text += WidgetText(Children->GetChildAt(Index));
    return Text;
}

class FStart : public IAutomationLatentCommand
{
    bool Update() override
    {
        FRequestPlaySessionParams Parameters;
        Parameters.EditorPlaySettings = DuplicateObject<ULevelEditorPlaySettings>(GetMutableDefault<ULevelEditorPlaySettings>(), GetTransientPackage());
        Parameters.EditorPlaySettings->NewWindowWidth = 1280;
        Parameters.EditorPlaySettings->NewWindowHeight = 720;
        Parameters.DestinationSlateViewport = nullptr;
        GEditor->RequestPlaySession(Parameters);
        return true;
    }
};

class FRun : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started = FPlatformTime::Seconds();
    double Changed = Started;
    int32 Step = 0;
    int32 MenuStep = 0;
    int32 MenuNavigationStep = 0;
    int32 HapticSettingsStep = 0;
    TWeakObjectPtr<APopupOfficeDevice> Computer;
    TWeakObjectPtr<AMiningOpening> Opening;
    FVector2D PreviousCursor = FVector2D::ZeroVector;
public:
    explicit FRun(FAutomationTestBase* InTest) : Test(InTest) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds() - Started > 90) { Test->AddError(FString::Printf(TEXT("Gamepad viewport timed out at step %d"), Step)); return true; }
        UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
        if (!World || !World->HasBegunPlay() || World->GetTimeSeconds() < 3) return false;
        auto* Player = Cast<ABrainMiningPlayerController>(World->GetFirstPlayerController());
        auto* Pawn = Player ? Cast<ACharacter>(Player->GetPawn()) : nullptr;
        auto* Input = UMiningGamepadSubsystem::Find(Player);
        auto* Viewport = World->GetGameViewport();
        if (!Player || !Pawn || !Input || !Viewport || !Viewport->GetGameViewportWidget()) return false;
        auto& Slate = FSlateApplication::Get();
        const int32 User = Player->GetLocalPlayer()->GetControllerId();
        const auto Key = [&Slate, User](FKey Button, bool bPressed)
        {
            const FKeyEvent Event(Button, FModifierKeysState(), User, false, 0, 0);
            if (bPressed) Slate.ProcessKeyDownEvent(Event); else Slate.ProcessKeyUpEvent(Event);
        };
        const auto Tap = [&Key](FKey Button) { Key(Button, true); Key(Button, false); };
        const auto Axis = [&Slate, User](FKey Button, float Value)
        {
            Slate.ProcessAnalogInputEvent(FAnalogInputEvent(Button, FModifierKeysState(), User, false, 0, 0, Value));
        };
        if (Step == 0)
        {
            if (MenuStep == 0)
            {
                Slate.ProcessApplicationActivationEvent(true);
                Slate.SetUserFocus(User, Viewport->GetGameViewportWidget(), EFocusCause::SetDirectly);
                if (!Test->TestTrue(TEXT("Real PIE viewport receives the local player's input"), Input->CanReceiveInput(User))) return true;
                Axis(EKeys::Gamepad_LeftX, .8f);
                Test->TestTrue(TEXT("Slate analog event reaches movement"), UMiningGamepadSubsystem::ReadMove(Player).X > .5);
                Axis(EKeys::Gamepad_LeftX, 0);
                Key(EKeys::Gamepad_FaceButton_Bottom, true);
                Test->TestTrue(TEXT("Slate button event reaches jump"), UMiningGamepadSubsystem::ReadJumpHeld(Player));
                Key(EKeys::Gamepad_FaceButton_Bottom, false);
                Tap(EKeys::Gamepad_Special_Right);
                MenuStep = 1;
                return false;
            }
            if (MenuStep == 1)
            {
                Test->TestTrue(TEXT("Menu button opens the actual paused UI"), Player->IsPaused() && Input->GetContext() == EMiningInputContext::Menu);
                Test->TestTrue(TEXT("Input remains available with UIOnly button focus"), Input->CanReceiveInput(User));
                Tap(EKeys::Gamepad_FaceButton_Bottom);
                Test->TestFalse(TEXT("A activates the focused resume button"), Player->IsPaused());
                Tap(EKeys::Gamepad_Special_Right);
                MenuStep = 2;
                return false;
            }
            if (MenuStep == 2)
            {
                if (MenuNavigationStep++ < 2) { Tap(EKeys::Gamepad_DPad_Down); return false; }
                MenuNavigationStep = 0;
                Tap(EKeys::Gamepad_FaceButton_Bottom);
                MenuStep = 3;
                return false;
            }
            if (MenuStep == 3)
            {
                Test->TestTrue(TEXT("Controller navigation opens audio settings"), Player->IsAudioSettingsOpen());
                Test->TestTrue(TEXT("Audio settings focus remains available to the controller"), Input->CanReceiveInput(User));
                Test->TestTrue(TEXT("Settings pause the game"), Player->IsPaused());
                if (auto* Haptics = UMiningHapticsSubsystem::Find(Player))
                {
                    Test->TestFalse(TEXT("Paused settings cannot queue a gameplay impact"), Haptics->Play(EMiningHapticCue::Explosion));
                    Test->TestTrue(TEXT("Paused output remains zero"), Haptics->GetOutput().IsNearlyZero());
                }
                const auto Focused = Slate.GetUserFocusedWidget(User);
                if (HapticSettingsStep >= 5 && HapticSettingsStep <= 7)
                {
                    const TCHAR* Expected[] = {TEXT("震动总开关"), TEXT("钻机与喷气持续震动"), TEXT("轻微交互与演出提示")};
                    Test->TestTrue(TEXT("Controller can reach each vibration toggle"), Focused.IsValid() && WidgetText(Focused.ToSharedRef()).Contains(Expected[HapticSettingsStep - 5]));
                }
                if (HapticSettingsStep < 8) { ++HapticSettingsStep; Tap(EKeys::Gamepad_DPad_Down); return false; }
                Test->TestTrue(TEXT("Controller can reach the vibration strength slider"), Focused.IsValid() && Focused->GetTypeAsString() == TEXT("SSlider"));
                Tap(EKeys::Gamepad_FaceButton_Right);
                Test->TestFalse(TEXT("B closes audio settings"), Player->IsAudioSettingsOpen());
                Tap(EKeys::Gamepad_Special_Right);
                MenuStep = 4;
                return false;
            }
            if (MenuStep == 4)
            {
                if (MenuNavigationStep++ < 3) { Tap(EKeys::Gamepad_DPad_Down); return false; }
                Tap(EKeys::Gamepad_FaceButton_Bottom);
                MenuStep = 5;
                return false;
            }
            auto* Experience = Player->GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>();
            if (MenuStep == 5)
            {
                Test->TestTrue(TEXT("Quit requires a separate confirmation"), Experience->bQuitConfirmation);
                Tap(EKeys::Gamepad_FaceButton_Bottom);
                MenuStep = 6;
                return false;
            }
            Test->TestFalse(TEXT("Quit confirmation initially focuses safe cancellation"), Experience->bQuitConfirmation);
            Tap(EKeys::Gamepad_FaceButton_Right);
            Test->TestFalse(TEXT("B closes pause without leaking another pause action"), Player->IsPaused());
            for (TActorIterator<APopupOfficeDevice> Device(World); Device; ++Device)
                if (!Device->IsPrinter() && !Device->IsLocked()) { Computer = *Device; break; }
            if (!Test->TestTrue(TEXT("Office has an interactive computer"), Computer.IsValid())) return true;
            const FVector Position = Computer->GetActorLocation() - Computer->GetActorForwardVector() * 215 + FVector(0, 0, 98);
            Pawn->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            Player->SetControlRotation((Computer->InteractionPoint() - (Position + FVector(0, 0, 60))).Rotation());
            Player->PlayerCameraManager->UpdateCamera(.01f);
            Changed = FPlatformTime::Seconds();
            ++Step;
            return false;
        }
        if (Step == 1)
        {
            if (FPlatformTime::Seconds() - Changed < .25) return false;
            Tap(EKeys::Gamepad_FaceButton_Left);
            if (!Test->TestTrue(TEXT("X enters the targeted computer through scene routing"), Input->GetContext() == EMiningInputContext::Office)) return true;
            Changed = FPlatformTime::Seconds();
            ++Step;
            return false;
        }
        if (Step == 2)
        {
            if (FPlatformTime::Seconds() - Changed < 1.2) return false;
            if (!Test->TestTrue(TEXT("Computer session owns the camera"), Player->GetViewTarget() == Computer.Get())) return true;
            const auto OfficeCapture = Viewport->GetMouseCaptureMode();
            Tap(EKeys::Gamepad_Special_Right);
            Test->TestTrue(TEXT("Menu pauses the computer without exiting it"), Player->IsPaused() && Input->GetContext() == EMiningInputContext::Menu);
            Test->TestTrue(TEXT("Paused computer retains its view target"), Player->GetViewTarget() == Computer.Get());
            Tap(EKeys::Gamepad_FaceButton_Right);
            Test->TestFalse(TEXT("B resumes the computer"), Player->IsPaused());
            Test->TestTrue(TEXT("Computer context is restored after pause"), Input->GetContext() == EMiningInputContext::Office);
            Test->TestTrue(TEXT("Computer pointer and capture are restored"), Player->bShowMouseCursor && Viewport->GetMouseCaptureMode() == OfficeCapture);
            Test->TestTrue(TEXT("Resumed computer can receive controller input"), Input->CanReceiveInput(User));
            PreviousCursor = Slate.GetCursorPos();
            Test->AddInfo(FString::Printf(TEXT("Office viewport geometry=%s, cursor=%s"), *Viewport->GetGameViewportWidget()->GetCachedGeometry().GetLocalSize().ToString(), *PreviousCursor.ToString()));
            Axis(EKeys::Gamepad_RightX, .8f);
            Input->TickInput(.05f);
            Axis(EKeys::Gamepad_RightX, 0);
            Test->TestTrue(TEXT("Right stick moves the actual cursor"), FVector2D::Distance(PreviousCursor, Slate.GetCursorPos()) > 1);
            if (Computer->Pending() == 0) Computer->AddProblem();
            const FVector2D Pixel = Computer->GetDesktop()->GetLatestClosePoint();
            auto* Surface = Computer->GetSurface();
            if (!FApp::CanEverRender())
            {
                const auto Window = Surface->GetVirtualWindow();
                if (!Test->TestTrue(TEXT("World desktop owns a Slate window"), Window.IsValid())) return true;
                const FVector2D Size = Surface->GetDrawSize();
                Window->SlatePrepass(1.f);
                auto& HitGrid = Window->GetHittestGrid();
                HitGrid.SetHittestArea(FVector2D::ZeroVector, Size);
                HitGrid.Clear();
                const FPaintArgs Args(nullptr, HitGrid, FVector2D::ZeroVector, FPlatformTime::Seconds(), .016f);
                FSlateWindowElementList Elements(Window.ToSharedRef());
                Window->Paint(Args, FGeometry::MakeRoot(Size, FSlateLayoutTransform(1.f)), FSlateRect(0, 0, Size.X, Size.Y), Elements, 0, FWidgetStyle(), true);
            }
            FVector Target = Surface->GetComponentTransform().TransformPosition(FVector(0, -(Pixel.X - 640), -(Pixel.Y - 400)));
            if (Computer->ScreenProjectionMaterial)
            {
                UStaticMeshComponent* Shell = nullptr;
                TArray<UStaticMeshComponent*> Meshes;
                Computer->GetComponents(Meshes);
                for (auto* Mesh : Meshes) if (Mesh->GetFName() == TEXT("OfficeArtShell")) Shell = Mesh;
                if (!Test->TestNotNull(TEXT("Curved monitor shell"), Shell)) return true;
                const FVector2D UV = Pixel / Surface->GetDrawSize();
                FVector Local((.5f - UV.X) * 35.87354f, -18.f, 41.48716f - UV.Y * 27.40217f);
                FVector Origin; FRotator Rotation;
                Player->GetPlayerViewPoint(Origin, Rotation);
                for (int32 Iteration = 0; Iteration < 4; ++Iteration)
                {
                    const FVector Direction = (Shell->GetComponentTransform().TransformPosition(Local) - Origin).GetSafeNormal();
                    FHitResult Hit;
                    FCollisionQueryParams Query(SCENE_QUERY_STAT(GamepadOfficeCursor), true);
                    Query.bReturnFaceIndex = true;
                    if (!Test->TestTrue(TEXT("Cursor ray reaches physical monitor glass"), Shell->LineTraceComponent(Hit, Origin, Origin + Direction * 1000, Query))) return true;
                    Local.Y = Shell->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint).Y;
                }
                Target = Shell->GetComponentTransform().TransformPosition(Local);
            }
            FVector2D Screen;
            if (!Test->TestTrue(TEXT("Reply projects into player viewport"), Player->ProjectWorldLocationToScreen(Target, Screen))) return true;
            Player->SetMouseLocation(FMath::RoundToInt(Screen.X), FMath::RoundToInt(Screen.Y));
            Computer->TickScreen(.01f, Player);
            if (auto* Pointer = Computer->FindComponentByClass<UWidgetInteractionComponent>())
                Test->AddInfo(FString::Printf(TEXT("Reply pixel=%s, pointer hit=%s, hit-visible=%d"), *Pixel.ToString(), *Pointer->Get2DHitLocation().ToString(), Pointer->IsOverHitTestVisibleWidget()));
            const int32 Before = Computer->Handled();
            Tap(EKeys::Gamepad_FaceButton_Bottom);
            Test->TestEqual(TEXT("A clicks the actual reply under the cursor exactly once"), Computer->Handled(), Before + 1);
            Tap(EKeys::Gamepad_FaceButton_Right);
            Changed = FPlatformTime::Seconds();
            ++Step;
            return false;
        }
        if (Step == 3)
        {
            if (FPlatformTime::Seconds() - Changed < 1.2) return false;
            Test->TestTrue(TEXT("Leaving the computer restores exploration context"), Input->GetContext() == EMiningInputContext::Exploration);
            Test->TestFalse(TEXT("Computer exit releases movement lock"), Player->IsMoveInputIgnored());
            Test->TestFalse(TEXT("Computer exit releases look lock"), Player->IsLookInputIgnored());
            Input->ResetDevice();
            Opening = World->SpawnActor<AMiningOpening>();
            ++Step;
            return false;
        }
        if (!Test->TestTrue(TEXT("Opening actor exists"), Opening.IsValid())) return true;
        if (Step == 4)
        {
            if (!Player->bShowMouseCursor) return false;
            Test->TestTrue(TEXT("Opening menu is routed as UI"), Input->GetContext() == EMiningInputContext::Menu);
            Test->TestTrue(TEXT("Opening button focus accepts the controller"), Input->CanReceiveInput(User));
            Tap(EKeys::Gamepad_FaceButton_Bottom);
            Test->TestTrue(TEXT("A starts the actual opening sequence"), Opening->bStarted);
            Test->TestTrue(TEXT("Opening sequence blocks gameplay"), Input->GetContext() == EMiningInputContext::Blocked);
            Test->TestFalse(TEXT("Starting the opening never begins flight"), UMiningGamepadSubsystem::ReadJumpHeld(Player));
            ++Step;
            return false;
        }
        if (!Opening->bFinished) return false;
        Test->TestTrue(TEXT("Opening completion restores exploration"), Input->GetContext() == EMiningInputContext::Exploration);
        Input->ResetDevice();
        return true;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningGamepadViewportTest, "BrainMining.Gamepad.ViewportOffice", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningGamepadViewportTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/PopupClose/Maps/Lvl_PopupClose"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<MiningGamepadViewportTests::FStart>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<MiningGamepadViewportTests::FRun>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
namespace MiningGamepadViewportTests
{
class FCardRun : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started = FPlatformTime::Seconds();
    int32 Step = 0, LastRound = 0;
    FVector EntryLocation;
public:
    explicit FCardRun(FAutomationTestBase* InTest) : Test(InTest) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds() - Started > 100) { Test->AddError(TEXT("Gamepad card encounter timed out")); return true; }
        auto* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
        auto* Player = World ? World->GetFirstPlayerController() : nullptr;
        APawn* Pawn = Player ? Player->GetPawn() : nullptr;
        auto* Cards = Player ? Player->FindComponentByClass<UCardRoomInteractionComponent>() : nullptr;
        auto* Input = UMiningGamepadSubsystem::Find(Player);
        auto* Viewport = World ? World->GetGameViewport() : nullptr;
        if (!Pawn || !Cards || !Cards->Stage || !Input || !Viewport || !Viewport->GetGameViewportWidget() || World->GetTimeSeconds() < 3) return false;
        auto& Slate = FSlateApplication::Get();
        const int32 User = Player->GetLocalPlayer()->GetControllerId();
        const auto Key = [&Slate, User](FKey Button, bool Pressed, bool Repeat = false)
        {
            const FKeyEvent Event(Button, FModifierKeysState(), User, Repeat, 0, 0);
            if (Pressed) Slate.ProcessKeyDownEvent(Event); else Slate.ProcessKeyUpEvent(Event);
        };
        const auto Tap = [&Key](FKey Button) { Key(Button, true); Key(Button, false); };
        auto& Run = Cards->Stage->Run;
        if (Step == 0)
        {
            Slate.ProcessApplicationActivationEvent(true);
            Slate.SetUserFocus(User, Viewport->GetGameViewportWidget(), EFocusCause::SetDirectly);
            Pawn->SetActorLocation(Cards->Stage->GetApproachLocation() + FVector(2000, 0, 0));
            Tap(EKeys::Gamepad_FaceButton_Left);
            Test->TestTrue(TEXT("X does not seat a distant player"), Run.Phase == ECardRoomPhase::Exploring);
            Pawn->SetActorLocation(Cards->Stage->GetApproachLocation());
            EntryLocation = Pawn->GetActorLocation();
            Tap(EKeys::Gamepad_FaceButton_Left);
            if (!Test->TestTrue(TEXT("Slate X seats player and starts matching"), Run.Phase == ECardRoomPhase::Matching)) return true;
            Test->TestTrue(TEXT("Seated encounter owns input"), Input->GetContext() == EMiningInputContext::Cards);
            Step = 1;
            return false;
        }
        if (Step == 1)
        {
            if (!Test->TestTrue(TEXT("Focused card view continues receiving gamepad events"), Input->CanReceiveInput(User))) return true;
            if (Run.Phase != ECardRoomPhase::Choosing) return false;
            Key(EKeys::Gamepad_FaceButton_Bottom, true);
            Key(EKeys::Gamepad_FaceButton_Bottom, true, true);
            Key(EKeys::Gamepad_FaceButton_Bottom, false);
            Test->TestTrue(TEXT("A selects only once while held"), Run.Staged.Contains(0));
            Test->TestFalse(TEXT("Selecting cards never starts flight"), UMiningGamepadSubsystem::ReadJumpHeld(Player));
            Tap(EKeys::Gamepad_Special_Right);
            Test->TestTrue(TEXT("Menu pauses over card view"), Input->GetContext() == EMiningInputContext::Menu);
            Step = 2;
            return false;
        }
        if (Step == 2)
        {
            Tap(EKeys::Gamepad_FaceButton_Left);
            Test->TestTrue(TEXT("X cannot play through pause overlay"), Run.Phase == ECardRoomPhase::Choosing && Run.Staged.Contains(0));
            Tap(EKeys::Gamepad_FaceButton_Right);
            Step = 3;
            return false;
        }
        if (Run.Phase == ECardRoomPhase::Choosing && Run.Round != LastRound)
        {
            if (!Test->TestTrue(TEXT("Card input resumes after pause and between rounds"), Input->CanReceiveInput(User))) return true;
            Test->TestEqual(TEXT("Each round starts on the first card"), Cards->GetFocusedCardIndex(), 0);
            if (Run.Round == 1)
            {
                Test->TestTrue(TEXT("Pause retains selected cards"), Run.Staged.Contains(0));
                Tap(EKeys::Gamepad_FaceButton_Right);
                Test->TestTrue(TEXT("B clears cards while keeping the player seated"), Run.Staged.IsEmpty() && Player->IsMoveInputIgnored());
            }
            if (Run.Round < 4)
            {
                Tap(EKeys::Gamepad_FaceButton_Left);
                Test->TestTrue(TEXT("Incomplete hand is not played"), Run.Phase == ECardRoomPhase::Choosing);
            }
            if (Run.Round == 3)
            {
                Slate.ProcessAnalogInputEvent(FAnalogInputEvent(EKeys::Gamepad_LeftX, FModifierKeysState(), User, false, 0, 0, .9f));
                Input->TickInput(.016f);
                Slate.ProcessAnalogInputEvent(FAnalogInputEvent(EKeys::Gamepad_LeftX, FModifierKeysState(), User, false, 0, 0, 0));
                Test->TestEqual(TEXT("Left stick navigates cards"), Cards->GetFocusedCardIndex(), 1);
                Tap(EKeys::Gamepad_FaceButton_Bottom);
                Tap(EKeys::Gamepad_LeftShoulder);
                Test->TestEqual(TEXT("LB navigates to previous card"), Cards->GetFocusedCardIndex(), 0);
                Tap(EKeys::Gamepad_RightShoulder);
                Test->TestEqual(TEXT("RB navigates to next card without equipping bomb"), Cards->GetFocusedCardIndex(), 1);
                Tap(EKeys::Gamepad_DPad_Left);
                Tap(EKeys::Gamepad_FaceButton_Bottom);
                Test->TestEqual(TEXT("Both combination cards can be selected"), Run.Staged.Num(), 2);
            }
            else if (Run.Round == 4)
            {
                Tap(EKeys::Gamepad_DPad_Left);
                Test->TestEqual(TEXT("Navigation reaches last card in overlapping seventeen-card fan"), Cards->GetFocusedCardIndex(), 16);
                Tap(EKeys::Gamepad_FaceButton_Bottom);
                Test->TestTrue(TEXT("Last overlapping card can be selected"), Run.Staged.Contains(16));
                Tap(EKeys::Gamepad_DPad_Right);
                Test->TestEqual(TEXT("Navigation wraps back to first card"), Cards->GetFocusedCardIndex(), 0);
                Tap(EKeys::Gamepad_FaceButton_Left);
                Test->TestTrue(TEXT("Final-round first X selects all before playing"), Run.Phase == ECardRoomPhase::Choosing && Run.Staged.Num() == 17);
                Tap(EKeys::Gamepad_FaceButton_Top);
                Test->TestTrue(TEXT("Y clears an already complete hand"), Run.Staged.IsEmpty());
                Key(EKeys::Gamepad_FaceButton_Top, true);
                Key(EKeys::Gamepad_FaceButton_Top, true, true);
                Key(EKeys::Gamepad_FaceButton_Top, false);
                Test->TestEqual(TEXT("Y selects all seventeen once"), Run.Staged.Num(), 17);
            }
            else Tap(EKeys::Gamepad_FaceButton_Bottom);
            LastRound = Run.Round;
            Tap(EKeys::Gamepad_FaceButton_Left);
            if (!Test->TestTrue(TEXT("X starts round resolution"), Run.Phase == ECardRoomPhase::Resolving)) return true;
            const int32 Selected = Run.Staged.Num();
            for (FKey Button : {EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Top, EKeys::Gamepad_FaceButton_Left, EKeys::Gamepad_RightTrigger, EKeys::Gamepad_DPad_Down}) Tap(Button);
            Test->TestTrue(TEXT("Animation phase ignores gameplay buttons"), Run.Phase == ECardRoomPhase::Resolving && Run.Staged.Num() == Selected);
            return false;
        }
        if (Run.Phase != ECardRoomPhase::Victory) return false;
        Test->TestEqual(TEXT("All four rounds completed via controller"), LastRound, 4);
        Test->TestTrue(TEXT("Victory advertises the controller reward action"), Input->GetHint().Contains(TEXT("领取钻机")));
        Tap(EKeys::Gamepad_FaceButton_Left);
        Test->TestTrue(TEXT("X claims the drill reward"), Run.Phase == ECardRoomPhase::Collected);
        Test->TestFalse(TEXT("Reward exits card interaction mode"), Cards->bRoomActive);
        Test->TestTrue(TEXT("Reward restores entry position"), Pawn->GetActorLocation().Equals(EntryLocation, 1));
        Test->TestTrue(TEXT("Reward restores pawn collision"), Pawn->GetActorEnableCollision());
        Test->TestFalse(TEXT("Reward unlocks movement"), Player->IsMoveInputIgnored());
        Test->TestFalse(TEXT("Reward unlocks camera"), Player->IsLookInputIgnored());
        Test->TestFalse(TEXT("Reward hides mouse cursor"), Player->bShowMouseCursor);
        Test->TestTrue(TEXT("Reward returns gamepad to exploration"), Input->GetContext() == EMiningInputContext::Exploration);
        Test->TestFalse(TEXT("Reward cannot be collected twice"), Cards->Stage->CollectReward());
        Input->ResetDevice();
        return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningGamepadCardViewportTest, "BrainMining.Gamepad.ViewportCards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningGamepadCardViewportTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Rooms/CardDuel/Maps/Lvl_CardRoom_P1"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<MiningGamepadViewportTests::FStart>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<MiningGamepadViewportTests::FCardRun>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
