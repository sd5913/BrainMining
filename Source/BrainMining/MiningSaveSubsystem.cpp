#include "MiningSaveSubsystem.h"
#include "MiningLocalizationSubsystem.h"
#include "MiningAchievementSubsystem.h"
#include "Showcase/MiningExhibitHall.h"
#include "Showcase/MiningTreasure.h"
#include "MiningExperienceSubsystem.h"
#include "MiningWalletSubsystem.h"
#include "MiningProgressionComponent.h"
#include "MiningCollection.h"
#include "MiningCharacterMovementComponent.h"
#include "StartRoom/MiningOpening.h"
#include "StartRoom/MiningStartRoom.h"
#include "GeneratedMiningRock.h"
#include "Subtitles/SubtitleSubsystem.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "UDynamicMesh.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "CardRoomStage.h"
#include "EchoPassageStage.h"
#include "Async/Async.h"
#include "SaveGameSystem.h"
#include "PlatformFeatures.h"

namespace
{
    FString GetSaveSlot()
    {
        FString Slot=TEXT("BrainMiningProgress_v1");
#if !UE_BUILD_SHIPPING
        FParse::Value(FCommandLine::Get(),TEXT("MiningSaveSlot="),Slot);
#endif
        return Slot;
    }
    FString RockKey(const AActor* Actor)
    {
        // Level asset name is stable in packaged games and across PIE instances.
        FString Level=Actor->GetLevel()->GetOutermost()->GetName();
        const int32 PIE=Level.Find(TEXT("UEDPIE_"));
        if(PIE!=INDEX_NONE){const int32 End=Level.Find(TEXT("_"),ESearchCase::CaseSensitive,ESearchDir::FromStart,PIE+7);Level.RemoveAt(PIE,End-PIE+1);}
        for(const FName Tag:Actor->Tags)if(Tag.ToString().StartsWith(TEXT("BM_SaveId_")))return Level+TEXT(":")+Tag.ToString();
        return Level+TEXT(":")+Actor->GetName();
    }
    bool ValidSnapshot(const UMiningSaveGame* Data)
    {
        return Data && Data->Version>=1 && Data->Version<=6 && Data->Upgrades.Num()==4 && Data->Area>=0 &&
            !Data->PlayerTransform.ContainsNaN() && !Data->ViewRotation.ContainsNaN();
    }
    bool IsLegacyTransientTVMesh(const UMiningSaveGame* Data,const FMiningSavedMesh& Mesh)
    {
        // Pre-v4 saves used session-specific UObject numbers for these generated
        // room sections. There is no safe mapping back to a section in those files.
        // Rebuild that room terrain while retaining the valid campaign snapshot.
        return Data->Version<4 && Mesh.ActorKey.Contains(TEXT("/Passages/Echo/Maps/")) &&
            Mesh.ActorKey.Contains(TEXT(":DiggableRock_214"));
    }
    TArray<FString> RecoverySlots()
    {
        const FString Slot=GetSaveSlot();
        // A complete staging write can be newer than the backup after interruption.
        return {Slot,Slot+TEXT("_pending"),Slot+TEXT("_backup")};
    }
}

