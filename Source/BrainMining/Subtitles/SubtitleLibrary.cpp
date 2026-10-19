#include "Subtitles/SubtitleLibrary.h"

#include "Subtitles/SubtitleSubsystem.h"
#include "Subtitles/MainGameSubtitleDirector.h"
#include "EngineUtils.h"

void USubtitleLibrary::PlaySubtitleSegment(const UObject* WorldContextObject, FName SegmentId)
{
	if (USubtitleSubsystem* Subtitles = USubtitleSubsystem::Get(WorldContextObject))
	{
		Subtitles->PlaySegment(SegmentId);
	}
}

void USubtitleLibrary::PlayMainGameSubtitle(const UObject* WorldContextObject, FName SegmentId)
{
	if(!WorldContextObject || !WorldContextObject->GetWorld()) return;
	for(TActorIterator<AMainGameSubtitleDirector> It(WorldContextObject->GetWorld()); It; ++It)
	{
		It->PlayOnce(SegmentId);
		break;
	}
}
