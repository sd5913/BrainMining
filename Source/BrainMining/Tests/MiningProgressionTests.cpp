#include "MiningProgressionComponent.h"
#include "MiningWalletSubsystem.h"
#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningProgressionRulesTest, "BrainMining.Progression.RewardsAndResources", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningProgressionRulesTest::RunTest(const FString& Parameters)
{
    auto* Instance = NewObject<UGameInstance>();
    auto* Wallet = NewObject<UMiningWalletSubsystem>(Instance);
    TestEqual(TEXT("No upgrade never doubles"), Wallet->CalculateMineralReward(20, 0), int64(20));
    Wallet->MineralDoubleChance = .5f;
    TestEqual(TEXT("Below 50 percent roll doubles"), Wallet->CalculateMineralReward(20, .499f), int64(40));
    TestEqual(TEXT("Boundary belongs to normal reward"), Wallet->CalculateMineralReward(20, .5f), int64(20));
    TestEqual(TEXT("Invalid negative value cannot debit player"), Wallet->CalculateMineralReward(-20, 0), int64(0));
    Wallet->Balance = 49;
    TestFalse(TEXT("Unaffordable purchase cannot charge"), Wallet->Spend(50));
    TestEqual(TEXT("Failed charge keeps balance"), Wallet->Balance, int64(49));
    TestFalse(TEXT("Negative price cannot mint coins"), Wallet->Spend(-50));

    UWorld* ResourceWorld = UWorld::CreateWorld(EWorldType::Editor, false);
    AActor* ResourceOwner = ResourceWorld->SpawnActor<AActor>();
    for (int32 FPS : {15, 60, 120})
    {
        auto* Bomb = NewObject<UMiningBombToolComponent>();
        Bomb->bUseInventory = true; Bomb->BombCount = 0; Bomb->MaxBombs = 3; Bomb->RechargeSeconds = 8;
        for (int32 Frame = 0; Frame < FPS*17; ++Frame) Bomb->UpdateInventory(1.f/FPS);
        TestEqual(TEXT("Bomb refill is independent of frame rate"), Bomb->BombCount, 2);
        TestTrue(TEXT("Remainder carries into the next bomb"), FMath::IsNearlyEqual(Bomb->RechargeProgress, .125f, .002f));
        Bomb->UpdateInventory(100);
        TestEqual(TEXT("Refill never exceeds capacity"), Bomb->BombCount, 3);
        TestEqual(TEXT("Full capacity cannot bank refill time"), Bomb->RechargeProgress, 0.f);

        auto* Mining = NewObject<UMiningComponent>(ResourceOwner);
        ResourceOwner->AddInstanceComponent(Mining); Mining->RegisterComponent();
        Mining->bUseDrillEnergy = true; Mining->bDrillMode = true;
        Mining->DrillConsumptionRate = 7.5f; Mining->DrillEnergy = 100;
        Mining->BeginMiningInput();
        for (int32 Frame = 0; Frame < FPS*4; ++Frame) Mining->UpdateDrillEnergy(1.f/FPS);
        TestTrue(TEXT("Upgraded drill consumption is stable"), FMath::IsNearlyEqual(Mining->DrillEnergy, 70.f, .01f));
        Mining->SetInputSuspended(true); Mining->UpdateDrillEnergy(2);
        TestTrue(TEXT("UI suspension does not drain drill"), FMath::IsNearlyEqual(Mining->DrillEnergy, 70.f, .01f));
        Mining->SetInputSuspended(false); Mining->BeginMiningInput(); Mining->UpdateDrillEnergy(20);
        TestFalse(TEXT("Empty energy stows the drill"), Mining->bDrillMode);
        Mining->DrillRecoveryRate = 15; Mining->MaxDrillEnergy = 150;
        Mining->UpdateDrillEnergy(5);
        TestEqual(TEXT("Stored drill recovers at thirty percent of the configured rate"), Mining->DrillEnergy, 22.5f);
        Mining->bDrillMode = true;
        Mining->UpdateDrillEnergy(1);
        TestEqual(TEXT("Equipped idle drill uses the same reduced recovery"), Mining->DrillEnergy, 27.f);
        ResourceWorld->TimeSeconds += 100;
        Mining->TickComponent(0, LEVELTICK_All, nullptr);
        TestTrue(TEXT("Idle drill has no timed expiry"), Mining->bDrillMode);
        TestEqual(TEXT("Energy mode has no countdown"), Mining->GetDrillUseRemaining(), 0.f);
        TestEqual(TEXT("Energy mode has no cooldown"), Mining->GetDrillCooldownRemaining(), 0.f);
        Mining->bInfiniteDrillEnergy = true; Mining->bDrillMode = true; Mining->UpdateDrillEnergy(100);
        TestTrue(TEXT("Infinite upgrade keeps drill active"), Mining->bDrillMode);
        TestEqual(TEXT("Infinite energy stays full"), Mining->DrillEnergy, 150.f);
    }
    ResourceWorld->DestroyWorld(false);

    auto* Flight = NewObject<UMiningCharacterMovementComponent>();
    Flight->FlightSettings.bEnabled = true; Flight->CoolingDelay = .5f;
    Flight->UpdateJetpackHeat(1, true);
    const float HeatBeforeCooling = Flight->JetpackHeat;
    Flight->UpdateJetpackHeat(.25f, false);
    TestEqual(TEXT("Cooling respects its upgraded delay"), Flight->JetpackHeat, HeatBeforeCooling);
    Flight->UpdateJetpackHeat(.5f, false);
    TestTrue(TEXT("Long frame cools only after delay expires"), FMath::IsNearlyEqual(Flight->JetpackHeat, HeatBeforeCooling - .125f));
    Flight->bCreativeFlight = true;
    Flight->UpdateJetpackHeat(30, true);
    TestEqual(TEXT("Creative flight generates no heat"), Flight->JetpackHeat, 0.f);
    Flight->SetFlightInputHeld(false);
    TestEqual(TEXT("Creative flight holds altitude after release"), Flight->NewFallVelocity(FVector(0,0,200), FVector(0,0,-980), .1f).Z, 0.);
    Flight->PushFlightBlock();
    TestFalse(TEXT("Story flight blocks still apply at max level"), Flight->IsFlightAvailable());
    return true;
}
#endif
