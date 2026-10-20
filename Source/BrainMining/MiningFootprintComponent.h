#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MiningFootprintComponent.generated.h"

class UMaterialInterface;

/** Grounded movement leaves a short, fading trail back through the excavation. */
UCLASS(ClassGroup=(Mining),meta=(BlueprintSpawnableComponent))
class BRAINMINING_API UMiningFootprintComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UMiningFootprintComponent();
    virtual void BeginPlay() override;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Footprints",meta=(ClampMin="1")) float Lifetime=15.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Footprints",meta=(ClampMin="10")) float StepDistance=55.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Footprints") bool bEnabled=true;
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
private:
    UPROPERTY() TObjectPtr<UMaterialInterface> FootprintMaterial;
    FVector Previous=FVector::ZeroVector;
    float Distance=0;
    bool bHavePrevious=false,bLeft=false;
};
