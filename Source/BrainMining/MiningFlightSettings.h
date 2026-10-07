#pragma once

#include "CoreMinimal.h"
#include "MiningFlightSettings.generated.h"

USTRUCT(BlueprintType)
struct BRAINMINING_API FMiningFlightSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight", meta=(DisplayName="Enable Hold-to-Fly", ToolTip="Hold the jump button to rise; release to fall. Collision remains enabled. Applied on the next Play; no terrain rebuild needed."))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight", meta=(ClampMin="50", ClampMax="2000", Units="cm/s", EditCondition="bEnabled", ToolTip="Maximum upward speed at player scale 1. Automatically follows Player Scale."))
	float AscentSpeed = 450;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight", meta=(ClampMin="50", ClampMax="3000", Units="cm/s", EditCondition="bEnabled", ToolTip="Maximum falling speed at player scale 1. Lower values make descent easier to control."))
	float MaxFallSpeed = 650;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight", meta=(ClampMin="50", ClampMax="2000", Units="cm/s", EditCondition="bEnabled", ToolTip="WASD speed while airborne, including descent. Follows Player Scale and Movement Speed Multiplier. Ground speed is unchanged."))
	float AirMoveSpeed = 500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight", meta=(ClampMin="100", ClampMax="10000", Units="cm/s^2", EditCondition="bEnabled", ToolTip="Net upward acceleration while held, after counteracting gravity. Higher values stop a fall and start rising faster. Follows Player Scale."))
	float LiftAcceleration = 1800;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight", meta=(ClampMin="0.1", ClampMax="4.0", EditCondition="bEnabled", ToolTip="Gravity multiplier after release, on top of the existing player-scaled gravity. Release cancels upward momentum immediately; this controls subsequent downward acceleration."))
	float FallGravityMultiplier = 1;

	FMiningFlightSettings Sanitized() const;
};
