#pragma once

#include "CoreMinimal.h"
#include "MiningPickaxeSettings.h"

class ADiggableRock;

struct FMiningPickaxePatch
{
	TWeakObjectPtr<ADiggableRock> Rock;
	FVector LocalPoint = FVector::ZeroVector;
	FVector LocalSurfacePoint = FVector::ZeroVector;
	FTransform LocalFrame;
	FVector HalfSize = FVector::ZeroVector;
	float Radius = 0;
	float EarlyDepthLimit = 0;
	int32 Seed = 0;
	int32 Id = 0;
	int32 Strikes = 0;
	TArray<FMiningPickaxeStage> Stages;
	float Roughness = 1;
	bool bPending = false;
};
