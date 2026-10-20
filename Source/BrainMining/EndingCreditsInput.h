#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class APlayerController;

namespace EndingCreditsInput
{
    // Returns false outside the completed ending, leaving normal menus alone.
    BRAINMINING_API bool Handle(APlayerController* Player,const FKey& Key);
}
