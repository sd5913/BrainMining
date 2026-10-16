#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MiningBombToolComponent.h"
#include "MiningFlightSettings.h"
#include "MiningProgressionComponent.generated.h"

class UMiningWalletSubsystem;

UENUM(BlueprintType)
enum class EMiningUpgradeEffect : uint8
{
    MiningDepth, MiningRadius, MiningInterval, MineralDoubleChance,
    CoolingDelay, CoolingRate, HeatCapacity, CreativeFlight,
    BombRecharge, BombCapacity, BombRadius, BombCapacityAndFuse,
    DrillRecovery, DrillCapacity, DrillConsumption, InfiniteDrill, PickaxeProfile
};

USTRUCT(BlueprintType)
struct FMiningUpgradeStep
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int64 Price = 50;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EMiningUpgradeEffect Effect = EMiningUpgradeEffect::MiningDepth;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01")) float Value = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01")) float SecondaryValue = 1;
    bool IsValid() const;
};

USTRUCT(BlueprintType)
struct FMiningUpgradeTrack
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMiningUpgradeStep> Steps;
};

UCLASS(ClassGroup=(Mining), meta=(BlueprintSpawnableComponent))
class BRAINMINING_API UMiningProgressionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UMiningProgressionComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Progression") TArray<FMiningUpgradeTrack> Tracks;
    UFUNCTION(BlueprintCallable, Category="Progression") bool InitializeProgression();
    UFUNCTION(BlueprintCallable, Category="Progression") bool Purchase(int32 TrackIndex, int32 RequestedLevel);
    UFUNCTION(BlueprintCallable, Category="Progression") void ApplyUpgrades();
    UFUNCTION(BlueprintPure, Category="Progression") int32 GetLevel(int32 TrackIndex) const;
    UFUNCTION(BlueprintPure, Category="Progression") int64 GetNextPrice(int32 TrackIndex) const;
    UFUNCTION(BlueprintPure, Category="Progression") FText GetNextDescription(int32 TrackIndex) const;
    UFUNCTION(BlueprintPure, Category="Progression") UMiningWalletSubsystem* GetWallet() const;
    UFUNCTION(BlueprintPure, Category="Progression") bool UsesMineralInventory() const;
    UFUNCTION(BlueprintPure, Category="Progression") bool CanUpgradePickaxe() const;
    UFUNCTION(BlueprintPure, Category="Progression") FText GetNextMaterialRequirements() const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Progression") bool bInitialized = false;
private:
    void RefreshPickaxeTrack();
    float BaseRadius = 160;
    float BaseDepth = 1;
    float BaseInterval = .3f;
    float BaseCoolingDelay = 1;
    float BaseDrillCapacity = 100;
    float BaseDrillRecovery = 10;
    float BaseDrillConsumption = 10;
    int32 BaseBombCapacity = 3;
    float BaseBombRecharge = 10;
    FMiningFlightSettings BaseFlight;
    FMiningBombSettings BaseBomb;
};