bool UMiningSaveSubsystem::HasSave() const
{
    if(CachedHasSave.IsSet())return CachedHasSave.GetValue();
    if(auto* End=Cast<UMiningSaveGame>(UGameplayStatics::LoadGameFromSlot(GetSaveSlot()+TEXT("_completion"),0));End && End->CampaignComplete)
    {CachedHasSave=false;return false;}
    for(const FString& Slot:RecoverySlots())
        if(UGameplayStatics::DoesSaveGameExist(Slot,0) && ValidSnapshot(Cast<UMiningSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot,0)))){CachedHasSave=true;return true;}
    CachedHasSave=false;return false;
}
TArray<int32> UMiningSaveSubsystem::GetCollectionProgress() const
{
    TArray<int32> Result=GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->CampaignCollection;
    TArray<FString> Slots=RecoverySlots();Slots.Insert(GetSaveSlot()+TEXT("_completion"),0);
    for(const FString& Slot:Slots)
        if(auto* Data=Cast<UMiningSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot,0));ValidSnapshot(Data))
        {
            Result.SetNumZeroed(FMath::Max(Result.Num(),Data->Collection.Num()));
            for(int32 I=0;I<Data->Collection.Num();++I)Result[I]=FMath::Max(Result[I],Data->Collection[I]);
            break;
        }
    return Result;
}
bool UMiningSaveSubsystem::HasCollectionProgress() const
{
    return GetCollectionProgress().ContainsByPredicate([](int32 Count){return Count>0;});
}
void UMiningSaveSubsystem::StartNewGame(bool bKeepCollection)
{
    GetGameInstance()->GetSubsystem<UMiningAchievementSubsystem>()->BeginRun();
    FinishPendingWrite();MeshTask=TFuture<FMiningSavedMesh>();MeshCache.Reset();TutorialSoil.Reset();
    FrozenCheckpoint=nullptr;PendingRoomRestore=nullptr;
    bSessionStarted=true;bLoadedProgress=false;SessionWorld=GetWorld();SaveAge=0;PreviousArea=INDEX_NONE;
    FallingSeconds=0;UpgradeHintLevel=INDEX_NONE;RestoredMinerals.Reset();
    CompletionNotices.Reset();TutorialActive=false;TutorialSoilPassed=false;TutorialCompleted=0;PrototypesFound=0;
    const TArray<int32> Collection=bKeepCollection?GetCollectionProgress():TArray<int32>();
    auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
    Wallet->Balance=0;Wallet->Minerals.Reset();Wallet->CollectedMineralTotals.Reset();Wallet->UpgradeLevels={0,0,0,0};Wallet->MineralDoubleChance=0;
    Wallet->bBombSkillUnlocked=false;Wallet->bDrillSkillUnlocked=false;
    RunCollectedItems.Reset();
    GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->CampaignCollection=Collection;
    for(TActorIterator<AMiningCollectionField> It(GetWorld());It;++It)
    {It->Inventory=Collection;It->Inventory.SetNumZeroed(AMiningCollectible::ItemTypeCount);}
    if(auto* Room=AMiningStartRoom::Find(GetWorld()))Room->RestoreEquipment(false,false,false);
    if(auto* PC=GetWorld()->GetFirstPlayerController();PC && PC->GetPawn())
        if(auto* Progression=PC->GetPawn()->FindComponentByClass<UMiningProgressionComponent>())Progression->ApplyUpgrades();
    // A confirmed new run must never fall back to the previous run on restart.
    for(const FString& Slot:RecoverySlots())UGameplayStatics::DeleteGameInSlot(Slot,0);
    UGameplayStatics::DeleteGameInSlot(GetSaveSlot()+TEXT("_completion"),0);CachedHasSave=false;
}

