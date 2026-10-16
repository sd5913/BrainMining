#include "MiningRoomAudioSubsystem.h"
#include "MiningAudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoomAudioAssetsTest, "BrainMining.RoomAudio.ImportedAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRoomAudioAssetsTest::RunTest(const FString&)
{
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *(FPaths::ProjectContentDir() / TEXT("Audio/Rooms/*.uasset")), true, false);
    TestEqual(TEXT("All prepared room cues imported"), Files.Num(), 71);
    for (const FString& File : Files)
    {
        const FString Name = FPaths::GetBaseFilename(File);
        const FString Path = FString::Printf(TEXT("/Game/Audio/Rooms/%s.%s"), *Name, *Name);
        auto* Sound = LoadObject<USoundWave>(nullptr, *Path);
        if (!TestNotNull(*Path, Sound)) continue;
        TestEqual(TEXT("Room cues are mono"), Sound->NumChannels, 1);
        TestTrue(TEXT("Audio has a finite positive duration"), FMath::IsFinite(Sound->Duration) && Sound->Duration > .05f && Sound->Duration < 120.f);
        TestEqual(TEXT("Loop flags agree with names"), bool(Sound->bLooping), Name.EndsWith(TEXT("_Loop")));
        if (TestNotNull(TEXT("Mix routing exists"), Sound->SoundClassObject.Get()))
            TestEqual(TEXT("Room routing preserves effects and ambience controls"), Sound->SoundClassObject->GetName(), Name.StartsWith(TEXT("A_")) ? FString(TEXT("SC_Ambience")) : FString(TEXT("SC_Effects")));
        TestTrue(TEXT("Voices have a concurrency limit"), bool(Sound->bOverrideConcurrency));
        if (Sound->bLooping) TestEqual(TEXT("Silent loops retain playback"), Sound->VirtualizationMode, EVirtualizationMode::PlayWhenSilent);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoomAudioLifecycleTest, "BrainMining.RoomAudio.Lifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRoomAudioLifecycleTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("Lvl_MainGame"), CreatePackage(TEXT("/Temp/RoomAudioTests/Lvl_MainGame")));
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Enabled = IConsoleManager::Get().FindConsoleVariable(TEXT("bm.Audio.Enabled"));
    if (!TestNotNull(TEXT("Audio setting exists"), Enabled))
    {
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
        return false;
    }
    const int32 PreviousEnabled = Enabled->GetInt();
    ON_SCOPE_EXIT
    {
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
        Enabled->SetWithCurrentPriority(PreviousEnabled);
    };
    Enabled->SetWithCurrentPriority(1);
    auto* Audio = World->GetSubsystem<UMiningRoomAudioSubsystem>();
    if (!TestNotNull(TEXT("Game world creates room audio service"), Audio)) return false;
    auto* Pawn = World->SpawnActor<APawn>();
    auto* Controller = World->SpawnActor<APlayerController>();
    World->AddController(Controller);
    Controller->SetAsLocalPlayerController();
    Controller->Possess(Pawn);
    auto* Master = NewObject<UMiningAudioComponent>(Pawn);
    Master->RegisterComponent();
    Master->Activate(true);
    TestTrue(TEXT("Fixture is the MainGame profile"), UMiningAudioComponent::IsMainGameWorld(World));
    UMiningRoomAudioSubsystem::Emit(Pawn, TEXT("S_Confirm"));
    UMiningRoomAudioSubsystem::Emit(Pawn, TEXT("S_Confirm"));
    TestEqual(TEXT("Repeated feedback is throttled per source"), Audio->EventCount(TEXT("S_Confirm")), 1);
    UMiningRoomAudioSubsystem::Emit(Controller, TEXT("S_Confirm"));
    TestEqual(TEXT("Independent sources retain feedback"), Audio->EventCount(TEXT("S_Confirm")), 2);
    for (int32 Index = 0; Index < 50; ++Index)
        UMiningRoomAudioSubsystem::Loop(Pawn, TEXT("S_Water_Loop"), .3f);
    TestEqual(TEXT("Frame updates reuse one loop"), Audio->LoopCount(), 1);
    UMiningRoomAudioSubsystem::StopSource(Pawn);
    TestEqual(TEXT("Retry or exit removes its loop"), Audio->LoopCount(), 0);
    auto* Temporary = World->SpawnActor<AActor>();
    UMiningRoomAudioSubsystem::Loop(Temporary, TEXT("S_Mechanism_Loop"), .2f, true);
    Temporary->Destroy();
    Audio->Tick(1.f);
    TestEqual(TEXT("Destroyed sources cannot leak loops"), Audio->LoopCount(), 0);
    UMiningRoomAudioSubsystem::Loop(Pawn, TEXT("S_Water_Loop"), .3f);
    Enabled->SetWithCurrentPriority(0);
    Audio->Tick(.1f);
    UMiningRoomAudioSubsystem::Emit(Pawn, TEXT("S_Reject"));
    TestEqual(TEXT("Global mute stops room loops"), Audio->LoopCount(), 0);
    TestEqual(TEXT("Global mute blocks new feedback"), Audio->EventCount(TEXT("S_Reject")), 0);
    Enabled->SetWithCurrentPriority(1);
    UMiningRoomAudioSubsystem::Loop(Pawn, TEXT("S_Water_Loop"), .3f);
    Master->Deactivate();
    Audio->Tick(.1f);
    TestEqual(TEXT("Audio component deactivation also stops room loops"), Audio->LoopCount(), 0);
    return true;
}
#endif
