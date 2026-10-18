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
    UPROPERTY() TArray<TObjectPtr<AActor>> Curtains;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector TrailEnd=FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float FlowHeight=140;
private:
    UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Pools;
    TWeakObjectPtr<AMiningCollectible> SourceItem;
    int32 Kind=0;
    float Age=0;
    float CurtainOpening=0;
};
