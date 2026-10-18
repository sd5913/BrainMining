#pragma once

#include "CoreMinimal.h"

/** A short retro voice phrase generated entirely in memory, in mono PCM16. */
struct FSubtitleVoiceClip
{
	static constexpr int32 SampleRate = 24000;
	TArray<int16> Samples;
	float Duration = 0.f;
};

namespace SubtitleVoice
{
	BRAINMINING_API float TextDuration(const FString& Text);
	BRAINMINING_API FSubtitleVoiceClip Generate(const FString& Text, int32 Seed);
}