bool UMiningSaveSubsystem::SaveProgress()
{
    FinishPendingWrite();return SaveProgressInternal(false);
}
void UMiningSaveSubsystem::FinishPendingWrite()
{
    if(!WriteTask.IsValid())return;
    const bool Saved=WriteTask.Get();WriteTask=TFuture<bool>();
    if(Saved){SaveAge=0;CachedHasSave=true;++AutosavesCompleted;}
    else UE_LOG(LogTemp,Error,TEXT("Background progress write failed; previous save retained"));
}
void UMiningSaveSubsystem::EndSession()
{
    FinishPendingWrite();FrozenCheckpoint=nullptr;PendingRoomRestore=nullptr;bSessionStarted=false;bLoadedProgress=false;RunCollectedItems.Reset();RestoredMinerals.Reset();CompletionNotices.Reset();
    MeshTask=TFuture<FMiningSavedMesh>();MeshCache.Reset();TutorialSoil.Reset();
}
void UMiningSaveSubsystem::Deinitialize(){FinishPendingWrite();Super::Deinitialize();}
void UMiningSaveSubsystem::MarkCampaignComplete()
{
    FinishPendingWrite();
    auto* End=NewObject<UMiningSaveGame>(this);End->Version=5;End->Upgrades={0,0,0,0};End->CampaignComplete=true;
    auto* Achievements=GetGameInstance()->GetSubsystem<UMiningAchievementSubsystem>();
    End->AchievementEligible=Achievements->IsRunEligible();End->AchievementOwner=Achievements->GetOwnerId();
    End->HasVerifiedAchievementCollection=true;End->AchievementCollectionIds=Achievements->GetProgress().CollectionIds.Array();
    Achievements->RecordMilestone(TEXT("ACH_CAMPAIGN_COMPLETE"));
    End->Collection=GetCollectionProgress();
    if(UGameplayStatics::SaveGameToSlot(End,GetSaveSlot()+TEXT("_completion"),0))CachedHasSave=false;
    bSessionStarted=false;
}
bool UMiningSaveSubsystem::PrepareMeshCache()
{
    if(MeshTask.IsValid())
    {
        if(!MeshTask.IsReady())return false;
        auto Mesh=MeshTask.Get();MeshTask=TFuture<FMiningSavedMesh>();
        const FString Key=Mesh.ActorKey+FString::Printf(TEXT("#%d"),Mesh.Chunk);MeshCache.Add(Key,MoveTemp(Mesh));
    }
    // Copy at most one changed chunk per frame; only immutable geometry goes
    // to the worker. The final snapshot is taken after every revision matches.
    for(TActorIterator<ADiggableRock> It(GetWorld());It;++It)
    {
        auto* Rock=*It;auto* Generated=Cast<AGeneratedMiningRock>(Rock);
        if(Rock->IsTerrainCutPending() || (Generated && Generated->bSimplificationPending))return false;
        const FString ActorKey=RockKey(Rock);
        const auto Queue=[&](UDynamicMeshComponent* Component,int32 Chunk,int32 Cuts)
        {
            if(!Component || Cuts<=0)return false;
            const FString Key=ActorKey+FString::Printf(TEXT("#%d"),Chunk);
            if(auto* Cached=MeshCache.Find(Key);Cached && Cached->Cuts==Cuts){Cached->ActorCuts=Rock->SuccessfulCuts;return false;}
            FMiningSavedMesh Saved;Saved.ActorKey=ActorKey;Saved.Chunk=Chunk;Saved.Cuts=Cuts;Saved.ActorCuts=Rock->SuccessfulCuts;
            auto Copy=MakeShared<UE::Geometry::FDynamicMesh3,ESPMode::ThreadSafe>();
            Component->GetDynamicMesh()->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Source){*Copy=Source;});
            MeshTask=Async(EAsyncExecution::ThreadPool,[Copy,Saved=MoveTemp(Saved)]() mutable
            {FMemoryWriter Writer(Saved.Data,true);Copy->Serialize(Writer);return MoveTemp(Saved);});
            return true;
        };
        if(Generated && Generated->bChunked)
        {for(int32 I=0;I<Generated->Chunks.Num();++I)if(Queue(Generated->Chunks[I].Mesh,I,Generated->Chunks[I].Cuts))return false;}
        else if(Queue(Rock->RockMesh,INDEX_NONE,Rock->SuccessfulCuts))return false;
    }
    return true;
}
bool UMiningSaveSubsystem::SaveProgressInternal(bool bAsync)
{
    if(WriteTask.IsValid())return false;
    const double Started=FPlatformTime::Seconds();
    UWorld* World=GetWorld();
    auto* Hall=AMiningExhibitHall::Find(World);
    auto* PC=World?World->GetFirstPlayerController():nullptr;
    auto* Character=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
    if(!bSessionStarted || bRestoring || !World || !World->GetMapName().EndsWith(TEXT("Lvl_MainGame")) ||
       !Hall || !Hall->bReady || !Hall->Areas.IsValidIndex(Hall->ActiveArea) || !Character ||
       AMiningOpening::IsOpening(World))return false;
    if(FrozenCheckpoint && FrozenCheckpoint->Checkpoint==CheckpointMode() && FrozenCheckpoint->Area==Hall->ActiveArea)
        return true; // Preserve inventory, terrain and position together until the next checkpoint.
    if(FallingSeconds>.1f && CheckpointMode()==0)return false;
    // Never capture a half-committed boolean mesh or an in-flight simplification.
    for(TActorIterator<ADiggableRock> It(World);It;++It)
    {
        if(It->IsTerrainCutPending())return false;
        if(auto* Generated=Cast<AGeneratedMiningRock>(*It);Generated && Generated->bSimplificationPending)return false;
    }
    auto* Data=Cast<UMiningSaveGame>(UGameplayStatics::CreateSaveGameObject(UMiningSaveGame::StaticClass()));
    Data->Version=6;
    auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
    Data->Area=Hall->ActiveArea;Data->UnlockedArea=Hall->UnlockedArea;Data->CompletedAreas=Hall->GetCompletedAreas();
    if(auto* Achievements=GetGameInstance()->GetSubsystem<UMiningAchievementSubsystem>())
    {
        Data->AchievementEligible=Achievements->IsRunEligible();Data->AchievementOwner=Achievements->GetOwnerId();
        Data->HasVerifiedAchievementCollection=true;Data->AchievementCollectionIds=Achievements->GetProgress().CollectionIds.Array();
    }
    Data->PlayerTransform=Character->GetActorTransform();Data->ViewRotation=PC->GetControlRotation();
    Data->Balance=Wallet->Balance;Data->Minerals=Wallet->Minerals;Data->CollectedMineralTotals=Wallet->CollectedMineralTotals;Data->Upgrades=Wallet->UpgradeLevels;
    Data->TutorialActive=TutorialActive;Data->TutorialCompleted=TutorialCompleted;Data->PrototypesFound=PrototypesFound;
    Data->TutorialSoilPassed=TutorialSoilPassed;
    Data->Bomb=Wallet->bBombSkillUnlocked;Data->Drill=Wallet->bDrillSkillUnlocked;
    Data->Collection=GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->CampaignCollection;
    if(Hall->Collection)Data->Collection=Hall->Collection->Inventory;
    if(auto* Room=AMiningStartRoom::Find(World))
    {Data->Pickaxe=Room->bPickaxeUnlocked;Data->Jetpack=Room->bJetpackUnlocked;Data->Headlamp=Room->bHeadlampUnlocked;}
    CaptureRoomState(Data);
    Data->RunCollectedItems=RunCollectedItems;
    Data->CollectedMinerals=RestoredMinerals;
    for(TActorIterator<AMiningTreasure> It(World);It;++It)
        if(It->bCollected && It->SavePlacementIndex!=INDEX_NONE)Data->CollectedMinerals.Add(It->SavePlacementIndex);
    for(TActorIterator<ADiggableRock> It(World);It;++It)
    {
        const auto Capture=[&](UDynamicMeshComponent* Component,int32 Chunk,int32 Cuts)
        {
            if(!Component || Cuts<=0)return;
            if(bAsync)
            {
                const FString Key=RockKey(*It)+FString::Printf(TEXT("#%d"),Chunk);
                const auto* Cached=MeshCache.Find(Key);check(Cached && Cached->Cuts==Cuts);
                Data->Meshes.Add(*Cached);Data->Meshes.Last().ActorCuts=It->SuccessfulCuts;return;
            }
            auto& Mesh=Data->Meshes.AddDefaulted_GetRef();Mesh.ActorKey=RockKey(*It);Mesh.Chunk=Chunk;Mesh.Cuts=Cuts;Mesh.ActorCuts=It->SuccessfulCuts;
            FMemoryWriter Writer(Mesh.Data,true);
            Component->GetDynamicMesh()->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Source){auto Copy=Source;Copy.Serialize(Writer);});
        };
        if(auto* Generated=Cast<AGeneratedMiningRock>(*It);Generated && Generated->bChunked)
        {for(int32 I=0;I<Generated->Chunks.Num();++I)Capture(Generated->Chunks[I].Mesh,I,Generated->Chunks[I].Cuts);}
        else Capture(It->RockMesh,INDEX_NONE,It->SuccessfulCuts);
    }
    TArray<uint8> Bytes;if(!UGameplayStatics::SaveGameToMemory(Data,Bytes))return false;
    auto* System=IPlatformFeaturesModule::Get().GetSaveGameSystem();if(!System)return false;
    auto Write=[System,Slot=GetSaveSlot(),Bytes=MoveTemp(Bytes)]()
    {
        if(!System->SaveGame(false,*(Slot+TEXT("_pending")),0,Bytes))return false;
        TArray<uint8> Previous;
        if(System->LoadGame(false,*Slot,0,Previous))
            if(!System->SaveGame(false,*(Slot+TEXT("_backup")),0,Previous))return false;
        return System->SaveGame(false,*Slot,0,Bytes);
    };
    LastSnapshotMilliseconds=(FPlatformTime::Seconds()-Started)*1000;
    UE_LOG(LogTemp,Display,TEXT("Progress snapshot: area %d, %d meshes, game thread %.2f ms, async=%d"),Data->Area,Data->Meshes.Num(),LastSnapshotMilliseconds,bAsync);
    if(Data->Checkpoint)bAsync=false; // Milestones must reach disk before play can advance.
    if(bAsync){FrozenCheckpoint=nullptr;WriteTask=Async(EAsyncExecution::ThreadPool,MoveTemp(Write));return true;}
    const bool Saved=Write();
    if(Saved){FrozenCheckpoint=Data->Checkpoint?Data:nullptr;SaveAge=0;CachedHasSave=true;UE_LOG(LogTemp,Display,TEXT("Progress saved: area %d, %d edited meshes"),Data->Area,Data->Meshes.Num());}
    return Saved;
}

