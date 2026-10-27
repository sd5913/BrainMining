#include "MiningAchievementSubsystem.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void UMiningAchievementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (IsRunningCommandlet() || GIsAutomationTesting || (GetWorld() && GetWorld()->WorldType == EWorldType::PIE)) return;
    const bool TestProfile = FParse::Param(FCommandLine::Get(), TEXT("MiningQA"));
    FString UserDirectory;
    if (TestProfile && (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirectory) || UserDirectory.IsEmpty())) return;
    Provider = TestProfile ? nullptr : MiningPlatform::CreateAchievementProvider();
    OwnerId = Provider.IsValid() ? Provider->AccountId() : FString();
    if (OwnerId.IsEmpty()) OwnerId = TestProfile ? TEXT("qa") : TEXT("local");
    Slot = TEXT("BrainMiningAchievements_v1_") + OwnerId;
    LoadProfile();
    bInitialized = true;
}

void UMiningAchievementSubsystem::Deinitialize()
{
    bInitialized = false;
    ++RequestSerial;
    if (bDirty) SaveProfile();
    Provider.Reset();
    Super::Deinitialize();
}

void UMiningAchievementSubsystem::LoadProfile()
{
    bool FoundFile = false;
    bool FoundValid = false;
    for (const FString& Candidate : {Slot, Slot + TEXT("_pending"), Slot + TEXT("_backup")})
    {
        FoundFile |= UGameplayStatics::DoesSaveGameExist(Candidate, 0);
        auto* Saved = Cast<UMiningAchievementSave>(UGameplayStatics::LoadGameFromSlot(Candidate, 0));
        if (!Saved || Saved->Version != 1 || Saved->OwnerId != OwnerId || Saved->Revision < 0 || !Saved->Progress.IsValid()) continue;
        if (!FoundValid || Saved->Revision > Revision)
        {
            Progress = Saved->Progress;
            Revision = Saved->Revision;
            FoundValid = true;
        }
    }
    bStorageHealthy = !FoundFile || FoundValid;
    if (!bStorageHealthy) UE_LOG(LogTemp, Error, TEXT("Achievement profile could not be recovered; preserving files for %s"), *OwnerId);
}

bool UMiningAchievementSubsystem::SaveProfile()
{
    if (!bStorageHealthy || Revision == MAX_int64) return false;
    auto* Saved = NewObject<UMiningAchievementSave>();
    Saved->OwnerId = OwnerId;
    Saved->Revision = Revision + 1;
    Saved->Progress = Progress;
    if (!UGameplayStatics::SaveGameToSlot(Saved, Slot + TEXT("_pending"), 0)) return false;
    if (auto* Previous = Cast<UMiningAchievementSave>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
        Previous && Previous->Version == 1 && Previous->OwnerId == OwnerId && Previous->Progress.IsValid())
        if (!UGameplayStatics::SaveGameToSlot(Previous, Slot + TEXT("_backup"), 0)) return false;
    if (!UGameplayStatics::SaveGameToSlot(Saved, Slot, 0)) return false;
    Revision = Saved->Revision;
    bDirty = false;
    return true;
}

bool UMiningAchievementSubsystem::CanRecord() const
{
    if (!bInitialized || !bRunEligible || GIsAutomationTesting || !GetWorld() || GetWorld()->WorldType == EWorldType::PIE) return false;
    const FString Map = GetWorld()->GetOutermost()->GetName();
    if (Map != TEXT("/Game/MainGame/Maps/Lvl_MainGame") && Map != TEXT("/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo")) return false;
    if (Provider.IsValid())
    {
        const FString Current = Provider->AccountId();
        if (!Current.IsEmpty() && Current != OwnerId) return false;
    }
    return true;
}

void UMiningAchievementSubsystem::Changed()
{
    bDirty = true;
    if (!SaveProfile()) NextSaveTime = FPlatformTime::Seconds() + 5;
}

void UMiningAchievementSubsystem::RecordCollection(int32 ItemId)
{
    if (CanRecord() && Progress.RecordCollection(ItemId)) Changed();
}

void UMiningAchievementSubsystem::RecordPickaxe(int32 Level)
{
    if (CanRecord() && Progress.RecordPickaxe(Level)) Changed();
}

