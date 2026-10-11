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

	/** Fallback for non-BrainMiningCharacter owners; edit PickaxeModel on the character instead. */
	UPROPERTY(EditAnywhere, Category="Mining|Legacy Runtime Pose", AdvancedDisplay)
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

	/** Fallback for other pawn types. Character models are edited directly on DrillRightModel/DrillLeftModel. */
	UPROPERTY(EditAnywhere, Category="Mining|Legacy Runtime Pose", AdvancedDisplay)
	TSoftObjectPtr<UStaticMesh> DrillMeshAsset;
	/** The source FBX's rigid rotation take, editable as a float curve in UE. */
	UPROPERTY(EditAnywhere, Category="Mining|Drill|Assets")
	TSoftObjectPtr<UCurveFloat> DrillSpinCurveAsset;
	/** Axis in the drill model's mesh coordinates; follows Model rotation, not Mount X. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Animation", meta=(ToolTip="Spin axis in mesh-local coordinates. The prepared SM_Drill uses X=1, Y=0, Z=0. Model and Mount poses are preserved."))
	FVector DrillSpinAxis = FVector::ForwardVector;
	/** A point on the shaft axis, in mesh-local centimeters. Zero uses the FBX origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Animation", meta=(Units="cm", ToolTip="Point on the spin axis in mesh-local coordinates. Adjust only when the replacement mesh's origin is offset from its shaft."))
	FVector DrillSpinPivot = FVector::ZeroVector;
	/** Rigid rotation about the model's own shaft without moving artist-authored components. */
	static FTransform BuildDrillSpinTransform(const FTransform& ModelPose, const FVector& LocalAxis, const FVector& LocalPivot, float Degrees);
	/** Legacy fallback only. On BP_FirstPersonCharacter move DrillRightMount in its viewport. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Legacy Runtime Pose", AdvancedDisplay)
	FVector DrillLocation = FVector(64, 25, -10);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Legacy Runtime Pose", AdvancedDisplay)
	FRotator DrillRotation = FRotator(14, -18, 0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Legacy Runtime Pose", AdvancedDisplay, meta=(ClampMin="0.1", ClampMax="3"))
	float DrillScale = 0.72f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Presentation")
	bool bDualDrills = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Legacy Runtime Pose", AdvancedDisplay)
	FVector LeftDrillLocation = FVector(64, -25, -10);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Legacy Runtime Pose", AdvancedDisplay)
	FRotator LeftDrillRotation = FRotator(14, 18, 0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Presentation", meta=(ClampMin="0.05", ClampMax="0.6"))
	float ToolEquipDuration = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Effects", meta=(ClampMin="12", ClampMax="64"))
	int32 DrillDebrisChunkCount = 56;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Effects", meta=(ClampMin="0.5", ClampMax="4"))
	float DrillDebrisLifetime = 1.2f;
	UPROPERTY(EditAnywhere, Category="Mining|Drill|Dust")
	TSoftObjectPtr<UMaterialInterface> DrillDustMaterialAsset;
	UPROPERTY(EditAnywhere, Category="Mining|Drill|Dust")
	TSoftObjectPtr<UMaterialInterface> DrillSmokeMaterialAsset;
	UPROPERTY(EditAnywhere, Category="Mining|Drill|Dust")
	TSoftObjectPtr<UMaterialInterface> DrillSparkMaterialAsset;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Dust", meta=(ClampMin="0",ClampMax="2"))
	float DrillSparkStrength = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Dust", meta=(ClampMin="0", ClampMax="2"))
	float DrillDustStrength = 1.f;
	/** Average particles per successful cut; emission varies by 20 percent. Zero disables a layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Dust", meta=(ClampMin="0", ClampMax="64"))
	int32 DrillDustParticleCount = 24;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Smoke", meta=(ClampMin="0", ClampMax="24"))
	int32 DrillSmokeParticleCount = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Smoke", meta=(ClampMin="0", ClampMax="2"))
	float DrillSmokeStrength = 1.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Dust", meta=(ClampMin="0.25", ClampMax="3"))
	float DrillDustSize = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Smoke", meta=(ClampMin="0.25", ClampMax="3"))
	float DrillSmokeSize = 1.25f;
	/** Multiplier for all contact particle lifetimes. Active bursts remain capped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Effects", meta=(ClampMin="0.25", ClampMax="2"))
	float DrillContactLifetimeScale = 1.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Sparks", meta=(ClampMin="0", ClampMax="32"))
	int32 DrillSparkParticleCount = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Sparks", meta=(ClampMin="0.25", ClampMax="4"))
	float DrillSparkSize = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Sparks", meta=(ClampMin="0.25", ClampMax="3"))
	float DrillSparkSpeed = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Sparks", meta=(ClampMin="0", ClampMax="50"))
	float DrillSparkBrightness = 18.f;
	/** Used only when a soil material does not expose its color as a parameter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Drill|Dust")
	FLinearColor DrillDustColor = FLinearColor(0.22f, 0.16f, 0.09f, 1.f);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Drill|Presentation")
	TObjectPtr<USceneComponent> DrillPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Drill|Presentation")
	TObjectPtr<UStaticMeshComponent> DrillMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Drill|Presentation")
	TObjectPtr<USceneComponent> LeftDrillPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Drill|Presentation")
	TObjectPtr<UStaticMeshComponent> LeftDrillMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Drill|Presentation")
	bool bDrillRunning = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Drill|Presentation")
	float DrillSpinDegrees = 0;

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
	TObjectPtr<UCurveFloat> DrillSpinCurve;
	UPROPERTY(Transient)
	TSubclassOf<AActor> LoadedDebrisClass;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> LoadedDebrisMeshes;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PendingSurfaceMaterial;
	TArray<TWeakObjectPtr<AActor>> DebrisActors;
	TArray<TWeakObjectPtr<AActor>> DustActors;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LoadedDustMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LoadedSmokeMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LoadedSparkMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> DustPlane;
	FSoftObjectPath LastDrillAssetPath;
	FTimerHandle ImpactTimer;
	FHitResult PendingHit;
	float SwingDuration = 0.3f;
	double SwingStart = 0;
	double DrillAnimationTime = 0;
	float EquipElapsed = 1;
	bool bPendingDrillImpact = false;
	bool bUsesCharacterComponents = false;
	static constexpr float SourceSwingDuration = 13.f / 30.f;
	static constexpr float SourceImpactTime = 0.19f;

	UFUNCTION()
	void PlaySwing(float MiningInterval);
	UFUNCTION()
	void QueueImpact(const FHitResult& Hit);
	UFUNCTION()
	void UpdateToolMode(bool bDrillMode);
	void SpawnImpact();
	bool CanShowEffects() const;
	/** Writes only animation children; artist-authored mounts and models remain untouched. */
	void ApplyToolMotion(USceneComponent* Motion, const FVector& LegacyLocation, const FRotator& LegacyRotation,
		const FVector& CameraOffset, const FRotator& Tilt, float SpinDegrees = 0, UStaticMeshComponent* SpinModel = nullptr);
};
