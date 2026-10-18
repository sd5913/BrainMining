#include "MiningCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GeneratedMiningRock.h"

namespace
{
// Extra counter-thrust per unit of downward speed; fades continuously to zero at hover.
constexpr double AirCatchResponse = 12.;
constexpr double AirCatchDriftResponse = 20.;
// Cross a small velocity band under gentle lift instead of holding zero velocity.
double GetCatchDriftLift(const FMiningFlightSettings& Settings)
{
	const double FullLift = Settings.LiftAcceleration * Settings.BoostAccelerationMultiplier;
	return Settings.AirCatchHangSeconds > UE_SMALL_NUMBER
		? FMath::Min(FullLift * .5, 120. / Settings.AirCatchHangSeconds) : FullLift;
}
// Integral of smoothstep after the gentle drift interval, including its full-thrust tail.
double IntegrateCatchRise(double Time, double Duration)
{
	const double Alpha = FMath::Clamp(Time / Duration, 0., 1.);
	return Duration * (Alpha * Alpha * Alpha - .5 * FMath::Pow(Alpha, 4.))
		+ FMath::Max(0., Time - Duration);
}
// Integral of a coast envelope: hold briefly, then smoothly hand gravity back over.
// Averaging the envelope avoids a different transition when the frame rate changes.
double IntegrateReleaseCoast(double Time, double Duration)
{
	const double Hold = Duration * .35;
	const double Blend = Duration - Hold;
	const double Alpha = FMath::Clamp((Time - Hold) / Blend, 0., 1.);
	return FMath::Min(Time, Hold) + Blend * (Alpha - Alpha * Alpha * Alpha + .5 * FMath::Pow(Alpha, 4.));
}
}

UMiningCharacterMovementComponent::UMiningCharacterMovementComponent()
{
	FlightSettings.bEnabled = false;
}

void UMiningCharacterMovementComponent::SetFlightInputHeld(bool bHeld)
{
	if (!IsFlightAvailable())
	{
		ResetFlightInput();
		return;
	}
	if (bFlightInputHeld == bHeld) return;
	const auto Settings = FlightSettings.Sanitized();
	if (bHeld)
	{
		bFlightInputHeld = true;
		FlightHoldSeconds = 0.f;
		const bool bAirCatch = IsFalling() && (bJetpackUsedInAir || FVector::DotProduct(Velocity, -GetGravityDirection()) <= 0.);
		if (bAirCatch || DoubleTapRemaining > 0.f || bCreativeFlight) IgniteJetpack();
		DoubleTapRemaining = 0.f;
	}
	else
	{
		// A short ordinary jump keeps its momentum and costs no heat.
		DoubleTapRemaining = !bJetpackEngaged && FlightHoldSeconds < Settings.HoldToIgniteSeconds
			? Settings.DoubleTapWindowSeconds : 0.f;
		if (IsFlightThrustActive() && !bCreativeFlight && ThrustSeconds > 0.f && FlightHoldSeconds > 0.f
			&& FVector::DotProduct(Velocity, -GetGravityDirection()) >= 0.f)
		{
			JetpackHangRemaining = Settings.ReleaseHangSeconds;
			ReleaseHangDuration = JetpackHangRemaining;
			ReleaseCoastWeight = ReleaseHangDuration > 0.f ? 1.f : 0.f;
		}
		bFlightInputHeld = bJetpackEngaged = false;
		FlightHoldSeconds = ThrustSeconds = 0.f;
		ResetAirCatchRecovery();
	}
}

