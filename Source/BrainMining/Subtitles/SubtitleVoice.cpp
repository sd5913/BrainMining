#include "Subtitles/SubtitleVoice.h"

namespace
{
	struct FCharacterSlot
	{
		bool bVoiced;
		float Seconds;
	};

    bool IsWordCharacter(TCHAR Character)
    {
        return (FChar::IsAlpha(Character) || FChar::IsDigit(Character)) &&
            !(Character >= 0x2e80 && Character <= 0x9fff) &&
            !(Character >= 0xac00 && Character <= 0xd7af) &&
            !(Character >= 0xf900 && Character <= 0xfaff) &&
            !(Character >= 0xff00 && Character <= 0xffef);
    }

	TArray<FCharacterSlot> CharacterSlots(const FString& Text)
	{
		TArray<FCharacterSlot> Slots;
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			const TCHAR Character = Text[Index];
            if (IsWordCharacter(Character))
            {
                int32 WordLength = 1;
                while (Index + 1 < Text.Len())
                {
                    const TCHAR Next = Text[Index + 1];
                    if (IsWordCharacter(Next) || ((Next == '\'' || Next == 0x2019) && Index + 2 < Text.Len() && IsWordCharacter(Text[Index + 2])))
                    { ++Index; ++WordLength; }
                    else break;
                }
                Slots.Add({true, FMath::Clamp(.16f + .018f * WordLength, .20f, .42f)});
                continue;
            }
			// UTF-16 surrogate pairs represent one character, not two syllables.
			if (Character >= 0xd800 && Character <= 0xdbff && Index + 1 < Text.Len() &&
				Text[Index + 1] >= 0xdc00 && Text[Index + 1] <= 0xdfff) ++Index;
			if (FChar::IsWhitespace(Character))
			{
				Slots.Add({ false, Character == '\n' ? .28f : .08f });
				continue;
			}
			switch (Character)
			{
			case '.': case '!': case '?': case 0x3002: case 0xff01: case 0xff1f: case 0x2026:
				Slots.Add({ false, .28f }); break;
			case ',': case ';': case ':': case 0xff0c: case 0xff1b: case 0xff1a: case 0x3001:
				Slots.Add({ false, .18f }); break;
			case '"': case '\'': case '(': case ')': case '[': case ']': case '-':
			case 0x2014: case 0x2013: case 0x201c: case 0x201d: case 0x2018: case 0x2019:
			case 0xff08: case 0xff09: case 0x3010: case 0x3011:
				Slots.Add({ false, .06f }); break;
			default:
				Slots.Add({ true, .16f }); break;
			}
		}
		return Slots;
	}

	float DurationForSlots(const TArray<FCharacterSlot>& Slots)
	{
		float Seconds = .4f; // Quiet reading time after the final syllable.
		for (const auto& Slot : Slots) Seconds += Slot.Seconds;
		return FMath::Max(1.6f, Seconds);
	}
}

float SubtitleVoice::TextDuration(const FString& Text)
{
	return DurationForSlots(CharacterSlots(Text));
}

FSubtitleVoiceClip SubtitleVoice::Generate(const FString& Text, int32 Seed)
{
	const auto Slots = CharacterSlots(Text);
	FSubtitleVoiceClip Clip;
	Clip.Duration = DurationForSlots(Slots);
	if (!Slots.ContainsByPredicate([](const FCharacterSlot& Slot) { return Slot.bVoiced; })) return Clip;
	Clip.Samples.SetNumZeroed(FMath::CeilToInt(Clip.Duration * Clip.SampleRate));
	FRandomStream Random(Seed);
	float Cursor = .03f;
	float PreviousPitch = 0.f;
	for (const auto& Slot : Slots)
	{
		if (Slot.bVoiced)
		{
			float Semitones = Random.FRandRange(-3.f, 3.f);
			if (FMath::Abs(Semitones - PreviousPitch) < .6f)
				Semitones = PreviousPitch > 0.f ? -1.5f : 1.5f;
			PreviousPitch = Semitones;
			const float Frequency = 210.f * FMath::Pow(2.f, Semitones / 12.f);
			const float Length = Random.FRandRange(.055f, .075f);
			const float Gain = Random.FRandRange(.14f, .18f);
			const int32 Start = FMath::RoundToInt(Cursor * Clip.SampleRate);
			const int32 Count = FMath::RoundToInt(Length * Clip.SampleRate);
			float Phase = 0.f;
			for (int32 Sample = 0; Sample < Count && Start + Sample < Clip.Samples.Num(); ++Sample)
			{
				const float Time = float(Sample) / Clip.SampleRate;
				// Smooth attack/release avoids clicks; odd harmonics give a soft
				// 8-bit buzz without playing the same unchanging beep each time.
				const float Envelope = FMath::Min(Time / .004f, 1.f) *
					FMath::Clamp((Length - Time) / .018f, 0.f, 1.f);
				Phase += 2.f * PI * Frequency * (1.f + .006f * FMath::Sin(Time * 2.f * PI * 24.f)) / Clip.SampleRate;
				const float Tone = (FMath::Sin(Phase) + .3f * FMath::Sin(Phase * 3.f) + .12f * FMath::Sin(Phase * 5.f)) / 1.42f;
				Clip.Samples[Start + Sample] = int16(FMath::RoundToInt(Tone * Envelope * Gain * 32767.f));
			}
		}
		Cursor += Slot.Seconds;
	}
	return Clip;
}
