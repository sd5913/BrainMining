#include "Showcase/MiningExhibitHall.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Level.h"
#include "Engine/World.h"
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

void AMiningExhibitGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    auto* Hall=AMiningExhibitHall::Find(GetWorld());
    if(!Hall || !Hall->bContinuousCampaign) return;
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
            if(Actor->IsA<APlayerStart>()) {Actor->Destroy();continue;}
            if(Area.SuppressedSourceActors.Contains(Actor->GetFName()))
            {Actor->SetActorHiddenInGame(true);Actor->SetActorEnableCollision(false);Actor->SetActorTickEnabled(false);}
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
            Escape->CardStage=Cards;Escape->Entrance=Entrance;
        }
    }
}
