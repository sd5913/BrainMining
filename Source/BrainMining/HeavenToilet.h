#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeavenToilet.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UNiagaraComponent;

/** Ambient fountain whose dense duck stream slowly follows a nearby player. */
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
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|DuckGush",meta=(ClampMin="1")) float GushEnterDistance=700.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|DuckGush",meta=(ClampMin="1")) float GushExitDistance=850.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|DuckGush",meta=(ClampMin="0.1")) float GushSeconds=10.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|DuckGush",meta=(ClampMin="0",Units="deg/s")) float GushTurnSpeed=18.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|DuckGush",meta=(ClampMin="0.1")) float GushPauseSeconds=2.5f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|DuckGush",meta=(ClampMin="1",ClampMax="60")) float GushDucksPerSecond=28.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|DuckGush",meta=(ClampMin="0")) float GushUpSpeed=560.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|DuckGush",meta=(ClampMin="64",ClampMax="256")) int32 MaxDucks=160;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet|State") bool bGushNearby=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet|State") bool bGushing=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet|State") FVector GushDirection=FVector::ForwardVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet|State") int32 DucksLaunched=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet|State") int32 DucksRecycled=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet|State") int32 ActiveDucks=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet|State") int32 FlyingDucks=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet|State") float LidOpenAmount=0.f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet|State") bool bPlayerNearby=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet|State") bool bWaterRunning=false;
private:
 void ApplyLidPose();
 void LaunchDuck();
 void StartDuckBurst();
 void StartGush(const FVector& PlayerLocation);
 void UpdateGushAim(const FVector& PlayerLocation,float DeltaSeconds,bool bSnap=false);
 int32 AcquireDuck();
 void ResetDuck();
 UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> DuckPool;
 struct FDuckMotion
 {
  FVector Velocity=FVector::ZeroVector;
  FRotator Spin=FRotator::ZeroRotator;
  FVector Scale=FVector::OneVector;
  float Age=0.f, RestAge=0.f, RecycleAge=0.f;
  bool Flying=false, Recycling=false;
 };
 TArray<FDuckMotion> DuckMotion;
 float BurstAge=0.f,NextDuckDelay=0.f;
 int32 PendingDucks=0;
 float GushAge=0.f, GushForwardSpeed=0.f;
};
