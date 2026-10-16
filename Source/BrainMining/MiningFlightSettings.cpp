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
	Result.LiftAcceleration = Safe(LiftAcceleration, 100, 10000, 1500);
	Result.FallGravityMultiplier = Safe(FallGravityMultiplier, 0.1f, 4, 1);
	Result.HoldToIgniteSeconds = Safe(HoldToIgniteSeconds, .05f, 1, .22f);
	Result.DoubleTapWindowSeconds = Safe(DoubleTapWindowSeconds, .05f, 1, .30f);
	Result.SpoolUpSeconds = Safe(SpoolUpSeconds, .05f, 2, .28f);
	Result.InitialThrustFraction = Safe(InitialThrustFraction, .05f, 1, .18f);
	Result.BoostSpeedMultiplier = Safe(BoostSpeedMultiplier, 1, 5, 2.4f);
	Result.BoostAccelerationMultiplier = Safe(BoostAccelerationMultiplier, 1, 5, 2.6f);
	Result.ReleaseHangSeconds = Safe(ReleaseHangSeconds, 0, 2, .50f);
	Result.HangGravityMultiplier = Safe(HangGravityMultiplier, .01f, 1, .18f);
	Result.HangDrag = Safe(HangDrag, 1, 20, 7);
	Result.FastFallGravityMultiplier = Safe(FastFallGravityMultiplier, 1, 5, 2.8f);
	Result.FastFallSpeedMultiplier = Safe(FastFallSpeedMultiplier, 1, 8, 5);
	Result.HeatUpSeconds = Safe(HeatUpSeconds, 0.1f, 120, 5);
	Result.PassiveCoolSeconds = Safe(PassiveCoolSeconds, 0.1f, 120, 2);
	Result.OverheatLockSeconds = Safe(OverheatLockSeconds, 0.1f, 30, 3);
	return Result;
}
