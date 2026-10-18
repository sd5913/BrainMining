#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

class UFont;

/** Shared project typography. Sizes, outlines and other layout remain caller-owned. */
namespace BrainMiningFonts
{
    BRAINMINING_API UFont* Asset(bool bTitle = false);
    BRAINMINING_API FSlateFontInfo Body(float Size);
    BRAINMINING_API FSlateFontInfo Title(float Size);
    BRAINMINING_API FSlateFontInfo Style(const FName& Weight, float Size);
    BRAINMINING_API FSlateFontInfo Restyle(FSlateFontInfo Font, bool bTitle = false);
}
