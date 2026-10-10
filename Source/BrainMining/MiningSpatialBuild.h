#pragma once

#include "MiningSpatialGeometry.h"
#include "MiningChunkGeometry.h"

namespace MiningSpatial
{
	struct FPortalInput
	{
		bool bBound = false;
		int32 Wall = 0;
		FTransform Transform;
		FVector HalfSize = FVector::ZeroVector, Center = FVector::ZeroVector, Normal = FVector::ZeroVector;
	};
	struct FSourceInput
	{
		FString Name;
		EMiningManualKind Kind = EMiningManualKind::Passage;
		bool bHidden = false;
		bool bMain = false;
		bool bOwned = true;
		double Tolerance = 8;
		FTransform Transform;
		FVector Size = FVector::ZeroVector, Up = FVector::UpVector;
		TArray<FMiningSpatialKnot> Knots;
		FPortalInput Portals[2];
	};
	struct FRegionInput
	{
		FVector Origin = FVector::ZeroVector;
		FBox Bounds = FBox(ForceInit);
		FBox Rooms[2];
		TArray<FBox> SceneRooms;
		int32 PartitionIndex = 0, PartitionCount = 1;
		double Bedrock = 0, Tolerance = 8;
		bool bAutomaticBedrock = false;
		double ExcavationMargin = 400;
		int32 StartWall = 0, EndWall = 0;
		TArray<FSourceInput> Sources;
		TArray<uint8> Key;
	};
	struct FRegionBuild
	{
		FRegionInput Input;
		MiningHollow::FPlan Hollow;
		MiningManual::FPlan Plan;
		UE::Geometry::FDynamicMesh3 Rock;
		TArray<UE::Geometry::FDynamicMesh3> Cutters;
		TSharedPtr<const MiningChunks::FSpatialGrid, ESPMode::ThreadSafe> Grid;
		FString Error;
		FDiagnostic Diagnostic;
		int32 ErrorSource = INDEX_NONE;
		double ComputeMilliseconds = 0;
		bool bSuccess = false;
	};
	using FBuildPtr = TSharedPtr<const FRegionBuild, ESPMode::ThreadSafe>;
	BRAINMINING_API bool Capture(const AMiningSpatialRegion& Region, FVector Origin, double Bedrock, FRegionInput& Input, FString& Error);
	BRAINMINING_API bool BuildGeometry(const FRegionInput& Input, FRegionBuild& Result);
	BRAINMINING_API FBuildPtr Compute(FRegionInput Input, double ChunkLength, int32 Budget, FBuildPtr Previous = nullptr);
	BRAINMINING_API const FRegionBuild* Prepared(const AMiningSpatialRegion* Region);
	class BRAINMINING_API FPreparedScope
	{
	public:
		explicit FPreparedScope(const TMap<const AMiningSpatialRegion*, FBuildPtr>& Builds);
		~FPreparedScope();
	private:
		const TMap<const AMiningSpatialRegion*, FBuildPtr>* Previous;
	};
}
