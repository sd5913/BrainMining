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
	bFlightInputHeld = bHeld && FlightSettings.bEnabled;
	if (bWasHeld && !bFlightInputHeld)
	{
		const FVector Up = -GetGravityDirection();
		Velocity -= Up * FMath::Max(0.0, FVector::DotProduct(Velocity, Up));
	}
}

FVector UMiningCharacterMovementComponent::NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const
{
	if (!FlightSettings.bEnabled)
	{
		return Super::NewFallVelocity(InitialVelocity, Gravity, DeltaTime);
	}
	if (DeltaTime <= 0) return InitialVelocity;
	const FMiningFlightSettings Settings = FlightSettings.Sanitized();
	const FVector Up = -GetGravityDirection();
	const double OldVerticalSpeed = FVector::DotProduct(InitialVelocity, Up);
	double VerticalSpeed = bFlightInputHeld
		? FMath::FInterpConstantTo(OldVerticalSpeed, double(Settings.AscentSpeed * FlightSizeScale), double(DeltaTime), double(Settings.LiftAcceleration * FlightSizeScale))
		: OldVerticalSpeed + FVector::DotProduct(Gravity, Up) * Settings.FallGravityMultiplier * DeltaTime;
	VerticalSpeed = FMath::Clamp(VerticalSpeed, -double(Settings.MaxFallSpeed * FlightSizeScale), double(Settings.AscentSpeed * FlightSizeScale));
	return InitialVelocity + Up * (VerticalSpeed - OldVerticalSpeed);
}

float UMiningCharacterMovementComponent::GetMaxSpeed() const
{
	return FlightSettings.bEnabled && IsFalling()
		? FlightSettings.Sanitized().AirMoveSpeed * FlightMovementScale
		: Super::GetMaxSpeed();
}

FVector UMiningCharacterMovementComponent::GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration)
{
	return FlightSettings.bEnabled ? FallAcceleration : Super::GetAirControl(DeltaTime, TickAirControl, FallAcceleration);
}

void UMiningCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	if (!FlightSettings.bEnabled || !CharacterOwner || !CharacterOwner->GetController() || CharacterOwner->IsMoveInputIgnored()
		|| (!IsMovingOnGround() && !IsFalling()))
	{
		SetFlightInputHeld(false);
		return;
	}
	if (bFlightInputHeld && IsMovingOnGround())
	{
		CharacterOwner->StopJumping();
		SetMovementMode(MOVE_Falling);
	}
}
