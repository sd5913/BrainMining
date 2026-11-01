#include "MiningAchievementProvider.h"
#include "MiningAchievementState.h"
#include "MiningAchievementDiagnostics.h"

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
            return UserId.IsValid() && UserId->IsValid() ? MiningPlatform::SteamAccountKey(SteamUtils()->GetAppID(), UserId->ToString()) : FString();
        }

        virtual void Query(TFunction<void(bool, TSet<FName>)> Callback) override
        {
            MiningAchievementTrace(TEXT("query requested"));
            auto* Subsystem = SteamSubsystem();
            const auto UserId = SteamUserId();
            auto Achievements = Subsystem ? Subsystem->GetAchievementsInterface() : nullptr;
            if (!Achievements.IsValid() || !UserId.IsValid() || !UserId->IsValid()) { Callback(false, {}); return; }
            const FString Account = AccountId();
            Achievements->QueryAchievements(*UserId, FOnQueryAchievementsCompleteDelegate::CreateLambda(
                [Self = AsShared(), Account, Achievements, Callback = MoveTemp(Callback)](const FUniqueNetId& QueriedId, bool Succeeded)
                {
                    MiningAchievementTrace(FString::Printf(TEXT("query callback success=%d"), Succeeded));
                    TSet<FName> Remote;
                    if (Self->AccountId() != Account) Succeeded = false;
                    if (Succeeded)
                    {
                        for (const auto& Definition : MiningAchievements::Definitions())
                        {
                            FOnlineAchievement Achievement;
                            if (Achievements->GetCachedAchievement(QueriedId, Definition.Id.ToString(), Achievement) != EOnlineCachedResult::Success)
                            {
                                MiningAchievementTrace(TEXT("missing cached achievement: ") + Definition.Id.ToString());
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
            MiningAchievementTrace(FString::Printf(TEXT("unlock requested count=%d"), Ids.Num()));
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
                    MiningAchievementTrace(FString::Printf(TEXT("write callback success=%d"), Succeeded));
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

FString MiningPlatform::SteamAccountKey(uint32 AppId, const FString& SteamId)
{
    if (AppId == 0 || AppId == 480 || SteamId.IsEmpty()) return FString();
    // The released demo owns the legacy namespace. Never migrate it into another app.
    if (AppId == 5524430) return TEXT("steam_") + SteamId;
    return FString::Printf(TEXT("steam_app_%u_"), AppId) + SteamId;
}

FString MiningPlatform::ProgressSaveSlot(uint32 AppId)
{
    const FString Base = TEXT("BrainMiningProgress_v1");
    return AppId > 0 && AppId != 480 && AppId != 5524430 ? Base + FString::Printf(TEXT("_app_%u"), AppId) : Base;
}

uint32 MiningPlatform::ConfiguredSteamAppId()
{
#if BRAINMINING_WITH_STEAM
    int32 AppId = 0;
    if (GConfig) GConfig->GetInt(TEXT("BrainMining.Steam"), TEXT("ExpectedAppId"), AppId, GEngineIni);
    if (AppId > 0 && AppId != 480) return uint32(AppId);
#endif
    return 0;
}

TSharedPtr<IMiningAchievementProvider> MiningPlatform::CreateAchievementProvider()
{
#if BRAINMINING_WITH_STEAM
    FString FirstId;
    GConfig->GetString(TEXT("OnlineSubsystemSteam"), TEXT("Achievement_0_Id"), FirstId, GEngineIni);
    MiningAchievementTrace(TEXT("provider initialize; first config id=[") + FirstId + TEXT("]"));
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