void UMiningCharacterMovementComponent::IgniteJetpack()
{
	bJetpackEngaged = true;
	ResetAirCatchRecovery();
	const auto Settings = FlightSettings.Sanitized();
	if (!bCreativeFlight && !IsJetpackOverheated() && IsFalling()
		&& FVector::DotProduct(Velocity, -GetGravityDirection()) <= 0.)
	{
		bAirCatchRecovery = true;
		AirCatchRecoveryTime = -Settings.AirCatchHangSeconds;
		AirCatchLiftWeight = GetCatchDriftLift(Settings) / (Settings.LiftAcceleration * Settings.BoostAccelerationMultiplier);
	}
	// First takeoff keeps its wind-up; an airborne correction needs thrust immediately.
	const bool bAirCatch = IsFalling() && (bJetpackUsedInAir || FVector::DotProduct(Velocity, -GetGravityDirection()) <= 0.);
	ThrustSeconds = bAirCatch ? FlightSettings.Sanitized().SpoolUpSeconds : 0.f;
	// An overheated press cannot create a free coast or change a normal jump's gravity.
	if (!IsJetpackOverheated()) JetpackHangRemaining = ReleaseCoastWeight = ReleaseHangDuration = 0.f;
}

void UMiningCharacterMovementComponent::ResetAirCatchRecovery()
{
	bAirCatchRecovery = false;
	AirCatchRecoveryTime = 0.f;
	AirCatchLiftWeight = 1.f;
}

void UMiningCharacterMovementComponent::ResetFlightInput()
{
	if (bJetpackEngaged || JetpackHangRemaining > 0.f || ReleaseCoastWeight > 0.f)
	{
		const FVector Up = -GetGravityDirection();
		Velocity -= Up * FMath::Max(0.0, FVector::DotProduct(Velocity, Up));
	}
	bFlightInputHeld = bJetpackEngaged = bJetpackUsedInAir = false;
	FlightHoldSeconds = DoubleTapRemaining = ThrustSeconds = JetpackHangRemaining = 0.f;
	ReleaseCoastWeight = ReleaseHangDuration = 0.f;
	ResetAirCatchRecovery();
}

void UMiningCharacterMovementComponent::UpdateFlightState(float DeltaSeconds)
{
	if (!IsFlightAvailable()) { ResetFlightInput(); return; }
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) return;
	DoubleTapRemaining = FMath::Max(0.f, DoubleTapRemaining - DeltaSeconds);
	ReleaseCoastWeight = 0.f;
	if (JetpackHangRemaining > 0.f && ReleaseHangDuration > UE_SMALL_NUMBER)
	{
		const double Elapsed = ReleaseHangDuration - JetpackHangRemaining;
		ReleaseCoastWeight = FMath::Clamp(float((IntegrateReleaseCoast(Elapsed + DeltaSeconds, ReleaseHangDuration)
			- IntegrateReleaseCoast(Elapsed, ReleaseHangDuration)) / DeltaSeconds), 0.f, 1.f);
	}
	JetpackHangRemaining = FMath::Max(0.f, JetpackHangRemaining - DeltaSeconds);
	if (!bFlightInputHeld) return;
	const auto Settings = FlightSettings.Sanitized();
	const float PreviousHold = FlightHoldSeconds;
	FlightHoldSeconds += DeltaSeconds;
	float ThrustDelta = DeltaSeconds;
	if (!bJetpackEngaged && FlightHoldSeconds >= Settings.HoldToIgniteSeconds)
	{
		IgniteJetpack();
		ThrustDelta = FlightHoldSeconds - FMath::Max(PreviousHold, Settings.HoldToIgniteSeconds);
	}
	if (IsJetpackOverheated())
	{
		ThrustSeconds = JetpackHangRemaining = ReleaseCoastWeight = ReleaseHangDuration = 0.f;
		ResetAirCatchRecovery();
	}
	else if (IsFlightThrustActive())
	{
		bJetpackUsedInAir = true;
		ThrustSeconds += ThrustDelta;
		AirCatchLiftWeight = 1.f;
		if (bAirCatchRecovery)
		{
			const double DriftLift = GetCatchDriftLift(Settings);
			const double DriftSpeed = .5 * DriftLift * Settings.AirCatchHangSeconds * FlightSizeScale;
			const float DriftWeight = DriftLift / (Settings.LiftAcceleration * Settings.BoostAccelerationMultiplier);
			AirCatchLiftWeight = DriftWeight;
			if (FVector::DotProduct(Velocity, -GetGravityDirection()) >= -DriftSpeed)
			{
				const float PreviousTime = AirCatchRecoveryTime;
				AirCatchRecoveryTime += ThrustDelta;
				if (ThrustDelta > UE_SMALL_NUMBER)
				{
					const float RiseWeight = FMath::Clamp(float((IntegrateCatchRise(AirCatchRecoveryTime, Settings.AirCatchRiseSeconds)
						- IntegrateCatchRise(PreviousTime, Settings.AirCatchRiseSeconds)) / ThrustDelta), 0.f, 1.f);
					AirCatchLiftWeight = FMath::Lerp(DriftWeight, 1.f, RiseWeight);
				}
				if (AirCatchRecoveryTime >= Settings.AirCatchRiseSeconds) bAirCatchRecovery = false;
			}
		}
	}
}

void UMiningCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (CharacterOwner && CharacterOwner->IsPlayerControlled())
	{
		const FVector Position = CharacterOwner->GetActorLocation();
		if (!bPreparedTerrain || FVector::DistSquared(Position, LastTerrainPreparePosition) >= FMath::Square(500.f))
		{
			AGeneratedMiningRock::PrepareTerrainAt(GetWorld(), Position, 5000.f + Velocity.Size() * FMath::Max(0.f, DeltaTime));
			LastTerrainPreparePosition = Position;
			bPreparedTerrain = true;
		}
	}
	const bool bCanThrust = CharacterOwner && CharacterOwner->GetController() && !CharacterOwner->IsMoveInputIgnored()
		&& (IsMovingOnGround() || IsFalling());
	if (!bCanThrust) ResetFlightInput();
	UpdateFlightState(DeltaTime);
	UpdateJetpackHeat(DeltaTime, bCanThrust && bFlightInputHeld && (bJetpackEngaged || bCreativeFlight));
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UMiningCharacterMovementComponent::UpdateJetpackHeat(float DeltaSeconds, bool bThrustRequested)
{
	if (!FlightSettings.bEnabled || !FlightSettings.bEnableHeat || bCreativeFlight)
	{
		JetpackHeat = JetpackOverheatRemaining = JetpackOverheatDuration = 0.f;
		CoolingDelayRemaining = 0;
		return;
	}
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) return;
	const auto Settings = FlightSettings.Sanitized();
	if (bThrustRequested) CoolingDelayRemaining = FMath::Max(0.f, CoolingDelay);
	// Consume the remainder across transitions so cooldown is frame-rate independent.
	float RemainingTime = DeltaSeconds;
	while (RemainingTime > 0.f)
	{
		if (IsJetpackOverheated())
		{
			const float Step = FMath::Min(RemainingTime, JetpackOverheatRemaining);
			JetpackOverheatRemaining = FMath::Max(0.f, JetpackOverheatRemaining - Step);
			RemainingTime -= Step;
			const float CooldownDuration = JetpackOverheatDuration > 0.f ? JetpackOverheatDuration : Settings.OverheatLockSeconds;
			JetpackHeat = FMath::Clamp(JetpackOverheatRemaining / CooldownDuration, 0.f, 1.f);
		}
		else if (bThrustRequested)
		{
			const float TimeToFull = (1.f - JetpackHeat) * Settings.HeatUpSeconds;
			if (RemainingTime + UE_SMALL_NUMBER < TimeToFull)
			{
				JetpackHeat = FMath::Clamp(JetpackHeat + RemainingTime / Settings.HeatUpSeconds, 0.f, 1.f);
				return;
			}
			RemainingTime = FMath::Max(0.f, RemainingTime - TimeToFull);
			JetpackHeat = 1.f;
			JetpackOverheatRemaining = Settings.OverheatLockSeconds;
			JetpackOverheatDuration = Settings.OverheatLockSeconds;
			JetpackHangRemaining = ThrustSeconds = ReleaseCoastWeight = ReleaseHangDuration = 0.f;
			ResetAirCatchRecovery();
			const FVector Up = -GetGravityDirection();
			Velocity -= Up * FMath::Max(0.0, FVector::DotProduct(Velocity, Up));
		}
		else
		{
			const float Waiting = FMath::Min(RemainingTime, CoolingDelayRemaining);
			CoolingDelayRemaining -= Waiting;
			RemainingTime -= Waiting;
			JetpackHeat = FMath::Max(0.f, JetpackHeat - RemainingTime / Settings.PassiveCoolSeconds);
			return;
		}
	}
}

