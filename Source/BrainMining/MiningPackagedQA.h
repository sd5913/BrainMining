#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "MiningPackagedQA.generated.h"

// Opt-in, local packaged regression harness. Never ticks in a normal launch.
UCLASS()
class UMiningPackagedQA : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return bEnabled && !IsTemplate(); }
    virtual bool IsTickableWhenPaused() const override { return bEnabled; }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMiningPackagedQA,STATGROUP_Tickables); }
private:
    bool bEnabled=false;
    FString Directory, LastId, SampleLabel;
    double LastFrame=0, NextPoll=0;
    TArray<double> FrameSamples;
    void Execute(const TSharedPtr<class FJsonObject>& Command);
};
