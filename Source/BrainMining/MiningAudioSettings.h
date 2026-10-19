#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "MiningAudioSettings.generated.h"

class USoundClass;
class USoundMix;

UCLASS()
class BRAINMINING_API UMiningAudioSettings : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return !IsTemplate(); }
    virtual bool IsTickableWhenPaused() const override { return true; }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMiningAudioSettings, STATGROUP_Tickables); }
    float GetVolume(int32 Channel) const;
    void SetVolume(int32 Channel, float Value);
    void RestoreDefaults();
    void LoadVolumes(const FString& Filename);
    void SaveVolumes(const FString& Filename) const;
    void Save() const;
    float GetLookSensitivity() const { return LookSensitivity; }
    void SetLookSensitivity(float Value) { if (FMath::IsFinite(Value)) LookSensitivity=FMath::Clamp(Value,.2f,3.f); }
    USoundClass* GetClass(int32 Channel) const;
private:
    float LookSensitivity=1.f;
    void Apply();
    TArray<float> Volumes = {1.f, 1.f, .85f, .8f, 1.f};
    UPROPERTY(Transient) TArray<TObjectPtr<USoundClass>> Classes;
    UPROPERTY(Transient) TObjectPtr<USoundMix> Mix;
    TWeakObjectPtr<UWorld> AppliedWorld;
};
