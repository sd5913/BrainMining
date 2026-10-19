#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "DeviceProfiles/DeviceProfileManager.h"
#include "IDeviceProfileSelectorModule.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningAndroidCompatibilityTest, "BrainMining.Android.CompatibilityProfiles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningAndroidCompatibilityTest::RunTest(const FString&)
{
    auto* Selector = FModuleManager::LoadModulePtr<IDeviceProfileSelectorModule>(TEXT("AndroidDeviceProfileSelector"));
    if (!TestNotNull(TEXT("Android preview selector is available"), Selector)) return false;
    TMap<FString, FString> Properties = {
        {TEXT("SRC_ResolutionX"), TEXT("1080")}, {TEXT("SRC_ResolutionY"), TEXT("2400")},
        {TEXT("SRC_InsetsLeft"), TEXT("0")}, {TEXT("SRC_InsetsTop"), TEXT("0")},
        {TEXT("SRC_InsetsRight"), TEXT("0")}, {TEXT("SRC_InsetsBottom"), TEXT("0")},
        {TEXT("SRC_GPUFamily"), TEXT("")}, {TEXT("SRC_AndroidVersion"), TEXT("10")},
        {TEXT("SRC_VulkanAvailable"), TEXT("true")}, {TEXT("SRC_SM5Available"), TEXT("false")},
        {TEXT("SRC_DeviceMake"), TEXT("")}, {TEXT("SRC_DeviceModel"), TEXT("")},
        {TEXT("SRC_HMDSystemName"), TEXT("")}, {TEXT("SRC_Hardware"), TEXT("")},
        {TEXT("SRC_Chipset"), TEXT("")}, {TEXT("SRC_GLVersion"), TEXT("OpenGL ES 3.2")},
        {TEXT("SRC_VulkanVersion"), TEXT("1.1.0")}, {TEXT("SRC_UsingHoudini"), TEXT("false")},
        {TEXT("SRC_TotalPhysicalGB"), TEXT("6")}, {TEXT("SRC_DeviceBuildNumber"), TEXT("")},
        {TEXT("SRC_VKQuality"), TEXT("0")}
    };
    auto OriginalProperties = Properties;
    for (auto& Property : OriginalProperties)
    {
        FString Value;
        if (Selector->GetSelectorPropertyValue(FName(*Property.Key), Value)) Property.Value = Value;
    }
    ON_SCOPE_EXIT { Selector->SetSelectorProperties(OriginalProperties); };

    struct FDeviceCase
    {
        const TCHAR* GPU;
        const TCHAR* AndroidVersion;
        bool bVulkan;
        bool bNeedsCompatibility;
    };
    const FDeviceCase Devices[] = {
        {TEXT("Mali-G51"), TEXT("10"), true, true},
        {TEXT("Mali-G76"), TEXT("10"), true, true},
        {TEXT("Mali-G77"), TEXT("10"), true, true},
        {TEXT("Mali-G51"), TEXT("10"), false, true},
        {TEXT("Mali-G76"), TEXT("10"), false, true},
        {TEXT("Mali-G77"), TEXT("10.0"), false, true},
        {TEXT("Mali-G77"), TEXT("11"), true, false},
        {TEXT("Mali-G51"), TEXT("9"), false, false},
        {TEXT("Mali-G76"), TEXT("13"), true, false},
        {TEXT("Adreno (TM) 650"), TEXT("13"), true, false},
        {TEXT("Adreno (TM) 613"), TEXT("15"), true, false},
        {TEXT("Adreno (TM) 650"), TEXT("10"), true, false}
    };
    for (const FDeviceCase& Device : Devices)
    {
        Properties[TEXT("SRC_GPUFamily")] = Device.GPU;
        Properties[TEXT("SRC_AndroidVersion")] = Device.AndroidVersion;
        Properties[TEXT("SRC_VulkanAvailable")] = Device.bVulkan ? TEXT("true") : TEXT("false");
        Selector->SetSelectorProperties(Properties);
        const FString Profile = Selector->GetDeviceProfileName();
        UDeviceProfileManager::Get().FindProfile(Profile, true, TEXT("Android"));
        const auto EffectiveCVars = UDeviceProfileManager::GatherDeviceProfileCVars(Profile, UDeviceProfileManager::EDeviceProfileMode::DPM_CacheValues);
        const auto BaselineCVars = UDeviceProfileManager::GatherDeviceProfileCVars(Profile, UDeviceProfileManager::EDeviceProfileMode::DPM_CacheValuesIgnoreMatchingRules);
        const bool bHasCompatibility = EffectiveCVars.FindRef(TEXT("r.PSOPrecaching")) == TEXT("0");
        TestEqual(FString::Printf(TEXT("%s Android %s Vulkan %d (%s)"), Device.GPU, Device.AndroidVersion, Device.bVulkan, *Profile), bHasCompatibility, Device.bNeedsCompatibility);
        if (Device.bNeedsCompatibility)
        {
            TestEqual(TEXT("Compatibility fragment overrides the selected profile's Vulkan preference"), EffectiveCVars.FindRef(TEXT("r.Android.DisableVulkanSupport")), FString(TEXT("1")));
            TestEqual(TEXT("Compatibility fragment disables Swappy"), EffectiveCVars.FindRef(TEXT("a.UseSwappyForFramePacing")), FString(TEXT("0")));
        }
        else
        {
            TestTrue(TEXT("Unaffected device retains every original profile setting"), EffectiveCVars.OrderIndependentCompareEqual(BaselineCVars));
        }
    }
    const auto CompatibilityCVars = UDeviceProfileManager::GatherDeviceProfileCVars(TEXT("BM_Android10MaliCompatibility"), UDeviceProfileManager::EDeviceProfileMode::DPM_SetCVarsForSingleFragment);
    TestEqual(TEXT("ES fallback is selected"), CompatibilityCVars.FindRef(TEXT("r.Android.DisableVulkanSupport")), FString(TEXT("1")));
    TestEqual(TEXT("PSO precaching is disabled"), CompatibilityCVars.FindRef(TEXT("r.PSOPrecaching")), FString(TEXT("0")));
    TestEqual(TEXT("OpenGL precaching is disabled"), CompatibilityCVars.FindRef(TEXT("r.OpenGL.AllowPSOPrecaching")), FString(TEXT("0")));
    TestEqual(TEXT("Pipeline file precompile is disabled"), CompatibilityCVars.FindRef(TEXT("r.ShaderPipelineCache.Enabled")), FString(TEXT("0")));
    TestEqual(TEXT("Sandbox Swappy loader is avoided"), CompatibilityCVars.FindRef(TEXT("a.UseSwappyForFramePacing")), FString(TEXT("0")));
    return true;
}
#endif