bool UMiningSaveSubsystem::LoadProgress()
{
    FinishPendingWrite();if(!HasSave())return false;CompletionNotices.Reset();
    MeshCache.Reset();MeshTask=TFuture<FMiningSavedMesh>();TutorialSoil.Reset();
    auto* Hall=AMiningExhibitHall::Find(GetWorld());
    auto* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;
    if(!Hall || !Hall->bReady || !PC || !PC->GetPawn())return false;
    TMap<FString,ADiggableRock*> Rocks;
    for(TActorIterator<ADiggableRock> It(GetWorld());It;++It)Rocks.Add(RockKey(*It),*It);
    UMiningSaveGame* Data=nullptr;
    // Validate references before changing live state, including recovery candidates.
    for(const FString& Slot:RecoverySlots())
    {
        if(!UGameplayStatics::DoesSaveGameExist(Slot,0))continue;
        auto* Candidate=Cast<UMiningSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot,0));
        if(!ValidSnapshot(Candidate) || !Hall->Areas.IsValidIndex(Candidate->Area))continue;
        // The two runtime escape walls formerly used UObject sequence numbers.
        // Recover those old keys only when class and full local bounds identify
        // exactly one known wall; never discard an arbitrary missing terrain edit.
        if(Candidate->Version<=5)for(const auto& Saved:Candidate->Meshes)
        {
            if(Rocks.Contains(Saved.ActorKey) || Saved.Chunk!=INDEX_NONE ||
                (!Saved.ActorKey.Contains(TEXT(":DiggableRock_")) && !Saved.ActorKey.Contains(TEXT(":GeneratedMiningRock_"))))continue;
            FMemoryReader Reader(Saved.Data,true);UE::Geometry::FDynamicMesh3 Geometry;Geometry.Serialize(Reader);
            const auto Bounds=Geometry.GetBounds();ADiggableRock* Match=nullptr;int32 Matches=0;
            for(TActorIterator<ADiggableRock> It(GetWorld());It;++It)
            {
                if(!It->ActorHasTag(TEXT("BM_SaveId_CardBreach")) && !It->ActorHasTag(TEXT("BM_SaveId_StageFacing")))continue;
                It->RockMesh->GetDynamicMesh()->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Current)
                {
                    const auto B=Current.GetBounds();
                    if((B.Min-Bounds.Min).Length()<2 && (B.Max-Bounds.Max).Length()<2){Match=*It;++Matches;}
                });
            }
            if(Matches==1){Rocks.Add(Saved.ActorKey,Match);UE_LOG(LogTemp,Display,TEXT("Migrated runtime terrain %s to %s"),*Saved.ActorKey,*RockKey(Match));}
        }
        bool Valid=true;
        for(const auto& Saved:Candidate->Meshes)
        {
            if(IsLegacyTransientTVMesh(Candidate,Saved))continue;
            auto* Rock=Rocks.FindRef(Saved.ActorKey);if(!Rock){UE_LOG(LogTemp,Warning,TEXT("Save reference missing: %s"),*Saved.ActorKey);Valid=false;break;}
            if(Saved.Chunk!=INDEX_NONE)
            {auto* Generated=Cast<AGeneratedMiningRock>(Rock);if(!Generated || !Generated->Chunks.IsValidIndex(Saved.Chunk)){Valid=false;break;}}
        }
        if(Valid){Data=Candidate;break;}
    }
    if(!Data)return false;
    if(Data->Version<6)
    {
        // Old files saved equipment but not room phases. Resume a completed duel
        // at the safe reward milestone instead of putting an unlocked drill into
        // a fresh encounter. Completed campaign areas remain completed.
        int32 JourneyArea=INDEX_NONE,OfficeArea=INDEX_NONE;
        for(int32 I=0;I<Hall->Areas.Num();++I)
        {
            if(Hall->AreaKind(I)==EMiningExperienceKind::Journey)JourneyArea=I;
            if(Hall->AreaKind(I)==EMiningExperienceKind::Office)OfficeArea=I;
        }
        if(Data->CompletedAreas.Contains(JourneyArea))Data->CardState=3;
        else if(Data->Area==JourneyArea && Data->Drill)
        {
            Data->CardState=2;Data->Checkpoint=4;Data->Drill=false;
            for(TActorIterator<ACardRoomStage> It(GetWorld());It;++It)
            {
                Data->PlayerTransform=FTransform(It->GetActorRotation(),It->GetActorTransform().TransformPosition(FVector(-75,0,51)));
                Data->ViewRotation=It->GetActorRotation();break;
            }
        }
        if(Data->CompletedAreas.Contains(OfficeArea))Data->OfficeState=3;
        else if(Data->Area==OfficeArea && Data->Bomb){Data->OfficeState=2;Data->Checkpoint=2;Data->Bomb=false;}
        if(Hall->AreaKind(Data->Area)==EMiningExperienceKind::Ending)Data->Checkpoint=5;
        if(Data->CardState>0)Data->EchoCollapsed=Data->EchoLanded=Data->EchoCompleted=true;
    }
    auto* Achievements=GetGameInstance()->GetSubsystem<UMiningAchievementSubsystem>();
    Achievements->BeginRun(false);
    bRestoring=true;
    auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
    Wallet->Balance=Data->Balance;Wallet->Minerals=Data->Minerals;Wallet->CollectedMineralTotals=Data->CollectedMineralTotals;Wallet->UpgradeLevels=Data->Upgrades;
    TutorialActive=Data->TutorialActive;TutorialCompleted=Data->TutorialCompleted;PrototypesFound=Data->PrototypesFound;
    TutorialSoilPassed=Data->TutorialSoilPassed;
    // Older releases already showed the later objectives. Keep progressed saves
    // there, including players past this wall who have not collected anything yet.
    if(Data->Version==1)
    {
        TutorialSoilPassed=Data->Area>0 || TutorialCompleted!=0 || PrototypesFound>0;
        for(const auto& Pair:Rocks)
            if(Pair.Value->ActorHasTag(TEXT("BM_MainGame_StartRoomJunction")))
                TutorialSoilPassed|=Pair.Value->GetActorTransform().InverseTransformPosition(Data->PlayerTransform.GetLocation()).X>Pair.Value->RockSize.X*.5f;
    }
    Wallet->bBombSkillUnlocked=Data->Bomb;Wallet->bDrillSkillUnlocked=Data->Drill;
    GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->CampaignCollection=Data->Collection;
    if(Hall->Collection){Hall->Collection->Inventory=Data->Collection;Hall->Collection->Inventory.SetNumZeroed(AMiningCollectible::ItemTypeCount);}
    Hall->UnlockedArea=FMath::Clamp(Data->UnlockedArea,Data->Area,Hall->Areas.Num()-1);
    Hall->RestoreCompletedAreas(Data->CompletedAreas);
    for(const auto& Saved:Data->Meshes)
    {
        if(IsLegacyTransientTVMesh(Data,Saved))continue;
        MeshCache.Add(Saved.ActorKey+FString::Printf(TEXT("#%d"),Saved.Chunk),Saved);
        auto* Rock=Rocks.FindRef(Saved.ActorKey);
        FMemoryReader Reader(Saved.Data,true);UE::Geometry::FDynamicMesh3 Geometry;Geometry.Serialize(Reader);
        if(auto* Generated=Cast<AGeneratedMiningRock>(Rock);Generated && Saved.Chunk!=INDEX_NONE)
        {
            auto& Chunk=Generated->Chunks[Saved.Chunk];Generated->EnsureChunkComponent(Chunk);
            if(Chunk.bRuntimeDeferred){Chunk.bRuntimeDeferred=false;--Generated->DeferredRuntimeChunks;}
            Chunk.Cuts=Saved.Cuts;Chunk.Triangles=Geometry.TriangleCount();
            Chunk.Mesh->SetMesh(MoveTemp(Geometry));Chunk.Mesh->UpdateCollision(false);
            Chunk.Mesh->SetVisibility(Chunk.Triangles>0);Chunk.Mesh->SetCollisionEnabled(Chunk.Triangles>0?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
            if(!Chunk.Mesh->IsRegistered())Chunk.Mesh->RegisterComponent();
            Rock->SuccessfulCuts=Saved.ActorCuts;Generated->UpdateChunkStats();
        }
        else {Rock->CommitMesh(MoveTemp(Geometry));Rock->SuccessfulCuts=Saved.Cuts;}
        Rock->OnTerrainCommitted.Broadcast(FBox(ForceInit));
        UE_LOG(LogTemp,Display,TEXT("Restored terrain %s cuts=%d saved=%d triangles=%d"),*Rock->GetName(),Rock->SuccessfulCuts,Saved.ActorCuts,Rock->TriangleCount);
    }
    Hall->EnterArea(Data->Area,false);
    AGeneratedMiningRock::PrepareTerrainAt(GetWorld(),Data->PlayerTransform.GetLocation(),1400);
    PC->GetPawn()->SetActorTransform(Data->PlayerTransform,false,nullptr,ETeleportType::TeleportPhysics);
    PC->SetControlRotation(Data->ViewRotation);
    if(auto* Move=PC->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>()){Move->StopMovementImmediately();Move->ResetFlightInput();}
    if(auto* Room=AMiningStartRoom::Find(GetWorld()))Room->RestoreEquipment(Data->Pickaxe,Data->Jetpack,Data->Headlamp);
    if(auto* Progression=PC->GetPawn()->FindComponentByClass<UMiningProgressionComponent>())Progression->ApplyUpgrades();
    RunCollectedItems=Data->RunCollectedItems;
    if(Data->Version<3)for(int32 I=0;I<Data->Collection.Num();++I)if(Data->Collection[I]>0)RunCollectedItems.Add(I);
    RestoredMinerals=Data->CollectedMinerals;RestoreCollectedItems();
    // Migrate old saves from stable authored placement IDs, never the spendable
    // wallet (which may contain GM grants or have paid for upgrades).
    if(Data->Version<5 && Hall->Treasure)
        for(int32 Index:Data->CollectedMinerals)
            if(Hall->Treasure->AuthoredPlacements.IsValidIndex(Index))
            {
                const FName Id=Hall->Treasure->AuthoredPlacements[Index].TreasureId;
                const EMiningMineral M=Id==TEXT("2")?EMiningMineral::Copper:Id==TEXT("4")?EMiningMineral::Iron:Id==TEXT("6")?EMiningMineral::Malachite:Id==TEXT("9")?EMiningMineral::Diamond:EMiningMineral::None;
                if(FMiningMineralCost::IsSupported(M))++Wallet->CollectedMineralTotals.FindOrAdd(M);
            }
    if(Hall->Collection)Hall->Collection->CheckMineralRewards(Data->Version<5);
    FrozenCheckpoint=Data->Checkpoint?Data:nullptr;PendingRoomRestore=Data;
    bRestoring=false;bLoadedProgress=true;bSessionStarted=true;SessionWorld=GetWorld();SaveAge=0;PreviousArea=Data->Area;
    Achievements->BeginRun(Data->AchievementEligible && UMiningAchievementSubsystem::IsSaveOwnerCompatible(Data->AchievementOwner,Achievements->GetOwnerId()));
    Achievements->ReconcileSave(Data->Collection,Data->Upgrades.IsValidIndex(0)?Data->Upgrades[0]:0,Data->CampaignComplete,Data->AchievementOwner,Data->HasVerifiedAchievementCollection?&Data->AchievementCollectionIds:nullptr);
    return true;
}

