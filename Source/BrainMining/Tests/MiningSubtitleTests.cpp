#if WITH_DEV_AUTOMATION_TESTS
#include "Subtitles/SubtitleSubsystem.h"
#include "Subtitles/SubtitleSegment.h"
#include "Subtitles/SubtitleSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningSubtitlePausedPlaybackTest,"BrainMining.Subtitles.PausedPlayback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningSubtitlePausedPlaybackTest::RunTest(const FString&)
{
    auto* Instance=NewObject<UGameInstance>(GEngine);
    Instance->InitializeStandalone();
    UWorld* World=Instance->GetWorld();
    auto* Subtitles=Instance->GetSubsystem<USubtitleSubsystem>();
    ON_SCOPE_EXIT
    {
        Instance->Shutdown();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);
    };
    // Advance the same real-time callback deterministically; world time stays paused.
    World->GetWorldSettings()->SetPauserPlayerState(World->SpawnActor<APlayerState>());
    TestTrue(TEXT("World is paused as it is in the collection menu"),World->IsPaused());
    Subtitles->PlayStatusLine(FText::FromString(TEXT("Temporary status")),1.f);
    Subtitles->TickPlayback(.4f);
    TestTrue(TEXT("Line remains for its reading duration"),Subtitles->IsPlaying());
    Subtitles->TickPlayback(.7f);
    TestFalse(TEXT("Status expires without resuming the game"),Subtitles->IsPlaying());
    TestTrue(TEXT("Expired text is empty"),Subtitles->GetActiveText().IsEmpty());
    TestTrue(TEXT("Subtitle expiry does not resume the world"),World->IsPaused());

    auto* Settings=GetMutableDefault<USubtitleSettings>();
    TGuardValue<float> Gap(Settings->LineGap,.25f);
    auto* Segment=NewObject<USubtitleSegment>(Subtitles);
    Segment->Lines.AddDefaulted_GetRef().Text=FText::FromString(TEXT("First line"));
    Segment->Lines.AddDefaulted_GetRef().Text=FText::FromString(TEXT("Second line"));
    Subtitles->StartSegment(Segment);
    Subtitles->TickPlayback(Subtitles->LineSecondsRemaining+.01f);
    TestTrue(TEXT("Multi-line dialogue enters its gap while paused"),Subtitles->bInLineGap);
    Subtitles->TickPlayback(.3f);
    TestFalse(TEXT("Gap also expires while paused"),Subtitles->bInLineGap);
    TestEqual(TEXT("Next line plays normally"),Subtitles->GetActiveText().ToString(),FString(TEXT("Second line")));
    Subtitles->TickPlayback(Subtitles->LineSecondsRemaining+.01f);
    TestFalse(TEXT("Entire dialogue finishes while paused"),Subtitles->IsPlaying());

    Subtitles->PlayStatusLine(FText::FromString(TEXT("Old status")),1.f);
    Subtitles->TickPlayback(.8f);
    Subtitles->PlayStatusLine(FText::FromString(TEXT("Replacement")),2.f);
    Subtitles->TickPlayback(.3f);
    TestEqual(TEXT("Previous deadline cannot retire a replacement"),Subtitles->GetActiveText().ToString(),FString(TEXT("Replacement")));
    Subtitles->StopAll();
    Subtitles->TickPlayback(10.f);
    TestFalse(TEXT("Stopped playback cannot reappear"),Subtitles->IsPlaying());
    Subtitles->PlayStatusLine(FText::FromString(TEXT("Travel")),1.f);
    Subtitles->PlaybackWorld.Reset();
    Subtitles->TickPlayback(.1f);
    TestFalse(TEXT("Expired world cannot leave a stale subtitle"),Subtitles->IsPlaying());
    Subtitles->PlayInteractionLines(TEXT("OldWorld"),{FText::FromString(TEXT("Old dialogue"))});
    Subtitles->PlaybackWorld.Reset();
    Subtitles->PlayStatusLine(FText::FromString(TEXT("New world status")),1.f);
    TestEqual(TEXT("New world dialogue is not swallowed by a previous world's queue"),Subtitles->GetActiveText().ToString(),FString(TEXT("New world status")));
    return true;
}
#endif
