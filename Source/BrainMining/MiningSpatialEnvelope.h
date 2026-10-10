#pragma once

#include "MiningSpatialGeometry.h"

namespace MiningSpatial
{
	BRAINMINING_API TArray<AMiningSpatialRegion*> SceneRegions(const AMiningSpatialRegion& Region);
	BRAINMINING_API FBox ScenePartition(const FBox& Bounds, int32 Index, int32 Count);
}
