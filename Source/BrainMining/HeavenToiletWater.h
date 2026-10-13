#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeavenToiletWater.generated.h"

class ADiggableRock;
class UDynamicMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Shallow, fixed-level basin. Cosmetic spill follows actual excavated air cells. */
UCLASS()
class BRAINMINING_API AHeavenToiletWater : public AActor
{
    GENERATED_BODY()
public:
    AHeavenToiletWater();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UDynamicMeshComponent> Basin;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UDynamicMeshComponent> Spill;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water") float Radius=1320;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water") float WaterHeight=48;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water") TObjectPtr<UMaterialInterface> WaterMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water") TObjectPtr<UMaterialInterface> SpillMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water") TArray<TObjectPtr<ADiggableRock>> RetainingSoil;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water") float InnerX=1400;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water") float OuterX=420;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water") float MouthX=1260;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water") float CorridorHalfWidth=170;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Water") bool bSpilling=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Water") int32 ConnectedCells=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Water") int32 BreachCount=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Water") float SpillStrength=0;
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Water") void RebuildWater();
    UFUNCTION(BlueprintCallable, Category="Water") void RefreshBreach();
private:
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BasinMID;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SpillMID;
    TArray<int32> ObservedCuts;
    float CheckTime=0,SpillAge=0;
    FVector LastPawnPosition=FVector::ZeroVector;
};
