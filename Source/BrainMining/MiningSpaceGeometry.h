#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "MiningLayoutSpace.h"
#include "MiningRoutePlanner.h"
#include "MiningDeveloperMapData.h"

class AMiningLayoutRoom;

namespace MiningSpaces
{
	UE::Geometry::FDynamicMesh3 ShapeMesh(EMiningSpaceShape Shape, const FBox& Bounds);
	bool Apply(const TArray<AMiningLayoutSpace*>& Spaces, const TArray<AMiningLayoutRoom*>& Rooms,
		const TArray<FBox>& RoomBounds, const TArray<MiningRoutes::FPlan>& Plans, const FVector& Origin,
		double Thickness, TArray<UE::Geometry::FDynamicMesh3>& Segments,
		TArray<UE::Geometry::FDynamicMesh3>& Cavities, UE::Geometry::FDynamicMesh3& Shell, FString& Error,
		TArray<FMiningMapRegion>* MapRegions = nullptr);
}
