#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningDebrisBurst.generated.h"

class UMaterialInterface;
class UStaticMesh;

/** Short-lived physical soil chunks using the material of the surface that was mined. */
UCLASS()
class BRAINMINING_API AMiningDebrisBurst : public AActor
{

	GENERATED_BODY()
public:
	AMiningDebrisBurst();
	void Initialize(const TArray<TObjectPtr<UStaticMesh>>& Meshes, UMaterialInterface* Material, const FVector& SurfaceNormal, float CutRadius, int32 Count, float Lifetime);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Effects")
	TObjectPtr<UMaterialInterface> SourceMaterial;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Effects")
	int32 ChunkCount = 0;
};
