#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Components/SceneComponent.h"
#include "EndingCrowd.generated.h"

class USkeletalMesh;
class UAnimSequence;
class UStaticMesh;
class USkeletalMeshComponent;
class ULightComponent;
class UInstancedStaticMeshComponent;

USTRUCT(BlueprintType)
struct FEndingPedestrian
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) TObjectPtr<USkeletalMesh> Driver;
    UPROPERTY(EditAnywhere) TArray<TObjectPtr<USkeletalMesh>> Clothing;
    UPROPERTY(EditAnywhere) TObjectPtr<USkeletalMesh> Face;
    UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> Hair;
    UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> Walk;
    UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> Idle;
    UPROPERTY(EditAnywhere) float Speed = 110;
};

/** Explicit asset references keep the acquired crowd and retargeted clips cookable. */
UCLASS(BlueprintType)
class BRAINMINING_API UEndingCrowdAssets : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere) TArray<FEndingPedestrian> People;
    UPROPERTY(EditAnywhere) TArray<TObjectPtr<UStaticMesh>> BakedWalkers;
    UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> BakedHero;
};

/** Ending-only modular crowd: shared animation phases, individual walking lanes. */
UCLASS()
class BRAINMINING_API UEndingCrowdComponent : public USceneComponent
{
    GENERATED_BODY()
public:
    bool Build(UEndingCrowdAssets* Assets);
    void UpdateCrowd(bool bActive, float DeltaTime);
    int32 PersonCount() const { return People.Num()+BakedCount; }
    int32 BatchCount() const { return Batches.Num(); }
    const TArray<TObjectPtr<USkeletalMeshComponent>>& PoseDrivers() const { return Drivers; }
private:
    friend class FEndingCrowdContinuityTest;
    bool BuildBaked(UEndingCrowdAssets* Assets);
    UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Batches;
    /** OverCrowd actor owns moving ISM batches for the VAT crowd path. */
    UPROPERTY(Transient) TObjectPtr<AActor> OverCrowdInstancer;
    TArray<TArray<FTransform>> BatchTransforms;
    TArray<TArray<FVector>> BatchOrigins;
    TArray<TArray<FVector>> BatchVelocities;
    int32 BakedCount=0;
    bool bCrowdActive=false;
    UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> People;
    UPROPERTY(Transient) TArray<TObjectPtr<USkeletalMeshComponent>> Drivers;
    UPROPERTY(Transient) TArray<TObjectPtr<ULightComponent>> Lights;
    TArray<FVector> Origins;
    TArray<float> Speeds;
    float TravelTime = 0;
};
