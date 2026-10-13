#include "MiningComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningToolModeSettingsTest, "BrainMining.Tools.DrillSessionAndCooldown", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningToolModeSettingsTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	AActor* Owner = World->SpawnActor<AActor>();
	UMiningComponent* Mining = NewObject<UMiningComponent>(Owner);
	Owner->AddInstanceComponent(Mining);
	Mining->RegisterComponent();
	Mining->bUseDrillEnergy = false; // Explicit legacy compatibility; energy mode tested separately.
	Mining->Interval = 0.5f;
	Mining->Radius = 120;
	TestEqual(TEXT("Original per-level pickaxe interval retained"), Mining->GetMiningInterval(), 0.5f);
	TestEqual(TEXT("Original per-level pickaxe radius retained"), Mining->GetMiningRadius(), 120.f);
	Mining->SetInputSuspended(true);
	TestFalse(TEXT("Tab blocks activating a drill behind the upgrade panel"), Mining->ActivateDrill());
	Mining->SetInputSuspended(false);
	TestTrue(TEXT("Ready drill can activate"), Mining->ActivateDrill());
	TestTrue(TEXT("Activation selects drill"), Mining->bDrillMode);
	TestEqual(TEXT("Activation starts ten-second use window"), Mining->GetDrillUseRemaining(), 10.f);
	TestEqual(TEXT("No cooldown while active"), Mining->GetDrillCooldownRemaining(), 0.f);
	TestFalse(TEXT("Pressing V again cannot extend a session"), Mining->ActivateDrill());
	TestTrue(TEXT("Drill has 2.5 times the attempt frequency"), FMath::IsNearlyEqual(Mining->GetMiningInterval(), 0.2f));
	TestTrue(TEXT("Drill cuts a larger volume"), FMath::IsNearlyEqual(Mining->GetMiningRadius(), 144.f));
	TestEqual(TEXT("Selection never starts mining itself"), Mining->IsMiningActive(), false);
	World->TimeSeconds = 10;
	Mining->TickComponent(0, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Drill automatically returns to pickaxe after ten seconds"), Mining->bDrillMode);
	TestEqual(TEXT("Cooldown starts at ten seconds"), Mining->GetDrillCooldownRemaining(), 10.f);
	TestFalse(TEXT("Drill cannot be activated during cooldown"), Mining->ActivateDrill());
	TestEqual(TEXT("Pickaxe timing restored without changing stored settings"), Mining->GetMiningInterval(), 0.5f);
	TestEqual(TEXT("Pickaxe radius restored without changing stored settings"), Mining->GetMiningRadius(), 120.f);
	Mining->Interval = std::numeric_limits<float>::quiet_NaN();
	Mining->Radius = std::numeric_limits<float>::quiet_NaN();
	Mining->DrillIntervalMultiplier = std::numeric_limits<float>::quiet_NaN();
	Mining->DrillRadiusMultiplier = std::numeric_limits<float>::quiet_NaN();
	World->TimeSeconds = 19.9;
	TestFalse(TEXT("Cooldown cannot be bypassed just before expiry"), Mining->ActivateDrill());
	World->TimeSeconds = 20;
	TestTrue(TEXT("Drill becomes available exactly at cooldown end"), Mining->ActivateDrill());
	TestTrue(TEXT("Invalid designer values retain finite timing"), FMath::IsFinite(Mining->GetMiningInterval()) && Mining->GetMiningInterval() >= 0.04f);
	TestTrue(TEXT("Invalid designer values retain finite geometry radius"), FMath::IsFinite(Mining->GetMiningRadius()) && Mining->GetMiningRadius() > 0);
	World->TimeSeconds = 45;
	Mining->TickComponent(0, LEVELTICK_All, nullptr);
	TestEqual(TEXT("A long frame cannot extend cooldown beyond the deadline"), Mining->GetDrillCooldownRemaining(), 0.f);
	World->DestroyWorld(false);
	return true;
}
#endif
