#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

namespace MiningChunks
{
	struct FSpatialCell
	{
		FBox Cell = FBox(ForceInit), Bounds = FBox(ForceInit);
		TSharedPtr<const UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> Mesh;
	};
	struct FSpatialGrid
	{
		FBox Bounds = FBox(ForceInit);
		FVector Offset = FVector::ZeroVector;
		double Length = 0;
		TArray<FSpatialCell> Cells;
		int32 RebuiltCells = 0;
	};
	BRAINMINING_API bool SplitSpatialIncremental(const UE::Geometry::FDynamicMesh3& Source, double Length, FSpatialGrid& Grid, const FSpatialGrid* Previous = nullptr, const FBox& Dirty = FBox(ForceInit));
	BRAINMINING_API FBox ChangedBounds(const UE::Geometry::FDynamicMesh3& Before, const UE::Geometry::FDynamicMesh3& After);
	inline constexpr int32 MaxSpatialChunks = 8192;
	inline constexpr int32 MaxChunkTriangles = 100000;
	bool SpatialChunkCounts(const FVector& Size, double Length, FIntVector& Counts);
	bool Split(const UE::Geometry::FDynamicMesh3& Source, double Length, TArray<UE::Geometry::FDynamicMesh3>& Meshes, TArray<FBox>& Bounds);
	bool SplitSpatial(const UE::Geometry::FDynamicMesh3& Source, double Length, TArray<UE::Geometry::FDynamicMesh3>& Meshes, TArray<FBox>& Bounds);
	bool SplitExternalSpatial(const UE::Geometry::FDynamicMesh3& Source, double Length, TArray<UE::Geometry::FDynamicMesh3>& Meshes, TArray<FBox>& Bounds);
	BRAINMINING_API bool Subtract(const UE::Geometry::FDynamicMesh3& Source, const UE::Geometry::FDynamicMesh3& Cutter, UE::Geometry::FDynamicMesh3& Result, bool bPreserveCutEdges = false, double TimeBudgetSeconds = 0);
	bool Union(const UE::Geometry::FDynamicMesh3& Source, const UE::Geometry::FDynamicMesh3& Added, UE::Geometry::FDynamicMesh3& Result, bool bPreserveCutEdges = false);
	int32 Simplify(UE::Geometry::FDynamicMesh3& Mesh, const UE::Geometry::FDynamicMesh3& Initial, const FBox& Bounds, double Tolerance);
	void Normals(UE::Geometry::FDynamicMesh3& Mesh);
}
