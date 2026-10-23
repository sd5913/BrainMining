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
        return Level+TEXT(":")+Actor->GetName();
    }
    bool ValidSnapshot(const UMiningSaveGame* Data)
    {
        return Data && Data->Version==1 && Data->Upgrades.Num()==4 && Data->Area>=0 &&
            !Data->PlayerTransform.ContainsNaN() && !Data->ViewRotation.ContainsNaN();
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
void UMiningSaveSubsystem::StartNewGame()
{
    bSessionStarted=true;bLoadedProgress=false;SessionWorld=GetWorld();SaveAge=0;PreviousArea=INDEX_NONE;
    FallingSeconds=0;UpgradeHintLevel=INDEX_NONE;RestoredMinerals.Reset();
    TutorialActive=false;TutorialCompleted=0;PrototypesFound=0;
    // Keep the previous slot until a new valid snapshot has successfully saved.
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
    auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
    Data->Area=Hall->ActiveArea;Data->UnlockedArea=Hall->UnlockedArea;Data->CompletedAreas=Hall->GetCompletedAreas();
    Data->PlayerTransform=Character->GetActorTransform();Data->ViewRotation=PC->GetControlRotation();
    // Room minigames restart at their entrance; inventory and finished sections persist.
    if(Hall->AreaKind(Data->Area)!=EMiningExperienceKind::Mining)
    {Data->PlayerTransform=Hall->Areas[Data->Area].Entry;Data->ViewRotation=Data->PlayerTransform.Rotator();}
    Data->Balance=Wallet->Balance;Data->Minerals=Wallet->Minerals;Data->Upgrades=Wallet->UpgradeLevels;
    Data->TutorialActive=TutorialActive;Data->TutorialCompleted=TutorialCompleted;Data->PrototypesFound=PrototypesFound;
    Data->Bomb=Wallet->bBombSkillUnlocked;Data->Drill=Wallet->bDrillSkillUnlocked;
    Data->Collection=GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->CampaignCollection;
    if(Hall->Collection)Data->Collection=Hall->Collection->Inventory;
    if(auto* Room=AMiningStartRoom::Find(World))
    {Data->Pickaxe=Room->bPickaxeUnlocked;Data->Jetpack=Room->bJetpackUnlocked;Data->Headlamp=Room->bHeadlampUnlocked;}
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
    Wallet->bBombSkillUnlocked=Data->Bomb;Wallet->bDrillSkillUnlocked=Data->Drill;
    GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->CampaignCollection=Data->Collection;
    if(Hall->Collection)Hall->Collection->Inventory=Data->Collection;
    Hall->UnlockedArea=FMath::Clamp(Data->UnlockedArea,Data->Area,Hall->Areas.Num()-1);
    Hall->RestoreCompletedAreas(Data->CompletedAreas);
    for(const auto& Saved:Data->Meshes)
    {
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
    RestoredMinerals=Data->CollectedMinerals;RestoreCollectedItems();
    bRestoring=false;bLoadedProgress=true;bSessionStarted=true;SessionWorld=GetWorld();SaveAge=0;PreviousArea=Data->Area;
    return true;
}

void UMiningSaveSubsystem::RestoreCollectedItems()
{
    for(TActorIterator<AMiningTreasure> It(GetWorld());It;++It)
        if(RestoredMinerals.Contains(It->SavePlacementIndex))
        {It->bCollected=true;It->DropBody->SetSimulatePhysics(false);It->SetActorHiddenInGame(true);It->SetActorEnableCollision(false);}
    const auto& Owned=GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->CampaignCollection;
    for(TActorIterator<AMiningCollectible> It(GetWorld());It;++It)
        if(!It->ActorHasTag(TEXT("CollectionPreview")) && Owned.IsValidIndex(It->ItemId) && Owned[It->ItemId]>0)
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
    if(!TutorialActive)
    {
        // The second soil wall is the upper-shaft junction, before the first mine.
        for(TActorIterator<ADiggableRock> It(GetWorld());It;++It)
            if(It->ActorHasTag(TEXT("BM_MainGame_StartRoomJunction")) &&
               It->GetComponentsBoundingBox(true).ComputeSquaredDistanceToPoint(Position)<FMath::Square(650.f))
            {TutorialActive=true;break;}
        if(Hall->ActiveArea>0)TutorialActive=true;
        if(TutorialActive)
            if(auto* Subtitles=USubtitleSubsystem::Get(this))
                Subtitles->PlayInteractionLines(TEXT("TutorialIntro"),{FText::FromString(TEXT("没思路的话就看看左侧引导栏吧"))});
    }
    auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
    if(Wallet->GetUpgradeLevel(0)>0)TutorialCompleted|=4;
    if(Hall->Collection && Hall->Collection->Inventory.ContainsByPredicate([](int32 Count){return Count>0;}))TutorialCompleted|=8;
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
    Sign->SetDecalMaterial(Material);Sign->DecalSize=FVector(16,65,65);Sign->SetFadeScreenSize(.002f);
    Sign->RegisterComponent();Sign->SetWorldLocationAndRotation(Hit.ImpactPoint+Hit.ImpactNormal*4,FRotationMatrix::MakeFromXY(-Hit.ImpactNormal,-FVector::UpVector).Rotator());
}
