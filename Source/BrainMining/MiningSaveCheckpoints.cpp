#include "MiningSaveSubsystem.h"
#include "Showcase/MiningExhibitHall.h"
#include "PopupClose/PopupCloseStage.h"
#include "MiningRoomEscape.h"
#include "CardRoomStage.h"
#include "CardRoomPlayer.h"
#include "EchoPassageStage.h"
#include "WenSi/WenSiMirrorPuzzle.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    template<class T> T* FirstCheckpointActor(UWorld* W) { TActorIterator<T> It(W);return It?*It:nullptr; }
    int32 CardState(UWorld* W)
    {
        if(auto* Escape=FirstCheckpointActor<AMiningRoomEscape>(W);Escape && Escape->Phase==EMiningEscapePhase::Escaped)return 3;
        if(auto* Stage=FirstCheckpointActor<ACardRoomStage>(W))
        {
            if(Stage->Run.Phase>=ECardRoomPhase::Victory)return 2;
            if(Stage->Run.Phase!=ECardRoomPhase::Exploring)return 1;
        }
        return 0;
    }
}

int32 UMiningSaveSubsystem::CheckpointMode() const
{
    auto* W=GetWorld();auto* Hall=AMiningExhibitHall::Find(W);
    if(!Hall)return 0;
    if(FrozenCheckpoint && FrozenCheckpoint->Checkpoint==5)return 5;
    switch(Hall->AreaKind(Hall->ActiveArea))
    {
    case EMiningExperienceKind::Office:
        if(auto* Office=FirstCheckpointActor<APopupCloseStage>(W)){const int32 State=Office->SaveState();return State<3?State:0;}
        break;
    case EMiningExperienceKind::Journey:
        {const int32 State=CardState(W);return State==1?3:(State==2?4:0);}
    case EMiningExperienceKind::Ending:return 5;
    default:break;
    }
    return 0;
}

void UMiningSaveSubsystem::CaptureRoomState(UMiningSaveGame* Data)
{
    Data->Checkpoint=CheckpointMode();
    if(auto* Office=FirstCheckpointActor<APopupCloseStage>(GetWorld()))Data->OfficeState=Office->SaveState();
    Data->CardState=CardState(GetWorld());
    if(Data->Checkpoint==3 || Data->Checkpoint==4)
        if(auto* Interaction=GetWorld()->GetFirstPlayerController()->FindComponentByClass<UCardRoomInteractionComponent>())
        {Data->PlayerTransform=Interaction->GetResumeTransform();Data->ViewRotation=Data->PlayerTransform.Rotator();}
    if(Data->Checkpoint==4)Data->Drill=false;
    if(Data->Checkpoint==2)Data->Bomb=false;
    if(Data->Checkpoint==5)
    {
        auto* Hall=AMiningExhibitHall::Find(GetWorld());
        Data->PlayerTransform=Hall->Areas[Data->Area].Entry;Data->ViewRotation=Data->PlayerTransform.Rotator();
    }
    if(auto* Echo=FirstCheckpointActor<AEchoPassageStage>(GetWorld()))
    {
        Data->EchoCollapsed=Echo->bCollapsed;Data->EchoLanded=Echo->bLanded;
        Data->EchoCompleted=Echo->bCompleted;Data->EchoKnocks=Echo->KnockCount;
    }
    if(auto* Museum=FirstCheckpointActor<AWenSiMirrorPuzzle>(GetWorld()))
    {
        Data->MuseumRewardUnlocked=Museum->bRewardUnlocked;
        Data->MuseumPlayerWord=int32(Museum->GetPlayerWord());
        for(auto Exhibit:Museum->Exhibits)if(Exhibit)
        {Data->MuseumWords.Add(Exhibit->Kind,int32(Exhibit->Word));if(Exhibit->bBallLanded)Data->MuseumLandedExhibits.Add(Exhibit->Kind);}
    }
}

bool UMiningSaveSubsystem::BeginEndingCheckpoint()
{
    FinishPendingWrite();
    auto* Previous=FrozenCheckpoint.Get();
    // Request one durable snapshot before travel, then freeze it for the ending.
    FrozenCheckpoint=NewObject<UMiningSaveGame>(this);
    FrozenCheckpoint->Checkpoint=5;FrozenCheckpoint->Area=INDEX_NONE;
    if(SaveProgressInternal(false))return true;
    FrozenCheckpoint=Previous;return false;
}

void UMiningSaveSubsystem::RestoreRoomState()
{
    auto* Data=PendingRoomRestore.Get();if(!Data)return;
    auto* W=GetWorld();auto* PC=W->GetFirstPlayerController();
    if(Data->Checkpoint==5)
    {
        PendingRoomRestore=nullptr;
        UGameplayStatics::OpenLevel(this,TEXT("/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo"),true);
        return;
    }
    // Opening owns view/input until its release. Re-enter the encounter afterwards.
    if(auto* Office=FirstCheckpointActor<APopupCloseStage>(W))Office->RestoreSaveState(Data->OfficeState);
    if(auto* Echo=FirstCheckpointActor<AEchoPassageStage>(W))
    {
        Echo->RestoreProgress(Data->EchoCollapsed,Data->EchoLanded,Data->EchoCompleted,Data->EchoKnocks);
    }
    if(Data->CardState>=2)
        if(auto* Escape=FirstCheckpointActor<AMiningRoomEscape>(W))Escape->RestoreSaveState(Data->CardState);
    if(auto* Museum=FirstCheckpointActor<AWenSiMirrorPuzzle>(W);Museum && !Data->MuseumWords.IsEmpty())
    {
        for(auto Exhibit:Museum->Exhibits)if(Exhibit)
            if(const auto* Word=Data->MuseumWords.Find(Exhibit->Kind))
            {Exhibit->Word=EWenSiMirrorWord(*Word);Exhibit->bBallLanded=Data->MuseumLandedExhibits.Contains(Exhibit->Kind);Exhibit->Apply();}
        Museum->bRewardUnlocked=Data->MuseumRewardUnlocked;
        if(Museum->bAreaActive){Museum->SetPlayerWord(EWenSiMirrorWord(Data->MuseumPlayerWord));PC->GetPawn()->SetActorTransform(Data->PlayerTransform,false,nullptr,ETeleportType::TeleportPhysics);}
    }
    if(Data->Checkpoint==3 || Data->Checkpoint==4)
        if(auto* Interaction=PC->FindComponentByClass<UCardRoomInteractionComponent>())
        {
            // Seat normally to restore the original movement/collision release path.
            auto* Stage=Interaction->Stage.Get();
            if(Data->Checkpoint==4)Stage->Run=FCardRoomRun();
            Interaction->SetRoomActive(true);Interaction->Interact();
            if(Data->Checkpoint==4)
            {Stage->Run.Phase=ECardRoomPhase::Victory;Stage->Run.Round=4;Stage->Run.RivalHP=0;Stage->Run.PhaseTime=10.f;}
        }
    PendingRoomRestore=nullptr;
}
