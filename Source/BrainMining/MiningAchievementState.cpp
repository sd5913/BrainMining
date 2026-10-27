#include "MiningAchievementState.h"

namespace MiningAchievements
{
    const TArray<FMiningAchievementDefinition>& Definitions()
    {
        static const TArray<FMiningAchievementDefinition> Entries = {
            {TEXT("ACH_FIRST_COLLECTION"), TEXT("Achievement.FirstCollection")},
            {TEXT("ACH_COLLECTION_5"), TEXT("Achievement.Collection5")},
            {TEXT("ACH_COLLECTION_COMPLETE"), TEXT("Achievement.CollectionComplete")},
            {TEXT("ACH_PICKAXE_UPGRADE"), TEXT("Achievement.PickaxeUpgrade")},
            {TEXT("ACH_PICKAXE_MAX"), TEXT("Achievement.PickaxeMax")},
            {TEXT("ACH_CARD_ROOM_CLEAR"), TEXT("Achievement.CardRoom")},
            {TEXT("ACH_MUSEUM_CLEAR"), TEXT("Achievement.Museum")},
            {TEXT("ACH_CAMPAIGN_COMPLETE"), TEXT("Achievement.Campaign")}
        };
        return Entries;
    }

    bool IsKnown(FName Id)
    {
        return Definitions().ContainsByPredicate([Id](const FMiningAchievementDefinition& Entry) { return Entry.Id == Id; });
    }

    const TSet<int32>& RequiredCollection()
    {
        static const TSet<int32> Required = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22};
        return Required;
    }
}

bool FMiningAchievementProgress::Unlock(FName Id)
{
    if (!MiningAchievements::IsKnown(Id) || Unlocked.Contains(Id)) return false;
    Unlocked.Add(Id);
    return true;
}

bool FMiningAchievementProgress::RecordCollection(int32 ItemId)
{
    if (!MiningAchievements::RequiredCollection().Contains(ItemId) || CollectionIds.Contains(ItemId)) return false;
    CollectionIds.Add(ItemId);
    Unlock(TEXT("ACH_FIRST_COLLECTION"));
    if (CollectionIds.Num() >= 5) Unlock(TEXT("ACH_COLLECTION_5"));
    if (MiningAchievements::RequiredCollection().Difference(CollectionIds).IsEmpty()) Unlock(TEXT("ACH_COLLECTION_COMPLETE"));
    return true;
}

bool FMiningAchievementProgress::RecordSavedCollections(const TArray<int32>& Inventory, const TArray<int32>* VerifiedIds)
{
    bool Updated = false;
    if (VerifiedIds)
    {
        for (int32 Id : *VerifiedIds) Updated |= RecordCollection(Id);
    }
    else
    {
        for (int32 Index = 0; Index < Inventory.Num(); ++Index)
            if (Inventory[Index] > 0) Updated |= RecordCollection(Index);
    }
    return Updated;
}

bool FMiningAchievementProgress::RecordPickaxe(int32 Level)
{
    if (Level < 1 || Level > 4 || Level <= BestPickaxeLevel) return false;
    BestPickaxeLevel = Level;
    Unlock(TEXT("ACH_PICKAXE_UPGRADE"));
    if (Level == 4) Unlock(TEXT("ACH_PICKAXE_MAX"));
    return true;
}

bool FMiningAchievementProgress::IsValid() const
{
    if (BestPickaxeLevel < 0 || BestPickaxeLevel > 4 || CollectionIds.Num() > MiningAchievements::RequiredCollection().Num()) return false;
    for (FName Id : Unlocked) if (!MiningAchievements::IsKnown(Id)) return false;
    for (int32 Id : CollectionIds) if (!MiningAchievements::RequiredCollection().Contains(Id)) return false;
    return true;
}
