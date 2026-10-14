#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

namespace MiningPickaxe
{
	BRAINMINING_API UE::Geometry::FDynamicMesh3 MakeCutter(const FTransform& Frame, const FVector& HalfSize, int32 Seed = 0);
}
