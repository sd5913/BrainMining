#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningCollectionEffects.generated.h"

class UInstancedStaticMeshComponent;
class AMiningCollectible;

// Fixed instance pools: no per-frame spawning, and every effect belongs to the field reset.
UCLASS()
class BRAINMINING_API AMiningCollectionEffects : public AActor
{
    GENERATED_BODY()
public:
    AMiningCollectionEffects();
    void Initialize(int32 EffectKind, AMiningCollectible* Item=nullptr, int32 ConfettiPerColor=64);
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bStageActive=false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bCurtainOpen=false;
    // TV theatre opens on approach, then fires only after walking onto the stage.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSeparateStageTriggers=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CurtainSlideDistance=250;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CelebrationForward=500;
    UPROPERTY() TArray<TObjectPtr<AActor>> Curtains;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector TrailEnd=FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float FlowHeight=140;
    float GetCelebrationAge() const { return bStageActive?Age:0.f; }
private:
    UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Pools;
    TWeakObjectPtr<AMiningCollectible> SourceItem;
    int32 Kind=0;
    float Age=0;
    float CurtainOpening=0;
    TArray<FVector> CurtainClosedLocations;
};
