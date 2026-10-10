#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

namespace MiningChunks
{
	inline constexpr int32 MaxSpatialChunks = 8192;
	inline constexpr int32 MaxChunkTriangles = 100000;
	bool SpatialChunkCounts(const FVector& Size, double Length, FIntVector& Counts);
	bool Split(const UE::Geometry::FDynamicMesh3& Source, double Length, TArray<UE::Geometry::FDynamicMesh3>& Meshes, TArray<FBox>& Bounds);
	bool SplitSpatial(const UE::Geometry::FDynamicMesh3& Source, double Length, TArray<UE::Geometry::FDynamicMesh3>& Meshes, TArray<FBox>& Bounds);
	bool Subtract(const UE::Geometry::FDynamicMesh3& Source, const UE::Geometry::FDynamicMesh3& Cutter, UE::Geometry::FDynamicMesh3& Result);
	int32 Simplify(UE::Geometry::FDynamicMesh3& Mesh, const UE::Geometry::FDynamicMesh3& Initial, const FBox& Bounds, double Tolerance);
	void Normals(UE::Geometry::FDynamicMesh3& Mesh);
}
