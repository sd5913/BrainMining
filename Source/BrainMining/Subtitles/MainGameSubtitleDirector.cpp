#include "Subtitles/MainGameSubtitleDirector.h"
#include "Subtitles/SubtitleSubsystem.h"
#include "Subtitles/SubtitleTrigger.h"
#include "CardRoomStage.h"
#include "EchoPassageStage.h"
#include "MiningRoomEscape.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Components/SceneComponent.h"

AMainGameSubtitleDirector::AMainGameSubtitleDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .1f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AMainGameSubtitleDirector::BeginPlay()
{
    Super::BeginPlay();
    for(TActorIterator<ACardRoomStage> It(GetWorld()); It; ++It)
        It->OnRoomCompleted.AddUniqueDynamic(this, &AMainGameSubtitleDirector::OnCardsCompleted);
}

void AMainGameSubtitleDirector::PlayOnce(FName SegmentId)
{
    if(Played.Contains(SegmentId)) return;
    if(auto* Subtitles = USubtitleSubsystem::Get(this))
    {
        Played.Add(SegmentId);
        Subtitles->PlaySegment(SegmentId);
    }
}

void AMainGameSubtitleDirector::OnCardsCompleted(FName RoomId)
{
    PlayOnce(TEXT("卡牌玩法2"));
}

void AMainGameSubtitleDirector::Tick(float Dt)
{
    Super::Tick(Dt);
    auto* PC = GetWorld()->GetFirstPlayerController();
    if(!PC || PC->IsPaused() || !PC->GetPawn()) return;
    for(TActorIterator<ACardRoomStage> It(GetWorld()); It; ++It)
        if(It->Run.Round == 1 && It->Run.Phase == ECardRoomPhase::Choosing)
            PlayOnce(TEXT("卡牌玩法1"));
    for(TActorIterator<AEchoPassageStage> It(GetWorld()); It; ++It)
        if(It->bLanded) PlayOnce(TEXT("盲人敲地玩法"));
    for(TActorIterator<AMiningRoomEscape> It(GetWorld()); It; ++It)
        if(It->Phase == EMiningEscapePhase::Flooding)
        {
            // Preserve narrative order even if a save/debug jump skips the win event.
            PlayOnce(TEXT("卡牌玩法2"));
            PlayOnce(TEXT("水涌玩法"));
        }
    if(MuseumEntry && MuseumEntry->HasPlayed() && MuseumEnd && !MuseumEnd->HasPlayed()
        && MuseumBounds.IsInsideOrOn(PC->GetPawn()->GetActorLocation()))
    {
        MuseumSeconds += Dt;
        if(MuseumSeconds >= MuseumHintSeconds) PlayOnce(TEXT("文思玩法2"));
    }
}
