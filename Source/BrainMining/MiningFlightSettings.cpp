#include "MiningFlightSettings.h"

FMiningFlightSettings FMiningFlightSettings::Sanitized() const
{
	FMiningFlightSettings Result = *this;
	const auto Safe = [](float Value, float Minimum, float Maximum, float Default)
	{
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, Minimum, Maximum) : Default;
	};
	Result.AscentSpeed = Safe(AscentSpeed, 50, 2000, 450);
	Result.MaxFallSpeed = Safe(MaxFallSpeed, 50, 3000, 650);
	Result.AirMoveSpeed = Safe(AirMoveSpeed, 50, 2000, 500);
	Result.LiftAcceleration = Safe(LiftAcceleration, 100, 10000, 1800);
	Result.FallGravityMultiplier = Safe(FallGravityMultiplier, 0.1f, 4, 1);
	return Result;
}
