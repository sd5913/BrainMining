#include "MiningAchievementProvider.h"
#include "MiningAchievementState.h"

#if BRAINMINING_WITH_STEAM
#include "OnlineSubsystem.h"
#include "Misc/ConfigCacheIni.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineAchievementsInterface.h"
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace
{
    IOnlineSubsystem* SteamSubsystem()
    {
        auto* Subsystem = IOnlineSubsystem::Get();
        if (!Subsystem || Subsystem->GetSubsystemName() != FName(TEXT("STEAM")) || !SteamUtils()) return nullptr;
        int32 ExpectedAppId = 0;
        GConfig->GetInt(TEXT("BrainMining.Steam"), TEXT("ExpectedAppId"), ExpectedAppId, GEngineIni);
        return ExpectedAppId > 0 && ExpectedAppId != 480 && SteamUtils()->GetAppID() == uint32(ExpectedAppId) ? Subsystem : nullptr;
    }

    FUniqueNetIdPtr SteamUserId()
    {
        auto* Subsystem = SteamSubsystem();
        auto Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
        return Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
    }

    class FMiningSteamAchievements : public IMiningAchievementProvider, public TSharedFromThis<FMiningSteamAchievements>
    {
    public:
        virtual FString AccountId() const override
        {
            const auto UserId = SteamUserId();
            return UserId.IsValid() && UserId->IsValid() ? TEXT("steam_") + UserId->ToString() : FString();
        }

        virtual void Query(TFunction<void(bool, TSet<FName>)> Callback) override
        {
            auto* Subsystem = SteamSubsystem();
            const auto UserId = SteamUserId();
            auto Achievements = Subsystem ? Subsystem->GetAchievementsInterface() : nullptr;
            if (!Achievements.IsValid() || !UserId.IsValid() || !UserId->IsValid()) { Callback(false, {}); return; }
            const FString Account = AccountId();
            Achievements->QueryAchievements(*UserId, FOnQueryAchievementsCompleteDelegate::CreateLambda(
                [Self = AsShared(), Account, Achievements, Callback = MoveTemp(Callback)](const FUniqueNetId& QueriedId, bool Succeeded)
                {
                    TSet<FName> Remote;
                    if (Self->AccountId() != Account) Succeeded = false;
                    if (Succeeded)
                    {
                        for (const auto& Definition : MiningAchievements::Definitions())
                        {
                            FOnlineAchievement Achievement;
                            if (Achievements->GetCachedAchievement(QueriedId, Definition.Id.ToString(), Achievement) != EOnlineCachedResult::Success)
                            {
                                UE_LOG(LogTemp, Warning, TEXT("Steam achievement missing from config or published backend: %s"), *Definition.Id.ToString());
                                Succeeded = false;
                                break;
                            }
                            if (Achievement.Progress >= 100.0) Remote.Add(Definition.Id);
                        }
                    }
                    Callback(Succeeded, MoveTemp(Remote));
                }));
        }

        virtual void Unlock(const TSet<FName>& Ids, TFunction<void(bool)> Callback) override
        {
            auto* Subsystem = SteamSubsystem();
            const auto UserId = SteamUserId();
            auto Achievements = Subsystem ? Subsystem->GetAchievementsInterface() : nullptr;
            if (!Achievements.IsValid() || !UserId.IsValid() || !UserId->IsValid() || Ids.IsEmpty()) { Callback(false); return; }
            FOnlineAchievementsWriteRef Write = MakeShared<FOnlineAchievementsWrite>();
            for (FName Id : Ids)
            {
                FOnlineAchievement Cached;
                if (!MiningAchievements::IsKnown(Id) || Achievements->GetCachedAchievement(*UserId, Id.ToString(), Cached) != EOnlineCachedResult::Success)
                { Callback(false); return; }
                Write->SetFloatStat(Id.ToString(), 100.f);
            }
            const FString Account = AccountId();
            Achievements->WriteAchievements(*UserId, Write, FOnAchievementsWrittenDelegate::CreateLambda(
                [Self = AsShared(), Account, Ids, Write, Callback = MoveTemp(Callback)](const FUniqueNetId&, bool Succeeded) mutable
                {
                    if (!Succeeded || Self->AccountId() != Account) { Callback(false); return; }
                    Self->Query([Ids, Callback = MoveTemp(Callback)](bool Verified, TSet<FName> Remote)
                    {
                        Callback(Verified && Ids.Difference(Remote).IsEmpty());
                    });
                }));
        }
    };
}
#endif

TSharedPtr<IMiningAchievementProvider> MiningPlatform::CreateAchievementProvider()
{
#if BRAINMINING_WITH_STEAM
    if (SteamSubsystem()) return MakeShared<FMiningSteamAchievements>();
#endif
    return nullptr;
}

FString MiningPlatform::GameLanguage()
{
#if BRAINMINING_WITH_STEAM
    if (SteamSubsystem() && SteamApps()) return UTF8_TO_TCHAR(SteamApps()->GetCurrentGameLanguage());
#endif
    return FString();
}
