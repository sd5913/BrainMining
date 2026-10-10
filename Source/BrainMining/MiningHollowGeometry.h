#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "MiningHollowRegion.h"

namespace MiningHollow
{
	struct FSettings
	{
		FBox FirstRoom;
		FBox LastRoom;
		TArray<FVector> Points;
		TArray<FMiningHollowPlug> Plugs;
		double Radius = 350;
		double Width = 2600;
		double Above = 500;
		double Below = 400;
		double MaxSlope = 25;
	};
	struct FPlan
	{
		FBox Bounds;
		TArray<FVector> Points;
		TArray<double> Distances;
		TArray<FVector2D> PlugIntervals;
		TArray<FMiningGuideVolume> Guides;
		double Radius = 350;
		double Length = 0;
	};
	bool Plan(const FSettings& Settings, FPlan& Result, FString& Error);
	bool ValidateChain(const TArray<FPlan>& Plans, const TArray<FBox>& Rooms, double BedrockThickness, FString& Error);
	TArray<FVector> Slice(const FPlan& Plan, double Start, double End);
	TArray<FVector> Ring(const FVector& Center, double Radius);
	bool Tube(const TArray<FVector>& Points, double Radius, UE::Geometry::FDynamicMesh3& Mesh);
	bool Fill(const FPlan& Plan, UE::Geometry::FDynamicMesh3& Mesh, FString& Error);
	bool IsGuide(const FVector& Point, const TArray<FMiningGuideVolume>& Guides);
}
