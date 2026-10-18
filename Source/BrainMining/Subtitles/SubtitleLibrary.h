#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SubtitleLibrary.generated.h"

UCLASS()
class BRAINMINING_API USubtitleLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="字幕", meta=(WorldContext="WorldContextObject", DisplayName="播放字幕段"))
	static void PlaySubtitleSegment(const UObject* WorldContextObject, FName SegmentId);
	// Optional campaign narrative: enabled by the director in the subtitle sublevel.
	static void PlayMainGameSubtitle(const UObject* WorldContextObject, FName SegmentId);
};
