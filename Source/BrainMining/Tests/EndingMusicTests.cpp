#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "EndingEmergence.h"
#include "MiningAudioSettings.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Sound/SoundWave.h"
#include "Tests/AutomationCommon.h"

class FEndingMusicContinuityCheck : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<AEndingEmergence> Director;
    TWeakObjectPtr<UAudioComponent> OriginalMusic;
    double Started = FPlatformTime::Seconds();
    double CheckAt = 0;
    int32 Step = 0;
public:
    explicit FEndingMusicContinuityCheck(FAutomationTestBase* InTest) : Test(InTest) {}
    bool Update() override
    {
        const double Now = FPlatformTime::Seconds();
        if(Now - Started > 90)
        {
            if(Director.IsValid()) Director->SetActorTickEnabled(true);
            Test->AddError(TEXT("Ending music check timed out"));
            return true;
        }
        if(!Director.IsValid())
        {
            auto* World = AutomationCommon::GetAnyGameWorld();
            if(!World) return false;
            for(TActorIterator<AEndingEmergence> Actor(World); Actor; ++Actor)
                if(Actor->bReady) { Director = *Actor; break; }
            if(!Director.IsValid()) return false;
            auto* Ending = Director.Get();
            if(!Test->TestNotNull(TEXT("Opening cell starts music"), Ending->Music.Get())) return true;
            auto* Wave = Cast<USoundWave>(Ending->Music->Sound);
            if(!Test->TestNotNull(TEXT("Ending music is a SoundWave"), Wave)) return true;
            Test->TestEqual(TEXT("Selected track plays"), Wave->GetFName(), FName(TEXT("M_EE_LightThought_Loop")));
            Test->TestTrue(TEXT("Music loops"), Wave->bLooping);
            Test->TestEqual(TEXT("Music stays stereo"), Wave->NumChannels, 2);
            Test->TestTrue(TEXT("Loop duration is 150.2 seconds"), FMath::IsNearlyEqual(Wave->Duration, 150.2f, .05f));
            Test->TestTrue(TEXT("Music is playing"), Ending->Music->IsPlaying());
            Test->TestTrue(TEXT("Entrance music fades towards its target gain"), Ending->Music->VolumeMultiplier>=0 && Ending->Music->VolumeMultiplier<=.45f);
            Test->TestEqual(TEXT("Music retains the intended final gain"), Ending->MusicTargetVolume,.45f);
            Test->TestTrue(TEXT("Collection pause preserves music"), Ending->Music->bIsUISound);
            auto* Settings = World->GetGameInstance()->GetSubsystem<UMiningAudioSettings>();
            Test->TestTrue(TEXT("Music follows the music slider"), Settings && Ending->Music->SoundClassOverride == Settings->GetClass(1));
            OriginalMusic = Ending->Music;
            Ending->SetActorTickEnabled(false);
            Ending->Music->Play(Wave->Duration - .2f);
            CheckAt = Now + 2;
            return false;
        }
        if(Now < CheckAt) return false;
        auto* Ending = Director.Get();
        if(Step==100)
        {
            if(Ending->MusicFadeAge<4.5f)return false;
            Test->TestTrue(TEXT("Entrance fade reaches the intended music gain"),FMath::IsNearlyEqual(Ending->Music->VolumeMultiplier,.45f,.001f));
            return true;
        }
        if(Step == 0)
        {
            Test->TestTrue(TEXT("Playback survives the loop boundary"), Ending->Music->IsPlaying());
            Ending->SetMusic(TEXT("LightThought_Loop"), .45f);
            Test->TestTrue(TEXT("Same-track request preserves the component"), Ending->Music.Get() == OriginalMusic.Get());
            Ending->EnterBlood();
            Test->TestTrue(TEXT("Blood entry preserves music"), Ending->Music.Get() == OriginalMusic.Get());
            Ending->PreviewFilm();
        }
        const int32 Shots[] = {0, 3, 7, 8, 9, 10, 11, 12, 13};
        if(Step < UE_ARRAY_COUNT(Shots))
        {
            Ending->SetShot(Shots[Step]);
            Test->TestTrue(*FString::Printf(TEXT("Shot %d keeps one playing music component"), Shots[Step]),
                Ending->Music.Get() == OriginalMusic.Get() && Ending->Music->IsPlaying());
            ++Step;
            CheckAt = Now + .15;
            return false;
        }
        Ending->UpdateCredits();
        Test->TestTrue(TEXT("Credits retain background music"), Ending->Music.Get() == OriginalMusic.Get() && Ending->Music->IsPlaying());
        Ending->RemoveCredits();
        Ending->RestartDemo();
        Test->TestTrue(TEXT("Replay replaces the previous music component"), Ending->Music && Ending->Music.Get() != OriginalMusic.Get());
        Test->TestTrue(TEXT("Replay starts the same soundtrack"), Ending->Music && Ending->Music->IsPlaying()
            && Ending->Music->Sound->GetFName() == FName(TEXT("M_EE_LightThought_Loop")));
        Test->TestEqual(TEXT("Replay music begins silently before fading in"),Ending->Music->VolumeMultiplier,0.f);
        Ending->SetActorTickEnabled(true);
        Step=100;
        return false;
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndingMusicContinuityTest, "BrainMining.EndingEmergence.MusicContinuity", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FEndingMusicContinuityTest::RunTest(const FString&)
{
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEndingMusicContinuityCheck>(this));
    return true;
}
#endif
