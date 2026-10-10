#include "MiningRoomEscape.h"
#include "MiningEncounterPlayer.h"
#include "MiningGiantRoomEntrance.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningComponent.h"
#include "DiggableRock.h"
#include "CardRoomStage.h"
#include "CardRoomPlayer.h"
#include "EchoPassagePlayer.h"
#include "SMiningEscapeView.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Character.h"
#include "Camera/PlayerCameraManager.h"

AMiningRoomEscape::AMiningRoomEscape(){PrimaryActorTick.bCanEverTick=true;}
void AMiningRoomEscape::BeginPlay()
{
    Super::BeginPlay();
    Player=Cast<AMiningEncounterPlayerController>(GetWorld()->GetFirstPlayerController());
    if(!Player.IsValid() || !CardStage || !Entrance){SetActorTickEnabled(false);return;}
    CardStage->OnOpponentWallImpact.AddDynamic(this,&AMiningRoomEscape::OnWallImpact);
    CardStage->OnCollectibleClaimed.AddDynamic(this,&AMiningRoomEscape::OnRewardClaimed);
    Player->EchoInteraction->RestartOverride.BindUObject(this,&AMiningRoomEscape::HandleRestart);
    Player->EchoInteraction->OnRestarted.AddUObject(this,&AMiningRoomEscape::ResetJourney);
    if(GEngine && GEngine->GameViewport){SAssignNew(View,SMiningEscapeView).Owner(this);GEngine->GameViewport->AddViewportWidgetContent(View.ToSharedRef(),60);}
    ResetJourney();
}
void AMiningRoomEscape::OnWallImpact(FName RoomId)
{
    if(bWallBroken || !BreachWall)return;
    // A real hole in the rear wall. Reused mesh boolean preserves collision around the impact.
    for(float X=-200;X<=200;X+=100)BreachWall->MineSphere(BreachWall->GetActorLocation()+FVector(X,0,0),450);
    bWallBroken=true;
    if(Player.IsValid() && Player->PlayerCameraManager)Player->PlayerCameraManager->StartCameraFade(.18,0,.35,FLinearColor(1,.72,.35),false,false);
}
void AMiningRoomEscape::OnRewardClaimed(FName RoomId)
{
    if(Phase!=EMiningEscapePhase::Idle)return;
    OnWallImpact(RoomId);Phase=EMiningEscapePhase::Warning;PhaseTime=0;Elapsed=0;
    ApplyExploration();
}
void AMiningRoomEscape::ApplyExploration()
{
    if(!Player.IsValid())return;
    Entrance->bExternalEncounterActive=true;
    Player->CardInteraction->ReleaseForExploration();
    Player->EchoInteraction->SetExplorationActive(false);
    Player->EncounterPolicy->SetMiningAllowed(true);
}
void AMiningRoomEscape::StartFlood()
{
    Phase=EMiningEscapePhase::Flooding;PhaseTime=0;Elapsed=0;WaterZ=WaterStartZ;bEnteredComputer=false;ClearedNodes=0;++Attempts;
    ApplyExploration();UpdateWater();
}
void AMiningRoomEscape::RetryEscape()
{
    if(Phase==EMiningEscapePhase::Idle || !Player.IsValid() || !Player->GetPawn())return;
    if(bFailureLock){Player->SetIgnoreMoveInput(false);bFailureLock=false;}
    if(auto* Mine=Player->GetPawn()->FindComponentByClass<UMiningComponent>())Mine->CancelMining();
    for(const auto& Rock:ComputerRocks)if(IsValid(Rock))Rock->ResetRock();
    APawn* Pawn=Player->GetPawn();Pawn->SetActorLocation(Checkpoint,false,nullptr,ETeleportType::TeleportPhysics);
    Pawn->SetActorEnableCollision(true);Pawn->SetActorRotation(FRotator(0,35,0));Player->SetControlRotation(FRotator(18,35,0));
    if(auto* Move=Pawn->FindComponentByClass<UMiningCharacterMovementComponent>())
    {Move->SetFlightInputHeld(false);Move->StopMovementImmediately();Move->SetMovementMode(MOVE_Falling);}
    FailureReason.Empty();StartFlood();
}
bool AMiningRoomEscape::HandleRestart()
{
    if(Phase==EMiningEscapePhase::Idle)return false;
    RetryEscape();return true;
}
void AMiningRoomEscape::ResetJourney()
{
    if(bFailureLock && Player.IsValid())Player->SetIgnoreMoveInput(false);
    bFailureLock=false;Phase=EMiningEscapePhase::Idle;PhaseTime=Elapsed=0;Attempts=0;ClearedNodes=0;WaterZ=WaterStartZ;bWallBroken=false;bEnteredComputer=false;
    if(Entrance)Entrance->bExternalEncounterActive=false;
    if(BreachWall)BreachWall->ResetRock();
    for(const auto& Rock:ComputerRocks)if(IsValid(Rock))Rock->ResetRock();
    UpdateWater();
}
void AMiningRoomEscape::Fail(const FString& Reason)
{
    if(Phase!=EMiningEscapePhase::Flooding)return;
    Phase=EMiningEscapePhase::Failed;PhaseTime=0;FailureReason=Reason;
    if(Player.IsValid())
    {
        Player->EncounterPolicy->SetMiningAllowed(false);
        if(!bFailureLock){Player->SetIgnoreMoveInput(true);bFailureLock=true;}
        if(auto* Move=Player->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>()){Move->SetFlightInputHeld(false);Move->StopMovementImmediately();}
    }
}
FVector AMiningRoomEscape::GuideTarget() const
{
    if(!Player.IsValid() || !Player->GetPawn())return ComputerEntry;
    const FVector P=Player->GetPawn()->GetActorLocation();
    if(P.X<11740 || FMath::Abs(P.Y-5000)>550)return ComputerEntry;
    if(ComputerRocks.IsValidIndex(ClearedNodes) && ComputerRocks[ClearedNodes])return ComputerRocks[ClearedNodes]->GetActorLocation()+FVector(30,0,70);
    return Exit;
}
void AMiningRoomEscape::UpdateWater()
{
    const bool Visible=Phase==EMiningEscapePhase::Flooding || Phase==EMiningEscapePhase::Failed || Phase==EMiningEscapePhase::Escaped;
    if(Water){Water->SetActorHiddenInGame(!Visible);FVector P=Water->GetActorLocation();P.Z=WaterZ-12;Water->SetActorLocation(P);}
    for(const auto& Stream:Streams)if(Stream)Stream->SetActorHiddenInGame(!Visible || Phase==EMiningEscapePhase::Escaped);
    for(int32 i=0;i<Foam.Num();++i)if(Foam[i])
    {Foam[i]->SetActorHiddenInGame(!Visible);FVector P=Foam[i]->GetActorLocation();P.Z=WaterZ+3+FMath::Sin(GetWorld()->GetTimeSeconds()*2+i)*4;Foam[i]->SetActorLocation(P);}
}
void AMiningRoomEscape::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);if(!Player.IsValid() || !Player->GetPawn())return;
    PhaseTime+=DeltaSeconds;
    if(Phase==EMiningEscapePhase::Warning && PhaseTime>=FMath::Max(.1f,WarningSeconds))StartFlood();
    else if(Phase==EMiningEscapePhase::Failed && PhaseTime>=FMath::Max(.1f,RetrySeconds))RetryEscape();
    else if(Phase==EMiningEscapePhase::Flooding)
    {
        Elapsed+=DeltaSeconds;WaterZ=FMath::Lerp(WaterStartZ,WaterEndZ,FMath::Clamp(Elapsed/FMath::Max(1.f,FloodSeconds),0.f,1.f));
        const FVector P=Player->GetPawn()->GetActorLocation();
        const bool InComputer=P.X>11900 && P.X<13480 && FMath::Abs(P.Y-5000)<460 && P.Z>1550 && P.Z<2790;
        bEnteredComputer|=InComputer;
        if(ComputerRocks.IsValidIndex(ClearedNodes))
        {
            auto* Rock=ComputerRocks[ClearedNodes].Get();const FVector D=Rock?P-Rock->GetActorLocation():FVector(9999);
            if(Rock && Rock->SuccessfulCuts>0 && D.X> -50 && D.X<260 && FMath::Abs(D.Y)<230 && FMath::Abs(D.Z)<240)++ClearedNodes;
        }
        // Only reaching the upper exit after entering the computer completes the escape.
        if(bEnteredComputer && ClearedNodes==ComputerRocks.Num() && P.X>Exit.X-140 && FMath::Abs(P.Y-Exit.Y)<210 && FMath::Abs(P.Z-Exit.Z)<160)
        {Phase=EMiningEscapePhase::Escaped;PhaseTime=0;Player->EncounterPolicy->SetMiningAllowed(false);}
        else if(InComputer && WaterZ>=1690)Fail(TEXT("水涌入电脑，电路短路！"));
        else if(P.Z+50<WaterZ || P.Z<0)Fail(TEXT("被上涨的水淹没了！"));
        else if(Elapsed>=FloodSeconds)Fail(TEXT("房间被灌满，未能及时逃出！"));
        UpdateWater();
    }
}
void AMiningRoomEscape::EndPlay(const EEndPlayReason::Type Reason)
{
    if(CardStage){CardStage->OnOpponentWallImpact.RemoveAll(this);CardStage->OnCollectibleClaimed.RemoveAll(this);}
    if(Player.IsValid())
    {
        if(bFailureLock)Player->SetIgnoreMoveInput(false);
        if(Player->EchoInteraction->RestartOverride.IsBoundToObject(this))Player->EchoInteraction->RestartOverride.Unbind();
        Player->EchoInteraction->OnRestarted.RemoveAll(this);
    }
    if(Entrance)Entrance->bExternalEncounterActive=false;
    if(View && GEngine && GEngine->GameViewport)GEngine->GameViewport->RemoveViewportWidgetContent(View.ToSharedRef());
    View.Reset();Super::EndPlay(Reason);
}
