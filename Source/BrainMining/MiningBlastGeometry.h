#pragma once
#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

namespace MiningBlast
{
// A single seeded fracture volume shared by every touched rock/chunk. Positive
// radial displacement keeps it closed; broad lobes and small facets break the rim.
BRAINMINING_API UE::Geometry::FDynamicMesh3 MakeCutter(FVector Center, float Radius, float Roughness, int32 Seed);
}
