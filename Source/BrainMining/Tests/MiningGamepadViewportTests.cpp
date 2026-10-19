#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "MiningGamepadSubsystem.h"
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

namespace MiningGamepadViewportTests
{
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
#endif
