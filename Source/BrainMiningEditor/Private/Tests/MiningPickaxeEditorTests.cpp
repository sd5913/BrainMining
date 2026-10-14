#include "MiningPickaxeSettings.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningPickaxeConfigPersistenceTest, "BrainMining.Pickaxe.EditorConfigPersistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningPickaxeConfigPersistenceTest::RunTest(const FString&)
{
    TestTrue(TEXT("Pickaxe balance tab is registered"), FGlobalTabmanager::Get()->HasTabSpawner(FName(TEXT("MiningPickaxeBalance"))));
    const FProperty* LevelsProperty = UMiningPickaxeSettings::StaticClass()->FindPropertyByName(TEXT("Levels"));
    if (!TestNotNull(TEXT("Levels are reflected"), LevelsProperty)) return false;
    TestTrue(TEXT("Levels persist to config"), LevelsProperty->HasAllPropertyFlags(CPF_Config | CPF_Edit | CPF_EditFixedSize));
    auto* Draft = NewObject<UMiningPickaxeSettings>();
    Draft->Levels = UMiningPickaxeSettings::MakeRecommendedLevels();
    Draft->Levels[1].Price = 789;
    Draft->Levels[2].Width = 444;
    Draft->Levels[4].Stages[0].DepthPercent = 63;
    Draft->Levels[4].Roughness = .35f;
    Draft->Levels[4].Description = TEXT("测试：两击挖开");
    const auto SavedDefault = GetDefault<UMiningPickaxeSettings>()->Levels;
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/PickaxeBalance"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Filename = FPaths::ConvertRelativePathToFull(FPaths::Combine(Directory, FGuid::NewGuid().ToString() + TEXT(".ini")));
    ON_SCOPE_EXIT { GConfig->UnloadFile(Filename); IFileManager::Get().Delete(*Filename); };
    TestTrue(TEXT("Editor save accepts the target config file"), Draft->TryUpdateDefaultConfigFile(Filename));
    if (!TestTrue(TEXT("Draft writes a real config file"), FPaths::FileExists(Filename))) return false;
    auto* Reloaded = NewObject<UMiningPickaxeSettings>();
    Reloaded->LoadConfig(UMiningPickaxeSettings::StaticClass(), *Filename);
    FString Error;
    if (!TestTrue(TEXT("Reloaded config is valid"), Reloaded->Validate(Error))) return false;
    if (!TestEqual(TEXT("All five levels survive reload"), Reloaded->Levels.Num(), 5)) return false;
    TestEqual(TEXT("Price survives restart"), Reloaded->Levels[1].Price, int64(789));
    TestEqual(TEXT("Width survives restart"), Reloaded->Levels[2].Width, 444.f);
    TestEqual(TEXT("Stage count survives restart"), Reloaded->Levels[4].Stages.Num(), 2);
    TestEqual(TEXT("Stage depth survives restart"), Reloaded->Levels[4].Stages[0].DepthPercent, 63.f);
    TestEqual(TEXT("Randomness survives restart"), Reloaded->Levels[4].Roughness, .35f);
    TestEqual(TEXT("Chinese shop text survives restart"), Reloaded->Levels[4].Description, FString(TEXT("测试：两击挖开")));
    TestTrue(TEXT("Temporary saves leave project defaults intact"), FMiningPickaxeLevel::StaticStruct()->CompareScriptStruct(
        &SavedDefault[1], &GetDefault<UMiningPickaxeSettings>()->Levels[1], 0));
    return true;
}
#endif
