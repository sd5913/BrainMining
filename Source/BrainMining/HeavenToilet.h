#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeavenToilet.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UNiagaraComponent;

/** A continuously running fountain with bounded, colliding rubber-duck bursts. */
UCLASS()
class BRAINMINING_API AHeavenToilet : public AActor
{
 GENERATED_BODY()
public:
 AHeavenToilet();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;

 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet") TObjectPtr<UStaticMeshComponent> Body;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet") TObjectPtr<USceneComponent> LidHinge;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet") TObjectPtr<UStaticMeshComponent> Lid;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet") TObjectPtr<USceneComponent> WaterOutlet;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet") TObjectPtr<UNiagaraComponent> WaterJet;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet") TObjectPtr<UNiagaraComponent> OpeningSplash;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet|SprayDuck") TObjectPtr<UStaticMeshComponent> SprayDuck;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|Proximity",meta=(ClampMin="1")) float OpenDistance=220.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|Proximity",meta=(ClampMin="1")) float CloseDistance=280.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|Animation",meta=(ClampMin="0.1")) float OpenSeconds=.7f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|Animation",meta=(ClampMin="0.1")) float CloseSeconds=1.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|Animation") float ClosedLidRoll=96.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|SprayDuck",meta=(ClampMin="0")) float DuckForwardSpeed=480.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|SprayDuck",meta=(ClampMin="0")) float DuckUpSpeed=260.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|SprayDuck",meta=(ClampMin="0.1")) float DuckFlightSeconds=1.35f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|SprayDuck",meta=(ClampMin="0")) float DuckGravity=620.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|SprayDuck",meta=(ClampMin="0.001")) float DuckScale=0.54f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|Water") bool bContinuousSpray=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|SprayDuck",meta=(ClampMin="1")) float DuckBurstInterval=3.2f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet|State") float LidOpenAmount=0.f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet|State") bool bPlayerNearby=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet|State") bool bWaterRunning=false;
private:
 void ApplyLidPose();
 void LaunchDuck();
 void StartDuckBurst();
 void ResetDuck();
 UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> DuckPool;
 struct FDuckMotion { FVector Velocity=FVector::ZeroVector; FRotator Spin=FRotator::ZeroRotator; float Age=0; bool Flying=false; };
 TArray<FDuckMotion> DuckMotion;
 float BurstAge=0.f,NextDuckDelay=0.f;
 int32 PendingDucks=0;
 int32 NextDuck=0;
};
