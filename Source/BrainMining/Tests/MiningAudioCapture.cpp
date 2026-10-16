#include "MiningAudioComponent.h"
#include "MiningAudioSettings.h"
#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningBombToolComponent.h"
#include "BrainMiningPlayerController.h"
#include "Containers/Ticker.h"
#include "UnrealClient.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    void CaptureMiningAudio(UWorld* World)
    {
        if (!World || !World->IsGameWorld() || UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Lvl_ExternalMining_Demo")) return;
        auto* Controller = World->GetFirstPlayerController();
        APawn* Pawn = Controller ? Controller->GetPawn().Get() : nullptr;
        auto* Audio = Pawn ? Pawn->FindComponentByClass<UMiningAudioComponent>() : nullptr;
        auto* Mining = Pawn ? Pawn->FindComponentByClass<UMiningComponent>() : nullptr;
        auto* Flight = Pawn ? Pawn->FindComponentByClass<UMiningCharacterMovementComponent>() : nullptr;
        auto* Bomb = Pawn ? Pawn->FindComponentByClass<UMiningBombToolComponent>() : nullptr;
        auto* Settings = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UMiningAudioSettings>() : nullptr;
        if (!Audio || !Mining || !Flight || !Bomb || !Settings) { UE_LOG(LogTemp, Error, TEXT("AUDIO_CAPTURE_MISSING_PLAYER")); return; }
        TArray<float> Original;
        for (int32 Channel = 0; Channel < 5; ++Channel) Original.Add(Settings->GetVolume(Channel));
        TWeakObjectPtr<APawn> WeakPawn(Pawn);
        TWeakObjectPtr<UWorld> WeakWorld(World);
        const bool bOriginalInfinite = Mining->bInfiniteDrillEnergy;
        auto Step = MakeShared<int32>(0);
        auto Timer = MakeShared<FTimerHandle>();
        World->GetTimerManager().SetTimer(*Timer, [WeakWorld, WeakPawn, Step, Timer, Original, bOriginalInfinite]()
        {
            if (!WeakWorld.IsValid() || !WeakPawn.IsValid()) return;
            auto* ActiveWorld = WeakWorld.Get();
            auto* ActivePawn = WeakPawn.Get();
            auto* Audio = ActivePawn->FindComponentByClass<UMiningAudioComponent>();
            auto* Mining = ActivePawn->FindComponentByClass<UMiningComponent>();
            auto* Flight = ActivePawn->FindComponentByClass<UMiningCharacterMovementComponent>();
            auto* Bomb = ActivePawn->FindComponentByClass<UMiningBombToolComponent>();
            auto* Settings = ActiveWorld->GetGameInstance()->GetSubsystem<UMiningAudioSettings>();
            UE_LOG(LogTemp, Display, TEXT("AUDIO_CAPTURE_STEP %d loops=%d time=%.3f"), *Step, Audio->GetToolLoopCount(), ActiveWorld->GetTimeSeconds());
            switch (*Step)
            {
            case 0:
                Settings->RestoreDefaults();
                UAudioMixerBlueprintLibrary::StartRecordingOutput(ActiveWorld, 30.f);
                break;
            case 1: Mining->bInfiniteDrillEnergy = true; Mining->ActivateDrill(); break;
            case 2: Mining->BeginMiningInput(); Mining->OnMiningHit.Broadcast(FHitResult()); break;
            case 3: Mining->EndMiningInput(); Mining->SelectPickaxe(); break;
            case 4: Flight->SetFlightInputHeld(true); break;
            case 5: Flight->SetFlightInputHeld(false); break;
            case 6:
                Bomb->ToggleEquipped();
                UE_LOG(LogTemp, Display, TEXT("AUDIO_CAPTURE_BOMB_THROW %d"), Bomb->ThrowBomb());
                break;
            case 7: Audio->PlayCollect(); break;
            case 8: Settings->SetVolume(0, 0.f); break;
            case 9:
                Settings->SetVolume(0, 1.f);
                {
                    TArray<UAudioComponent*> Components;
                    ActivePawn->GetComponents(Components);
                    for (auto* Component : Components)
                        if (Component->Sound && Component->Sound->GetFName() == FName(TEXT("M_DreamyFlashback_Loop")))
                            Component->Play(FMath::Max(0.f, Component->Sound->GetDuration() - 2.f));
                }
                break;
            case 12:
                UAudioMixerBlueprintLibrary::StopRecordingOutput(ActiveWorld, EAudioRecordingExportType::WavFile, TEXT("AudioP1Runtime"), FPaths::ProjectSavedDir());
                Mining->bInfiniteDrillEnergy = bOriginalInfinite;
                for (int32 Channel = 0; Channel < 5; ++Channel) Settings->SetVolume(Channel, Original[Channel]);
                UE_LOG(LogTemp, Display, TEXT("AUDIO_CAPTURE_COMPLETE"));
                ActiveWorld->GetTimerManager().ClearTimer(*Timer);
                break;
            }
            ++*Step;
        }, 2.f, true, 2.f);
    }
    FAutoConsoleCommandWithWorld CaptureAudioCommand(TEXT("bm.Audio.Capture"), TEXT("Record a 24s scripted mining-map audio smoke test. Development builds only."), FConsoleCommandWithWorldDelegate::CreateStatic(&CaptureMiningAudio));
    FAutoConsoleCommandWithWorld PreviewSettingsCommand(TEXT("bm.Audio.SettingsPreview"), TEXT("Capture the audio settings panel and verify its F8 lifecycle."), FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        auto* Controller = World ? Cast<ABrainMiningPlayerController>(World->GetFirstPlayerController()) : nullptr;
        if (!Controller || !World->IsGameWorld()) return;
        Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F8, IE_Pressed, 1.f));
        if (!Controller->IsAudioSettingsOpen()) { UE_LOG(LogTemp, Error, TEXT("AUDIO_SETTINGS_PREVIEW_FAILED")); return; }
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("AudioSettings.png"), true, false);
        TWeakObjectPtr<ABrainMiningPlayerController> WeakController(Controller);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakController](float)
        {
            if (auto* Player = WeakController.Get())
            {
                Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F8, IE_Pressed, 1.f));
                if (!Player->IsAudioSettingsOpen() && !Player->IsPaused() && !Player->IsMoveInputIgnored() && !Player->IsLookInputIgnored())
                {
                    UE_LOG(LogTemp, Display, TEXT("AUDIO_SETTINGS_PREVIEW_OK"));
                }
                else { UE_LOG(LogTemp, Error, TEXT("AUDIO_SETTINGS_CLOSE_FAILED")); }
            }
            return false;
        }), 2.f);
    }));
}
#endif
