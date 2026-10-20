#pragma once

#include "CoreMinimal.h"

class ABrainMiningPlayerController;
struct FSlateBrush;
class SWidget;

namespace MiningMobileUI
{
    const FSlateBrush* Brush(FName Name);
    FLinearColor Ink();
    FLinearColor Cream();
    FLinearColor Gold();
    FLinearColor Teal();
    float Scale(FVector2D Size);
    TSharedRef<SWidget> MakeToolbar(ABrainMiningPlayerController* Controller);
}
