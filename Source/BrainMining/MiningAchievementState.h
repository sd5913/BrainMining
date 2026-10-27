#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "MiningAchievementState.generated.h"

struct FMiningAchievementDefinition
{
    FName Id;
    FString TextKey;
};

namespace MiningAchievements
{
    BRAINMINING_API const TArray<FMiningAchievementDefinition>& Definitions();
    BRAINMINING_API bool IsKnown(FName Id);
    BRAINMINING_API const TSet<int32>& RequiredCollection();
}

USTRUCT()
struct BRAINMINING_API FMiningAchievementProgress
{
    GENERATED_BODY()
    UPROPERTY() TSet<FName> Unlocked;
    UPROPERTY() TSet<int32> CollectionIds;
    UPROPERTY() int32 BestPickaxeLevel = 0;
    bool RecordCollection(int32 ItemId);
    bool RecordSavedCollections(const TArray<int32>& Inventory, const TArray<int32>* VerifiedIds);
    bool RecordPickaxe(int32 Level);
    bool Unlock(FName Id);
    bool IsValid() const;
};

UCLASS()
class BRAINMINING_API UMiningAchievementSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Version = 1;
    UPROPERTY() FString OwnerId;
    UPROPERTY() int64 Revision = 0;
    UPROPERTY() FMiningAchievementProgress Progress;
};
