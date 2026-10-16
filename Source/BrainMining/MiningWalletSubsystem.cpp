#include "MiningWalletSubsystem.h"
#include "Engine/World.h"

bool UMiningWalletSubsystem::UsesMineralInventory() const
{
    return bUseMineralInventory || (GetWorld() && GetWorld()->GetOutermost()->GetName().Contains(TEXT("/MainGame/Maps/")));
}

bool UMiningWalletSubsystem::AddMineral(FName TreasureId, int32 Amount)
{
    EMiningMineral Mineral;
    if (TreasureId == TEXT("2")) Mineral = EMiningMineral::Copper;
    else if (TreasureId == TEXT("4")) Mineral = EMiningMineral::Iron;
    else if (TreasureId == TEXT("6")) Mineral = EMiningMineral::Malachite;
    else if (TreasureId == TEXT("9")) Mineral = EMiningMineral::Diamond;
    else return false;
    if (Amount <= 0 || GetMineralCount(Mineral) > MAX_int32 - Amount) return false;
    Minerals.FindOrAdd(Mineral) += Amount;
    return true;
}

bool UMiningWalletSubsystem::CanSpendMinerals(const TArray<FMiningMineralCost>& Costs) const
{
    if (Costs.IsEmpty()) return false;
    TSet<EMiningMineral> Seen;
    for (const auto& Cost : Costs)
    {
        if (!FMiningMineralCost::IsSupported(Cost.Mineral) || Cost.Count <= 0 || Seen.Contains(Cost.Mineral)
            || GetMineralCount(Cost.Mineral) < Cost.Count) return false;
        Seen.Add(Cost.Mineral);
    }
    return true;
}

bool UMiningWalletSubsystem::SpendMinerals(const TArray<FMiningMineralCost>& Costs)
{
    // Validate the entire recipe before touching any stack.
    if (!CanSpendMinerals(Costs)) return false;
    for (const auto& Cost : Costs) Minerals.FindOrAdd(Cost.Mineral) -= Cost.Count;
    return true;
}
