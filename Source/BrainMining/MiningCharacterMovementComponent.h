#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MiningFlightSettings.h"
#include "MiningCharacterMovementComponent.generated.h"

UCLASS()
class BRAINMINING_API UMiningCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UMiningCharacterMovementComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight", meta=(ShowOnlyInnerProperties))
	FMiningFlightSettings FlightSettings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Player Flight")
	bool bFlightInputHeld = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Player Flight")
	float FlightSizeScale = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Player Flight")
	float FlightMovementScale = 1;

	UFUNCTION(BlueprintCallable, Category="Player Flight")
	void SetFlightInputHeld(bool bHeld);

	virtual FVector NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const override;
	virtual float GetMaxSpeed() const override;
	virtual FVector GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
};
