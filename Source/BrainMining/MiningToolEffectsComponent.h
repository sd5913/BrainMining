#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MiningToolEffectsComponent.generated.h"

class UCurveFloat;
class UMiningComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;

/** Presentation for the existing mining operation; never performs a rock cut. */
UCLASS(ClassGroup=(Mining), meta=(BlueprintSpawnableComponent))
class BRAINMINING_API UMiningToolEffectsComponent : public UActorComponent
{

	GENERATED_BODY()
public:
	UMiningToolEffectsComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Effects")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category="Mining|Effects")
	TSoftObjectPtr<UStaticMesh> PickaxeMeshAsset;
	UPROPERTY(EditAnywhere, Category="Mining|Effects")
	TSoftObjectPtr<UCurveFloat> SwingCurveAsset;
	UPROPERTY(EditAnywhere, Category="Mining|Effects")
	TSoftClassPtr<AActor> DebrisClass;
	UPROPERTY(EditAnywhere, Category="Mining|Effects")
	TArray<TSoftObjectPtr<UStaticMesh>> DebrisMeshAssets;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Effects", meta=(ClampMin="12", ClampMax="64"))
	int32 DebrisChunkCount = 36;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Effects", meta=(ClampMin="0.5", ClampMax="4.0"))
	float DebrisLifetime = 2.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Effects")
	TObjectPtr<USceneComponent> PickaxePivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Effects")
	TObjectPtr<UStaticMeshComponent> PickaxeMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Effects")
	bool bSwinging = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Effects")
	int32 SwingCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Effects")
	int32 DebrisBurstCount = 0;

	UFUNCTION(BlueprintCallable, Category="Mining|Effects")
	void ResetEffects();

private:
	UPROPERTY(Transient)
	TObjectPtr<UMiningComponent> Mining;
	UPROPERTY(Transient)
	TObjectPtr<UCurveFloat> SwingCurve;
	UPROPERTY(Transient)
	TSubclassOf<AActor> LoadedDebrisClass;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> LoadedDebrisMeshes;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PendingSurfaceMaterial;
	TArray<TWeakObjectPtr<AActor>> DebrisActors;
	FTimerHandle ImpactTimer;
	FHitResult PendingHit;
	float SwingDuration = 0.3f;
	double SwingStart = 0;
	static constexpr float SourceSwingDuration = 13.f / 30.f;
	static constexpr float SourceImpactTime = 0.19f;

	UFUNCTION()
	void PlaySwing(float MiningInterval);
	UFUNCTION()
	void QueueImpact(const FHitResult& Hit);
	void SpawnImpact();
	bool CanShowEffects() const;
};
