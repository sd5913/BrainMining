#include "MiningPickaxeSettings.h"
#include "MiningComponent.h"
#include "MiningProgressionComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningWalletSubsystem.h"
#include "Showcase/MiningTreasure.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningGlowRangeTest,"BrainMining.Pickaxe.MineralGlowRange",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningGlowRangeTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto* Item=World->SpawnActor<AMiningTreasure>();
    FMiningTreasureDefinition Definition;Definition.HintRadius=120.f;
    Definition.bBuriedHint=true;
    Item->UpdateGlow(0.f,true);
    TestFalse(TEXT("Unconfigured decal cannot display the engine fallback"),Item->BuriedGlow->IsVisible());
    Item->AttachCampaignHint(Definition);
    if(TestTrue(TEXT("Campaign glow material attached"),!Item->HintMaterials.IsEmpty()))
    {
        Item->ApplyGlowRadius(1.25f);
        TestTrue(TEXT("Decal volume shrinks immediately"),Item->BuriedGlow->DecalSize.Equals(FVector(150.f)));
        TestEqual(TEXT("Material radius matches decal bounds"),Item->HintMaterials[0]->K2_GetScalarParameterValue(TEXT("HintRadius")),150.f);
        Item->ApplyGlowRadius(2.f);
        TestEqual(TEXT("Restoring saved range updates existing material"),Item->HintMaterials[0]->K2_GetScalarParameterValue(TEXT("HintRadius")),240.f);
        Item->ApplyGlowRadius(std::numeric_limits<float>::quiet_NaN());
        TestTrue(TEXT("Invalid runtime range remains finite"),Item->BuriedGlow->DecalSize.Equals(FVector(240.f)));
        Item->UpdateGlow(0.f,true);
        TestTrue(TEXT("Valid buried glow is visible"),Item->BuriedGlow->IsVisible());
        Item->UpdateGlow(0.f,false);
        TestFalse(TEXT("Room lighting suppression still hides the glow"),Item->BuriedGlow->IsVisible());
        Item->BuriedGlow->SetDecalMaterial(nullptr);
        Item->UpdateGlow(0.f,true);
        TestFalse(TEXT("Missing material never displays a bright fallback rectangle"),Item->BuriedGlow->IsVisible());
    }
    World->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningPickaxeBalanceValidationTest, "BrainMining.Pickaxe.BalanceValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningPickaxeBalanceValidationTest::RunTest(const FString&)
{
    auto* Settings = NewObject<UMiningPickaxeSettings>();
    Settings->Levels = UMiningPickaxeSettings::MakeRecommendedLevels();
    FString Error;
    TestTrue(TEXT("Recommended levels are valid"), Settings->Validate(Error));
    TestTrue(TEXT("Project config is valid"), GetDefault<UMiningPickaxeSettings>()->Validate(Error));
    const float OriginalGlow=Settings->MineralGlowRadiusMultiplier;
    Settings->MineralGlowRadiusMultiplier=std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Nonfinite glow range is rejected"),Settings->Validate(Error));
    Settings->MineralGlowRadiusMultiplier=.1f;
    TestFalse(TEXT("Out of range glow is rejected"),Settings->Validate(Error));
    Settings->MineralGlowRadiusMultiplier=OriginalGlow;
    const auto Defaults = Settings->Levels;
    Settings->Levels[0].Stages[1].DepthPercent = 20;
    TestFalse(TEXT("Backward depth is rejected"), Settings->Validate(Error));
    Settings->Levels = Defaults;
    Settings->Levels[4].Stages.Last().WidthPercent = 99;
    TestFalse(TEXT("Incomplete final opening is rejected"), Settings->Validate(Error));
    Settings->Levels = Defaults;
    Settings->Levels[1].Price = 0;
    TestFalse(TEXT("Free upgrades are rejected"), Settings->Validate(Error));
    Settings->Levels = Defaults;
    Settings->Levels[2].Depth = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Nonfinite geometry is rejected"), Settings->Validate(Error));
    Settings->Levels = Defaults;
    Settings->Levels[3].Stages.Reset();
    TestFalse(TEXT("Empty hit sequence is rejected"), Settings->Validate(Error));
    Settings->Levels = Defaults;
    Settings->Levels.RemoveAt(4);
    TestFalse(TEXT("Missing level is rejected"), Settings->Validate(Error));
    auto* Mining = NewObject<UMiningComponent>();
    TestTrue(TEXT("Valid profile applies"), Mining->ApplyPickaxeBalance(Defaults[4]));
    auto Invalid = Defaults[4]; Invalid.Interval = 0;
    TestFalse(TEXT("Invalid profile cannot enter runtime"), Mining->ApplyPickaxeBalance(Invalid));
    TestEqual(TEXT("Failed edit retains current timing"), Mining->GetMiningInterval(), Defaults[4].Interval);
    Mining->PickaxeSizeScale = .7f;
    TestTrue(TEXT("Character size still scales pickaxe width"), FMath::IsNearlyEqual(Mining->GetMiningRadius(), Defaults[4].Width * .5f * .7f));
    Mining->Radius = 77; Mining->Interval = .7f; Mining->bDrillMode = true;
    TestTrue(TEXT("Drill radius stays independent"), FMath::IsNearlyEqual(Mining->GetMiningRadius(), 77 * Mining->DrillRadiusMultiplier));
    TestTrue(TEXT("Drill interval stays independent"), FMath::IsNearlyEqual(Mining->GetMiningInterval(), .7f * Mining->DrillIntervalMultiplier));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningPickaxeBalancePurchaseTest, "BrainMining.Pickaxe.BalancePurchaseAndReapply", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningPickaxeBalancePurchaseTest::RunTest(const FString&)
{
    auto* Settings = GetMutableDefault<UMiningPickaxeSettings>();
    const auto Original = Settings->Levels;
    Settings->Levels = UMiningPickaxeSettings::MakeRecommendedLevels();
    Settings->Levels[1].Price = 123;
    Settings->Levels[0].Depth = 333;
    Settings->Levels[1].Description = TEXT("测试价格与深度");
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    auto* Instance = NewObject<UGameInstance>(GEngine);
    auto& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(World);
    Context.OwningGameInstance = Instance;
    World->SetGameInstance(Instance);
    Instance->Init();
    ON_SCOPE_EXIT
    {
        Settings->Levels = Original;
        Instance->Shutdown();
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
    };
    auto* Owner = World->SpawnActor<AActor>();
    auto* Mining = NewObject<UMiningComponent>(Owner); Owner->AddInstanceComponent(Mining);
    auto* Flight = NewObject<UMiningCharacterMovementComponent>(Owner); Owner->AddInstanceComponent(Flight);
    auto* Bomb = NewObject<UMiningBombToolComponent>(Owner); Owner->AddInstanceComponent(Bomb);
    auto* Progression = NewObject<UMiningProgressionComponent>(Owner); Owner->AddInstanceComponent(Progression);
    auto* Wallet = Instance->GetSubsystem<UMiningWalletSubsystem>();
    if (!TestNotNull(TEXT("Wallet initialized"), Wallet)) return false;
    TestTrue(TEXT("Progression initialized"), Progression->InitializeProgression());
    Progression->Tracks[0].Steps[0].Price = 99999;
    TestEqual(TEXT("Shop reads central price, not stale blueprint track"), Progression->GetNextPrice(0), int64(123));
    TestEqual(TEXT("Shop reads central description"), Progression->GetNextDescription(0).ToString(), FString(TEXT("测试价格与深度")));
    Wallet->Balance = 122;
    TestFalse(TEXT("Insufficient balance rejects purchase"), Progression->Purchase(0, 1));
    TestEqual(TEXT("Rejected purchase preserves balance"), Wallet->Balance, int64(122));
    Wallet->Balance = 100000;
    TestTrue(TEXT("Configured first upgrade buys"), Progression->Purchase(0, 1));
    TestEqual(TEXT("Configured price charged exactly once"), Wallet->Balance, int64(99877));
    TestEqual(TEXT("Configured depth reaches runtime"), Mining->PickaxeBalance.Depth, 333.f);
    TestFalse(TEXT("Duplicate upgrade rejected"), Progression->Purchase(0, 1));
    TestFalse(TEXT("Skipping upgrade rejected"), Progression->Purchase(0, 3));
    for (int32 Level = 2; Level <= 4; ++Level) TestTrue(TEXT("Next level buys"), Progression->Purchase(0, Level));
    TestEqual(TEXT("Every strength-based swing is an independent cut"), Mining->PickaxeBalance.Stages.Num(), 1);
    TestEqual(TEXT("Old global coin bonus is removed"), Wallet->MineralDoubleChance, 0.f);
    TestEqual(TEXT("Full price sum charged"), Wallet->Balance, int64(43877));
    TestEqual(TEXT("Full level has no next price"), Progression->GetNextPrice(0), int64(0));
    Progression->ApplyUpgrades();
    Progression->ApplyUpgrades();
    TestEqual(TEXT("Reapply does not compound width"), Mining->PickaxeBalance.Width, Settings->Levels[4].Width);
    TestEqual(TEXT("Pickaxe upgrades do not resize drill base"), Mining->Radius, 160.f);
    TestEqual(TEXT("Pickaxe upgrades do not speed up drill base"), Mining->Interval, .3f);
    return true;
}
#endif
