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
	Movement->UpdateFlightState(.1f);
	TestFalse(TEXT("A short first press is a normal jump, not thrust"), Movement->IsFlightThrustActive());
	Movement->Velocity = FVector(120, -80, 300);
	Movement->SetFlightInputHeld(false);
	TestEqual(TEXT("Short tap release preserves the ordinary jump"), Movement->Velocity.Z, 300.0);
	TestEqual(TEXT("Ordinary jump has no release hover"), Movement->JetpackHangRemaining, 0.f);
	Movement->UpdateFlightState(.2f);
	Movement->SetFlightInputHeld(true);
	TestTrue(TEXT("Second press inside the window ignites immediately"), Movement->IsFlightThrustActive());
	Movement->UpdateFlightState(.01f);
	const FVector Lifted = Movement->NewFallVelocity(FVector(120, -80, 0), Gravity, 0.1f);
	TestTrue(TEXT("Ignition begins with gentle lift"), Lifted.Z > 0 && Lifted.Z < 50);
	TestEqual(TEXT("Thrust preserves horizontal velocity"), FVector(Lifted.X, Lifted.Y, 0), FVector(120, -80, 0));
	Movement->UpdateFlightState(.3f);
	const FVector Boosted = Movement->NewFallVelocity(FVector(120, -80, 0), Gravity, .1f);
	TestTrue(TEXT("Spool creates a strong upward kick"), Boosted.Z > Lifted.Z * 8);
	Movement->Velocity = Boosted;
	Movement->SetFlightInputHeld(false);
	TestEqual(TEXT("Release retains momentum without a velocity snap"), Movement->Velocity, Boosted);
	const FVector Coasting = Movement->NewFallVelocity(Movement->Velocity, Gravity, .1f);
	TestTrue(TEXT("Release eases into the apex instead of dropping immediately"), Coasting.Z > 0 && Coasting.Z < Boosted.Z);
	Movement->UpdateFlightState(.6f);
	Movement->UpdateFlightState(.1f);
	TestTrue(TEXT("After hover, gravity accelerates the fall sharply"), Movement->NewFallVelocity(FVector::ZeroVector, Gravity, .1f).Z < -250);
	Movement->Velocity = FVector(120, -80, -400);
	Movement->SetFlightInputHeld(false);
	TestEqual(TEXT("Repeated release does not erase falling speed"), Movement->Velocity.Z, -400.0);
	Movement->SetFlightInputHeld(true);
	Movement->UpdateFlightState(.23f);
	const FVector Braked = Movement->NewFallVelocity(Movement->Velocity, Gravity, 0.02f);
	TestTrue(TEXT("Press while falling brakes instead of teleporting or snapping"), Braked.Z > -400 && Braked.Z < 0);
	Movement->UpdateFlightState(.3f);
	TestTrue(TEXT("Holding recovers from a fall"), Movement->NewFallVelocity(Braked, Gravity, 0.3f).Z > 0);
	Movement->SetFlightInputHeld(false);
	TestEqual(TEXT("Releasing during descent cannot conjure a hover"), Movement->JetpackHangRemaining, 0.f);
	TestEqual(TEXT("Zero duration preserves velocity"), Movement->NewFallVelocity(Braked, Gravity, 0), Braked);
	Movement->FlightSettings.bEnabled = false;
	Movement->SetFlightInputHeld(true);
	TestFalse(TEXT("Disabled flight cannot latch thrust"), Movement->bFlightInputHeld);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningFlightGestureTest, "BrainMining.Flight.GesturesAndReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningFlightGestureTest::RunTest(const FString& Parameters)
{
	auto* Movement = NewObject<UMiningCharacterMovementComponent>();
	Movement->FlightSettings.bEnabled = true;
	Movement->SetFlightInputHeld(true);
	Movement->UpdateFlightState(.1f);
	Movement->UpdateJetpackHeat(.1f, Movement->IsFlightThrustActive());
	TestEqual(TEXT("Short tap costs no heat"), Movement->JetpackHeat, 0.f);
	Movement->SetFlightInputHeld(false);
	Movement->UpdateFlightState(.31f);
	Movement->SetFlightInputHeld(true);
	TestFalse(TEXT("Expired second press remains a normal jump"), Movement->IsFlightThrustActive());
	Movement->UpdateFlightState(.21f);
	TestFalse(TEXT("Holding below the threshold does not ignite"), Movement->IsFlightThrustActive());
	Movement->UpdateFlightState(.02f);
	TestTrue(TEXT("Continuous hold ignites at the threshold"), Movement->IsFlightThrustActive());
	Movement->PushFlightBlock();
	Movement->SetFlightInputHeld(true);
	Movement->UpdateFlightState(1.f);
	TestFalse(TEXT("Scripted flight block cannot be bypassed"), Movement->IsFlightThrustActive());
	Movement->PopFlightBlock();
	TestFalse(TEXT("Unblocking cannot resume a stale gesture"), Movement->bFlightInputHeld);
	Movement->SetFlightInputHeld(true);
	Movement->SetFlightInputHeld(false);
	Movement->ResetFlightInput();
	Movement->SetFlightInputHeld(true);
	TestFalse(TEXT("Reset clears double-tap history"), Movement->IsFlightThrustActive());
	Movement->UpdateFlightState(.3f);
	Movement->UpdateJetpackHeat(5.f, true);
	Movement->SetFlightInputHeld(false);
	TestEqual(TEXT("Overheat cannot grant release hover"), Movement->JetpackHangRemaining, 0.f);
	Movement->SetFlightInputHeld(true);
	Movement->UpdateFlightState(.3f);
	TestFalse(TEXT("Long hold cannot bypass overheat lock"), Movement->IsFlightThrustActive());
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
				Movement->UpdateFlightState(1.f / FrameRate);
				Velocity = Movement->NewFallVelocity(Velocity, Gravity, 1.f / FrameRate);
			}
			TestTrue(TEXT("Sustained lift respects scaled ascent cap at all frame rates"), FMath::IsNearlyEqual(Velocity.Z, double(1080 * Scale), 0.01));
			TestTrue(TEXT("Air speed follows both size and movement multiplier"), FMath::IsNearlyEqual(Movement->GetMaxSpeed(), 500 * Scale * 1.2f, 0.01f));
			Movement->Velocity = Velocity;
			Movement->SetFlightInputHeld(false);
			Velocity = Movement->Velocity;
			for (int32 Frame = 0; Frame < FrameRate * 3; ++Frame)
			{
				Movement->UpdateFlightState(1.f / FrameRate);
				Velocity = Movement->NewFallVelocity(Velocity, Gravity, 1.f / FrameRate);
			}
			TestTrue(TEXT("Long fall respects scaled terminal speed"), FMath::IsNearlyEqual(Velocity.Z, double(-3250 * Scale), 0.01));
			Movement->MovementMode = MOVE_Walking;
			TestEqual(TEXT("Walking speed is not replaced"), Movement->GetMaxSpeed(), Movement->MaxWalkSpeed);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningReleaseCurveTest, "BrainMining.Flight.SmoothReleaseAndAcceleratingFall", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningReleaseCurveTest::RunTest(const FString& Parameters)
{
	double ReferenceHeight = 0.;
	double ReferenceSpeed = 0.;
	for (int32 FPS : {120, 60, 30, 15})
	{
		auto* Movement = NewObject<UMiningCharacterMovementComponent>();
		Movement->FlightSettings.bEnabled = true;
		Movement->SetFlightInputHeld(true);
		Movement->UpdateFlightState(1.f);
		Movement->Velocity = FVector(100, 40, 1080);
		Movement->SetFlightInputHeld(false);
		const float DT = 1.f / FPS;
		FVector Speed = Movement->Velocity;
		double Height = 0.;
		double PreviousAcceleration = 0.;
		double LargestBoundaryStep = 0.;
		for (int32 Frame = 0; Frame < FPS * 2; ++Frame)
		{
			Movement->UpdateFlightState(DT);
			const FVector Next = Movement->NewFallVelocity(Speed, FVector(0, 0, -980), DT);
			// Physics may query alternate trajectories or split the same frame for collision.
			const FVector Half = Movement->NewFallVelocity(Speed, FVector(0, 0, -980), DT * .5f);
			TestTrue(TEXT("Substeps do not change the release curve or advance its clock"),
				Next.Equals(Movement->NewFallVelocity(Half, FVector(0, 0, -980), DT * .5f), .01));
			const double Acceleration = (Next.Z - Speed.Z) / DT;
			if (FPS == 120 && Frame >= 54 && Frame <= 65)
			{
				LargestBoundaryStep = FMath::Max(LargestBoundaryStep, FMath::Abs(Acceleration - PreviousAcceleration));
			}
			PreviousAcceleration = Acceleration;
			Height += (Speed.Z + Next.Z) * .5 * DT;
			Speed = Next;
			if (Frame + 1 == FPS)
			{
				TestTrue(TEXT("One-second descent already exceeds the old feather-like speed cap"), Speed.Z < -1500);
				if (FPS == 120) { ReferenceHeight = Height; ReferenceSpeed = Speed.Z; }
				TestTrue(TEXT("Release height stays consistent across frame rates"), FMath::Abs(Height - ReferenceHeight) < 8.);
				TestTrue(TEXT("Release speed stays consistent across frame rates"), FMath::Abs(Speed.Z - ReferenceSpeed) < 10.);
				AddInfo(FString::Printf(TEXT("Release at %d Hz, 1 s: height %.1f cm, velocity %.1f cm/s"), FPS, Height, Speed.Z));
			}
			if (Frame + 1 == FPS * 3 / 2)
			{
				TestTrue(TEXT("Longer descent keeps gathering speed instead of plateauing early"), Speed.Z < -2700);
			}
		}
		if (FPS == 120) TestTrue(TEXT("Hang expiration has no abrupt acceleration step"), LargestBoundaryStep < 100.);
		TestEqual(TEXT("Release preserves horizontal control velocity"), FVector(Speed.X, Speed.Y, 0), FVector(100, 40, 0));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningAirCatchTest, "BrainMining.Flight.AirCatchAndRepeatedReignite", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningAirCatchTest::RunTest(const FString& Parameters)
{
	for (const float Scale : {.25f, .7f, 1.f, 2.f})
	{
		for (const int32 FPS : {15, 30, 60, 120})
		{
			for (const double FallSpeed : {300., 1700., 3250.})
			{
				auto* Movement = NewObject<UMiningCharacterMovementComponent>();
				Movement->FlightSettings.bEnabled = true;
				Movement->FlightSizeScale = Scale;
				Movement->UpdatedComponent = NewObject<USceneComponent>();
				Movement->MovementMode = MOVE_Falling;
				Movement->Velocity = FVector(100, 40, -FallSpeed * Scale);
				Movement->SetFlightInputHeld(true);
				TestTrue(TEXT("Even a first press after walking off a ledge ignites immediately"), Movement->IsFlightThrustActive());
				TestEqual(TEXT("Air catch never snaps velocity on the input event"), Movement->Velocity.Z, -FallSpeed * Scale);
				const float DT = 1.f / FPS;
				const FVector Gravity(0, 0, -980 * Scale);
				double Drop = 0.;
				float StopTime = 0.f;
				for (int32 Frame = 0; Frame < FMath::CeilToInt(.3f * FPS); ++Frame)
				{
					Movement->UpdateFlightState(DT);
					const FVector Before = Movement->Velocity;
					const FVector After = Movement->NewFallVelocity(Before, Gravity, DT);
					const FVector Half = Movement->NewFallVelocity(Before, Gravity, DT * .5f);
					TestTrue(TEXT("Air brake crosses zero consistently under physics substeps"),
						After.Equals(Movement->NewFallVelocity(Half, Gravity, DT * .5f), .01));
					if (Before.Z < 0.) Drop += FMath::Max(0., -(Before.Z + After.Z) * .5 * DT);
					if (After.Z >= 0. && StopTime == 0.f) StopTime = (Frame + 1) * DT;
					Movement->Velocity = After;
				}
				TestTrue(TEXT("Default terminal fall is arrested within a quarter second"), StopTime > 0.f && StopTime <= .25f);
				TestTrue(TEXT("Catching a fall costs less than 2.3 player-scaled metres"), Drop < 230. * Scale);
				TestTrue(TEXT("Holding continues straight into a clear ascent"), Movement->Velocity.Z > 400. * Scale);
				TestEqual(TEXT("Air catch preserves lateral movement"), FVector(Movement->Velocity.X, Movement->Velocity.Y, 0), FVector(100, 40, 0));
				if (FPS == 60 && Scale == 1.f && FallSpeed == 3250.)
				{
					AddInfo(FString::Printf(TEXT("Terminal-speed catch: stopped within %.3f s, lost %.1f cm, velocity at 0.3 s %.1f cm/s"), StopTime, Drop, Movement->Velocity.Z));
				}
				for (int32 Cycle = 0; Cycle < 3; ++Cycle)
				{
					Movement->SetFlightInputHeld(false);
					TestTrue(TEXT("Release after recovery grants the normal coast"), Movement->JetpackHangRemaining > 0.f);
					for (int32 Frame = 0; Frame < FPS; ++Frame)
					{
						Movement->UpdateFlightState(DT);
						Movement->Velocity = Movement->NewFallVelocity(Movement->Velocity, Gravity, DT);
					}
					TestTrue(TEXT("Release still allows a fast fall"), Movement->Velocity.Z < -1000. * Scale);
					Movement->SetFlightInputHeld(true);
					TestTrue(TEXT("Repeated airborne catches do not depend on the double-tap window"), Movement->IsFlightThrustActive());
					for (int32 Frame = 0; Frame < FMath::CeilToInt(.3f * FPS); ++Frame)
					{
						Movement->UpdateFlightState(DT);
						Movement->Velocity = Movement->NewFallVelocity(Movement->Velocity, Gravity, DT);
					}
					TestTrue(TEXT("Repeated catch returns to ascent"), Movement->Velocity.Z > 400. * Scale);
				}
				Movement->SetFlightInputHeld(false);
				Movement->SetFlightInputHeld(true);
				Movement->SetFlightInputHeld(false);
				TestEqual(TEXT("An unconsumed same-frame tap cannot renew coast for free"), Movement->JetpackHangRemaining, 0.f);
				Movement->SetFlightInputHeld(true);
				Movement->UpdateJetpackHeat(5.f, true);
				Movement->SetFlightInputHeld(false);
				Movement->Velocity.Z = -1700. * Scale;
				Movement->SetFlightInputHeld(true);
				TestFalse(TEXT("Air catch does not bypass overheat"), Movement->IsFlightThrustActive());
				TestTrue(TEXT("Overheat cannot grant free aerial braking"), Movement->NewFallVelocity(Movement->Velocity, Gravity, DT).Z < Movement->Velocity.Z);
				Movement->PushFlightBlock();
				Movement->SetFlightInputHeld(true);
				TestFalse(TEXT("Air catch respects scripted flight blocks"), Movement->IsFlightThrustActive());
			}
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
	Invalid.SpoolUpSeconds = 0;
	Invalid.ReleaseHangSeconds = std::numeric_limits<float>::quiet_NaN();
	Invalid.HangDrag = 0;
	const FMiningFlightSettings Safe = Invalid.Sanitized();
	TestEqual(TEXT("Ascent lower bound"), Safe.AscentSpeed, 50.f);
	TestEqual(TEXT("Fall upper bound"), Safe.MaxFallSpeed, 3000.f);
	TestEqual(TEXT("Invalid air speed defaults"), Safe.AirMoveSpeed, 500.f);
	TestEqual(TEXT("Invalid acceleration defaults"), Safe.LiftAcceleration, 1800.f);
	TestEqual(TEXT("Gravity cannot be accidentally disabled"), Safe.FallGravityMultiplier, 0.1f);
	TestEqual(TEXT("Spool cannot divide by zero"), Safe.SpoolUpSeconds, .05f);
	TestEqual(TEXT("Invalid hover duration defaults"), Safe.ReleaseHangSeconds, .5f);
	TestEqual(TEXT("Hang drag cannot divide by zero"), Safe.HangDrag, 1.f);
	UMiningCharacterMovementComponent* Movement = NewObject<UMiningCharacterMovementComponent>();
	Movement->FlightSettings.bEnabled = true;
	Movement->FlightSettings.FallGravityMultiplier = 2;
	TestTrue(TEXT("Editable gravity changes actual descent"), FMath::IsNearlyEqual(Movement->NewFallVelocity(FVector::ZeroVector, FVector(0, 0, -980), 0.1f).Z, -196.0, 0.001));
	TestEqual(TEXT("Full horizontal control remains available in the air"), Movement->GetAirControl(0.1f, 0.5f, FVector(100, 40, 0)), FVector(100, 40, 0));
	return true;
}

#endif
