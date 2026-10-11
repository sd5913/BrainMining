#include "MiningCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

UMiningCharacterMovementComponent::UMiningCharacterMovementComponent()
{
	FlightSettings.bEnabled = false;
}

void UMiningCharacterMovementComponent::SetFlightInputHeld(bool bHeld)
{
	const bool bWasHeld = bFlightInputHeld;
	bFlightInputHeld = bHeld && IsFlightAvailable();
	if (bWasHeld && !bFlightInputHeld)
	{
		const FVector Up = -GetGravityDirection();
		Velocity -= Up * FMath::Max(0.0, FVector::DotProduct(Velocity, Up));
	}
}

void UMiningCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	const bool bCanThrust = CharacterOwner && CharacterOwner->GetController() && !CharacterOwner->IsMoveInputIgnored()
		&& (IsMovingOnGround() || IsFalling());
	UpdateJetpackHeat(DeltaTime, bCanThrust && bFlightInputHeld);
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UMiningCharacterMovementComponent::UpdateJetpackHeat(float DeltaSeconds, bool bThrustRequested)
{
	if (!FlightSettings.bEnabled || !FlightSettings.bEnableHeat)
	{
		JetpackHeat = JetpackOverheatRemaining = 0.f;
		return;
	}
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) return;
	const auto Settings = FlightSettings.Sanitized();
	// Consume the remainder across transitions so cooldown is frame-rate independent.
	float RemainingTime = DeltaSeconds;
	while (RemainingTime > 0.f)
	{
		if (IsJetpackOverheated())
		{
			const float Step = FMath::Min(RemainingTime, JetpackOverheatRemaining);
			JetpackOverheatRemaining = FMath::Max(0.f, JetpackOverheatRemaining - Step);
			RemainingTime -= Step;
			JetpackHeat = IsJetpackOverheated() ? 1.f : 0.f;
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
			const FVector Up = -GetGravityDirection();
			Velocity -= Up * FMath::Max(0.0, FVector::DotProduct(Velocity, Up));
		}
		else
		{
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
	double VerticalSpeed = IsFlightThrustActive()
		? FMath::FInterpConstantTo(OldVerticalSpeed, double(Settings.AscentSpeed * FlightSizeScale), double(DeltaTime), double(Settings.LiftAcceleration * FlightSizeScale))
		: OldVerticalSpeed + FVector::DotProduct(Gravity, Up) * Settings.FallGravityMultiplier * DeltaTime;
	VerticalSpeed = FMath::Clamp(VerticalSpeed, -double(Settings.MaxFallSpeed * FlightSizeScale), double(Settings.AscentSpeed * FlightSizeScale));
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
		SetFlightInputHeld(false);
		return;
	}
	if (IsFlightThrustActive() && IsMovingOnGround())
	{
		CharacterOwner->StopJumping();
		SetMovementMode(MOVE_Falling);
	}
}
