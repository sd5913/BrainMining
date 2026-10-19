#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningCavernMotes.generated.h"

class UInstancedStaticMeshComponent;

/** A bounded dust field covering one mushroom climb, managed with its campaign area. */
UCLASS()
class BRAINMINING_API AMiningCavernMotes : public AActor
{
    GENERATED_BODY()
public:
    AMiningCavernMotes();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector HalfExtent=FVector(1100,650,1500);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="32",ClampMax="800")) int32 ParticleCount=600;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UInstancedStaticMeshComponent> Motes;
private:
    TArray<FVector> RestPositions;
    TArray<float> Sizes;
    float Age=0;
};
