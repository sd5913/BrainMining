#include "MiningSaveSubsystem.h"
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
        return Data && Data->Version>=1 && Data->Version<=4 && Data->Upgrades.Num()==4 && Data->Area>=0 &&
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
    for(const FString& Slot:RecoverySlots())
        if(UGameplayStatics::DoesSaveGameExist(Slot,0) && ValidSnapshot(Cast<UMiningSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot,0))))return true;
    return false;
}
TArray<int32> UMiningSaveSubsystem::GetCollectionProgress() const
{
    TArray<int32> Result=GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->CampaignCollection;
    for(const FString& Slot:RecoverySlots())
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
    bSessionStarted=true;bLoadedProgress=false;SessionWorld=GetWorld();SaveAge=0;PreviousArea=INDEX_NONE;
    FallingSeconds=0;UpgradeHintLevel=INDEX_NONE;RestoredMinerals.Reset();
    TutorialActive=false;TutorialSoilPassed=false;TutorialCompleted=0;PrototypesFound=0;
    const TArray<int32> Collection=bKeepCollection?GetCollectionProgress():TArray<int32>();
    auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
    Wallet->Balance=0;Wallet->Minerals.Reset();Wallet->UpgradeLevels={0,0,0,0};Wallet->MineralDoubleChance=0;
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
}

bool UMiningSaveSubsystem::SaveProgress()
{
    UWorld* World=GetWorld();
    auto* Hall=AMiningExhibitHall::Find(World);
    auto* PC=World?World->GetFirstPlayerController():nullptr;
    auto* Character=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
    if(!bSessionStarted || bRestoring || !World || !World->GetMapName().EndsWith(TEXT("Lvl_MainGame")) ||
       !Hall || !Hall->bReady || !Hall->Areas.IsValidIndex(Hall->ActiveArea) || !Character ||
       AMiningOpening::IsOpening(World) || FallingSeconds>.1f)return false;
    // Never capture a half-committed boolean mesh or an in-flight simplification.
    for(TActorIterator<ADiggableRock> It(World);It;++It)
    {
        if(It->IsTerrainCutPending())return false;
        if(auto* Generated=Cast<AGeneratedMiningRock>(*It);Generated && Generated->bSimplificationPending)return false;
    }
    auto* Data=Cast<UMiningSaveGame>(UGameplayStatics::CreateSaveGameObject(UMiningSaveGame::StaticClass()));
    Data->Version=4;
    auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
    Data->Area=Hall->ActiveArea;Data->UnlockedArea=Hall->UnlockedArea;Data->CompletedAreas=Hall->GetCompletedAreas();
    Data->PlayerTransform=Character->GetActorTransform();Data->ViewRotation=PC->GetControlRotation();
    // Room minigames restart at their entrance; inventory and finished sections persist.
    if(Hall->AreaKind(Data->Area)!=EMiningExperienceKind::Mining)
    {Data->PlayerTransform=Hall->Areas[Data->Area].Entry;Data->ViewRotation=Data->PlayerTransform.Rotator();}
    Data->Balance=Wallet->Balance;Data->Minerals=Wallet->Minerals;Data->Upgrades=Wallet->UpgradeLevels;
    Data->TutorialActive=TutorialActive;Data->TutorialCompleted=TutorialCompleted;Data->PrototypesFound=PrototypesFound;
    Data->TutorialSoilPassed=TutorialSoilPassed;
    Data->Bomb=Wallet->bBombSkillUnlocked;Data->Drill=Wallet->bDrillSkillUnlocked;
    Data->Collection=GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->CampaignCollection;
    if(Hall->Collection)Data->Collection=Hall->Collection->Inventory;
    if(auto* Room=AMiningStartRoom::Find(World))
    {Data->Pickaxe=Room->bPickaxeUnlocked;Data->Jetpack=Room->bJetpackUnlocked;Data->Headlamp=Room->bHeadlampUnlocked;}
    Data->RunCollectedItems=RunCollectedItems;
    Data->CollectedMinerals=RestoredMinerals;
    for(TActorIterator<AMiningTreasure> It(World);It;++It)
        if(It->bCollected && It->SavePlacementIndex!=INDEX_NONE)Data->CollectedMinerals.Add(It->SavePlacementIndex);
    for(TActorIterator<ADiggableRock> It(World);It;++It)
    {
        const auto Capture=[&](UDynamicMeshComponent* Component,int32 Chunk,int32 Cuts)
        {
            if(!Component || Cuts<=0)return;
            auto& Mesh=Data->Meshes.AddDefaulted_GetRef();Mesh.ActorKey=RockKey(*It);Mesh.Chunk=Chunk;Mesh.Cuts=Cuts;Mesh.ActorCuts=It->SuccessfulCuts;
            FMemoryWriter Writer(Mesh.Data,true);
            Component->GetDynamicMesh()->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Source){auto Copy=Source;Copy.Serialize(Writer);});
        };
        if(auto* Generated=Cast<AGeneratedMiningRock>(*It);Generated && Generated->bChunked)
        {for(int32 I=0;I<Generated->Chunks.Num();++I)Capture(Generated->Chunks[I].Mesh,I,Generated->Chunks[I].Cuts);}
        else Capture(It->RockMesh,INDEX_NONE,It->SuccessfulCuts);
    }
    // Write a staging slot first; preserve the last working save if writing fails.
    if(!UGameplayStatics::SaveGameToSlot(Data,GetSaveSlot()+TEXT("_pending"),0))return false;
    if(UGameplayStatics::DoesSaveGameExist(GetSaveSlot(),0))
        if(auto* Previous=Cast<UMiningSaveGame>(UGameplayStatics::LoadGameFromSlot(GetSaveSlot(),0));ValidSnapshot(Previous))
            UGameplayStatics::SaveGameToSlot(Previous,GetSaveSlot()+TEXT("_backup"),0);
    const bool Saved=UGameplayStatics::SaveGameToSlot(Data,GetSaveSlot(),0);
    if(Saved){SaveAge=0;UE_LOG(LogTemp,Display,TEXT("Progress saved: area %d, %d edited meshes"),Data->Area,Data->Meshes.Num());}
    return Saved;
}

