#include "MiningAchievementState.h"
#include "MiningAchievementSubsystem.h"
#include "MiningSaveSubsystem.h"
#include "MiningExperienceSubsystem.h"
#include "MiningWalletSubsystem.h"
#include "HeavenToiletWater.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningAchievementMilestonesTest,"BrainMining.Achievements.NewMilestonesAndPickupEligibility",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningAchievementMilestonesTest::RunTest(const FString&)
{
    auto* Instance=NewObject<UGameInstance>(GEngine);
    Instance->InitializeStandalone(FName(*(TEXT("AchievementMilestones_")+FGuid::NewGuid().ToString(EGuidFormats::Digits))),CreatePackage(TEXT("/Game/MainGame/Maps/Lvl_MainGame")));
    UWorld* World=Instance->GetWorld();
    auto* System=Instance->GetSubsystem<UMiningAchievementSubsystem>();
    const FString Slot=TEXT("BrainMining_Automation_Milestones_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    System->Slot=Slot;System->OwnerId=TEXT("qa");System->bInitialized=true;System->Provider.Reset();
    ON_SCOPE_EXIT
    {
        Instance->Shutdown();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);
        for(const FString& Candidate:{Slot,Slot+TEXT("_pending"),Slot+TEXT("_backup")})UGameplayStatics::DeleteGameInSlot(Candidate,0);
    };
    TestEqual(TEXT("All eleven achievements registered"),MiningAchievements::Definitions().Num(),11);
    for(const FName Id:{FName(TEXT("ACH_FIRST_DIAMOND")),FName(TEXT("ACH_OFFICE_CLEAR")),FName(TEXT("ACH_HEAVEN_ROOM_ENTER"))})
    {
        System->RecordMilestone(Id);
        TestFalse(TEXT("Automation cannot award a real achievement"),System->Progress.Unlocked.Contains(Id));
    }
    // Isolated local profile, with no platform provider; exercise the normal recording gates.
    TGuardValue<bool> NormalPlay(GIsAutomationTesting,false);
    System->DisableForDebug();
    System->RecordMilestone(TEXT("ACH_OFFICE_CLEAR"));
    TestTrue(TEXT("Debug run cannot unlock milestones"),System->Progress.Unlocked.IsEmpty());
    System->BeginRun();
    World->WorldType=EWorldType::PIE;
    System->RecordMilestone(TEXT("ACH_HEAVEN_ROOM_ENTER"));
    TestTrue(TEXT("PIE cannot unlock milestones"),System->Progress.Unlocked.IsEmpty());
    World->WorldType=EWorldType::Game;
    auto* Wallet=Instance->GetSubsystem<UMiningWalletSubsystem>();
    Wallet->AddMineral(TEXT("9"));
    TestTrue(TEXT("Direct wallet grant is not a diamond pickup"),System->Progress.Unlocked.IsEmpty());
    auto* Saves=Instance->GetSubsystem<UMiningSaveSubsystem>();
    Saves->RecordMineral(EMiningMineral::Copper);
    TestTrue(TEXT("Other minerals do not unlock the diamond achievement"),System->Progress.Unlocked.IsEmpty());
    System->NextSyncTime=FPlatformTime::Seconds()+10;
    Saves->RecordMineral(EMiningMineral::Diamond);
    TestTrue(TEXT("Actual diamond pickup unlocks"),System->Progress.Unlocked.Contains(TEXT("ACH_FIRST_DIAMOND")));
    TestEqual(TEXT("Newly earned achievement wakes synchronization immediately"),System->NextSyncTime,0.0);
    System->FailureCount=1;
    const double RetryAt=FPlatformTime::Seconds()+30;
    System->NextSyncTime=RetryAt;
    System->RecordCollection(16);
    TestEqual(TEXT("New pickup preserves network-error backoff"),System->NextSyncTime,RetryAt);
    System->FailureCount=0;
    auto* Controller=World->SpawnActor<APlayerController>();
    World->AddController(Controller);
    auto* Pawn=World->SpawnActor<ACharacter>();
    Controller->Possess(Pawn);
    TestTrue(TEXT("Room entry observes the isolated test player"),UGameplayStatics::GetPlayerPawn(World,0)==Pawn);
    auto* Heaven=World->SpawnActor<AHeavenToiletWater>();
    Pawn->SetActorLocation(FVector(0,0,4400));Heaven->Tick(.01f);
    TestFalse(TEXT("Standing above the dome is not entering heaven"),System->Progress.Unlocked.Contains(TEXT("ACH_HEAVEN_ROOM_ENTER")));
    System->DisableForDebug();
    Pawn->SetActorLocation(FVector(0,0,3000));Heaven->Tick(.01f);
    TestFalse(TEXT("Debug entry into the real room is ignored"),System->Progress.Unlocked.Contains(TEXT("ACH_HEAVEN_ROOM_ENTER")));
    Pawn->SetActorLocation(FVector(Heaven->Radius+100,0,3000));Heaven->Tick(.01f);
    System->BeginRun();
    Pawn->SetActorLocation(FVector(0,0,3000));Heaven->Tick(.01f);
    TestTrue(TEXT("Normal reentry through the upper dome unlocks heaven"),System->Progress.Unlocked.Contains(TEXT("ACH_HEAVEN_ROOM_ENTER")));
    TestFalse(TEXT("Upper-dome achievement detection does not change the ground-level choir trigger"),Heaven->bPlayerInHeaven);
    System->RecordMilestone(TEXT("ACH_OFFICE_CLEAR"));
    TestEqual(TEXT("All three new milestones and the collection unlock independently"),System->Progress.Unlocked.Num(),4);
    const int64 Revision=System->Revision;
    System->RecordMilestone(TEXT("ACH_OFFICE_CLEAR"));
    Saves->RecordMineral(EMiningMineral::Diamond);
    TestEqual(TEXT("Repeated milestones do not rewrite the ledger"),System->Revision,Revision);
    auto* Reloaded=NewObject<UMiningAchievementSubsystem>(Instance);
    Reloaded->Slot=Slot;Reloaded->OwnerId=System->OwnerId;Reloaded->LoadProfile();
    TestEqual(TEXT("All new milestones survive profile reload"),Reloaded->Progress.Unlocked.Num(),4);
    TestTrue(TEXT("Extended ledger remains compatible with v1 storage"),Reloaded->Progress.IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningAchievementRulesTest,"BrainMining.Achievements.RulesAndDeduplication",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningAchievementRulesTest::RunTest(const FString&)
{
    FMiningAchievementProgress Progress;
    FMiningAchievementProgress KeptCatalogue;
    TArray<int32> DisplayInventory;DisplayInventory.Init(1,23);
    const TArray<int32> NoVerifiedItems;
    TestFalse(TEXT("Keeping a foreign or debug catalogue cannot award achievements after restart"), KeptCatalogue.RecordSavedCollections(DisplayInventory, &NoVerifiedItems));
    const TArray<int32> VerifiedItems={2,4};
    TestTrue(TEXT("Only verified account collection is restored"), KeptCatalogue.RecordSavedCollections(DisplayInventory, &VerifiedItems));
    TestEqual(TEXT("Display-only collectibles are not counted"), KeptCatalogue.CollectionIds.Num(), 2);
    TestTrue(TEXT("Legacy saves reconcile locally"), UMiningAchievementSubsystem::IsSaveOwnerCompatible(TEXT(""), TEXT("local")));
    TestFalse(TEXT("Legacy saves do not transfer to Steam"), UMiningAchievementSubsystem::IsSaveOwnerCompatible(TEXT(""), TEXT("steam_a")));
    TestFalse(TEXT("Local records do not transfer to Steam"), UMiningAchievementSubsystem::IsSaveOwnerCompatible(TEXT("local"), TEXT("steam_a")));
    TestFalse(TEXT("Different Steam owners stay isolated"), UMiningAchievementSubsystem::IsSaveOwnerCompatible(TEXT("steam_a"), TEXT("steam_b")));
    TestTrue(TEXT("Same account resumes"), UMiningAchievementSubsystem::IsSaveOwnerCompatible(TEXT("steam_a"), TEXT("steam_a")));
    TestFalse(TEXT("An uninitialized owner cannot claim a save"), UMiningAchievementSubsystem::IsSaveOwnerCompatible(TEXT(""), TEXT("")));
    TestFalse(TEXT("Invalid item is rejected"),Progress.RecordCollection(-1));
    TestTrue(TEXT("First collection recorded"),Progress.RecordCollection(0));
    TestFalse(TEXT("Repeat collection is not counted"),Progress.RecordCollection(0));
    TestEqual(TEXT("Unique item count"),Progress.CollectionIds.Num(),1);
    TestTrue(TEXT("First collection unlocks"),Progress.Unlocked.Contains(TEXT("ACH_FIRST_COLLECTION")));
    TestFalse(TEXT("Five collections not prematurely awarded"),Progress.Unlocked.Contains(TEXT("ACH_COLLECTION_5")));
    for(int32 Index=1;Index<5;++Index)Progress.RecordCollection(Index);
    TestTrue(TEXT("Five unique collections unlock"),Progress.Unlocked.Contains(TEXT("ACH_COLLECTION_5")));
    TestFalse(TEXT("Partial collection does not award full collection"),Progress.Unlocked.Contains(TEXT("ACH_COLLECTION_COMPLETE")));
    for(int32 Id:MiningAchievements::RequiredCollection())Progress.RecordCollection(Id);
    TestTrue(TEXT("Explicit release collection unlocks"),Progress.Unlocked.Contains(TEXT("ACH_COLLECTION_COMPLETE")));
    TestFalse(TEXT("Future item is excluded from release collection"),Progress.RecordCollection(23));
    TestFalse(TEXT("Invalid upgrade is rejected"),Progress.RecordPickaxe(5));
    TestTrue(TEXT("First upgrade recorded"),Progress.RecordPickaxe(1));
    TestFalse(TEXT("First upgrade does not award max level"),Progress.Unlocked.Contains(TEXT("ACH_PICKAXE_MAX")));
    Progress.RecordPickaxe(4);
    TestTrue(TEXT("Max level unlocks"),Progress.Unlocked.Contains(TEXT("ACH_PICKAXE_MAX")));
    TestFalse(TEXT("Old save cannot reduce best level"),Progress.RecordPickaxe(1));
    TestEqual(TEXT("Best level is monotonic"),Progress.BestPickaxeLevel,4);
    TestFalse(TEXT("Unknown API ID is rejected"),Progress.Unlock(TEXT("UNKNOWN")));
    TestTrue(TEXT("Ledger validates"),Progress.IsValid());
    return true;
}

namespace
{
    class FTestAchievementProvider : public IMiningAchievementProvider
    {
    public:
        FString User=TEXT("steam_test_a");
        TFunction<void(bool,TSet<FName>)> QueryCallback;
        TFunction<void(bool)> WriteCallback;
        TSet<FName> Submitted;
        int32 Queries=0;
        int32 Writes=0;
        virtual FString AccountId() const override {return User;}
        virtual void Query(TFunction<void(bool,TSet<FName>)> Callback) override {++Queries;QueryCallback=MoveTemp(Callback);}
        virtual void Unlock(const TSet<FName>& Ids,TFunction<void(bool)> Callback) override {++Writes;Submitted=Ids;WriteCallback=MoveTemp(Callback);}
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningAchievementSyncTest,"BrainMining.Achievements.QueryRetryAndAccountIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningAchievementSyncTest::RunTest(const FString&)
{
    auto* Instance=NewObject<UGameInstance>();
    auto* System=NewObject<UMiningAchievementSubsystem>(Instance);
    auto Provider=MakeShared<FTestAchievementProvider>();
    System->Provider=Provider;System->OwnerId=Provider->User;System->bInitialized=true;
    System->Progress.Unlock(TEXT("ACH_FIRST_COLLECTION"));
    System->Synchronize();
    TestEqual(TEXT("Query precedes write"),Provider->Queries,1);
    TestEqual(TEXT("No write before successful query"),Provider->Writes,0);
    Provider->QueryCallback(false,{});
    TestFalse(TEXT("Failed query does not mark ready"),System->bQueried);
    TestTrue(TEXT("Failure uses a delayed retry"),System->NextSyncTime>FPlatformTime::Seconds()+9);
    System->Synchronize();Provider->QueryCallback(true,{});
    System->Synchronize();
    TestEqual(TEXT("Ready query submits one batch"),Provider->Writes,1);
    TestTrue(TEXT("Batch contains only completed achievement"),Provider->Submitted.Contains(TEXT("ACH_FIRST_COLLECTION")));
    Provider->WriteCallback(false);
    TestFalse(TEXT("Failed write remains unconfirmed"),System->Confirmed.Contains(TEXT("ACH_FIRST_COLLECTION")));
    System->Synchronize();Provider->QueryCallback(true,{});
    System->Synchronize();Provider->WriteCallback(true);
    TestTrue(TEXT("Successful callback confirms delivery"),System->Confirmed.Contains(TEXT("ACH_FIRST_COLLECTION")));
    System->Synchronize();
    TestEqual(TEXT("Confirmed achievement is not resent"),Provider->Writes,2);
    System->Progress.Unlock(TEXT("ACH_CARD_ROOM_CLEAR"));
    Provider->User=TEXT("steam_test_b");System->Synchronize();
    TestEqual(TEXT("Different user receives no pending writes"),Provider->Writes,2);
    Provider->User=TEXT("steam_test_a");System->Synchronize();
    Provider->User=TEXT("steam_test_b");Provider->WriteCallback(true);
    TestFalse(TEXT("Account switch during callback does not acknowledge delivery"),System->Confirmed.Contains(TEXT("ACH_CARD_ROOM_CLEAR")));
    System->bInitialized=false;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningAchievementAppIsolationTest,"BrainMining.Achievements.DemoAndFullAppIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningAchievementAppIsolationTest::RunTest(const FString&)
{
    const FString DemoOwner=MiningPlatform::SteamAccountKey(5524430,TEXT("test_account"));
    const FString FullOwner=MiningPlatform::SteamAccountKey(5506030,TEXT("test_account"));
    TestEqual(TEXT("Released demo keeps its existing ledger"),DemoOwner,FString(TEXT("steam_test_account")));
    TestNotEqual(TEXT("Same account has independent app owners"),DemoOwner,FullOwner);
    TestFalse(TEXT("Demo save cannot reconcile full achievements"),UMiningAchievementSubsystem::IsSaveOwnerCompatible(DemoOwner,FullOwner));
    TestEqual(TEXT("Demo keeps its released progress slot"),MiningPlatform::ProgressSaveSlot(5524430),FString(TEXT("BrainMiningProgress_v1")));
    for(const FString& Suffix:{FString(),FString(TEXT("_pending")),FString(TEXT("_backup")),FString(TEXT("_completion"))})
        TestNotEqual(TEXT("All full progress variants are separate"),MiningPlatform::ProgressSaveSlot(5524430)+Suffix,MiningPlatform::ProgressSaveSlot(5506030)+Suffix);
    const FString Prefix=TEXT("BrainMining_Automation_AppIsolation_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT("_");
    auto* Instance=NewObject<UGameInstance>();
    auto* Demo=NewObject<UMiningAchievementSubsystem>(Instance);
    Demo->OwnerId=DemoOwner;Demo->Slot=Prefix+DemoOwner;Demo->Progress.RecordCollection(16);
    auto* Full=NewObject<UMiningAchievementSubsystem>(Instance);
    Full->OwnerId=FullOwner;Full->Slot=Prefix+FullOwner;
    ON_SCOPE_EXIT
    {
        Full->bInitialized=false;
        for(const FString& Base:{Demo->Slot,Full->Slot})
            for(const FString& Suffix:{FString(),FString(TEXT("_pending")),FString(TEXT("_backup"))})UGameplayStatics::DeleteGameInSlot(Base+Suffix,0);
    };
    TestTrue(TEXT("Existing demo achievement saved"),Demo->SaveProfile());
    Full->LoadProfile();
    TestTrue(TEXT("Full starts with healthy empty progress"),Full->bStorageHealthy && Full->Progress.Unlocked.IsEmpty() && Full->Progress.CollectionIds.IsEmpty());
    auto Provider=MakeShared<FTestAchievementProvider>();Provider->User=FullOwner;
    Full->Provider=Provider;Full->bInitialized=true;
    Full->Synchronize();Provider->QueryCallback(true,{});Full->Synchronize();
    TestEqual(TEXT("Demo unlock is never submitted to full backend"),Provider->Writes,0);
    Full->Progress.RecordCollection(16);TestTrue(TEXT("Full can independently earn same collectible"),Full->SaveProfile());Full->Synchronize();
    TestEqual(TEXT("Full-earned achievement is submitted normally"),Provider->Writes,1);
    TestTrue(TEXT("Full submission contains its own first collectible"),Provider->Submitted.Contains(TEXT("ACH_FIRST_COLLECTION")));
    Provider->WriteCallback(true);
    auto* ReloadedDemo=NewObject<UMiningAchievementSubsystem>(Instance);
    ReloadedDemo->OwnerId=DemoOwner;ReloadedDemo->Slot=Demo->Slot;ReloadedDemo->LoadProfile();
    TestTrue(TEXT("Full activity preserves demo progress"),ReloadedDemo->Progress.Unlocked.Contains(TEXT("ACH_FIRST_COLLECTION")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningAchievementKeptCollectionTest,"BrainMining.Achievements.KeptCollectionAcrossRuns",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningAchievementKeptCollectionTest::RunTest(const FString&)
{
    const FString Prefix=TEXT("BrainMining_Automation_KeptCollection_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString CampaignSlot=Prefix+TEXT("_campaign");
    const FString LedgerSlot=Prefix+TEXT("_ledger");
    const FString OriginalCommandLine=FCommandLine::Get();
    FCommandLine::Set(*FString::Printf(TEXT("-MiningSaveSlot=%s %s"),*CampaignSlot,*OriginalCommandLine));
    auto* Instance=NewObject<UGameInstance>(GEngine);
    // Keep the campaign package for production eligibility checks, but use a
    // unique world name: DestroyWorld does not immediately GC its named children.
    Instance->InitializeStandalone(FName(*Prefix),CreatePackage(TEXT("/Game/MainGame/Maps/Lvl_MainGame")));
    UWorld* World=Instance->GetWorld();World->WorldType=EWorldType::Game;
    auto* System=Instance->GetSubsystem<UMiningAchievementSubsystem>();
    auto* Saves=Instance->GetSubsystem<UMiningSaveSubsystem>();
    auto* Session=Instance->GetSubsystem<UMiningExperienceSubsystem>();
    auto Provider=MakeShared<FTestAchievementProvider>();
    Provider->User=MiningPlatform::SteamAccountKey(5506030,TEXT("kept_collection_test"));
    System->Slot=LedgerSlot;System->OwnerId=Provider->User;System->Provider=Provider;System->bInitialized=true;
    ON_SCOPE_EXIT
    {
        Instance->Shutdown();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);
        for(const FString& Base:{CampaignSlot,LedgerSlot})
            for(const FString& Suffix:{FString(),FString(TEXT("_pending")),FString(TEXT("_backup")),FString(TEXT("_completion"))})
                UGameplayStatics::DeleteGameInSlot(Base+Suffix,0);
        FCommandLine::Set(*OriginalCommandLine);
    };
    // Only the in-memory fake provider is attached. All save slots are unique.
    TGuardValue<bool> NormalPlay(GIsAutomationTesting,false);
    auto Snapshot=[&](const FString& Owner,bool Eligible,const TArray<int32>& Verified,bool MarkVerified=true)
    {
        auto* Data=NewObject<UMiningSaveGame>();
        Data->Version=6;Data->Upgrades={0,0,0,0};Data->CampaignComplete=true;
        Data->AchievementOwner=Owner;Data->AchievementEligible=Eligible;
        Data->HasVerifiedAchievementCollection=MarkVerified;Data->AchievementCollectionIds=Verified;
        Data->Collection.Init(1,23); // Visible catalogue is deliberately not trusted.
        TestTrue(TEXT("Isolated completion snapshot written"),UGameplayStatics::SaveGameToSlot(Data,CampaignSlot+TEXT("_completion"),0));
        return Data;
    };
    TArray<int32> FirstRun;
    for(int32 Id=0;Id<22;++Id)FirstRun.Add(Id);
    auto* First=Snapshot(System->OwnerId,true,FirstRun);
    First->Collection[22]=0;
    TestTrue(TEXT("Incomplete first run persisted"),UGameplayStatics::SaveGameToSlot(First,CampaignSlot+TEXT("_completion"),0));
    TestTrue(TEXT("Cloud-restored campaign starts without a local achievement ledger"),System->Progress.CollectionIds.IsEmpty());
    Saves->StartNewGame(true);
    TestEqual(TEXT("Keep restores all verified first-run achievements before deleting its save"),System->Progress.CollectionIds.Num(),22);
    TestEqual(TEXT("The incomplete visible collection is retained"),Session->CampaignCollection[22],0);
    TestFalse(TEXT("Partial first run does not unlock full collection"),System->Progress.Unlocked.Contains(TEXT("ACH_COLLECTION_COMPLETE")));
    Saves->RecordCollectible(22);
    TestTrue(TEXT("Collecting the missing item on the second run unlocks full collection"),System->Progress.Unlocked.Contains(TEXT("ACH_COLLECTION_COMPLETE")));
    System->Synchronize();Provider->QueryCallback(true,{});System->Synchronize();
    TestTrue(TEXT("Full-collection unlock reaches the Steam provider submission"),Provider->Submitted.Contains(TEXT("ACH_COLLECTION_COMPLETE")));
    Provider->WriteCallback(true);
    System->Synchronize();
    TestEqual(TEXT("Confirmed full-collection batch is not repeated"),Provider->Writes,1);

    System->Progress={};
    for(int32 Id:FirstRun)System->Progress.RecordCollection(Id);
    Snapshot(System->OwnerId,true,FirstRun);
    Saves->StartNewGame(true);
    TestEqual(TEXT("An intact same-machine ledger is preserved across runs"),System->Progress.CollectionIds.Num(),22);
    TestFalse(TEXT("An intact partial ledger does not unlock before the final pickup"),System->Progress.Unlocked.Contains(TEXT("ACH_COLLECTION_COMPLETE")));
    Saves->RecordCollectible(22);
    TestTrue(TEXT("Same-machine second-run completion still unlocks"),System->Progress.Unlocked.Contains(TEXT("ACH_COLLECTION_COMPLETE")));

    System->Progress={};Session->CampaignCollection.Reset();
    Snapshot(System->OwnerId,true,MiningAchievements::RequiredCollection().Array());
    Saves->StartNewGame(true);
    TestTrue(TEXT("Previously completed verified collection is repaired without another pickup"),System->Progress.Unlocked.Contains(TEXT("ACH_COLLECTION_COMPLETE")));

    System->Progress={};
    Snapshot(System->OwnerId,true,{});
    Saves->StartNewGame(true);
    TestTrue(TEXT("Display-only collection cannot fabricate verified achievement progress"),System->Progress.CollectionIds.IsEmpty());
    Snapshot(System->OwnerId,false,FirstRun);
    Saves->StartNewGame(true);
    TestTrue(TEXT("Debug completion snapshot cannot grant achievements"),System->Progress.CollectionIds.IsEmpty());
    Snapshot(MiningPlatform::SteamAccountKey(5506030,TEXT("other_player")),true,FirstRun);
    Saves->StartNewGame(true);
    TestTrue(TEXT("Another account's retained catalogue cannot grant achievements"),System->Progress.CollectionIds.IsEmpty());
    Snapshot(MiningPlatform::SteamAccountKey(5524430,TEXT("kept_collection_test")),true,FirstRun);
    Saves->StartNewGame(true);
    TestTrue(TEXT("Demo progress cannot transfer into full achievements"),System->Progress.CollectionIds.IsEmpty());
    Snapshot(System->OwnerId,true,FirstRun);
    Saves->StartNewGame(false);
    TestTrue(TEXT("Clear collection does not import the previous catalogue"),System->Progress.CollectionIds.IsEmpty() && Session->CampaignCollection.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningAchievementStorageTest,"BrainMining.Achievements.RecoveryAndOwnerValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningAchievementStorageTest::RunTest(const FString&)
{
    const FString Slot=TEXT("BrainMining_Automation_Achievements_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    ON_SCOPE_EXIT
    {
        for(const FString& Candidate:{Slot,Slot+TEXT("_pending"),Slot+TEXT("_backup")})UGameplayStatics::DeleteGameInSlot(Candidate,0);
    };
    auto* Instance=NewObject<UGameInstance>();
    auto* System=NewObject<UMiningAchievementSubsystem>(Instance);
    System->Slot=Slot;System->OwnerId=TEXT("steam_test_a");
    System->Progress.RecordCollection(0);System->bDirty=true;
    TestTrue(TEXT("Profile writes successfully"),System->SaveProfile());
    auto* Pending=NewObject<UMiningAchievementSave>();
    Pending->OwnerId=System->OwnerId;Pending->Revision=2;Pending->Progress=System->Progress;
    Pending->Progress.RecordPickaxe(4);
    TestTrue(TEXT("Newer pending file simulates interrupted commit"),UGameplayStatics::SaveGameToSlot(Pending,Slot+TEXT("_pending"),0));
    auto* Reloaded=NewObject<UMiningAchievementSubsystem>(Instance);
    Reloaded->Slot=Slot;Reloaded->OwnerId=System->OwnerId;Reloaded->LoadProfile();
    TestEqual(TEXT("Newest valid revision recovered"),Reloaded->Revision,int64(2));
    TestTrue(TEXT("Recovered pending unlock retained"),Reloaded->Progress.Unlocked.Contains(TEXT("ACH_PICKAXE_MAX")));
    Reloaded->BeginRun();
    TestTrue(TEXT("New run preserves account achievements"),Reloaded->Progress.Unlocked.Contains(TEXT("ACH_PICKAXE_MAX")));
    Reloaded->DisableForDebug();
    TestFalse(TEXT("Debug actions disable run eligibility"),Reloaded->IsRunEligible());
    auto* DifferentUser=NewObject<UMiningAchievementSubsystem>(Instance);
    DifferentUser->Slot=Slot;DifferentUser->OwnerId=TEXT("steam_test_b");
    AddExpectedError(TEXT("Achievement profile could not be recovered"),EAutomationExpectedErrorFlags::Contains,1);
    DifferentUser->LoadProfile();
    TestFalse(TEXT("Wrong-owner files are preserved rather than overwritten"),DifferentUser->bStorageHealthy);
    TestTrue(TEXT("No cross-account achievement migration"),DifferentUser->Progress.Unlocked.IsEmpty());
    Pending->Revision=3;Pending->Progress.CollectionIds=MiningAchievements::RequiredCollection();
    Pending->Progress.Unlocked.Remove(TEXT("ACH_COLLECTION_COMPLETE"));
    TestTrue(TEXT("Existing complete ledger missing its derived unlock is saved"),UGameplayStatics::SaveGameToSlot(Pending,Slot+TEXT("_pending"),0));
    auto* Repaired=NewObject<UMiningAchievementSubsystem>(Instance);
    Repaired->Slot=Slot;Repaired->OwnerId=System->OwnerId;Repaired->LoadProfile();
    TestTrue(TEXT("Complete verified ledger repairs the missing unlock on load"),Repaired->Progress.Unlocked.Contains(TEXT("ACH_COLLECTION_COMPLETE")));
    TestTrue(TEXT("Repair is persisted before Steam synchronization"),Repaired->bDirty && Repaired->SaveProfile());
    TestFalse(TEXT("Repaired ledger stays idempotent"),Repaired->Progress.RecordCollection(0));
    return true;
}
#endif
