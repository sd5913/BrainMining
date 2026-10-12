#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MiningWalletSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FMiningCoinsAdded, int64);

UCLASS()
class BRAINMINING_API UMiningWalletSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    FMiningCoinsAdded OnCoinsAdded;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 Balance = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<int32> UpgradeLevels = {0, 0, 0, 0};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float MineralDoubleChance = 0;
    // Ability ownership follows the game instance, just like upgrade purchases.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Abilities") bool bBombSkillRequiresUnlock = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Abilities") bool bBombSkillUnlocked = false;
    int32 GetUpgradeLevel(int32 Index) const { return UpgradeLevels.IsValidIndex(Index) ? UpgradeLevels[Index] : INDEX_NONE; }
    int64 CalculateMineralReward(int32 BaseValue, float Roll) const
    {
        return int64(FMath::Max(0, BaseValue)) * (FMath::IsFinite(Roll) && Roll >= 0 && Roll < MineralDoubleChance ? 2 : 1);
    }
    UFUNCTION(BlueprintCallable, Category="Mining|Wallet")
    void AddCoins(int64 Amount)
    {
        if (Amount <= 0) return;
        const int64 Credited = FMath::Min(Amount, MAX_int64 - Balance);
        if (Credited <= 0) return;
        Balance += Credited;
        OnCoinsAdded.Broadcast(Credited);
    }
    bool Spend(int64 Amount) { if (Amount <= 0 || Amount > Balance) return false; Balance -= Amount; return true; }
};