bool UMiningSaveSubsystem::LoadProgress()
{
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
        bool Valid=true;
        for(const auto& Saved:Candidate->Meshes)
        {
            if(IsLegacyTransientTVMesh(Candidate,Saved))continue;
            auto* Rock=Rocks.FindRef(Saved.ActorKey);if(!Rock){Valid=false;break;}
            if(Saved.Chunk!=INDEX_NONE)
            {auto* Generated=Cast<AGeneratedMiningRock>(Rock);if(!Generated || !Generated->Chunks.IsValidIndex(Saved.Chunk)){Valid=false;break;}}
        }
        if(Valid){Data=Candidate;break;}
    }
    if(!Data)return false;
    bRestoring=true;
    auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
    Wallet->Balance=Data->Balance;Wallet->Minerals=Data->Minerals;Wallet->UpgradeLevels=Data->Upgrades;
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
    if(Hall->Collection)Hall->Collection->Inventory=Data->Collection;
    Hall->UnlockedArea=FMath::Clamp(Data->UnlockedArea,Data->Area,Hall->Areas.Num()-1);
    Hall->RestoreCompletedAreas(Data->CompletedAreas);
    for(const auto& Saved:Data->Meshes)
    {
        if(IsLegacyTransientTVMesh(Data,Saved))continue;
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
    bRestoring=false;bLoadedProgress=true;bSessionStarted=true;SessionWorld=GetWorld();SaveAge=0;PreviousArea=Data->Area;
    return true;
}

void UMiningSaveSubsystem::RestoreCollectedItems()
{
    for(TActorIterator<AMiningTreasure> It(GetWorld());It;++It)
        if(RestoredMinerals.Contains(It->SavePlacementIndex))
        {It->bCollected=true;It->DropBody->SetSimulatePhysics(false);It->SetActorHiddenInGame(true);It->SetActorEnableCollision(false);}
    for(TActorIterator<AMiningCollectible> It(GetWorld());It;++It)
        if(!It->ActorHasTag(TEXT("CollectionPreview")) && RunCollectedItems.Contains(It->ItemId))
        {
            It->bCollected=true;It->Body->SetSimulatePhysics(false);It->SetActorHiddenInGame(true);It->SetActorEnableCollision(false);
            if(IsValid(It->PresentationActor)){It->PresentationActor->SetActorHiddenInGame(true);It->PresentationActor->SetActorEnableCollision(false);}
        }
}

void UMiningSaveSubsystem::Tick(float Dt)
{
    auto* World=GetWorld();if(!World || !World->IsGameWorld() || !World->GetMapName().EndsWith(TEXT("Lvl_MainGame")))return;
    auto* Hall=AMiningExhibitHall::Find(World);auto* PC=World->GetFirstPlayerController();
    auto* Character=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
    if(!Hall || !Hall->bReady || !Character || PC->IsPaused() || AMiningOpening::IsOpening(World))return;
    if(SessionWorld.Get()!=World){bSessionStarted=false;FallingSeconds=0;SessionWorld=World;}
    if(!bSessionStarted)return;
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
            if(auto* Subtitles=USubtitleSubsystem::Get(this))Subtitles->PlayInteractionLines(TEXT("VoidRecovery"),{FText::FromString(TEXT("呼，刚才想到什么来着？"))});
        }
    }
    ProbeAge+=Dt;SaveAge+=Dt;
    if(ProbeAge<.5f)return;ProbeAge=0;
    if(bLoadedProgress)RestoreCollectedItems();
    UpdateBombWallSigns();
    UpdateTutorial();
    if(auto* Progression=Character->FindComponentByClass<UMiningProgressionComponent>();Progression && Progression->CanUpgradePickaxe() && UpgradeHintLevel!=Progression->GetLevel(0))
        if(auto* Subtitles=USubtitleSubsystem::Get(this);Subtitles && !Subtitles->IsPlaying() && !PC->bShowMouseCursor)
        {Subtitles->PlayStatusLine(FText::FromString(TEXT("材料充足！按[Tab]升级矿镐")),4.f);UpgradeHintLevel=Progression->GetLevel(0);}
    if((Hall->ActiveArea!=PreviousArea || SaveAge>=60.f) && !PC->IsMoveInputIgnored() && !PC->bShowMouseCursor && Movement->IsMovingOnGround())
        if(SaveProgress())PreviousArea=Hall->ActiveArea;
}

