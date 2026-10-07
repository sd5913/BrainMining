#include "Subtitles/SubtitleLibrary.h"

#include "Subtitles/SubtitleSubsystem.h"

void USubtitleLibrary::PlaySubtitleSegment(const UObject* WorldContextObject, FName SegmentId)
{
	if (USubtitleSubsystem* Subtitles = USubtitleSubsystem::Get(WorldContextObject))
	{
		Subtitles->PlaySegment(SegmentId);
	}
}
