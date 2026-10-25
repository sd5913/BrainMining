#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MiningMobileGraphicsSettings.generated.h"

enum class EMiningMobileUpscaling : uint8 { Auto, Off, Quality, Balanced, Performance };

struct FMiningMobileGraphicsPolicy
{
    bool bUseASR = false;
    float ScreenPercentage = 100.f;
    int32 AntiAliasing = 0;
    int32 ShaderQuality = 1;
};

UCLASS()
class BRAINMINING_API UMiningMobileGraphicsSettings : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    EMiningMobileUpscaling GetMode() const { return Mode; }
    bool IsASRSupported() const;
    bool SetMode(EMiningMobileUpscaling NewMode);
    void SetOfficeAntiAliasing(bool bEnabled);
    void Load(const FString& Filename);
    void Save(const FString& Filename) const;
    static FMiningMobileGraphicsPolicy Resolve(EMiningMobileUpscaling Mode, bool bSupported, float OriginalPercentage, int32 OriginalAA, bool bOfficeAA);
private:
    void Apply();
    EMiningMobileUpscaling Mode = EMiningMobileUpscaling::Auto;
    float OriginalScreenPercentage = 100.f;
    int32 OriginalAntiAliasing = 0;
    int32 OriginalMobileAntiAliasing = 2;
    int32 OriginalTemporalUpsampling = 1;
    bool bOfficeAntiAliasing = false;
    bool bInitialized = false;
};