void UMiningSaveSubsystem::RestoreCollectedItems()
{
    for(TActorIterator<AMiningTreasure> It(GetWorld());It;++It)
        if(!It->bCollected && RestoredMinerals.Contains(It->SavePlacementIndex))
        {It->bCollected=true;It->DropBody->SetSimulatePhysics(false);It->SetActorHiddenInGame(true);It->SetActorEnableCollision(false);}
    for(TActorIterator<AMiningCollectible> It(GetWorld());It;++It)
        if(!It->bCollected && !It->ActorHasTag(TEXT("CollectionPreview")) && RunCollectedItems.Contains(It->ItemId))
        {
            It->bCollected=true;It->Body->SetSimulatePhysics(false);It->SetActorHiddenInGame(true);It->SetActorEnableCollision(false);
            if(IsValid(It->PresentationActor)){It->PresentationActor->SetActorHiddenInGame(true);It->PresentationActor->SetActorEnableCollision(false);}
        }
}

void UMiningSaveSubsystem::Tick(float Dt)
{
    if(WriteTask.IsValid() && WriteTask.IsReady())FinishPendingWrite();
    auto* World=GetWorld();if(!World || !World->IsGameWorld() || !World->GetMapName().EndsWith(TEXT("Lvl_MainGame")))return;
    auto* Hall=AMiningExhibitHall::Find(World);auto* PC=World->GetFirstPlayerController();
    auto* Character=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
    if(!Hall || !Hall->bReady || !Character || PC->IsPaused() || AMiningOpening::IsOpening(World))return;
    if(SessionWorld.Get()!=World){bSessionStarted=false;FallingSeconds=0;SessionWorld=World;}
    if(!bSessionStarted)return;
    if(PendingRoomRestore){RestoreRoomState();return;}
    auto* Movement=Character->GetCharacterMovement();
    // Only ordinary player-controlled falls qualify. Scripted room/camera travel is excluded.
    const bool Falling=PC->GetViewTarget()==Character && !PC->IsMoveInputIgnored() && !PC->bShowMouseCursor &&
        Movement->IsFalling() && Movement->Velocity.Z < -600.f;
    FallingSeconds=Falling?FallingSeconds+Dt:0;
    if(FallingSeconds>=3.f)
    {
        FHitResult Ground;FCollisionQueryParams Query(SCENE_QUERY_STAT(VoidRecovery),false,Character);
        if(!World->LineTraceSingleByChannel(Ground,Character->GetActorLocation(),Character->GetActorLocation()-FVector(0,0,2500),ECC_WorldStatic,Query))
        {
            Hall->ReturnToLobby();Movement->StopMovementImmediately();FallingSeconds=0;
            if(auto* Subtitles=USubtitleSubsystem::Get(this))Subtitles->PlayInteractionLines(TEXT("VoidRecovery"),{MiningText::Get(TEXT("UI.World.VoidRecovery"))});
        }
    }
    ProbeAge+=Dt;SaveAge+=Dt;
    const int32 Mode=CheckpointMode();
    const bool NewCheckpoint=Mode!=(FrozenCheckpoint?FrozenCheckpoint->Checkpoint:0);
    const bool WantsSave=NewCheckpoint || (!FrozenCheckpoint && (Hall->ActiveArea!=PreviousArea || SaveAge>=10.f));
    if(WantsSave && !WriteTask.IsValid() && (NewCheckpoint || (!PC->IsMoveInputIgnored() && !PC->bShowMouseCursor && Movement->IsMovingOnGround())))
        if(PrepareMeshCache() && SaveProgressInternal(true))PreviousArea=Hall->ActiveArea;
    if(ProbeAge<.5f)return;ProbeAge=0;
    if(bLoadedProgress)RestoreCollectedItems();
    UpdateTutorial();
    if(auto* Progression=Character->FindComponentByClass<UMiningProgressionComponent>();Progression && Progression->CanUpgradePickaxe() && UpgradeHintLevel!=Progression->GetLevel(0))
        if(auto* Subtitles=USubtitleSubsystem::Get(this);Subtitles && !Subtitles->IsPlaying() && !PC->bShowMouseCursor)
        {Subtitles->PlayStatusLine(MiningText::Get(TEXT("UI.Tutorial.UpgradeReady")),4.f);UpgradeHintLevel=Progression->GetLevel(0);}

}

