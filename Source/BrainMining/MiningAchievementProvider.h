#pragma once

#include "CoreMinimal.h"

class IMiningAchievementProvider
{
public:
    virtual ~IMiningAchievementProvider() = default;
    virtual FString AccountId() const = 0;
    virtual void Query(TFunction<void(bool, TSet<FName>)> Callback) = 0;
    virtual void Unlock(const TSet<FName>& Ids, TFunction<void(bool)> Callback) = 0;
};

namespace MiningPlatform
{
    BRAINMINING_API FString SteamAccountKey(uint32 AppId, const FString& SteamId);
    BRAINMINING_API FString ProgressSaveSlot(uint32 AppId);
    BRAINMINING_API uint32 ConfiguredSteamAppId();
    BRAINMINING_API TSharedPtr<IMiningAchievementProvider> CreateAchievementProvider();
    BRAINMINING_API FString GameLanguage();
}
