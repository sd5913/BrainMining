#include "Showcase/MiningExhibitHall.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Engine/PostProcessVolume.h"
#include "GameFramework/PlayerStart.h"
#include "PopupClose/PopupCloseOffice.h"
#include "PopupClose/PopupCloseStage.h"
#include "EchoPassageStage.h"
#include "CardRoomStage.h"
#include "MiningRoomEscape.h"
#include "MiningGiantRoomEntrance.h"
#include "MiningCellUniverse.h"
#include "WenSi/WenSiPuzzle.h"
#include "WenSi/WenSiMirrorPuzzle.h"
#include "Components/StaticMeshComponent.h"
#include "MiningWalletSubsystem.h"
#include "Engine/GameInstance.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/LightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "DynamicMeshActor.h"
#include "Components/DynamicMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "GeneratedMiningRock.h"

namespace
{
UPostProcessComponent* CreateSpatialExposure(AMiningExhibitHall* Hall,const FBox& Box,FName Tag,float Priority,float BlendRadius)
{
    FActorSpawnParameters Params;Params.OverrideLevel=Hall->GetLevel();
    auto* Region=Hall->GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),Box.GetCenter(),FRotator::ZeroRotator,Params);
    Region->Tags.Add(TEXT("BM_PlayerEffect"));Region->Tags.Add(Tag);
    auto* Bounds=NewObject<UBoxComponent>(Region);Region->SetRootComponent(Bounds);Region->AddInstanceComponent(Bounds);
    Bounds->SetBoxExtent(Box.GetExtent());Bounds->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    Bounds->SetGenerateOverlapEvents(false);Bounds->RegisterComponent();Bounds->SetWorldLocation(Box.GetCenter());
    auto* Exposure=NewObject<UPostProcessComponent>(Region);Region->AddInstanceComponent(Exposure);Exposure->SetupAttachment(Bounds);
    Exposure->Priority=Priority;Exposure->BlendRadius=BlendRadius;Exposure->BlendWeight=1;Exposure->bUnbound=false;Exposure->bEnabled=true;Exposure->RegisterComponent();
    return Exposure;
}

// Continuous rooms remain visible from the cave. Exposure depends on camera
// position rather than the gameplay activation threshold.
void LocalizeRoomExposure(AMiningExhibitHall* Hall, APostProcessVolume* Source, const FMiningExperienceArea& Area)
{
    if(!Source->bEnabled || Area.SuppressedSourceActors.Contains(Source->GetFName()) || !Area.ActivationBounds.IsValid) return;
    // Gameplay bounds include exterior connectors. Interior exposure there
    // erases the weak mining headlamp, particularly when looking back outside.
    FBox Interior=Area.ActivationBounds;
    if(Area.Tag==TEXT("BM_Campaign_MainGameOffice"))
    {Interior.Min.X=Area.Origin.X-1000;Interior.Max.X=Area.Origin.X+1000;}
    else if(Area.Tag==TEXT("BM_Campaign_MainGameMuseum"))
    {Interior.Min.X=Area.Origin.X-910;Interior.Max.X=Area.Origin.X+2460;}
    else if(Area.Tag==TEXT("BM_Campaign_MainGameJourney"))
    {Interior.Min.X=Area.Entry.GetLocation().X-220;Interior.Max.X=Area.Exit.X;}
    Interior.Min-=FVector(0,200,200);Interior.Max+=FVector(0,200,200);
    auto* Exposure=CreateSpatialExposure(Hall,Interior,TEXT("BM_MainGame_LocalRoomExposure"),Source->Priority,150);
    Exposure->GetOwner()->Tags.Add(Area.Tag);Exposure->Settings=Source->Settings;Exposure->BlendWeight=Source->BlendWeight;
    // Read source settings each run so prefab-style source edits still apply.
    Source->bEnabled=false;
}

