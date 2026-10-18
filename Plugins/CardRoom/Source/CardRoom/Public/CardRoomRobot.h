#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CardRoomRobot.generated.h"

// Rigid mechanical articulation preserves the supplied FBX instead of skinning its hard panels.
UCLASS()
class CARDROOM_API ACardRoomRobot : public AActor
{
    GENERATED_BODY()
public:
    ACardRoomRobot();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Robot") bool bEncounterDriven=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Robot") FName Expression=TEXT("Idle");
    void Pose(FName State,float Time,float DeltaSeconds,const FRotator& Look=FRotator::ZeroRotator);
private:
    UPROPERTY() TObjectPtr<USceneComponent> Head;
    UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> Parts;
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> Mouth;
};
