#include "MiningEncounterPlayer.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningComponent.h"
#include "MiningPrototypeGameMode.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterFlightScopeTest,"BrainMining.Encounters.ScopedFlight",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEncounterFlightScopeTest::RunTest(const FString&)
{
    auto* Move=NewObject<UMiningCharacterMovementComponent>();Move->FlightSettings.bEnabled=true;
    Move->SetFlightInputHeld(true);Move->PushFlightBlock();
    TestFalse(TEXT("Entry cancels held flight"),Move->bFlightInputHeld);
    TestTrue(TEXT("Authored flight remains enabled"),Move->FlightSettings.bEnabled);
    Move->SetFlightInputHeld(true);TestFalse(TEXT("Held space cannot re-enable flight"),Move->bFlightInputHeld);
    Move->PushFlightBlock();Move->PopFlightBlock();TestFalse(TEXT("Nested scope stays blocked"),Move->IsFlightAvailable());
    Move->PopFlightBlock();TestTrue(TEXT("Exit restores flight availability"),Move->IsFlightAvailable());
    TestFalse(TEXT("Exit does not resume stale held input"),Move->bFlightInputHeld);
    Move->SetFlightInputHeld(true);Move->UpdateFlightState(.3f);TestTrue(TEXT("A fresh hold ignites again"),Move->NewFallVelocity(FVector::ZeroVector,FVector(0,0,-980),.1f).Z>0);
    Move->FlightSettings.bEnabled=false;Move->PushFlightBlock();Move->PopFlightBlock();TestFalse(TEXT("Originally disabled flight stays disabled"),Move->IsFlightAvailable());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterPawnReuseTest,"BrainMining.Encounters.SharedPawnAndMiningIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEncounterPawnReuseTest::RunTest(const FString&)
{
    const auto* Mode=GetDefault<AMiningEncounterGameMode>();const auto* MiningMode=GetDefault<AMiningPrototypeGameMode>();
    TestTrue(TEXT("Encounter uses exact mining Blueprint character"),Mode->DefaultPawnClass==MiningMode->DefaultPawnClass);
    TestTrue(TEXT("Encounter controller inherits project input setup"),Mode->PlayerControllerClass->IsChildOf(ABrainMiningPlayerController::StaticClass()));
    auto* Mining=NewObject<UMiningComponent>();Mining->Status=TEXT("untouched");Mining->PushEncounterBlock();
    TestFalse(TEXT("Blocked mining exits before world trace"),Mining->TryMine());
    Mining->ResetField();TestEqual(TEXT("R cannot reset mining world while blocked"),Mining->Status,FString(TEXT("untouched")));
    Mining->PushEncounterBlock();Mining->PopEncounterBlock();TestTrue(TEXT("Nested mining scope stays blocked"),Mining->IsEncounterBlocked());
    Mining->PopEncounterBlock();TestFalse(TEXT("Exit restores mining"),Mining->IsEncounterBlocked());
    return true;
}
#endif
