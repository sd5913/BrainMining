#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "MiningHapticMixer.h"
#include "MiningHapticsSubsystem.h"
#include "MiningGamepadSubsystem.h"
#include "MiningComponent.h"
#include "MiningEncounterPlayer.h"
#include "MiningCharacterMovementComponent.h"
#include "BrainMiningCharacter.h"
#include "EchoPassageStage.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SOverlay.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningHapticPatternTest, "BrainMining.Gamepad.Haptics.PatternsAndMerging", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningHapticPatternTest::RunTest(const FString&)
{
    for (int32 Index = 0; Index < int32(EMiningHapticCue::Count); ++Index)
    {
        FMiningHapticMixer Mixer;
        const auto Cue = static_cast<EMiningHapticCue>(Index);
        const auto Pattern = FMiningHapticMixer::Pattern(Cue);
        TestTrue(TEXT("Every cue can start"), Mixer.Play(Cue));
        double Peak = 0;
        for (int32 Frame = 0; Frame < 120; ++Frame)
        {
            const auto Value = Mixer.Advance(1.f / 120.f);
            TestTrue(TEXT("Motor values remain finite and bounded"), !Value.ContainsNaN() && Value.X >= 0 && Value.X <= 1 && Value.Y >= 0 && Value.Y <= 1);
            Peak = FMath::Max(Peak, Value.GetMax());
        }
        TestTrue(TEXT("Every preset has perceptible output and terminates"), Peak > .01 && Mixer.NumActive() == 0);
        TestTrue(TEXT("Every preset is bounded in time"), Pattern.Duration > 0 && Pattern.Duration <= .5f);
    }
    FMiningHapticMixer Mixer;
    Mixer.Play(EMiningHapticCue::PickaxeContact);
    Mixer.Play(EMiningHapticCue::PickaxeBreak);
    TestEqual(TEXT("A breaking strike has one impact"), Mixer.NumActive(), 1);
    TestTrue(TEXT("Breaking replaces ordinary contact"), Mixer.IsActive(EMiningHapticCue::PickaxeBreak));
    TestFalse(TEXT("An ordinary impact cannot replace a break"), Mixer.Play(EMiningHapticCue::BlockedContact));
    Mixer.Reset();
    Mixer.Play(EMiningHapticCue::Explosion);
    Mixer.Play(EMiningHapticCue::DoorBreak);
    TestEqual(TEXT("Door breach and explosion merge"), Mixer.NumActive(), 1);
    TestFalse(TEXT("Rapid repeat events are suppressed"), Mixer.Play(EMiningHapticCue::DoorBreak));
    Mixer.Reset();
    Mixer.Play(EMiningHapticCue::Landing);
    Mixer.Play(EMiningHapticCue::EchoLanding);
    TestEqual(TEXT("Echo landing replaces regular landing"), Mixer.NumActive(), 1);
    TestTrue(TEXT("Echo landing owns the merged impact"), Mixer.IsActive(EMiningHapticCue::EchoLanding));
    Mixer.Reset();
    Mixer.Play(EMiningHapticCue::JetOverheat);
    const auto Ducked = Mixer.Advance(.01f, FVector2D(.3, .3));
    TestTrue(TEXT("Warnings duck the continuous background"), Ducked.X < .3 && Ducked.Y < .3);
    Mixer.Reset();
    TestTrue(TEXT("Reset clears every effect"), Mixer.Advance(.01f).IsNearlyZero());
    TestFalse(TEXT("Invalid gain rejected"), Mixer.Play(EMiningHapticCue::Explosion, std::numeric_limits<float>::infinity()));
    TestEqual(TEXT("Explosion peak at origin"), FMiningHapticMixer::DistanceGain(0, 1000), 1.f);
    TestEqual(TEXT("Explosion falls quadratically"), FMiningHapticMixer::DistanceGain(500, 1000), .25f);
    TestEqual(TEXT("Outside explosion range is silent"), FMiningHapticMixer::DistanceGain(1001, 1000), 0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningHapticIntegrationTest, "BrainMining.Gamepad.Haptics.LifecycleAndEvents", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningHapticIntegrationTest::RunTest(const FString&)
{
    auto* Instance = NewObject<UGameInstance>(GEngine);
    Instance->InitializeStandalone();
    auto* World = Instance->GetWorld();
    auto& Context = *Instance->GetWorldContext();
    auto* Local = NewObject<ULocalPlayer>(GEngine);
    auto* Viewport = NewObject<UGameViewportClient>(GEngine);
    Context.GameViewport = Viewport;
    Viewport->Init(Context, Instance, false);
    Viewport->SetViewportOverlayWidget(nullptr, SNew(SOverlay));
    Local->PlayerAdded(Viewport, 0);
    auto* Player = World->SpawnActor<AMiningEncounterPlayerController>();
    World->AddController(Player);
    Player->SetPlayer(Local);
    Player->bForceFeedbackEnabled = false;
    UClass* PawnClass = LoadClass<ABrainMiningCharacter>(nullptr, TEXT("/Game/MainGame/Blueprints/BP_MainGameCharacter.BP_MainGameCharacter_C"));
    auto* Pawn = PawnClass ? World->SpawnActor<ABrainMiningCharacter>(PawnClass, FVector(0, 0, 100), FRotator::ZeroRotator) : nullptr;
    Player->Possess(Pawn);
    Player->EncounterPolicy->SetMiningAllowed(true);
    auto* Input = Local->GetSubsystem<UMiningGamepadSubsystem>();
    auto* Haptics = Local->GetSubsystem<UMiningHapticsSubsystem>();
    auto& Slate = FSlateApplication::Get();
    const bool WasActive = Slate.IsActive();
    Slate.ProcessApplicationActivationEvent(true);
    ON_SCOPE_EXIT
    {
        if (Haptics) Haptics->Stop();
        Local->PlayerRemoved();
        Player->UnPossess();
        Local->PlayerController = nullptr;
        Instance->Shutdown();
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
        Slate.ProcessApplicationActivationEvent(WasActive);
    };
    if (!TestNotNull(TEXT("Real game character exists"), Pawn) || !TestNotNull(TEXT("Haptic subsystem exists"), Haptics)
        || !TestNotNull(TEXT("Input subsystem exists"), Input)) return false;
    Haptics->bEnabled = Haptics->bContinuous = true;
    Haptics->bOptional = false;
    Haptics->Strength = 1;
    TestFalse(TEXT("Keyboard mode is silent"), Haptics->Play(EMiningHapticCue::Collectible));
    Input->HandleAxis(EKeys::Gamepad_LeftX, .5f);
    Haptics->Tick(.01f);
    TestTrue(TEXT("Gamepad mode accepts semantic events"), Haptics->Play(EMiningHapticCue::Collectible));
    Haptics->Tick(.02f);
    TestTrue(TEXT("Semantic feedback produces motor output"), Haptics->GetOutput().GetMax() > 0);
    Input->MarkKeyboardMouse();
    TestTrue(TEXT("Switching to keyboard immediately stops motors"), Haptics->GetOutput().IsNearlyZero());
    Input->HandleAxis(EKeys::Gamepad_LeftX, .5f);
    Haptics->Play(EMiningHapticCue::Explosion);
    Haptics->Tick(.02f);
    Slate.ProcessApplicationActivationEvent(false);
    TestTrue(TEXT("Losing application focus immediately stops motors"), Haptics->GetOutput().IsNearlyZero());
    TestFalse(TEXT("Unfocused app rejects new feedback"), Haptics->Play(EMiningHapticCue::Explosion));
    Slate.ProcessApplicationActivationEvent(true);
    Input->HandleAxis(EKeys::Gamepad_LeftX, .5f);
    TestFalse(TEXT("Optional cues default off"), Haptics->Play(EMiningHapticCue::TitleLanded));
    Haptics->Play(EMiningHapticCue::Explosion);
    Player->bShowMouseCursor = true;
    Haptics->Tick(.01f);
    TestTrue(TEXT("Opening a menu clears queued impacts"), Haptics->GetOutput().IsNearlyZero());
    Player->bShowMouseCursor = false;
    UMiningHapticsSubsystem::Emit(Player, EMiningHapticCue::PuzzleComplete);
    Input->HandleButton(EKeys::Gamepad_Special_Left, true);
    TestTrue(TEXT("A result created after leaving a UI survives input context synchronization"), Haptics->IsCueActive(EMiningHapticCue::PuzzleComplete));
    Haptics->Stop();
    auto* Mining = Pawn->FindComponentByClass<UMiningComponent>();
    Mining->bDrillMode = true;
    Mining->BeginMiningInput();
    Haptics->Tick(.2f);
    Haptics->Stop();
    UMiningHapticsSubsystem::MiningContact(Mining);
    Haptics->Tick(.02f);
    TestTrue(TEXT("Drill contact creates continuous output"), Haptics->GetOutput().GetMax() > 0);
    Mining->CancelMining();
    for (int32 Frame = 0; Frame < 30; ++Frame) Haptics->Tick(.02f);
    TestTrue(TEXT("Releasing the drill fades to zero"), Haptics->GetOutput().IsNearlyZero(.002));
    Mining->DrillEnergy = Mining->MaxDrillEnergy * .1f;
    Haptics->Tick(.01f);
    TestTrue(TEXT("Crossing the low drill threshold alerts once"), Haptics->IsCueActive(EMiningHapticCue::DrillLow));
    Haptics->Stop();
    Haptics->Tick(.01f);
    TestFalse(TEXT("Remaining below the threshold does not repeat"), Haptics->IsCueActive(EMiningHapticCue::DrillLow));
    Mining->DrillEnergy = 0;
    Mining->bDrillMode = false;
    Haptics->Tick(.01f);
    TestTrue(TEXT("Drill exhaustion has a shutdown cue"), Haptics->IsCueActive(EMiningHapticCue::DrillEmpty));
    Haptics->Stop();
    auto* Flight = CastChecked<UMiningCharacterMovementComponent>(Pawn->GetCharacterMovement());
    Flight->SetMovementMode(MOVE_Falling);
    Flight->SetFlightInputHeld(true);
    Flight->UpdateFlightState(1.f);
    Haptics->Tick(.02f);
    TestTrue(TEXT("Real jet ignition emits its cue"), Haptics->IsCueActive(EMiningHapticCue::JetStart));
    Haptics->Stop();
    Flight->UpdateJetpackHeat(Flight->FlightSettings.HeatUpSeconds + .01f, true);
    Haptics->Tick(.02f);
    TestTrue(TEXT("Actual overheating emits a warning"), Haptics->IsCueActive(EMiningHapticCue::JetOverheat));
    Haptics->Stop();
    Haptics->Tick(.02f);
    TestFalse(TEXT("Overheat warning is not repeated during cooling"), Haptics->IsCueActive(EMiningHapticCue::JetOverheat));
    Flight->ResetFlightInput();
    Flight->JetpackOverheatRemaining = 0;
    Flight->Velocity = FVector(0, 0, -800);
    Haptics->Tick(.02f);
    Flight->SetMovementMode(MOVE_Walking);
    Haptics->Tick(.02f);
    TestTrue(TEXT("A substantial physical landing emits an impact"), Haptics->IsCueActive(EMiningHapticCue::Landing));
    Haptics->Stop();
    UMiningHapticsSubsystem::FinaleThrust(Pawn, true, false);
    Haptics->Tick(.02f);
    TestTrue(TEXT("Finale thrust has its own continuous channel"), Haptics->GetOutput().GetMax() > 0);
    Haptics->bContinuous = false;
    for (int32 Frame = 0; Frame < 30; ++Frame) Haptics->Tick(.02f);
    TestTrue(TEXT("Continuous option silences ongoing loops"), Haptics->GetOutput().IsNearlyZero(.002));
    TestTrue(TEXT("Discrete rewards remain enabled"), Haptics->Play(EMiningHapticCue::Equipment));
    Haptics->Stop();
    UMiningHapticsSubsystem::EmitAt(Pawn, EMiningHapticCue::Explosion, Pawn->GetActorLocation() + FVector(5000, 0, 0), 1000);
    TestFalse(TEXT("Distant explosions never queue feedback"), Haptics->IsCueActive(EMiningHapticCue::Explosion));
    auto* Echo = World->SpawnActor<AEchoPassageStage>();
    Echo->EmitEcho(Pawn->GetActorLocation(), true);
    TestTrue(TEXT("Real echo landing reaches the local feedback subsystem"), Haptics->IsCueActive(EMiningHapticCue::EchoLanding));
    Echo->LastEcho = -100;
    Echo->EmitEcho(Pawn->GetActorLocation());
    const uint64 Accepted = Haptics->GetAcceptedCueCount();
    Echo->EmitEcho(Pawn->GetActorLocation());
    TestEqual(TEXT("Echo cooldown rejects duplicate knocks before feedback"), Haptics->GetAcceptedCueCount(), Accepted);
    Echo->Collapse();
    TestTrue(TEXT("Real floor collapse emits feedback"), Haptics->IsCueActive(EMiningHapticCue::EchoCollapse));
    Echo->Complete();
    TestTrue(TEXT("Real echo completion emits a reward"), Haptics->IsCueActive(EMiningHapticCue::PuzzleComplete));
    Haptics->Tick(.02f);
    Input->ResetDevice();
    TestTrue(TEXT("Device reset clears output and pending cues"), Haptics->GetOutput().IsNearlyZero() && !Haptics->IsCueActive(EMiningHapticCue::PuzzleComplete));
    Input->HandleAxis(EKeys::Gamepad_LeftX, .5f);
    Haptics->Play(EMiningHapticCue::Explosion);
    auto* OtherPawn = World->SpawnActor<ACharacter>();
    Player->Possess(OtherPawn);
    Haptics->Tick(.01f);
    TestTrue(TEXT("Changing pawns clears old effects"), Haptics->GetOutput().IsNearlyZero());
    Haptics->bEnabled = false;
    TestFalse(TEXT("Global off rejects all feedback"), Haptics->Play(EMiningHapticCue::Explosion));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningHapticSettingsTest, "BrainMining.Gamepad.Haptics.PreferencePersistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningHapticSettingsTest::RunTest(const FString&)
{
    auto* Settings = NewObject<UMiningHapticsSubsystem>(NewObject<ULocalPlayer>(GEngine));
    const FString Path = FPaths::ProjectSavedDir() / TEXT("Validation/HapticPreferences.ini");
    ON_SCOPE_EXIT { IFileManager::Get().Delete(*Path); };
    Settings->bEnabled = false; Settings->bContinuous = false; Settings->bOptional = true; Settings->Strength = .35f;
    Settings->SavePreferences(Path);
    Settings->bEnabled = Settings->bContinuous = true; Settings->bOptional = false; Settings->Strength = 1;
    Settings->LoadPreferences(Path);
    TestTrue(TEXT("All vibration preferences round trip"), !Settings->bEnabled && !Settings->bContinuous && Settings->bOptional && FMath::IsNearlyEqual(Settings->Strength, .35f));
    FFileHelper::SaveStringToFile(TEXT("[BrainMining.Haptics]\nStrength=7\n"), *Path);
    Settings->LoadPreferences(Path);
    TestEqual(TEXT("Invalid saved strength is clamped"), Settings->Strength, 1.f);
    return true;
}
#endif
