#pragma once
#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

namespace MiningTerrainFragments
{
    // Chunk borders may connect to neighboring chunks; never erase those caps.
    BRAINMINING_API int32 RemoveSmallFloatingPieces(UE::Geometry::FDynamicMesh3& Mesh,
        const FBox& OriginalBounds,bool bProtectChunkBorders,float MaxDimension=40.f);
}
