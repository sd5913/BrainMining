#pragma once

#include "MiningSpatialGeometry.h"

namespace MiningSpatial
{
	bool AddEnvelope(const UE::Geometry::FDynamicMesh3& Shape, const TArray<FRing>& Rings, const FVector& Origin, double Margin, double Thickness, UE::Geometry::FDynamicMesh3& Domain, UE::Geometry::FDynamicMesh3& Outer, FString& Error);
}
