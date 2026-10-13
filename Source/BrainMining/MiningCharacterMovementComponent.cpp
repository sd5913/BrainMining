#include "MiningCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GeneratedMiningRock.h"

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
	UpdateJetpackHeat(DeltaTime, bCanThrust && bFlightInputHeld);
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
