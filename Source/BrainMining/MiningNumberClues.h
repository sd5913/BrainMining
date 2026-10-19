#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningNumberClues.generated.h"

class ADiggableRock;
class UInstancedStaticMeshComponent;

// Each instance is one complete numeral, with its own terrain support.
UCLASS()
class BRAINMINING_API AMiningNumberClues : public AActor
{
    GENERATED_BODY()
public:
    AMiningNumberClues();
    void Initialize(ADiggableRock* Rock,FVector Core,FVector Surface,FVector Inward);
    virtual void Tick(float Dt) override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UInstancedStaticMeshComponent> Numbers;
private:
    void TerrainCommitted(const FBox& Bounds);
    TWeakObjectPtr<ADiggableRock> Host;
    TArray<FVector> Supports;
    int32 ObservedCuts=INDEX_NONE;
};
