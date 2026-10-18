#include "MiningCharacterMovementComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningJetpackHeatTest, "BrainMining.Flight.JetpackHeatAndLockout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningJetpackHeatTest::RunTest(const FString& Parameters)
{
	auto* Movement = NewObject<UMiningCharacterMovementComponent>();
	Movement->FlightSettings.bEnabled = true;
	Movement->SetFlightInputHeld(true);
	Movement->UpdateFlightState(.3f);
	const float HeatBudget = Movement->FlightSettings.Sanitized().HeatUpSeconds;
	TestEqual(TEXT("Current jetpack capacity is two and a half seconds"), HeatBudget, 2.5f);
	Movement->UpdateJetpackHeat(HeatBudget * .5f, true);
	TestEqual(TEXT("Half the continuous flight budget fills half the bar"), Movement->JetpackHeat, .5f);
	Movement->UpdateJetpackHeat(.5f, false);
	TestEqual(TEXT("Release cools gradually without discarding accumulated heat"), Movement->JetpackHeat, .25f);
	Movement->Velocity = FVector(70, 20, 450);
	Movement->UpdateJetpackHeat(HeatBudget * .75f, true);
	TestTrue(TEXT("Full heat starts lockout"), Movement->IsJetpackOverheated());
	TestEqual(TEXT("Overheat stops upward momentum"), Movement->Velocity, FVector(70, 20, 0));
	TestFalse(TEXT("Holding or repressing cannot thrust through lockout"), Movement->IsFlightThrustActive());
	Movement->SetFlightInputHeld(false);
	Movement->SetFlightInputHeld(true);
	Movement->UpdateFlightState(.3f);
	TestTrue(TEXT("Overheated character falls using existing gravity"), Movement->NewFallVelocity(FVector::ZeroVector, FVector(0, 0, -980), .1f).Z < 0);
	Movement->UpdateJetpackHeat(1.f, false);
	TestTrue(TEXT("Cooling reduces heat to two thirds while flight stays locked"), FMath::IsNearlyEqual(Movement->JetpackHeat, 2.f / 3.f));
	Movement->UpdateJetpackHeat(1.f, true);
	TestTrue(TEXT("Lockout still active after two seconds regardless of input"), Movement->IsJetpackOverheated());
	TestTrue(TEXT("Locked bar retreats to one third after two seconds"), FMath::IsNearlyEqual(Movement->JetpackHeat, 1.f / 3.f));
	Movement->UpdateJetpackHeat(1.f, true);
	TestFalse(TEXT("Lockout ends after exactly three seconds"), Movement->IsJetpackOverheated());
	TestEqual(TEXT("Recovery empties the bar"), Movement->JetpackHeat, 0.f);
	TestTrue(TEXT("Held Space can resume flight after recovery"), Movement->IsFlightThrustActive());
	Movement->UpdateJetpackHeat(HeatBudget + Movement->FlightSettings.OverheatLockSeconds + HeatBudget * .2f, true);
	TestTrue(TEXT("A long frame crosses the heat/cooldown boundary correctly"), FMath::IsNearlyEqual(Movement->JetpackHeat, .2f));
	Movement->FlightSettings.bEnableHeat = false;
	Movement->UpdateJetpackHeat(1.f, true);
	TestEqual(TEXT("Disabling heat retains original unlimited hold flight"), Movement->JetpackHeat, 0.f);
	TestTrue(TEXT("Flight remains active with optional heat disabled"), Movement->IsFlightThrustActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningJetpackFrameTest, "BrainMining.Flight.JetpackFrameRates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningJetpackFrameTest::RunTest(const FString& Parameters)
{
	for (int32 FPS : {15, 30, 60, 120})
	{
		auto* Movement = NewObject<UMiningCharacterMovementComponent>();
		Movement->FlightSettings.bEnabled = true;
		const float HeatBudget = Movement->FlightSettings.Sanitized().HeatUpSeconds;
		auto Advance = [Movement, FPS](float Seconds)
		{
			for (float Remaining = Seconds; Remaining > SMALL_NUMBER;)
			{
				const float Step = FMath::Min(Remaining, 1.f / FPS);
				Movement->UpdateJetpackHeat(Step, true);
				Remaining -= Step;
			}
		};
		Advance(HeatBudget + 1.f);
		TestTrue(TEXT("All frame rates show the same cooldown progress"), Movement->IsJetpackOverheated() && FMath::IsNearlyEqual(Movement->JetpackHeat, 2.f / 3.f, .001f));
		TestTrue(TEXT("Lockout clock is frame-rate independent"), FMath::IsNearlyEqual(Movement->JetpackOverheatRemaining, 2.f, .001f));
		Advance(2.f + HeatBudget * .2f);
		TestFalse(TEXT("All frame rates recover and start the next heat cycle"), Movement->IsJetpackOverheated());
		TestTrue(TEXT("Next cycle has correct accumulated heat"), FMath::IsNearlyEqual(Movement->JetpackHeat, .2f, .001f));
	}
	return true;
}
#endif
