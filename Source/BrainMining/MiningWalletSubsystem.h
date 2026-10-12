#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MiningWalletSubsystem.generated.h"

UCLASS()
class BRAINMINING_API UMiningWalletSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 Balance = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<int32> UpgradeLevels = {0, 0, 0, 0};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float MineralDoubleChance = 0;
    int32 GetUpgradeLevel(int32 Index) const { return UpgradeLevels.IsValidIndex(Index) ? UpgradeLevels[Index] : INDEX_NONE; }
    int64 CalculateMineralReward(int32 BaseValue, float Roll) const
    {
        return int64(FMath::Max(0, BaseValue)) * (FMath::IsFinite(Roll) && Roll >= 0 && Roll < MineralDoubleChance ? 2 : 1);
    }
    UFUNCTION(BlueprintCallable, Category="Mining|Wallet")
    void AddCoins(int64 Amount) { if (Amount > 0) Balance += FMath::Min(Amount, MAX_int64-Balance); }
    bool Spend(int64 Amount) { if (Amount <= 0 || Amount > Balance) return false; Balance -= Amount; return true; }
};
