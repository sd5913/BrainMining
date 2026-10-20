#include "MiningAudioComponent.h"
#include "MiningComponent.h"
#include "MiningAudioSettings.h"
#include "MiningCharacterMovementComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Sound/SoundClass.h"
#include "Showcase/MiningExhibitHall.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Sound/SoundWave.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningAudioAssetsTest, "BrainMining.AudioPilot.ImportedAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningAudioAssetsTest::RunTest(const FString&)
{
    const auto Names = UMiningAudioComponent::AssetNames();
    TestEqual(TEXT("All selected sounds have runtime entries"), Names.Num(), 47);
    for (FName Name : Names)
    {
        const FString Path = FString::Printf(TEXT("/Game/Audio/Pilot/%s.%s"), *Name.ToString(), *Name.ToString());
        auto* Sound = LoadObject<USoundWave>(nullptr, *Path);
        if (!TestNotNull(*Path, Sound)) continue;
        const bool bMusic = Name.ToString().StartsWith(TEXT("M_"));
        TestEqual(*FString::Printf(TEXT("%s loop setting"), *Name.ToString()), bool(Sound->bLooping), Name.ToString().EndsWith(TEXT("_Loop")));
        TestNotNull(TEXT("Every sound has a mix category"), Sound->SoundClassObject.Get());
        TestEqual(*FString::Printf(TEXT("%s channel count"), *Name.ToString()), Sound->NumChannels, bMusic ? 2 : 1);
        TestTrue(*FString::Printf(TEXT("%s duration"), *Name.ToString()), Sound->Duration > 0 && Sound->Duration < (bMusic ? 130.f : 10.f));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningAudioRoutingTest, "BrainMining.AudioPilot.RoutingAndLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningAudioRoutingTest::RunTest(const FString&)
{
    auto* PawnClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
    if (!TestNotNull(TEXT("Actual game Blueprint character loads"), PawnClass)) return false;
    TestNotNull(TEXT("Existing Blueprint inherits the audio component"), PawnClass->GetDefaultObject<ACharacter>()->FindComponentByClass<UMiningAudioComponent>());
    auto* Enabled = IConsoleManager::Get().FindConsoleVariable(TEXT("bm.Audio.Enabled"));
    auto* Volume = IConsoleManager::Get().FindConsoleVariable(TEXT("bm.Audio.MusicVolume"));
    if (!TestNotNull(TEXT("Audio switch registered"), Enabled) || !TestNotNull(TEXT("Music volume registered"), Volume)) return false;
    const int32 PreviousEnabled = Enabled->GetInt();
    const float PreviousVolume = Volume->GetFloat();
    Enabled->SetWithCurrentPriority(1);
    Volume->SetWithCurrentPriority(.16f);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    UMiningAudioComponent* Audio = nullptr;
    ON_SCOPE_EXIT
    {
        if (Audio && Audio->HasBegunPlay()) Audio->EndPlay(EEndPlayReason::Destroyed);
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
        Enabled->SetWithCurrentPriority(PreviousEnabled);
        Volume->SetWithCurrentPriority(PreviousVolume);
    };
    auto* Pawn = World->SpawnActor<ACharacter>(PawnClass);
    auto* Controller = World->SpawnActor<AMiningExhibitController>();
    World->AddController(Controller);
    Controller->SetAsLocalPlayerController();
    Controller->Possess(Pawn);
    Controller->EncounterPolicy->Release();
    auto* Mining = Pawn->FindComponentByClass<UMiningComponent>();
    Audio = Pawn->FindComponentByClass<UMiningAudioComponent>();
    auto* Hall = World->SpawnActor<AMiningExhibitHall>();
    Hall->Tick(.01f);
    Hall->Areas.SetNum(3);
    Hall->Areas[0].Kind = EMiningExperienceKind::Mining;
    Hall->Areas[1].Kind = EMiningExperienceKind::Office;
    Hall->Areas[2].Kind = EMiningExperienceKind::Ending;
    Hall->ActiveArea = 1;
    Hall->bReady = true;
    TestFalse(TEXT("Fixture controller is available to hall interactions"), Hall->IsInteractionOpen());
    Audio->BeginPlay();
    Audio->Activate(true);
    TestTrue(TEXT("Contact listener attached"), Mining->OnPickaxeContact.IsBound());
    TestTrue(TEXT("Committed break listener attached"), Mining->OnPickaxeBreak.IsBound());
    Audio->TickComponent(1, LEVELTICK_All, nullptr);
    TestNull(TEXT("Office does not start exploration music"), Pawn->FindComponentByClass<UAudioComponent>());
    Hall->ActiveArea = 0;
    Audio->TickComponent(1, LEVELTICK_All, nullptr);
    auto* Music = Pawn->FindComponentByClass<UAudioComponent>();
    if (!TestNotNull(TEXT("Mining area creates a music component"), Music)) return false;
    TestEqual(TEXT("Music uses the imported loop"), Music->Sound->GetFName(), FName(TEXT("M_DreamyFlashback_Loop")));
    TestEqual(TEXT("Mining music reaches configured volume"), Music->VolumeMultiplier, .16f);
    Hall->ActiveArea = 1;
    Audio->TickComponent(.2f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Leaving mine fades progressively"), Music->VolumeMultiplier > 0 && Music->VolumeMultiplier < .16f);
    Audio->TickComponent(1, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Office transition reaches silence"), Music->VolumeMultiplier, 0.f);
    Hall->ActiveArea = 2;
    Audio->TickComponent(1, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Ending stays silent"), Music->VolumeMultiplier, 0.f);
    Hall->ActiveArea = 0;
    Audio->TickComponent(1, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Reentering mine restores music"), Music->VolumeMultiplier, .16f);
    Mining->bDrillMode = true;
    Audio->TickComponent(.1f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Equipped drill owns one idle loop"), Audio->GetToolLoopCount(), 1);
    Audio->TickComponent(.1f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Repeated ticks do not duplicate motor"), Audio->GetToolLoopCount(), 1);
    Mining->BeginMiningInput();
    Mining->OnMiningHit.Broadcast(FHitResult());
    Audio->TickComponent(.01f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Confirmed drilling adds contact texture"), Audio->GetToolLoopCount(), 2);
    Mining->EndMiningInput();
    auto* Flight = CastChecked<UMiningCharacterMovementComponent>(Pawn->GetCharacterMovement());
    Flight->FlightSettings.bEnabled = true;
    Flight->SetFlightInputHeld(true);
    Flight->UpdateFlightState(Flight->FlightSettings.Sanitized().HoldToIgniteSeconds + .01f);
    Audio->TickComponent(.01f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Thrust and drill have independent loops"), Audio->GetToolLoopCount(), 2);
    Flight->JetpackOverheatRemaining = 1.f;
    Audio->TickComponent(.01f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Overheat stops thrust while retaining drill"), Audio->GetToolLoopCount(), 1);
    Flight->SetFlightInputHeld(false);
    Flight->JetpackOverheatRemaining = 0.f;
    Mining->SetInputSuspended(true);
    Audio->TickComponent(.1f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Menu suspension destroys tool loops"), Audio->GetToolLoopCount(), 0);
    Mining->SetInputSuspended(false);
    Audio->TickComponent(.1f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Closing menu restores equipped motor"), Audio->GetToolLoopCount(), 1);
    Audio->Deactivate();
    TestEqual(TEXT("Deactivation stops all tool loops"), Audio->GetToolLoopCount(), 0);
    Audio->Activate(true);
    Hall->ActiveArea = 2;
    Audio->TickComponent(.1f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Ending transition destroys motor"), Audio->GetToolLoopCount(), 0);
    Hall->ActiveArea = 0;
    Enabled->SetWithCurrentPriority(0);
    Audio->TickComponent(1, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Audio switch fades music out"), Music->VolumeMultiplier, 0.f);
    TestEqual(TEXT("Audio switch destroys motor"), Audio->GetToolLoopCount(), 0);
    Enabled->SetWithCurrentPriority(1);
    Controller->UnPossess();
    Audio->TickComponent(1, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Unpossessed player cannot start music"), Music->VolumeMultiplier, 0.f);
    Audio->EndPlay(EEndPlayReason::Destroyed);
    TestFalse(TEXT("Contact listener detached"), Mining->OnPickaxeContact.IsBound());
    TestFalse(TEXT("Break listener detached"), Mining->OnPickaxeBreak.IsBound());
    TestFalse(TEXT("Music component unregistered on teardown"), Music->IsRegistered());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningAudioSettingsTest, "BrainMining.AudioPilot.SettingsPersistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningAudioSettingsTest::RunTest(const FString&)
{
    auto* Instance = NewObject<UGameInstance>();
    auto* Settings = NewObject<UMiningAudioSettings>(Instance);
    Settings->SetVolume(0, .35f);
    Settings->SetVolume(1, -2.f);
    Settings->SetVolume(2, 2.f);
    Settings->SetVolume(3, .42f);
    Settings->SetVolume(4, .6f);
    TestEqual(TEXT("Negative gain clamps to silence"), Settings->GetVolume(1), 0.f);
    TestEqual(TEXT("Gain cannot exceed one"), Settings->GetVolume(2), 1.f);
    const FString Filename = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("AudioSettingsTest"), TEXT(".ini"));
    ON_SCOPE_EXIT { IFileManager::Get().Delete(*Filename); };
    Settings->SetLookSensitivity(1.75f);
    Settings->SaveVolumes(Filename);
    auto* Restored = NewObject<UMiningAudioSettings>(Instance);
    Restored->LoadVolumes(Filename);
    for (int32 Channel = 0; Channel < 5; ++Channel)
        TestEqual(TEXT("Each channel survives a disk round trip"), Restored->GetVolume(Channel), Settings->GetVolume(Channel));
    TestEqual(TEXT("Camera sensitivity survives disk round trip"), Restored->GetLookSensitivity(), 1.75f);
    Restored->SetLookSensitivity(-1.f);
    TestEqual(TEXT("Camera sensitivity stays positive"), Restored->GetLookSensitivity(), .2f);
    Restored->RestoreDefaults();
    TestEqual(TEXT("Default camera sensitivity restored"), Restored->GetLookSensitivity(), 1.f);
    TestEqual(TEXT("Default master restored"), Restored->GetVolume(0), 1.f);
    TestEqual(TEXT("Default effects restored"), Restored->GetVolume(2), .85f);
    return true;
}
#endif
