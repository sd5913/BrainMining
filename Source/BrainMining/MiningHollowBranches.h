#pragma once

#include "MiningHollowGeometry.h"
#include "MiningDeveloperMapData.h"

namespace MiningHollowBranches
{
	struct FSettings
	{
		FString Name;
		FBox Chamber;
		TArray<FVector> InteriorPoints;
		double Connection = 0.15;
		double Radius = 180;
		double Thickness = 150;
		bool bHidden = false;
	};
	struct FPlan
	{
		FString Name;
		FBox Chamber;
		TArray<FVector> Points;
		TArray<FVector> OpenPoints;
		TArray<FVector> SealPoints;
		double Radius = 180;
		bool bHidden = false;
	};
	TArray<TArray<FVector>> Rings(const TArray<FVector>& Points, double Radius);
	bool Tube(const TArray<FVector>& Points, double Radius, UE::Geometry::FDynamicMesh3& Mesh);
	bool Plan(const MiningHollow::FPlan& Region, const TArray<FSettings>& Settings, TArray<FPlan>& Result, FString& Error);
	bool Carve(const TArray<FPlan>& Plans, UE::Geometry::FDynamicMesh3& Rock, FString& Error);
	void AddMapRegions(const TArray<FPlan>& Plans, const FVector& Origin, TArray<FMiningMapRegion>& Regions);
}
