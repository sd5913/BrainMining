#include "MiningProgressionComponent.h"
#include "MiningWalletSubsystem.h"
#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "PopupClose/PopupOfficeDevice.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMaterialRecipeTest, "BrainMining.MaterialProgression.RecipeValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningMaterialRecipeTest::RunTest(const FString&)
{
    auto* Settings = NewObject<UMiningPickaxeSettings>();
    Settings->Levels = UMiningPickaxeSettings::MakeRecommendedLevels();
    FString Error;
    Settings->Levels[1].MaterialCosts[0].Count = 0;
    TestFalse(TEXT("Zero material cost is invalid"), Settings->Validate(Error));
    Settings->Levels = UMiningPickaxeSettings::MakeRecommendedLevels();
    Settings->Levels[1].MaterialCosts[1].Mineral = EMiningMineral::Copper;
    TestFalse(TEXT("Duplicate mineral rows are invalid"), Settings->Validate(Error));
    Settings->Levels = UMiningPickaxeSettings::MakeRecommendedLevels();
    Settings->Levels[1].MaterialCosts.Reset();
    TestFalse(TEXT("Empty paid-level recipe is invalid"), Settings->Validate(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMaterialProgressionTest, "BrainMining.MaterialProgression.AtomicUpgrade", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningMaterialProgressionTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    auto* Instance = NewObject<UGameInstance>(GEngine);
    auto& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(World); Context.OwningGameInstance = Instance;
    World->SetGameInstance(Instance); Instance->Init();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    auto* Wallet = Instance->GetSubsystem<UMiningWalletSubsystem>();
    Wallet->bUseMineralInventory = true;
    Wallet->AddCoins(99999);
    TestEqual(TEXT("Legacy and office coin grants are disabled"), Wallet->Balance, int64(0));
    auto* Printer = World->SpawnActor<APopupOfficeDevice>();
    Printer->DeviceIndex = 2;
    Printer->AddProblem();
    TestTrue(TEXT("Office work still clears a real pending paper"), Printer->ProcessPaper());
    TestEqual(TEXT("Printer work no longer awards coins"), Wallet->Balance, int64(0));
    TestEqual(TEXT("Printer work does not create mining materials"), Wallet->GetMineralCount(EMiningMineral::Copper), 0);
    TestFalse(TEXT("Removed mineral cannot enter inventory"), Wallet->AddMineral(TEXT("1")));
    TestFalse(TEXT("Negative pickup cannot debit inventory"), Wallet->AddMineral(TEXT("2"), -1));
    auto* Owner = World->SpawnActor<AActor>();
    auto* Mining = NewObject<UMiningComponent>(Owner); Owner->AddInstanceComponent(Mining);
    auto* Flight = NewObject<UMiningCharacterMovementComponent>(Owner); Owner->AddInstanceComponent(Flight);
    auto* Bomb = NewObject<UMiningBombToolComponent>(Owner); Owner->AddInstanceComponent(Bomb);
    auto* Progression = NewObject<UMiningProgressionComponent>(Owner); Owner->AddInstanceComponent(Progression);
    TestTrue(TEXT("Initialize"), Progression->InitializeProgression());
    TestEqual(TEXT("Only the pickaxe track survives authored defaults"), Progression->Tracks.Num(), 1);
    Wallet->AddMineral(TEXT("2"), 28); Wallet->AddMineral(TEXT("4"), 1);
    TestFalse(TEXT("Missing iron rejects whole recipe"), Progression->Purchase(0, 1));
    TestEqual(TEXT("Failed purchase preserves copper"), Wallet->GetMineralCount(EMiningMineral::Copper), 28);
    Wallet->AddMineral(TEXT("4"), 19); Wallet->AddMineral(TEXT("6"), 6); Wallet->AddMineral(TEXT("9"), 2);
    const auto FlightSettings = Flight->FlightSettings;
    const auto BombSettings = Bomb->Settings;
    const auto DrillCapacity = Mining->MaxDrillEnergy;
    for (int32 Track = 1; Track < 4; ++Track) TestFalse(TEXT("Removed upgrades cannot be purchased"), Progression->Purchase(Track, 1));
    TestFalse(TEXT("Cannot skip a level"), Progression->Purchase(0, 2));
    for (int32 Level = 1; Level <= 4; ++Level)
    {
        TestTrue(TEXT("Recipe can be paid"), Progression->Purchase(0, Level));
        TestFalse(TEXT("Duplicate input cannot buy the same level twice"), Progression->Purchase(0, Level));
        TestEqual(TEXT("Destruction effects preserved"), Mining->PickaxeBalance.DestructionLevel, Level);
    }
    for (const auto Type : {EMiningMineral::Copper, EMiningMineral::Iron, EMiningMineral::Malachite, EMiningMineral::Diamond})
        TestEqual(TEXT("Exact total materials consumed"), Wallet->GetMineralCount(Type), 0);
    TestFalse(TEXT("Max level cannot consume materials"), Progression->Purchase(0, 5));
    Progression->ApplyUpgrades(); Progression->ApplyUpgrades();
    TestEqual(TEXT("Drill capacity unaffected"), Mining->MaxDrillEnergy, DrillCapacity);
    TestEqual(TEXT("Jetpack capacity unaffected"), Flight->FlightSettings.HeatUpSeconds, FlightSettings.HeatUpSeconds);
    TestEqual(TEXT("Bomb radius unaffected"), Bomb->Settings.BlastRadius, BombSettings.BlastRadius);
    Wallet->AddMineral(TEXT("2"), 10);
    TArray<FMiningMineralCost> Invalid = UMiningPickaxeSettings::ResolveLevel(1).MaterialCosts;
    Invalid[1] = Invalid[0];
    TestFalse(TEXT("Duplicate recipe rows rejected atomically"), Wallet->SpendMinerals(Invalid));
    TestEqual(TEXT("Invalid recipe preserves stack"), Wallet->GetMineralCount(EMiningMineral::Copper), 10);
    return true;
}
#endif
