#include "MiningCharacterMovementComponent.h"
#include "Components/SceneComponent.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningFlightVelocityTest, "BrainMining.Flight.HoldReleaseAndCatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningFlightVelocityTest::RunTest(const FString& Parameters)
{
	UMiningCharacterMovementComponent* Movement = NewObject<UMiningCharacterMovementComponent>();
	Movement->FlightSettings.bEnabled = true;
	Movement->SetFlightInputHeld(true);
	const FVector Gravity(0, 0, -980);
	const FVector Lifted = Movement->NewFallVelocity(FVector(120, -80, 0), Gravity, 0.1f);
	TestTrue(TEXT("Hold accelerates upward without a jump impulse"), FMath::IsNearlyEqual(Lifted.Z, 180.0, 0.001));
	TestEqual(TEXT("Thrust preserves horizontal velocity"), FVector(Lifted.X, Lifted.Y, 0), FVector(120, -80, 0));
	Movement->Velocity = Lifted;
	Movement->SetFlightInputHeld(false);
	TestEqual(TEXT("Release cancels upward drift immediately"), Movement->Velocity.Z, 0.0);
	TestTrue(TEXT("Release applies gravity"), Movement->NewFallVelocity(Movement->Velocity, Gravity, 0.1f).Z < -97);
	Movement->Velocity = FVector(120, -80, -400);
	Movement->SetFlightInputHeld(false);
	TestEqual(TEXT("Repeated release does not erase falling speed"), Movement->Velocity.Z, -400.0);
	Movement->SetFlightInputHeld(true);
	const FVector Braked = Movement->NewFallVelocity(Movement->Velocity, Gravity, 0.1f);
	TestTrue(TEXT("Press while falling brakes instead of teleporting or snapping"), Braked.Z > -400 && Braked.Z < 0);
	TestTrue(TEXT("Holding recovers from a fall"), Movement->NewFallVelocity(Braked, Gravity, 0.3f).Z > 0);
	TestEqual(TEXT("Zero duration preserves velocity"), Movement->NewFallVelocity(Braked, Gravity, 0), Braked);
	Movement->FlightSettings.bEnabled = false;
	Movement->SetFlightInputHeld(true);
	TestFalse(TEXT("Disabled flight cannot latch thrust"), Movement->bFlightInputHeld);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningFlightScalingTest, "BrainMining.Flight.ScalingAndFrameRates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningFlightScalingTest::RunTest(const FString& Parameters)
{
	for (const float Scale : {0.25f, 0.7f, 1.f, 2.f})
	{
		for (const int32 FrameRate : {15, 30, 60, 120})
		{
			UMiningCharacterMovementComponent* Movement = NewObject<UMiningCharacterMovementComponent>();
			Movement->FlightSettings.bEnabled = true;
			Movement->FlightSizeScale = Scale;
			Movement->FlightMovementScale = Scale * 1.2f;
			Movement->UpdatedComponent = NewObject<USceneComponent>();
			Movement->MovementMode = MOVE_Falling;
			Movement->SetFlightInputHeld(true);
			FVector Velocity = FVector::ZeroVector;
			const FVector Gravity(0, 0, -980 * Scale);
			for (int32 Frame = 0; Frame < FrameRate * 3; ++Frame)
			{
				Velocity = Movement->NewFallVelocity(Velocity, Gravity, 1.f / FrameRate);
			}
			TestTrue(TEXT("Sustained lift respects scaled ascent cap at all frame rates"), FMath::IsNearlyEqual(Velocity.Z, double(450 * Scale), 0.01));
			TestTrue(TEXT("Air speed follows both size and movement multiplier"), FMath::IsNearlyEqual(Movement->GetMaxSpeed(), 500 * Scale * 1.2f, 0.01f));
			Movement->Velocity = Velocity;
			Movement->SetFlightInputHeld(false);
			Velocity = Movement->Velocity;
			for (int32 Frame = 0; Frame < FrameRate * 3; ++Frame)
			{
				Velocity = Movement->NewFallVelocity(Velocity, Gravity, 1.f / FrameRate);
			}
			TestTrue(TEXT("Long fall respects scaled terminal speed"), FMath::IsNearlyEqual(Velocity.Z, double(-650 * Scale), 0.01));
			Movement->MovementMode = MOVE_Walking;
			TestEqual(TEXT("Walking speed is not replaced"), Movement->GetMaxSpeed(), Movement->MaxWalkSpeed);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningFlightSettingsTest, "BrainMining.Flight.EditableSettings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningFlightSettingsTest::RunTest(const FString& Parameters)
{
	FMiningFlightSettings Invalid;
	Invalid.AscentSpeed = -10;
	Invalid.MaxFallSpeed = 99999;
	Invalid.AirMoveSpeed = std::numeric_limits<float>::quiet_NaN();
	Invalid.LiftAcceleration = std::numeric_limits<float>::infinity();
	Invalid.FallGravityMultiplier = 0;
	const FMiningFlightSettings Safe = Invalid.Sanitized();
	TestEqual(TEXT("Ascent lower bound"), Safe.AscentSpeed, 50.f);
	TestEqual(TEXT("Fall upper bound"), Safe.MaxFallSpeed, 3000.f);
	TestEqual(TEXT("Invalid air speed defaults"), Safe.AirMoveSpeed, 500.f);
	TestEqual(TEXT("Invalid acceleration defaults"), Safe.LiftAcceleration, 1800.f);
	TestEqual(TEXT("Gravity cannot be accidentally disabled"), Safe.FallGravityMultiplier, 0.1f);
	UMiningCharacterMovementComponent* Movement = NewObject<UMiningCharacterMovementComponent>();
	Movement->FlightSettings.bEnabled = true;
	Movement->FlightSettings.FallGravityMultiplier = 2;
	TestTrue(TEXT("Editable gravity changes actual descent"), FMath::IsNearlyEqual(Movement->NewFallVelocity(FVector::ZeroVector, FVector(0, 0, -980), 0.1f).Z, -196.0, 0.001));
	TestEqual(TEXT("Full horizontal control remains available in the air"), Movement->GetAirControl(0.1f, 0.5f, FVector(100, 40, 0)), FVector(100, 40, 0));
	return true;
}

#endif
