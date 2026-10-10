#pragma once

#include "CoreMinimal.h"
#include "MiningSpatialTunnel.h"
#include "MiningManualGeometry.h"

namespace MiningSpatial
{
	struct FDiagnostic
	{
		int32 Knot = INDEX_NONE;
		int32 Segment = INDEX_NONE;
		int32 Ring = INDEX_NONE;
		FVector Location = FVector::ZeroVector;
		FBox Bounds = FBox(ForceInit);
	};
	struct FRing
	{
		FVector Center, Tangent, Right, Up;
		TArray<FVector2D> Profile;
		int32 Knot = INDEX_NONE;
		int32 Segment = INDEX_NONE;
	};
	BRAINMINING_API TArray<FMiningPenPoint> Rectangle(double Width = 500, double Height = 500);
	BRAINMINING_API TArray<FMiningPenPoint> Ellipse(double Width = 500, double Height = 500);
	BRAINMINING_API FVector Evaluate(const FMiningSpatialKnot& First, const FMiningSpatialKnot& Last, double Parameter);
	BRAINMINING_API bool Insert(TArray<FMiningSpatialKnot>& Knots, int32 Segment, double Parameter);
	BRAINMINING_API bool Sample(const TArray<FMiningSpatialKnot>& Knots, const FVector& InitialUp, double Tolerance, TArray<FRing>& Rings, FString& Error, FDiagnostic* Diagnostic = nullptr, bool bRelaxed = false);
	BRAINMINING_API bool Sweep(const TArray<FRing>& Rings, UE::Geometry::FDynamicMesh3& Mesh, FString& Error, bool bCheckIntersections = true, FDiagnostic* Diagnostic = nullptr, bool bResolveOverlaps = false);
	BRAINMINING_API bool Shape(const AMiningSpatialTunnel& Source, double Tolerance, UE::Geometry::FDynamicMesh3& Mesh, TArray<FRing>& Rings, FString& Error, FDiagnostic* Diagnostic = nullptr);
	BRAINMINING_API void StoreSurface(const UE::Geometry::FDynamicMesh3& Mesh, FMiningMapRegion& Region);
	bool Build(AMiningSpatialRegion& Region, const FVector& Origin, double Bedrock, MiningHollow::FPlan& Hollow, MiningManual::FPlan& Result, UE::Geometry::FDynamicMesh3& Rock, FString& Error);
}
