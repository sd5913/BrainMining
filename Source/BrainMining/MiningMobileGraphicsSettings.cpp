#include "MiningMobileGraphicsSettings.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "RHI.h"
#if BRAINMINING_WITH_ARM_ASR
#include "ArmASR.h"
#endif

namespace
{
    FString SettingsFile() { return FPaths::ProjectSavedDir() / TEXT("Config/BrainMiningMobileGraphics.ini"); }
    void SetGraphicsVariable(const TCHAR* Name, float Value)
    {
        if (auto* Variable = IConsoleManager::Get().FindConsoleVariable(Name)) Variable->SetWithCurrentPriority(Value);
    }
}

bool UMiningMobileGraphicsSettings::ShouldCreateSubsystem(UObject* Outer) const
{
    return PLATFORM_ANDROID && Super::ShouldCreateSubsystem(Outer);
}

bool UMiningMobileGraphicsSettings::IsASRSupported() const
{
#if PLATFORM_ANDROID && BRAINMINING_WITH_ARM_ASR
    return FArmASRModule::IsMobileSupported();
#else
    return false;
#endif
}

void UMiningMobileGraphicsSettings::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (auto* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))) OriginalScreenPercentage = Variable->GetFloat();
    if (auto* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"))) OriginalAntiAliasing = Variable->GetInt();
    if (auto* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.AntiAliasing"))) OriginalMobileAntiAliasing = Variable->GetInt();
    if (auto* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TemporalAA.Upsampling"))) OriginalTemporalUpsampling = Variable->GetInt();
    Load(SettingsFile());
    bInitialized = true;
    Apply();
}

FMiningMobileGraphicsPolicy UMiningMobileGraphicsSettings::Resolve(EMiningMobileUpscaling Requested, bool bSupported, float OriginalPercentage, int32 OriginalAA, bool bOfficeAA)
{
    FMiningMobileGraphicsPolicy Policy;
    Policy.ScreenPercentage = OriginalPercentage;
    Policy.AntiAliasing = bOfficeAA ? 2 : OriginalAA;
    if (!bSupported || Requested == EMiningMobileUpscaling::Off || uint8(Requested) > uint8(EMiningMobileUpscaling::Performance)) return Policy;
    Policy.bUseASR = true;
    Policy.AntiAliasing = 2;
    Policy.ScreenPercentage = 100.f / 1.5f;
    if (Requested == EMiningMobileUpscaling::Balanced)
    {
        Policy.ScreenPercentage = 100.f / 1.7f;
        Policy.ShaderQuality = 2;
    }
    else if (Requested == EMiningMobileUpscaling::Performance)
    {
        Policy.ScreenPercentage = 50.f;
        Policy.ShaderQuality = 3;
    }
    return Policy;
}

bool UMiningMobileGraphicsSettings::SetMode(EMiningMobileUpscaling NewMode)
{
    if (uint8(NewMode) > uint8(EMiningMobileUpscaling::Performance)) return false;
    const bool bChanged = Mode != NewMode;
    Mode = NewMode;
    Apply();
    if (bInitialized && bChanged && GetWorld())
        if (auto* Player = GetWorld()->GetFirstPlayerController(); Player && Player->PlayerCameraManager)
            Player->PlayerCameraManager->SetGameCameraCutThisFrame();
    if (bInitialized) Save(SettingsFile());
    return true;
}

void UMiningMobileGraphicsSettings::SetOfficeAntiAliasing(bool bEnabled)
{
    if (bOfficeAntiAliasing == bEnabled) return;
    bOfficeAntiAliasing = bEnabled;
    Apply();
}

void UMiningMobileGraphicsSettings::Apply()
{
    if (!bInitialized) return;
    const auto Policy = Resolve(Mode, IsASRSupported(), OriginalScreenPercentage, OriginalAntiAliasing, bOfficeAntiAliasing);
    SetGraphicsVariable(TEXT("r.ArmASR.Enable"), Policy.bUseASR ? 1 : 0);
    SetGraphicsVariable(TEXT("r.ArmASR.ShaderQuality"), Policy.ShaderQuality);
    SetGraphicsVariable(TEXT("r.ScreenPercentage"), Policy.ScreenPercentage);
    SetGraphicsVariable(TEXT("r.AntiAliasingMethod"), Policy.AntiAliasing);
    SetGraphicsVariable(TEXT("r.Mobile.AntiAliasing"), Policy.bUseASR ? 2 : OriginalMobileAntiAliasing);
    SetGraphicsVariable(TEXT("r.TemporalAA.Upsampling"), Policy.bUseASR ? 1 : OriginalTemporalUpsampling);
}

void UMiningMobileGraphicsSettings::Load(const FString& Filename)
{
    FConfigFile Config;
    Config.Read(Filename);
    int32 Stored = int32(EMiningMobileUpscaling::Auto);
    Config.GetInt(TEXT("BrainMining.MobileGraphics"), TEXT("Upscaling"), Stored);
    Mode = Stored >= 0 && Stored <= int32(EMiningMobileUpscaling::Performance) ? EMiningMobileUpscaling(Stored) : EMiningMobileUpscaling::Off;
}

void UMiningMobileGraphicsSettings::Save(const FString& Filename) const
{
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    FConfigFile Config;
    Config.Read(Filename);
    Config.SetInt64(TEXT("BrainMining.MobileGraphics"), TEXT("Upscaling"), int32(Mode));
    Config.Write(Filename);
}

void UMiningMobileGraphicsSettings::Deinitialize()
{
    if (bInitialized)
    {
        SetGraphicsVariable(TEXT("r.ArmASR.Enable"), 0);
        SetGraphicsVariable(TEXT("r.ScreenPercentage"), OriginalScreenPercentage);
        SetGraphicsVariable(TEXT("r.AntiAliasingMethod"), OriginalAntiAliasing);
        SetGraphicsVariable(TEXT("r.Mobile.AntiAliasing"), OriginalMobileAntiAliasing);
        SetGraphicsVariable(TEXT("r.TemporalAA.Upsampling"), OriginalTemporalUpsampling);
        bInitialized = false;
    }
    Super::Deinitialize();
}
