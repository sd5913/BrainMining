#include "Subtitles/SubtitleVoice.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSubtitleVoiceTimingTest, "BrainMining.Subtitles.GeneratedVoice.Timing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSubtitleVoiceTimingTest::RunTest(const FString&)
{
	const auto Short = SubtitleVoice::Generate(TEXT("你好。"), 17);
	const auto Long = SubtitleVoice::Generate(FString::ChrN(80, 'a'), 17);
	TestTrue(TEXT("Short lines retain readable minimum duration"), Short.Duration >= 1.6f);
	TestTrue(TEXT("Long subtitles scale beyond the former eight-second cap"), Long.Duration > 8.f);
	TestTrue(TEXT("Generated PCM matches subtitle duration"), FMath::Abs(float(Long.Samples.Num()) / Long.SampleRate - Long.Duration) < .001f);
	TestEqual(TEXT("Playback and synthesis use the same text duration"), Long.Duration, SubtitleVoice::TextDuration(FString::ChrN(80, 'a')));
	TestTrue(TEXT("Chinese and English punctuation alone remain silent"), SubtitleVoice::Generate(TEXT(" ，。！？… ,.!?\n"), 1).Samples.IsEmpty());
	TestTrue(TEXT("Empty captions do not allocate or play voice"), SubtitleVoice::Generate(TEXT(""), 1).Samples.IsEmpty());
	const auto Paused = SubtitleVoice::Generate(TEXT("甲，乙"), 9);
	// First syllable ends before 0.105s, second starts after the comma at 0.37s.
	bool bQuiet = true;
	for (int32 Index = int32(.15f * Paused.SampleRate); Index < int32(.35f * Paused.SampleRate); ++Index)
		bQuiet &= Paused.Samples[Index] == 0;
	TestTrue(TEXT("Comma creates a real pause between syllables"), bQuiet);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSubtitleVoiceVariationTest, "BrainMining.Subtitles.GeneratedVoice.Variation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSubtitleVoiceVariationTest::RunTest(const FString&)
{
	const FString Text = TEXT("你好，这是一段没有录音的字幕。听，声音会随着文字轻轻变化。昆虫的世界，还藏着很多秘密。");
	const auto Clip = SubtitleVoice::Generate(Text, 4827);
	const auto Other = SubtitleVoice::Generate(Text, 9301);
	TestTrue(TEXT("Each playback can use a different voice variation"), Clip.Samples != Other.Samples);
	int32 Peak = 0, LastCrossing = -1, ShortestPeriod = MAX_int32, LongestPeriod = 0;
	for (int32 Index = 1; Index < Clip.Samples.Num(); ++Index)
	{
		Peak = FMath::Max(Peak, FMath::Abs(int32(Clip.Samples[Index])));
		if (Clip.Samples[Index - 1] <= 0 && Clip.Samples[Index] > 0)
		{
			const int32 Period = Index - LastCrossing;
			if (LastCrossing >= 0 && Period > 70 && Period < 160)
			{
				ShortestPeriod = FMath::Min(ShortestPeriod, Period);
				LongestPeriod = FMath::Max(LongestPeriod, Period);
			}
			LastCrossing = Index;
		}
	}
	TestTrue(TEXT("Output is audible without clipping"), Peak > 1000 && Peak < 6500);
	TestTrue(TEXT("Actual PCM contains multiple pitches"), LongestPeriod - ShortestPeriod > 10);
	TestEqual(TEXT("Phrase begins in silence"), Clip.Samples[0], int16(0));
	TestEqual(TEXT("Phrase ends in silence"), Clip.Samples.Last(), int16(0));
	// A reviewable sample uses the same renderer as the runtime fallback.
	TArray<uint8> Wave;
	auto Tag = [&Wave](const ANSICHAR* Value) { Wave.Append(reinterpret_cast<const uint8*>(Value), 4); };
	auto LE16 = [&Wave](uint16 Value) { Wave.Add(uint8(Value)); Wave.Add(uint8(Value >> 8)); };
	auto LE32 = [&Wave](uint32 Value) { for (int32 Byte = 0; Byte < 4; ++Byte) Wave.Add(uint8(Value >> (Byte * 8))); };
	Tag("RIFF"); LE32(36 + Clip.Samples.Num() * 2); Tag("WAVE"); Tag("fmt "); LE32(16);
	LE16(1); LE16(1); LE32(Clip.SampleRate); LE32(Clip.SampleRate * 2); LE16(2); LE16(16);
	Tag("data"); LE32(Clip.Samples.Num() * 2);
	Wave.Append(reinterpret_cast<const uint8*>(Clip.Samples.GetData()), Clip.Samples.Num() * sizeof(int16));
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Validation/SubtitleVoice"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	TestTrue(TEXT("Generated preview WAV is saved"), FFileHelper::SaveArrayToFile(Wave, *FPaths::Combine(Directory, TEXT("preview.wav"))));
	return true;
}
#endif