void UMiningSaveSubsystem::RecordCollectible(int32 Id)
{
    if(!bRestoring)GetGameInstance()->GetSubsystem<UMiningAchievementSubsystem>()->RecordCollection(Id);
    // Preserve the legacy save bit without presenting the retired collectible objective.
    RunCollectedItems.Add(Id);TutorialCompleted|=8;
}
void UMiningSaveSubsystem::RecordMineral(EMiningMineral Mineral)
{
    if(!FMiningMineralCost::IsSupported(Mineral) || bRestoring)return;
    auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
    int32& Total=Wallet->CollectedMineralTotals.FindOrAdd(Mineral);Total=FMath::Min(Total,MAX_int32-1)+1;
    // This path runs only after an actual pickup, never for wallet grants or save restoration.
    if(Mineral==EMiningMineral::Diamond)
        GetGameInstance()->GetSubsystem<UMiningAchievementSubsystem>()->RecordMilestone(TEXT("ACH_FIRST_DIAMOND"));
    if(auto* Hall=AMiningExhibitHall::Find(GetWorld());Hall && Hall->Collection)Hall->Collection->CheckMineralRewards();
    if(TutorialActive && TutorialSoilPassed)
    {
        if(Mineral==EMiningMineral::Copper && !(TutorialCompleted&1))CompletionNotices.Add(MiningText::Get(TEXT("UI.Tutorial.Copper")));
        if(Mineral==EMiningMineral::Iron && !(TutorialCompleted&2))CompletionNotices.Add(MiningText::Get(TEXT("UI.Tutorial.Iron")));
    }
    if(Mineral==EMiningMineral::Copper)TutorialCompleted|=1;
    if(Mineral==EMiningMineral::Iron)TutorialCompleted|=2;
}

