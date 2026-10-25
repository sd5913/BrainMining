#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "MiningMobileGraphicsSettings.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMobileGraphicsPolicyTest, "BrainMining.Mobile.GraphicsPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningMobileGraphicsPolicyTest::RunTest(const FString&)
{
    using Mode = EMiningMobileUpscaling;
    for (Mode Choice : {Mode::Auto, Mode::Off, Mode::Quality, Mode::Balanced, Mode::Performance})
    {
        const auto Fallback = UMiningMobileGraphicsSettings::Resolve(Choice, false, 80.f, 0, false);
        TestFalse(TEXT("Unsupported hardware never requests ASR"), Fallback.bUseASR);
        TestEqual(TEXT("Fallback preserves device resolution"), Fallback.ScreenPercentage, 80.f);
        TestEqual(TEXT("Fallback preserves game AA"), Fallback.AntiAliasing, 0);
    }
    const auto Automatic = UMiningMobileGraphicsSettings::Resolve(Mode::Auto, true, 90.f, 0, false);
    const auto Quality = UMiningMobileGraphicsSettings::Resolve(Mode::Quality, true, 90.f, 0, false);
    const auto Balanced = UMiningMobileGraphicsSettings::Resolve(Mode::Balanced, true, 90.f, 0, false);
    const auto Performance = UMiningMobileGraphicsSettings::Resolve(Mode::Performance, true, 90.f, 0, false);
    TestTrue(TEXT("Auto chooses supported ASR"), Automatic.bUseASR);
    TestEqual(TEXT("Auto chooses quality first"), Automatic.ScreenPercentage, Quality.ScreenPercentage);
    TestTrue(TEXT("Presets progressively reduce scene pixels"), Quality.ScreenPercentage < 90.f && Quality.ScreenPercentage > Balanced.ScreenPercentage && Balanced.ScreenPercentage > Performance.ScreenPercentage);
    TestEqual(TEXT("ASR always receives temporal AA"), Performance.AntiAliasing, 2);
    TestEqual(TEXT("Performance selects its corresponding shader"), Performance.ShaderQuality, 3);
    const auto Off = UMiningMobileGraphicsSettings::Resolve(Mode::Off, true, 0.f, 0, false);
    TestFalse(TEXT("Off disables ASR"), Off.bUseASR);
    TestEqual(TEXT("Off preserves automatic screen percentage sentinel"), Off.ScreenPercentage, 0.f);
    TestEqual(TEXT("Off restores prior AA outside office"), Off.AntiAliasing, 0);
    TestEqual(TEXT("Office retains its AA while ASR is off"), UMiningMobileGraphicsSettings::Resolve(Mode::Off, true, 90.f, 0, true).AntiAliasing, 2);
    TestFalse(TEXT("Corrupt mode cannot activate ASR"), UMiningMobileGraphicsSettings::Resolve(Mode(255), true, 90.f, 0, false).bUseASR);
    TestFalse(TEXT("Subsystem does not activate in Windows Editor"), GetDefault<UMiningMobileGraphicsSettings>()->ShouldCreateSubsystem(GetTransientPackage()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMobileGraphicsPersistenceTest, "BrainMining.Mobile.GraphicsPersistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningMobileGraphicsPersistenceTest::RunTest(const FString&)
{
    using Mode = EMiningMobileUpscaling;
    const FString Filename = FPaths::ProjectSavedDir() / TEXT("Automation/MobileGraphics-") + FGuid::NewGuid().ToString() + TEXT(".ini");
    ON_SCOPE_EXIT { IFileManager::Get().Delete(*Filename); };
    auto* Instance = NewObject<UGameInstance>();
    auto* Settings = NewObject<UMiningMobileGraphicsSettings>(Instance);
    auto* Loaded = NewObject<UMiningMobileGraphicsSettings>(Instance);
    auto* Percentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
    const float Before = Percentage->GetFloat();
    TestEqual(TEXT("Fresh settings default to Auto"), uint8(Settings->GetMode()), uint8(Mode::Auto));
    Loaded->Load(Filename);
    TestEqual(TEXT("Missing settings file defaults to Auto"), uint8(Loaded->GetMode()), uint8(Mode::Auto));
    for (Mode Choice : {Mode::Auto, Mode::Off, Mode::Quality, Mode::Balanced, Mode::Performance})
    {
        TestTrue(TEXT("Valid mode accepted"), Settings->SetMode(Choice));
        Settings->Save(Filename);
        Loaded->Load(Filename);
        TestEqual(TEXT("Mode survives settings reload"), uint8(Loaded->GetMode()), uint8(Choice));
    }
    TestFalse(TEXT("Invalid API input rejected"), Settings->SetMode(Mode(255)));
    TestEqual(TEXT("Invalid input preserves selection"), uint8(Settings->GetMode()), uint8(Mode::Performance));
    FConfigFile Config;
    Config.SetInt64(TEXT("BrainMining.MobileGraphics"), TEXT("Upscaling"), -1);
    Config.Write(Filename);
    Loaded->Load(Filename);
    TestEqual(TEXT("Corrupt file falls back safely"), uint8(Loaded->GetMode()), uint8(Mode::Off));
    TestEqual(TEXT("Uninitialized settings never mutate desktop rendering"), Percentage->GetFloat(), Before);
    return true;
}
#endif