FVector UMiningCharacterMovementComponent::NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const
{
	if (!IsFlightAvailable())
	{
		return Super::NewFallVelocity(InitialVelocity, Gravity, DeltaTime);
	}
	if (DeltaTime <= 0) return InitialVelocity;
	const FMiningFlightSettings Settings = FlightSettings.Sanitized();
	const FVector Up = -GetGravityDirection();
	const double OldVerticalSpeed = FVector::DotProduct(InitialVelocity, Up);
	if (bCreativeFlight)
	{
		const auto* Controller = CharacterOwner ? Cast<APlayerController>(CharacterOwner->GetController()) : nullptr;
		const bool bDescend = Controller && Controller->IsInputKeyDown(EKeys::LeftControl) && !CharacterOwner->IsMoveInputIgnored();
		const double TargetSpeed = (bFlightInputHeld ? 1. : bDescend ? -1. : 0.) * Settings.AscentSpeed * FlightSizeScale;
		return InitialVelocity + Up * (TargetSpeed - OldVerticalSpeed);
	}
	const double BoostSpeed = Settings.AscentSpeed * Settings.BoostSpeedMultiplier * FlightSizeScale;
	double VerticalSpeed = OldVerticalSpeed;
	if (IsFlightThrustActive())
	{
		// A gentle ignition, then a steep ramp gives the lift a distinct kick.
		const double Alpha = FMath::Clamp(double(ThrustSeconds / Settings.SpoolUpSeconds), 0., 1.);
		const double Thrust = FMath::Lerp(double(Settings.InitialThrustFraction), double(Settings.BoostAccelerationMultiplier), Alpha * Alpha * Alpha);
		const double LiftRate = Settings.LiftAcceleration * FlightSizeScale * Thrust * AirCatchLiftWeight;
		if (bAirCatchRecovery)
		{
			const double DriftSpeed = .5 * GetCatchDriftLift(Settings) * Settings.AirCatchHangSeconds * FlightSizeScale;
			if (OldVerticalSpeed < -DriftSpeed)
			{
				// Strong braking fades continuously into gentle lift at the drift band.
				// Integrate exactly across its boundary so physics substeps agree.
				const double BrakeTarget = LiftRate / AirCatchDriftResponse - DriftSpeed;
				const double TimeToDrift = FMath::Loge((BrakeTarget - OldVerticalSpeed) / (BrakeTarget + DriftSpeed)) / AirCatchDriftResponse;
				VerticalSpeed = DeltaTime < TimeToDrift
					? BrakeTarget + (OldVerticalSpeed - BrakeTarget) * FMath::Exp(-AirCatchDriftResponse * DeltaTime)
					: FMath::Min(BoostSpeed, -DriftSpeed + LiftRate * (DeltaTime - TimeToDrift));
			}
			else
			{
				VerticalSpeed = FMath::Min(BoostSpeed, OldVerticalSpeed + LiftRate * DeltaTime);
			}
		}
		else if (OldVerticalSpeed < 0.)
		{
			// dv/dt = full lift - response * v while descending. Fast falls get a strong
			// catch without snapping velocity to zero; the extra brake vanishes at hover.
			const double BrakeLift = Settings.LiftAcceleration * FlightSizeScale * Settings.BoostAccelerationMultiplier;
			const double BrakeTarget = BrakeLift / AirCatchResponse;
			const double TimeToStop = FMath::Loge((BrakeTarget - OldVerticalSpeed) / BrakeTarget) / AirCatchResponse;
			if (DeltaTime < TimeToStop)
			{
				VerticalSpeed = BrakeTarget + (OldVerticalSpeed - BrakeTarget) * FMath::Exp(-AirCatchResponse * DeltaTime);
			}
			else
			{
				// Spend only the remainder ascending; keeps collision substeps equivalent.
				VerticalSpeed = FMath::Min(BoostSpeed, LiftRate * (DeltaTime - TimeToStop));
			}
		}
		else
		{
			const double LiftStep = LiftRate * DeltaTime;
			VerticalSpeed = OldVerticalSpeed + FMath::Clamp(BoostSpeed - OldVerticalSpeed, -LiftStep, LiftStep);
		}
	}
	else
	{
		const double GravityScaleAfterFlight = bJetpackUsedInAir ? Settings.FastFallGravityMultiplier : 1.f;
		const double Coast = IsJetpackOverheated() ? 0. : ReleaseCoastWeight;
		const double GravityMultiplier = FMath::Lerp(Settings.FallGravityMultiplier * GravityScaleAfterFlight,
			double(Settings.HangGravityMultiplier), Coast);
		const double VerticalAcceleration = FVector::DotProduct(Gravity, Up) * GravityMultiplier;
		const double Drag = Settings.HangDrag * Coast;
		// Fade out upward braking as gravity fades in. Never damp downward velocity:
		// that would act like a parachute and flatten the start of the fall.
		if (Drag > UE_SMALL_NUMBER && OldVerticalSpeed > 0.)
		{
			const double Terminal = VerticalAcceleration / Drag;
			VerticalSpeed = Terminal + (OldVerticalSpeed - Terminal) * FMath::Exp(-Drag * DeltaTime);
			if (VerticalSpeed < 0. && VerticalAcceleration < 0.)
			{
				// Split exactly at the apex so a long/substepped frame cannot brake descent.
				const double TimeToApex = FMath::Loge((OldVerticalSpeed - Terminal) / -Terminal) / Drag;
				VerticalSpeed = VerticalAcceleration * FMath::Max(0., double(DeltaTime) - TimeToApex);
			}
		}
		else
		{
			VerticalSpeed += VerticalAcceleration * DeltaTime;
		}
	}
	const double FallCap = Settings.MaxFallSpeed * FlightSizeScale * (bJetpackUsedInAir ? Settings.FastFallSpeedMultiplier : 1.f);
	// Do not cap a normal jump with the jetpack ascent speed.
	VerticalSpeed = FMath::Max(VerticalSpeed, -FallCap);
	return InitialVelocity + Up * (VerticalSpeed - OldVerticalSpeed);
}

float UMiningCharacterMovementComponent::GetMaxSpeed() const
{
	return IsFlightAvailable() && IsFalling()
		? FlightSettings.Sanitized().AirMoveSpeed * FlightMovementScale
		: Super::GetMaxSpeed();
}

FVector UMiningCharacterMovementComponent::GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration)
{
	return IsFlightAvailable() ? FallAcceleration : Super::GetAirControl(DeltaTime, TickAirControl, FallAcceleration);
}

void UMiningCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	if (!IsFlightAvailable() || !CharacterOwner || !CharacterOwner->GetController() || CharacterOwner->IsMoveInputIgnored()
		|| (!IsMovingOnGround() && !IsFalling()))
	{
		ResetFlightInput();
		return;
	}
	if (IsFlightThrustActive() && IsMovingOnGround())
	{
		CharacterOwner->StopJumping();
		SetMovementMode(MOVE_Falling);
	}
}

void UMiningCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	if ((IsMovingOnGround() && PreviousMovementMode == MOVE_Falling) || (!IsMovingOnGround() && !IsFalling()))
	{
		ResetFlightInput();
	}
}
