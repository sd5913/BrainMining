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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight|Jetpack Heat") float CoolingDelay = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Player Flight|Jetpack Heat") float CoolingDelayRemaining = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Player Flight") bool bCreativeFlight = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Player Flight")
	bool bFlightInputHeld = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Player Flight")
	float FlightSizeScale = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Player Flight")
	float FlightMovementScale = 1;

	UFUNCTION(BlueprintCallable, Category="Player Flight")
	void SetFlightInputHeld(bool bHeld);
    void PushFlightBlock() {++FlightBlockCount;SetFlightInputHeld(false);}
    void PopFlightBlock() {FlightBlockCount=FMath::Max(0,FlightBlockCount-1);}
    bool IsFlightAvailable() const {return FlightSettings.bEnabled && FlightBlockCount==0;}
private:
    FVector LastTerrainPreparePosition = FVector::ZeroVector;
    bool bPreparedTerrain = false;
    int32 FlightBlockCount=0;
	// Capture the duration at overheat start so runtime settings edits cannot jump the bar.
	float JetpackOverheatDuration = 0.f;
public:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Player Flight|Jetpack Heat")
	float JetpackHeat = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Player Flight|Jetpack Heat")
	float JetpackOverheatRemaining = 0.f;
	UFUNCTION(BlueprintPure, Category="Player Flight|Jetpack Heat")
	bool IsJetpackOverheated() const { return JetpackOverheatRemaining > 0.f; }
	UFUNCTION(BlueprintPure, Category="Player Flight")
	bool IsFlightThrustActive() const { return FlightSettings.bEnabled && bFlightInputHeld && !IsJetpackOverheated(); }
	/** Advances gameplay heat once per movement tick, independently of UI visibility. */
	void UpdateJetpackHeat(float DeltaSeconds, bool bThrustRequested);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual FVector NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const override;
	virtual float GetMaxSpeed() const override;
	virtual FVector GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
};
