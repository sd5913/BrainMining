#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

// Geometry shared by the independent external-passage tool and its tests.
namespace MiningExternal
{
	UE::Geometry::FDynamicMesh3 Box(const FVector& Size);
	BRAINMINING_API bool PrepareCutter(UE::Geometry::FDynamicMesh3& Mesh, FString& Error);
	bool Build(const FVector& Size, double ShellThickness,
		const TArray<UE::Geometry::FDynamicMesh3>& Cutters,
		UE::Geometry::FDynamicMesh3& Rock, UE::Geometry::FDynamicMesh3& Shell, FString& Error);
}
