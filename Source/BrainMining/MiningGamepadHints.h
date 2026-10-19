#pragma once

#include "CoreMinimal.h"

class SWidget;
class UMiningGamepadSubsystem;

enum class EMiningGamepadGlyph : uint8
{
    None, A, B, X, Y, LT, RT, LB, RB, RightStick, DPad, DPadUp, Menu, Count
};

struct FMiningGamepadHint
{
    EMiningGamepadGlyph Key;
    FString Label;
    EMiningGamepadGlyph AlternateKey;

    FMiningGamepadHint(EMiningGamepadGlyph InKey, FString InLabel, EMiningGamepadGlyph InAlternate = EMiningGamepadGlyph::None)
        : Key(InKey), Label(MoveTemp(InLabel)), AlternateKey(InAlternate) {}

    bool operator==(const FMiningGamepadHint& Other) const
    {
        return Key == Other.Key && Label == Other.Label && AlternateKey == Other.AlternateKey;
    }
};

const TCHAR* MiningGamepadGlyphName(EMiningGamepadGlyph Glyph);
TSharedRef<SWidget> MakeMiningGamepadHints(UMiningGamepadSubsystem* Input);
