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
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 PickaxeLevel = 0;
    void AddCoins(int64 Amount) { if (Amount > 0) Balance += FMath::Min(Amount, MAX_int64-Balance); }
    bool Spend(int64 Amount) { if (Amount <= 0 || Amount > Balance) return false; Balance -= Amount; return true; }
};