void UMiningSaveSubsystem::RecordMineral(EMiningMineral Mineral)
{
    if(Mineral==EMiningMineral::Copper)TutorialCompleted|=1;
    if(Mineral==EMiningMineral::Iron)TutorialCompleted|=2;
}

void UMiningSaveSubsystem::UpdateTutorial()
{
    auto* Hall=AMiningExhibitHall::Find(GetWorld());
    APawn* Pawn=GetWorld()->GetFirstPlayerController()->GetPawn();
    const FVector Position=Pawn->GetActorLocation();
    ADiggableRock* Soil=nullptr;
    for(TActorIterator<ADiggableRock> It(GetWorld());It;++It)
        if(It->ActorHasTag(TEXT("BM_MainGame_StartRoomJunction"))){Soil=*It;break;}
    if(!TutorialActive)
    {
        // The second soil wall is the upper-shaft junction, before the first mine.
        if(Soil && Soil->GetComponentsBoundingBox(true).ComputeSquaredDistanceToPoint(Position)<FMath::Square(650.f))TutorialActive=true;
        if(Hall->ActiveArea>0)TutorialActive=true;
        if(TutorialActive)
            if(auto* Subtitles=USubtitleSubsystem::Get(this))
                Subtitles->PlayInteractionLines(TEXT("TutorialIntro"),{FText::FromString(TEXT("没思路的话就看看左侧引导栏吧"))});
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
            TutorialSoilPassed=true;
            if(auto* Subtitles=USubtitleSubsystem::Get(this))
                Subtitles->PlayInteractionLines(TEXT("TutorialSoilPassed"),{FText::FromString(TEXT("哦！软软的泥土，这边一定是路"))});
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
        PrototypesFound=FMath::Max(PrototypesFound,2);
        // Match the authored card-room entrance volume, not sitting/finishing the duel.
        for(TActorIterator<ACardRoomStage> It(GetWorld());It;++It)
        {
            const FVector Local=It->GetActorTransform().InverseTransformPosition(Position)-FVector(-75,0,115);
            if(FMath::Abs(Local.X)<850 && FMath::Abs(Local.Y)<900 && FMath::Abs(Local.Z)<500)
                PrototypesFound=FMath::Max(PrototypesFound,3);
        }
    }
    if(Kind==EMiningExperienceKind::Museum)PrototypesFound=4;
}

void UMiningSaveSubsystem::UpdateBombWallSigns()
{
    auto* PC=GetWorld()->GetFirstPlayerController();if(!PC || !PC->GetPawn())return;
    FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
    // Generated guide meshes can be offset far from their actor origin. Trace the
    // visible surface itself, so winding/offset walls receive the icon reliably.
    FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(BombWallSign),true,PC->GetPawn());
    if(!GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+View.Vector()*2200.f,ECC_Visibility,Query))return;
    auto* Rock=Cast<ADiggableRock>(Hit.GetActor());
    if(!Rock || !Rock->ActorHasTag(TEXT("BM_BombOnly")))return;
    TArray<UDecalComponent*> Decals;Rock->GetComponents(Decals);
    if(Decals.ContainsByPredicate([](const UDecalComponent* Decal){return Decal->GetFName()==TEXT("BombWallIcon");}))return;
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MainGame/Materials/M_BombWallIcon"));if(!Material)return;
    auto* Sign=NewObject<UDecalComponent>(Rock,TEXT("BombWallIcon"));Rock->AddInstanceComponent(Sign);Sign->SetupAttachment(Rock->GetRootComponent());
    Sign->SetDecalMaterial(Material);Sign->DecalSize=FVector(16,110,110);Sign->SetFadeScreenSize(.002f);
    Sign->RegisterComponent();Sign->SetWorldLocationAndRotation(Hit.ImpactPoint+Hit.ImpactNormal*4,FRotationMatrix::MakeFromXY(-Hit.ImpactNormal,-FVector::UpVector).Rotator());
}