void UMiningSaveSubsystem::UpdateTutorial()
{
    const int32 Before=TutorialCompleted,BeforePrototypes=PrototypesFound;
    auto* Hall=AMiningExhibitHall::Find(GetWorld());
    APawn* Pawn=GetWorld()->GetFirstPlayerController()->GetPawn();
    const FVector Position=Pawn->GetActorLocation();
    if(!TutorialSoilPassed && !TutorialSoil.IsValid())
        for(TActorIterator<ADiggableRock> It(GetWorld());It;++It)
            if(It->ActorHasTag(TEXT("BM_MainGame_StartRoomJunction"))){TutorialSoil=*It;break;}
    ADiggableRock* Soil=TutorialSoil.Get();
    if(!TutorialActive)
    {
        // The second soil wall is the upper-shaft junction, before the first mine.
        if(Soil && Soil->GetComponentsBoundingBox(true).ComputeSquaredDistanceToPoint(Position)<FMath::Square(650.f))TutorialActive=true;
        if(Hall->ActiveArea>0)TutorialActive=true;
        if(TutorialActive)
            if(auto* Subtitles=USubtitleSubsystem::Get(this))
                Subtitles->PlayInteractionLines(TEXT("TutorialIntro"),{MiningText::Get(TEXT("UI.Tutorial.SoftSoil")),MiningText::Get(TEXT("UI.Tutorial.GuideHint"))});
    }
    if(TutorialActive && !TutorialSoilPassed && Soil && Soil->SuccessfulCuts>0 && !Soil->IsTerrainCutPending())
    {
        // Authored dimensions remain stable after excavation changes mesh bounds.
        // Require the entire capsule beyond this specific wall's exit face, inside
        // its corridor. Mining a hole while standing in front is not completion.
        const FVector Local=Soil->GetActorTransform().InverseTransformPosition(Position);
        const FVector Half=Soil->RockSize*.5f;
        const auto* Character=Cast<ACharacter>(Pawn);
        const float Radius=Character?Character->GetCapsuleComponent()->GetScaledCapsuleRadius():34.f;
        const float LocalRadius=Radius/FMath::Max(FMath::Abs(Soil->GetActorScale3D().X),.01f);
        if(Local.X>Half.X+LocalRadius && Local.X<Half.X+2000.f && FMath::Abs(Local.Y)<Half.Y && FMath::Abs(Local.Z)<Half.Z)
        {
            TutorialSoilPassed=true;CompletionNotices.Add(MiningText::Get(TEXT("UI.Tutorial.Soil")));
        }
    }
    auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
    if(Wallet->GetUpgradeLevel(0)>0)TutorialCompleted|=4;
    if(Wallet->GetUpgradeLevel(0)>=2)TutorialCompleted|=16;
    if(!RunCollectedItems.IsEmpty())TutorialCompleted|=8;
    const auto Kind=Hall->AreaKind(Hall->ActiveArea);
    if(Kind==EMiningExperienceKind::Office)PrototypesFound=FMath::Max(PrototypesFound,1);
    if(Kind==EMiningExperienceKind::Journey)
    {
        for(TActorIterator<AEchoPassageStage> It(GetWorld());It;++It)
        {
            // Same landing and room-entry latches used by the narration and exit chime.
            if(It->bLanded)PrototypesFound=FMath::Max(PrototypesFound,2);
            if(It->bCompleted)PrototypesFound=FMath::Max(PrototypesFound,3);
        }
    }
    if(Kind==EMiningExperienceKind::Museum)PrototypesFound=4;
    if(TutorialActive && TutorialSoilPassed)
    {
        if(!(Before&4) && (TutorialCompleted&4) && (TutorialCompleted&3)==3)CompletionNotices.Add(MiningText::Get(TEXT("UI.Tutorial.Upgrade1")));
        if(!(Before&16) && (TutorialCompleted&16) && (TutorialCompleted&3)==3)CompletionNotices.Add(MiningText::Get(TEXT("UI.Tutorial.Upgrade2")));
        for(int32 I=BeforePrototypes;I<PrototypesFound && I<4;++I)CompletionNotices.Add(FText::Format(MiningText::Get(TEXT("UI.Tutorial.Prototype")),I+1));
    }
}
