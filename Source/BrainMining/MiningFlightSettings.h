#pragma once

#include "CoreMinimal.h"
#include "MiningFlightSettings.generated.h"

USTRUCT(BlueprintType)
struct BRAINMINING_API FMiningFlightSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight", meta=(DisplayName="Enable Jetpack", ToolTip="Tap to jump; hold or tap twice to ignite the jetpack. Release to coast briefly, then fall quickly. Applied on the next Play."))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight", meta=(ClampMin="50", ClampMax="2000", Units="cm/s", EditCondition="bEnabled", ToolTip="Maximum upward speed at player scale 1. Automatically follows Player Scale."))
	float AscentSpeed = 450;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight", meta=(ClampMin="50", ClampMax="3000", Units="cm/s", EditCondition="bEnabled", ToolTip="Maximum falling speed at player scale 1. Lower values make descent easier to control."))
	float MaxFallSpeed = 650;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight", meta=(ClampMin="50", ClampMax="2000", Units="cm/s", EditCondition="bEnabled", ToolTip="WASD speed while airborne, including descent. Follows Player Scale and Movement Speed Multiplier. Ground speed is unchanged."))
	float AirMoveSpeed = 500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight", meta=(ClampMin="100", ClampMax="10000", Units="cm/s^2", EditCondition="bEnabled", ToolTip="Net upward acceleration while held, after counteracting gravity. Higher values stop a fall and start rising faster. Follows Player Scale."))
	float LiftAcceleration = 1800;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight", meta=(ClampMin="0.1", ClampMax="4.0", EditCondition="bEnabled", ToolTip="Base airborne gravity multiplier. Jetpack release adds a brief coast followed by Fast Fall Gravity Multiplier."))
	float FallGravityMultiplier = 1;

	// Multipliers retain existing level tuning while giving serialized maps the new flight profile.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight|Jetpack Feel", meta=(ClampMin="0.05", ClampMax="1", Units="s"))
	float HoldToIgniteSeconds = .22f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight|Jetpack Feel", meta=(ClampMin="0.05", ClampMax="1", Units="s"))
	float DoubleTapWindowSeconds = .30f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight|Jetpack Feel", meta=(ClampMin="0.05", ClampMax="2", Units="s"))
	float SpoolUpSeconds = .28f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight|Jetpack Feel", meta=(ClampMin="0.05", ClampMax="1"))
	float InitialThrustFraction = .18f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight|Jetpack Feel", meta=(ClampMin="1", ClampMax="5"))
	float BoostSpeedMultiplier = 2.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight|Jetpack Feel", meta=(ClampMin="1", ClampMax="5"))
	float BoostAccelerationMultiplier = 2.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight|Jetpack Feel", meta=(ClampMin="0", ClampMax="2", Units="s"))
	float ReleaseHangSeconds = .50f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight|Jetpack Feel", meta=(ClampMin="0.01", ClampMax="1"))
	float HangGravityMultiplier = .18f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight|Jetpack Feel", meta=(ClampMin="1", ClampMax="20", ToolTip="Upward-only release drag. Fades out smoothly as gravity takes over; never brakes downward motion."))
	float HangDrag = 7.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight|Jetpack Feel", meta=(ClampMin="1", ClampMax="5"))
	float FastFallGravityMultiplier = 2.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight|Jetpack Feel", meta=(ClampMin="1", ClampMax="8", ToolTip="Higher fall cap leaves room to accelerate through a long drop instead of reaching a slow constant speed early."))
	float FastFallSpeedMultiplier = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight|Jetpack Heat", meta=(EditCondition="bEnabled"))
	bool bEnableHeat = true;

	/** Continuous thrust time from empty to overheated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight|Jetpack Heat", meta=(ClampMin="0.1", ClampMax="120", Units="s", EditCondition="bEnabled && bEnableHeat"))
	float HeatUpSeconds = 5.f;

	/** Time to cool a full bar when the flight button is released. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight|Jetpack Heat", meta=(ClampMin="0.1", ClampMax="120", Units="s", EditCondition="bEnabled && bEnableHeat"))
	float PassiveCoolSeconds = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight|Jetpack Heat", meta=(ClampMin="0.1", ClampMax="30", Units="s", EditCondition="bEnabled && bEnableHeat"))
	float OverheatLockSeconds = 3.f;

	FMiningFlightSettings Sanitized() const;
};
