#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningDebrisBurst.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/** Short-lived physical soil chunks using the material of the surface that was mined. */
UCLASS()
class BRAINMINING_API AMiningDebrisBurst : public AActor
{

	GENERATED_BODY()
public:
	AMiningDebrisBurst();
	void PreparePool(const TArray<TObjectPtr<UStaticMesh>>& Meshes, int32 AdditionalCount);
	void ReleaseToPool();
	bool IsPooled() const { return bPooled; }
	bool IsPoolActive() const { return bPoolActive; }
	int32 GetPreparedChunkCount() const { return PooledChunks.Num(); }
	virtual void LifeSpanExpired() override;
	void Initialize(const TArray<TObjectPtr<UStaticMesh>>& Meshes, UMaterialInterface* Material, const FVector& SurfaceNormal, float CutRadius, int32 Count, float Lifetime);
	void InitializeBlast(const TArray<TObjectPtr<UStaticMesh>>& Meshes, UMaterialInterface* Material, const FVector& Direction, float CutRadius, int32 Count, float Lifetime);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Effects")
	TObjectPtr<UMaterialInterface> SourceMaterial;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Effects")
	int32 ChunkCount = 0;
private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PooledChunks;
	bool bPooled = false;
	bool bPoolActive = false;
	void SpawnChunks(const TArray<TObjectPtr<UStaticMesh>>& Meshes, UMaterialInterface* Material, const FVector& SurfaceNormal, float CutRadius, int32 Count, float Lifetime, bool bBlast);
};
