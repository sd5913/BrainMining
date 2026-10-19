#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeavenPipeDrip.generated.h"

/** Readable falling droplets and short-lived impact splashes at the exposed clue. */
UCLASS()
class BRAINMINING_API AHeavenPipeDrip : public AActor
{
    GENERATED_BODY()
public:
    AHeavenPipeDrip();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UInstancedStaticMeshComponent> Droplets;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UInstancedStaticMeshComponent> Splashes;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float FallDistance=184.f;
private:
    float Age=0;
};
