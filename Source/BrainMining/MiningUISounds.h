#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"

// Slate plays these as UI audio, including while the gameplay world is paused.
namespace MiningUISounds
{
    void Apply(FButtonStyle& Style, bool bClick = true);
    const FButtonStyle* DefaultButton();
}
