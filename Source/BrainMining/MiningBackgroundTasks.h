#pragma once

#include "CoreMinimal.h"

struct FMiningTaskPermit
{
    FMiningTaskPermit();
    ~FMiningTaskPermit();
};

TSharedPtr<FMiningTaskPermit, ESPMode::ThreadSafe> AcquireMiningTaskPermit();
