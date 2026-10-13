#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeavenToilet.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UNiagaraComponent;

/** Independent preview prop: proximity opens the original lid before real water starts. */
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
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|Proximity",meta=(ClampMin="1")) float OpenDistance=220.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|Proximity",meta=(ClampMin="1")) float CloseDistance=280.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|Animation",meta=(ClampMin="0.1")) float OpenSeconds=.7f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|Animation",meta=(ClampMin="0.1")) float CloseSeconds=1.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Toilet|Animation") float ClosedLidRoll=96.f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet|State") float LidOpenAmount=0.f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet|State") bool bPlayerNearby=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Toilet|State") bool bWaterRunning=false;
private:
 void ApplyLidPose();
};
