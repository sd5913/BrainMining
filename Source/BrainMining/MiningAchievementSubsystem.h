#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "MiningAchievementState.h"
#include "MiningAchievementProvider.h"
#include "MiningAchievementSubsystem.generated.h"

UCLASS()
class BRAINMINING_API UMiningAchievementSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return !IsTemplate() && bInitialized; }
    virtual bool IsTickableWhenPaused() const override { return true; }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMiningAchievementSubsystem, STATGROUP_Tickables); }
    void RecordCollection(int32 ItemId);
    void RecordPickaxe(int32 Level);
    void RecordMilestone(FName Id);
    void BeginRun(bool Eligible = true);
    void DisableForDebug();
    bool IsRunEligible() const { return bRunEligible; }
    FString GetOwnerId() const { return OwnerId; }
    static bool IsSaveOwnerCompatible(const FString& SaveOwner, const FString& CurrentOwner);
    void ReconcileSave(const TArray<int32>& Inventory, int32 PickaxeLevel, bool CampaignComplete, const FString& SaveOwner, const TArray<int32>* VerifiedIds = nullptr);
    const FMiningAchievementProgress& GetProgress() const { return Progress; }
private:
    friend class FMiningAchievementSyncTest;
    friend class FMiningAchievementStorageTest;
    friend class FMiningAchievementMilestonesTest;
    friend class FMiningAchievementAppIsolationTest;
    FMiningAchievementProgress Progress;
    TSharedPtr<IMiningAchievementProvider> Provider;
    FString OwnerId;
    FString Slot;
    int64 Revision = 0;
    uint64 RequestSerial = 0;
    TSet<FName> Confirmed;
    bool bInitialized = false;
    bool bRunEligible = true;
    bool bDirty = false;
    bool bStorageHealthy = true;
    bool bQueried = false;
    bool bRequestInFlight = false;
    double RequestStarted = 0;
    double NextSyncTime = 0;
    double NextSaveTime = 0;
    int32 FailureCount = 0;
    bool CanRecord() const;
    void LoadProfile();
    bool SaveProfile();
    void Changed();
    void Synchronize();
    void RequestFinished(bool Succeeded);
};
