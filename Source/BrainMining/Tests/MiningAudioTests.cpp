#include "MiningAudioComponent.h"
#include "MiningComponent.h"
#include "Showcase/MiningExhibitHall.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
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
    TestEqual(TEXT("All selected sounds have runtime entries"), Names.Num(), 33);
    for (FName Name : Names)
    {
        const FString Path = FString::Printf(TEXT("/Game/Audio/Pilot/%s.%s"), *Name.ToString(), *Name.ToString());
        auto* Sound = LoadObject<USoundWave>(nullptr, *Path);
        if (!TestNotNull(*Path, Sound)) continue;
        const bool bMusic = Name.ToString().StartsWith(TEXT("M_"));
        TestEqual(*FString::Printf(TEXT("%s loop setting"), *Name.ToString()), bool(Sound->bLooping), bMusic);
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
    auto* Pawn = World->SpawnActor<ACharacter>();
    auto* Controller = World->SpawnActor<APlayerController>();
    Controller->SetAsLocalPlayerController();
    Controller->Possess(Pawn);
    auto* Mining = NewObject<UMiningComponent>(Pawn);
    Pawn->AddInstanceComponent(Mining);
    Mining->RegisterComponent();
    Audio = NewObject<UMiningAudioComponent>(Pawn);
    Pawn->AddInstanceComponent(Audio);
    Audio->RegisterComponent();
    auto* Hall = World->SpawnActor<AMiningExhibitHall>();
    Hall->Areas.SetNum(3);
    Hall->Areas[0].Kind = EMiningExperienceKind::Mining;
    Hall->Areas[1].Kind = EMiningExperienceKind::Office;
    Hall->Areas[2].Kind = EMiningExperienceKind::Ending;
    Hall->ActiveArea = 1;
    Hall->bReady = true;
    Audio->BeginPlay();
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
    Enabled->SetWithCurrentPriority(0);
    Audio->TickComponent(1, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Audio switch fades music out"), Music->VolumeMultiplier, 0.f);
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
#endif
