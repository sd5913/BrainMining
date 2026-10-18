#include "MiningGUIEditorLibrary.h"
#include "Showcase/MiningExhibitHall.h"
#include "MiningRoomEscape.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"

bool UMiningGUIEditorLibrary::PrepareCollectionPreviewArea(APlayerController* Controller,int32 AreaIndex,bool CompleteTelevision)
{
    if(!Controller || Controller->GetWorld()->WorldType!=EWorldType::PIE)return false;
    auto* Hall=AMiningExhibitHall::Find(Controller->GetWorld());
    if(!Hall || !Hall->bReady || !Hall->Areas.IsValidIndex(AreaIndex))return false;
    Hall->UnlockedArea=FMath::Max(Hall->UnlockedArea,AreaIndex);
    if(!Hall->EnterArea(AreaIndex,false))return false;
    if(CompleteTelevision)
        for(TActorIterator<AMiningRoomEscape> It(Controller->GetWorld());It;++It)It->Phase=EMiningEscapePhase::Escaped;
    return true;
}
