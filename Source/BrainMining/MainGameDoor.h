#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MainGameDoor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** A small, data driven door used by the MainGame start-room entrance. */
UCLASS()
class BRAINMINING_API AMainGameDoor : public AActor
{
	GENERATED_BODY()

public:
	AMainGameDoor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Door")
	TObjectPtr<UBoxComponent> InteractionBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door|Animation", meta=(ClampMin="0.1"))
	float OpenDuration = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door|Animation")
	float OpenAngle = 92.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door|Animation")
	float OpenDirection = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category="Door")
	bool bIsOpen = false;

	UFUNCTION(BlueprintCallable, Category="Door")
	bool IsPlayerInRange(const APawn* Pawn) const;

	UFUNCTION(BlueprintCallable, Category="Door")
	void OpenDoor();

private:
	float AnimationTime = 0.0f;
	FRotator ClosedRotation;
};