void FinishNaturalRoomConnections(AMiningExhibitHall* Hall)
{
    if(!Hall->ActorHasTag(TEXT("BM_MainGame_NaturalRoomConnections"))) return;
    for(TActorIterator<AActor> It(Hall->GetWorld());It;++It)
    {
        AActor* Actor=*It;
        if(Actor->ActorHasTag(TEXT("BM_MainGame_NaturalRoomConnection")) && Actor->IsA<AGeneratedMiningRock>())
        {
            FVector Center,Extent;Actor->GetActorBounds(false,Center,Extent);
            // Clay plugs are short; only the six hollow rock collars get a zone.
            if(Extent.X>250)
            {
                FBox Passage(Center-Extent,Center+Extent);
                for(const auto& Area:Hall->Areas) if(Area.Kind==EMiningExperienceKind::Mining && Actor->ActorHasTag(Area.Tag))
                {if(Center.X>Area.Origin.X)Passage.Min.X-=650;else Passage.Max.X+=650;break;}
                auto* Exposure=CreateSpatialExposure(Hall,Passage,TEXT("BM_MainGame_PassageExposure"),10,200);
                auto& S=Exposure->Settings;
                S.bOverride_AutoExposureMethod=true;S.AutoExposureMethod=EAutoExposureMethod::AEM_Histogram;
                S.bOverride_AutoExposureMinBrightness=true;S.AutoExposureMinBrightness=-2;
                S.bOverride_AutoExposureMaxBrightness=true;S.AutoExposureMaxBrightness=4;
                S.bOverride_AutoExposureBias=true;S.AutoExposureBias=0;
                S.bOverride_AutoExposureApplyPhysicalCameraExposure=true;S.AutoExposureApplyPhysicalCameraExposure=false;
                S.bOverride_AutoExposureSpeedUp=true;S.AutoExposureSpeedUp=6;
                S.bOverride_AutoExposureSpeedDown=true;S.AutoExposureSpeedDown=6;
                // Meter visible highlights without crushing the surrounding
                // cave; local contrast retains both sides of a bright doorway.
                S.bOverride_AutoExposureLowPercent=true;S.AutoExposureLowPercent=90;
                S.bOverride_AutoExposureHighPercent=true;S.AutoExposureHighPercent=98;
                S.bOverride_LocalExposureMethod=true;S.LocalExposureMethod=ELocalExposureMethod::Bilateral;
                S.bOverride_LocalExposureHighlightContrastScale=true;S.LocalExposureHighlightContrastScale=.5f;
                S.bOverride_LocalExposureShadowContrastScale=true;S.LocalExposureShadowContrastScale=.35f;
                S.bOverride_LocalExposureDetailStrength=true;S.LocalExposureDetailStrength=1;
                S.bOverride_LocalExposureBlurredLuminanceBlend=true;S.LocalExposureBlurredLuminanceBlend=.5f;
            }
        }
        if(Actor->ActorHasTag(TEXT("BM_MainGame_NaturalRoomConnection")) && Actor->IsA<ADynamicMeshActor>())
            if(auto* Mesh=Actor->FindComponentByClass<UDynamicMeshComponent>())
            {
                Mesh->bUseAsyncCooking=false;Mesh->SetComplexAsSimpleCollisionEnabled(true,false);
                Mesh->GetBodySetup()->bDoubleSidedGeometry=true;Mesh->UpdateCollision(false);
            }
        bool bRoomActor=false;
        for(const auto& Area:Hall->Areas)
            if(Area.Kind!=EMiningExperienceKind::Mining && Area.SourceLevel &&
               (Actor->GetLevel()==Area.SourceLevel->GetLoadedLevel() || Actor->ActorHasTag(Area.Tag)))
            {bRoomActor=true;break;}
        if(!bRoomActor) continue;
        // Shadowless fill from isolated maps must not leak through cave/clay.
        TArray<ULightComponent*> Lights;Actor->GetComponents(Lights);
        for(auto* Light:Lights) Light->SetCastShadows(true);
        if(Actor->IsA<APopupCloseOffice>())
        {
            TArray<UInstancedStaticMeshComponent*> Groups;Actor->GetComponents(Groups);
            for(auto* Group:Groups) if(Group->GetFName()==TEXT("OfficeGroup0"))
                for(int32 I=Group->GetInstanceCount()-1;I>=0;--I)
                {
                    FTransform T;Group->GetInstanceTransform(I,T,false);
                    if(FMath::Abs(FMath::Abs(T.GetLocation().X)-1100.)<1.) Group->RemoveInstance(I);
                }
        }
        if(Actor->IsA<APopupCloseStage>())
        {
            TArray<UStaticMeshComponent*> Meshes;Actor->GetComponents(Meshes);
            for(auto* Mesh:Meshes) if(Mesh->GetName().StartsWith(TEXT("OfficeSafeExit")))
            {Mesh->SetVisibility(false);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);}
        }
    }
}
}

void AMiningExhibitGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    auto* Hall=AMiningExhibitHall::Find(GetWorld());
    if(!Hall || !Hall->bContinuousCampaign) return;
    // MainGame already owns bombs; retain the office pickup and breach sequence.
    if(Hall->ActorHasTag(TEXT("BM_MainGame_RoomsLinked")))
        if(auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>()) Wallet->bBombSkillUnlocked=true;
    // Always-loaded source levels are available before actor initialization/BeginPlay.
    // Overrides belong to this campaign instance; the source maps stay independently playable.
    for(const auto& Area:Hall->Areas)
    {
        if(!Area.SourceLevel) continue;
        ULevel* Level=Area.SourceLevel->GetLoadedLevel();
        if(!Level) {UE_LOG(LogTemp,Error,TEXT("Campaign source room not loaded: %s"),*Area.Tag.ToString());continue;}
        AMiningRoomEscape* Escape=nullptr;
        ACardRoomStage* Cards=nullptr;
        AMiningGiantRoomEntrance* Entrance=nullptr;
        const auto Actors=Level->Actors;
        for(AActor* Actor:Actors)
        {
            if(!IsValid(Actor)) continue;
            Actor->Tags.AddUnique(Area.Tag);
            if(Hall->ActorHasTag(TEXT("BM_MainGame_RoomsLinked")) && Area.Kind!=EMiningExperienceKind::Mining)
                if(auto* Volume=Cast<APostProcessVolume>(Actor)) LocalizeRoomExposure(Hall,Volume,Area);
            if(Actor->IsA<APlayerStart>()) {Actor->Destroy();continue;}
            if(Area.SuppressedSourceActors.Contains(Actor->GetFName()))
            {
                Actor->SetActorHiddenInGame(true);Actor->SetActorEnableCollision(false);Actor->SetActorTickEnabled(false);
                if(auto* Volume=Cast<APostProcessVolume>(Actor)) Volume->bEnabled=false;
            }
            if(auto* Office=Cast<APopupCloseOffice>(Actor)) Office->bWalkThroughConnections=true;
            if(auto* Stage=Cast<APopupCloseStage>(Actor)) {Stage->bAreaManaged=true;Stage->bStartOnAreaEntry=true;}
            if(auto* Echo=Cast<AEchoPassageStage>(Actor))
            {Echo->bWalkThroughConnections=true;Echo->bRoomInSameLevel=true;Echo->DestinationLevel.Reset();}
            if(auto* Puzzle=Cast<AWenSiPuzzle>(Actor)) {Puzzle->bAreaManaged=true;Puzzle->AreaPlayerStart=Area.Entry;}
            if(auto* Mirror=Cast<AWenSiMirrorPuzzle>(Actor)) {Mirror->bAreaManaged=true;Mirror->AreaPlayerStart=Area.Entry;}
            if(auto* Cell=Cast<AMiningCellUniverse>(Actor))
            {
                Cell->bAreaManaged=true;
                TArray<UStaticMeshComponent*> Components;Cell->GetComponents(Components);
                for(auto* Component:Components) if(Component->GetName().StartsWith(TEXT("BackWall")))
                {Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);Component->SetVisibility(false);}
            }
            if(auto* Found=Cast<AMiningRoomEscape>(Actor)) Escape=Found;
            if(auto* Found=Cast<ACardRoomStage>(Actor)) Cards=Found;
            if(auto* Found=Cast<AMiningGiantRoomEntrance>(Actor)) Entrance=Found;
        }
        if(Cards && Entrance)
        {
            if(!Escape)
            {
                FActorSpawnParameters P;P.OverrideLevel=Level;
                Escape=GetWorld()->SpawnActor<AMiningRoomEscape>(AMiningRoomEscape::StaticClass(),Area.Origin,FRotator::ZeroRotator,P);
                Escape->Tags.AddUnique(Area.Tag);
            }
            Escape->bAreaManaged=true;Escape->bWalkThroughConnections=true;
            Escape->SourceLevelOffset=Area.SourceLevel->LevelTransform.GetLocation();
            Escape->CardStage=Cards;Escape->Entrance=Entrance;
        }
    }
    // Source BeginPlay creates the original office vestibule meshes.
    GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Hall,[Hall](){FinishNaturalRoomConnections(Hall);}));
}
