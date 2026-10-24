#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MiningAromaComponent.generated.h"

// A small fixed pool of soft, camera-facing steam particles. No collision or
// per-frame component spawning; depth testing keeps buried steam inside soil.
UCLASS()
class UMiningAromaComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UMiningAromaComponent();
    FVector SourceLocal=FVector::ZeroVector;
    virtual void BeginPlay() override;
    virtual void TickComponent(float Dt,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
private:
    UPROPERTY() TObjectPtr<class UDynamicMeshComponent> Plume;
    float Age=0;
};
