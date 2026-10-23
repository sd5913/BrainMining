#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MiningMineralInventory.h"
#include "MiningWalletSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FMiningCoinsAdded, int64);

UCLASS()
class BRAINMINING_API UMiningWalletSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Inventory") bool bUseMineralInventory = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Inventory") TMap<EMiningMineral, int32> Minerals;
    // Actual pickups this run, independent of the spendable balance and GM grants.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Inventory") TMap<EMiningMineral, int32> CollectedMineralTotals;
    UFUNCTION(BlueprintPure, Category="Mining|Inventory") bool UsesMineralInventory() const;
    UFUNCTION(BlueprintCallable, Category="Mining|Inventory") bool AddMineral(FName TreasureId, int32 Amount = 1);
    UFUNCTION(BlueprintPure, Category="Mining|Inventory") int32 GetMineralCount(EMiningMineral Mineral) const { return Minerals.FindRef(Mineral); }
    bool CanSpendMinerals(const TArray<FMiningMineralCost>& Costs) const;
    bool SpendMinerals(const TArray<FMiningMineralCost>& Costs);
    FMiningCoinsAdded OnCoinsAdded;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 Balance = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<int32> UpgradeLevels = {0, 0, 0, 0};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float MineralDoubleChance = 0;
    // Ability ownership follows the game instance, just like upgrade purchases.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Abilities") bool bBombSkillRequiresUnlock = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Abilities") bool bBombSkillUnlocked = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Abilities") bool bDrillSkillRequiresUnlock = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Abilities") bool bDrillSkillUnlocked = false;
    int32 GetUpgradeLevel(int32 Index) const { return UpgradeLevels.IsValidIndex(Index) ? UpgradeLevels[Index] : INDEX_NONE; }
    int64 CalculateMineralReward(int32 BaseValue, float Roll) const
    {
        return int64(FMath::Max(0, BaseValue)) * (FMath::IsFinite(Roll) && Roll >= 0 && Roll < MineralDoubleChance ? 2 : 1);
    }
    UFUNCTION(BlueprintCallable, Category="Mining|Wallet")
    void AddCoins(int64 Amount)
    {
        if (UsesMineralInventory() || Amount <= 0) return;
        const int64 Credited = FMath::Min(Amount, MAX_int64 - Balance);
        if (Credited <= 0) return;
        Balance += Credited;
        OnCoinsAdded.Broadcast(Credited);
    }
    bool Spend(int64 Amount) { if (Amount <= 0 || Amount > Balance) return false; Balance -= Amount; return true; }
};
