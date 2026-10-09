#pragma once

#include "CoreMinimal.h"
#include "MiningManualShape.h"
#include "MiningDeveloperMapData.h"
#include "MiningHollowGeometry.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "MiningChunkGeometry.h"

class AMiningSpatialTunnel;

namespace MiningManual
{
	struct FShapePlan
	{
		AMiningManualShape* Source = nullptr;
		TArray<FVector2D> Polygon;
		TArray<FIntVector> Triangles;
		double MinY = 0;
		double MaxY = 0;
	};
	struct FPlan
	{
		TArray<FShapePlan> Shapes;
		TArray<AMiningSpatialTunnel*> SpatialSources;
		TArray<FMiningMapRegion> SpatialMaps;
		TArray<UE::Geometry::FDynamicMesh3> PortalCuts;
		bool bAutomaticBedrock = false;
		UE::Geometry::FDynamicMesh3 RockDomain, OuterDomain, Air;
		TSharedPtr<const MiningChunks::FSpatialGrid, ESPMode::ThreadSafe> SpatialGrid;
		int32 StartWall = 0;
		int32 EndWall = 1;
	};
	BRAINMINING_API FVector2D Evaluate(const FMiningPenPoint& First, const FMiningPenPoint& Last, double Parameter);
	BRAINMINING_API bool Insert(TArray<FMiningPenPoint>& Points, int32 Segment, double Parameter, bool bClosed);
	BRAINMINING_API bool BindGuide(TArray<FMiningPenPoint>& Points);
	BRAINMINING_API bool GuideBound(const TArray<FMiningPenPoint>& Points);
	BRAINMINING_API FVector2D GuidePoint(const TArray<FMiningPenPoint>& Points, double Station);
	BRAINMINING_API void Bend(TArray<FMiningPenPoint>& Points, double Station, const FVector2D& Delta);
	BRAINMINING_API bool Sample(const TArray<FMiningPenPoint>& Points, bool bClosed, double Tolerance, TArray<FVector2D>& Output, FString& Error);
	BRAINMINING_API bool Triangulate(const TArray<FVector2D>& Polygon, TArray<FIntVector>& Triangles, FString& Error);
	BRAINMINING_API bool Contains(const TArray<FVector2D>& Polygon, const FVector2D& Point);
	BRAINMINING_API bool Extrude(const TArray<FVector2D>& Polygon, double MinY, double MaxY, UE::Geometry::FDynamicMesh3& Mesh, FString& Error);
	BRAINMINING_API bool Shape(const AMiningManualShape& Source, double Tolerance, FShapePlan& Result, FString& Error);
	bool Build(AMiningManualRegion& Region, const FVector& Origin, double Bedrock, MiningHollow::FPlan& Hollow, FPlan& Result, UE::Geometry::FDynamicMesh3& Rock, FString& Error);
	void AddMapRegions(const FPlan& Plan, int32 Group, TArray<FMiningMapRegion>& Regions);
}
