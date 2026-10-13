#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MiningStartupOptimizeCommandlet.generated.h"

UCLASS()
class UMiningStartupOptimizeCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UMiningStartupOptimizeCommandlet();
	virtual int32 Main(const FString& Params) override;
};