void UMiningAchievementSubsystem::RecordMilestone(FName Id)
{
    if (Id != TEXT("ACH_CARD_ROOM_CLEAR") && Id != TEXT("ACH_MUSEUM_CLEAR") && Id != TEXT("ACH_CAMPAIGN_COMPLETE")) return;
    if (CanRecord() && Progress.Unlock(Id)) Changed();
}

void UMiningAchievementSubsystem::BeginRun(bool Eligible) { bRunEligible = Eligible; }
void UMiningAchievementSubsystem::DisableForDebug() { bRunEligible = false; }

bool UMiningAchievementSubsystem::IsSaveOwnerCompatible(const FString& SaveOwner, const FString& CurrentOwner)
{
    if (CurrentOwner.IsEmpty()) return false;
    return SaveOwner == CurrentOwner || (SaveOwner.IsEmpty() && (CurrentOwner == TEXT("local") || CurrentOwner == TEXT("qa")));
}

void UMiningAchievementSubsystem::ReconcileSave(const TArray<int32>& Inventory, int32 PickaxeLevel, bool CampaignComplete, const FString& SaveOwner, const TArray<int32>* VerifiedIds)
{
    if (!CanRecord() || !IsSaveOwnerCompatible(SaveOwner, OwnerId)) return;
    bool Updated = Progress.RecordSavedCollections(Inventory, VerifiedIds);
    Updated |= Progress.RecordPickaxe(PickaxeLevel);
    if (CampaignComplete) Updated |= Progress.Unlock(TEXT("ACH_CAMPAIGN_COMPLETE"));
    if (Updated) Changed();
}

void UMiningAchievementSubsystem::Tick(float DeltaTime)
{
    const double Now = FPlatformTime::Seconds();
    if (bDirty && Now >= NextSaveTime)
    {
        if (!SaveProfile()) NextSaveTime = Now + 10;
    }
    if (bRequestInFlight && Now - RequestStarted > 45)
    {
        ++RequestSerial;
        bQueried = false;
        RequestFinished(false);
    }
    if (!bDirty && !bRequestInFlight && Now >= NextSyncTime) Synchronize();
}

void UMiningAchievementSubsystem::RequestFinished(bool Succeeded)
{
    bRequestInFlight = false;
    FailureCount = Succeeded ? 0 : FMath::Min(FailureCount + 1, 6);
    NextSyncTime = FPlatformTime::Seconds() + (Succeeded ? 1.0 : FMath::Min(300.0, 5.0 * (1 << FailureCount)));
}

void UMiningAchievementSubsystem::Synchronize()
{
    NextSyncTime = FPlatformTime::Seconds() + 10;
    if (!bStorageHealthy || !Provider.IsValid() || OwnerId == TEXT("local") || Provider->AccountId() != OwnerId) return;
    TWeakObjectPtr<UMiningAchievementSubsystem> WeakThis(this);
    if (!bQueried)
    {
        bRequestInFlight = true;
        RequestStarted = FPlatformTime::Seconds();
        const uint64 Serial = ++RequestSerial;
        Provider->Query([WeakThis, Serial](bool Succeeded, TSet<FName> Remote)
        {
            auto* Self = WeakThis.Get();
            if (!Self || !Self->bInitialized || Serial != Self->RequestSerial) return;
            Succeeded &= Self->Provider.IsValid() && Self->Provider->AccountId() == Self->OwnerId;
            if (Succeeded)
            {
                Self->Confirmed = MoveTemp(Remote);
                Self->bQueried = true;
            }
            Self->RequestFinished(Succeeded);
        });
        return;
    }
    const TSet<FName> Pending = Progress.Unlocked.Difference(Confirmed);
    if (Pending.IsEmpty()) return;
    bRequestInFlight = true;
    RequestStarted = FPlatformTime::Seconds();
    const uint64 Serial = ++RequestSerial;
    Provider->Unlock(Pending, [WeakThis, Serial, Pending](bool Succeeded)
    {
        auto* Self = WeakThis.Get();
        if (!Self || !Self->bInitialized || Serial != Self->RequestSerial) return;
        Succeeded &= Self->Provider.IsValid() && Self->Provider->AccountId() == Self->OwnerId;
        if (Succeeded) Self->Confirmed.Append(Pending);
        else Self->bQueried = false;
        Self->RequestFinished(Succeeded);
    });
}
