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
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/PlayerCameraManager.h"

AMiningRoomEscape::AMiningRoomEscape()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("EscapeRoot"));
}
void AMiningRoomEscape::BeginPlay()
{
    Super::BeginPlay();
    Player=Cast<AMiningEncounterPlayerController>(GetWorld()->GetFirstPlayerController());
    if(!Player.IsValid() || !CardStage || !Entrance){SetActorTickEnabled(false);return;}
    Checkpoint+=SourceLevelOffset;ComputerEntry+=SourceLevelOffset;
    ComputerCheckpointPosition+=SourceLevelOffset;TunnelEnd+=SourceLevelOffset;Exit+=SourceLevelOffset;
    WaterStartZ+=SourceLevelOffset.Z;WaterEndZ+=SourceLevelOffset.Z;TableWaterZ+=SourceLevelOffset.Z;
    if(bBuildStudyLayout)BuildStudyLayout();
    CardStage->OnOpponentWallImpact.AddDynamic(this,&AMiningRoomEscape::OnWallImpact);
    CardStage->OnCollectibleClaimed.AddDynamic(this,&AMiningRoomEscape::OnRewardClaimed);
    Player->EchoInteraction->RestartOverride.BindUObject(this,&AMiningRoomEscape::HandleRestart);
    Player->EchoInteraction->OnRestarted.AddUObject(this,&AMiningRoomEscape::ResetJourney);
    if(GEngine && GEngine->GameViewport){SAssignNew(View,SMiningEscapeView).Owner(this);GEngine->GameViewport->AddViewportWidgetContent(View.ToSharedRef(),60);}
    ResetJourney();
    if (bAreaManaged) SetAreaActive(false);
}
void AMiningRoomEscape::SetAreaActive(bool bActive)
{
    bAreaActive = bActive;
    if (View) View->SetVisibility(bActive ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
    if (bActive && Phase != EMiningEscapePhase::Idle) ApplyExploration();
    if (!bActive) SetTelevisionMining(false);
    else if (bEnteredComputer) SetTelevisionMining(true);
    if (!bActive && bFailureLock && Player.IsValid()) { Player->SetIgnoreMoveInput(false); bFailureLock=false; }
}
void AMiningRoomEscape::OnWallImpact(FName RoomId)
{
    if(!CardStage || RoomId!=CardStage->RoomId || bWallBroken || !BreachWall)return;
    bWallBroken=BreachWall->MineSphere(BreachWall->GetActorLocation(),1150);
    BreachBurstTime=0;
    UpdateWater();
    if(Player.IsValid() && Player->PlayerCameraManager)Player->PlayerCameraManager->StartCameraFade(.18f,0,.35f,FLinearColor(1,.72f,.35f),false,false);
}
void AMiningRoomEscape::OnRewardClaimed(FName RoomId)
{
    if(!CardStage || RoomId!=CardStage->RoomId || Phase!=EMiningEscapePhase::Idle)return;
    OnWallImpact(RoomId);Phase=EMiningEscapePhase::Warning;PhaseTime=0;Elapsed=0;
    ApplyExploration();
    if(Player.IsValid() && Player->GetPawn())Checkpoint=Player->GetPawn()->GetActorLocation();
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
    SetTelevisionMining(false);
    Phase=EMiningEscapePhase::Flooding;PhaseTime=0;Elapsed=0;WaterZ=WaterStartZ;
    bEnteredComputer=false;bExitGuidance=false;bComputerCheckpoint=false;ComputerElapsed=0;ClearedNodes=0;++Attempts;
    ApplyExploration();UpdateWater();
}
void AMiningRoomEscape::RetryEscape()
{
    if(Phase==EMiningEscapePhase::Idle || !Player.IsValid() || !Player->GetPawn())return;
    const bool FromComputer=bComputerCheckpoint && Phase!=EMiningEscapePhase::Escaped;
    if(bFailureLock){Player->SetIgnoreMoveInput(false);bFailureLock=false;}
    if(auto* Mine=Player->GetPawn()->FindComponentByClass<UMiningComponent>())Mine->CancelMining();
    for(const auto& Rock:ComputerRocks)if(IsValid(Rock))Rock->ResetRock();
    APawn* Pawn=Player->GetPawn();
    Pawn->SetActorLocation(FromComputer?ComputerCheckpointPosition:Checkpoint,false,nullptr,ETeleportType::TeleportPhysics);
    Pawn->SetActorEnableCollision(true);
    const FRotator Aim=(ComputerEntry-Pawn->GetActorLocation()).Rotation();
    Pawn->SetActorRotation(FRotator(0,Aim.Yaw,0));Player->SetControlRotation(Aim);
    if(auto* Character=Cast<ACharacter>(Pawn))Character->StopJumping();
    if(auto* Move=Pawn->FindComponentByClass<UMiningCharacterMovementComponent>())
    {Move->ResetFlightInput();Move->StopMovementImmediately();Move->SetMovementMode(MOVE_Falling);}
    FailureReason.Empty();StartFlood();
    bComputerCheckpoint=FromComputer;bExitGuidance=FromComputer;
    if(FromComputer){WaterZ=ComputerStartWaterZ=CheckpointWaterZ;Elapsed=CheckpointElapsed;SetTelevisionMining(true);}
    UpdateWater();
}
bool AMiningRoomEscape::HandleRestart()
{
    if(bAreaManaged && !bAreaActive)return false;
    if(Phase==EMiningEscapePhase::Idle)return false;
    RetryEscape();return true;
}
void AMiningRoomEscape::ResetJourney()
{
    SetTelevisionMining(false);
    if(bFailureLock && Player.IsValid())Player->SetIgnoreMoveInput(false);
    bFailureLock=false;Phase=EMiningEscapePhase::Idle;PhaseTime=Elapsed=ComputerElapsed=0;
    Attempts=ClearedNodes=0;WaterZ=WaterStartZ;bWallBroken=bEnteredComputer=bComputerCheckpoint=bExitGuidance=false;
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
        if(Player->GetPawn())if(auto* Move=Player->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>())
        {Move->ResetFlightInput();Move->StopMovementImmediately();Move->SetMovementMode(MOVE_None);}
    }
}
void AMiningRoomEscape::SetTelevisionMining(bool Enabled)
{
    if(!Enabled)
    {
        if(ScopedMiner.IsValid())ScopedMiner->Radius=SavedMineRadius;
        ScopedMiner.Reset();return;
    }
    if(ScopedMiner.IsValid() || !Player.IsValid() || !Player->GetPawn())return;
    if(auto* Mine=Player->GetPawn()->FindComponentByClass<UMiningComponent>())
    {
        ScopedMiner=Mine;SavedMineRadius=Mine->Radius;
        // Give the original sphere cutter enough capsule clearance on a continuous incline.
        Mine->Radius=FMath::Max(Mine->Radius,TelevisionMineRadius);
    }
}
bool AMiningRoomEscape::IsInsideComputer(const FVector& Position) const
{
    const FVector Axis=(TunnelEnd-ComputerEntry).GetSafeNormal();
    const FVector Local=Axis.Rotation().UnrotateVector(Position-ComputerEntry);
    return Local.X>100 && Local.X<FVector::Distance(ComputerEntry,TunnelEnd)+ExitRadius &&
        FMath::Abs(Local.Y)<TunnelHalfWidth && FMath::Abs(Local.Z)<TunnelHalfHeight;
}
FVector AMiningRoomEscape::GuideTarget() const
{
    return bExitGuidance?(ClearedNodes==ComputerRocks.Num()?Exit:TunnelEnd):ComputerEntry;
}
float AMiningRoomEscape::ExitDistance() const
{
    return Player.IsValid() && Player->GetPawn()?FVector::Distance(Player->GetPawn()->GetActorLocation(),GuideTarget())/100.f:0.f;
}
void AMiningRoomEscape::UpdateWater()
{
    const bool Visible=Phase==EMiningEscapePhase::Flooding || Phase==EMiningEscapePhase::Failed || Phase==EMiningEscapePhase::Escaped;
    const bool WaterVisible=Visible || bWallBroken;
    if(Water){Water->SetActorHiddenInGame(!WaterVisible);FVector P=Water->GetActorLocation();P.Z=WaterZ;Water->SetActorLocation(P);}
    const bool Signal=Visible && WaterZ>=TableWaterZ-100 && Phase!=EMiningEscapePhase::Escaped;
    if(ScreenLabel)ScreenLabel->SetVisibility(TelevisionScreen && TelevisionScreen->SuccessfulCuts==0 && Signal);
    if(ScreenMaterial)ScreenMaterial->SetVectorParameterValue(TEXT("Color"),Signal?FLinearColor(.08f,.3f,.24f)*(1.8f+.6f*FMath::Sin(Elapsed*5)):FLinearColor(.001f,.001f,.001f));
    if(Beacon)Beacon->SetVisibility(Signal);
    for(const auto& Light:InteriorLights)if(Light)Light->SetVisibility(Signal || bEnteredComputer);
    for(int32 i=0;i<FloatingProps.Num();++i)
    {
        auto* Prop=FloatingProps[i].Get();
        Prop->SetVisibility(WaterVisible);Prop->SetCollisionEnabled(WaterVisible?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        FVector P=Prop->GetComponentLocation();P.Z=WaterZ+FloatingOffsets[i];Prop->SetWorldLocation(P);
    }
    if(ComputerWater)
    {
        ComputerWater->SetVisibility(Visible && bComputerCheckpoint);
        const float A=FMath::Clamp(ComputerElapsed/FMath::Max(1.f,ComputerSeconds),0.f,1.f);
        const FVector D=(TunnelEnd-ComputerEntry).GetSafeNormal();
        ComputerWater->SetWorldLocation(ComputerEntry+D*(-400+480*A));
    }
}
void AMiningRoomEscape::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);if((bAreaManaged && !bAreaActive) || !Player.IsValid() || !Player->GetPawn())return;
    TickPresentation(DeltaSeconds);PhaseTime+=DeltaSeconds;
    if(Phase==EMiningEscapePhase::Warning && PhaseTime>=FMath::Max(.1f,WarningSeconds))
    {
        StartFlood();
    }
    else if(Phase==EMiningEscapePhase::Failed && PhaseTime>=FMath::Max(.1f,RetrySeconds))RetryEscape();
    else if(Phase==EMiningEscapePhase::Flooding)
    {
        Elapsed+=DeltaSeconds;
        const FVector P=Player->GetPawn()->GetActorLocation();
        const bool InComputer=IsInsideComputer(P);
        // Direction changes before entering the screen; retreating a step must not flip the marker.
        // This presentation latch does not activate a checkpoint or its electrical countdown.
        bExitGuidance|=bComputerCheckpoint || FVector::DistSquared(P,ComputerEntry)<=FMath::Square(TelevisionGuideRadius);
        const bool AtTelevision=FVector::Dist(P,ComputerCheckpointPosition)<280;
        const bool ReachedTelevision=AtTelevision || (InComputer && !ComputerRocks.IsEmpty() && ComputerRocks[0]->SuccessfulCuts>0);
        // Never save an already submerged respawn shelf: that would create a death loop.
        if(!bComputerCheckpoint && ReachedTelevision && WaterZ>=ComputerCheckpointPosition.Z-60)
        {Fail(TEXT("电视入口已经进水，没能及时赶到！"));UpdateWater();return;}
        if(!bComputerCheckpoint && ReachedTelevision)
        {bComputerCheckpoint=true;ComputerElapsed=0;CheckpointElapsed=Elapsed-DeltaSeconds;CheckpointWaterZ=ComputerStartWaterZ=WaterZ;SetTelevisionMining(true);}
        bEnteredComputer|=InComputer && bComputerCheckpoint && TelevisionScreen && TelevisionScreen->SuccessfulCuts>0;
        if(bComputerCheckpoint)
        {
            ComputerElapsed+=DeltaSeconds;
            WaterZ=FMath::Lerp(ComputerStartWaterZ,ComputerEntry.Z,FMath::Clamp(ComputerElapsed/FMath::Max(1.f,ComputerSeconds),0.f,1.f));
        }
        else if(Elapsed<RapidFillSeconds)WaterZ=FMath::Lerp(WaterStartZ,TableWaterZ,Elapsed/FMath::Max(.1f,RapidFillSeconds));
        else WaterZ=FMath::Lerp(TableWaterZ,WaterEndZ,FMath::Clamp((Elapsed-RapidFillSeconds)/FMath::Max(1.f,FloodSeconds-RapidFillSeconds),0.f,1.f));
        // Sequentially cross the far face of each contiguous solid section.
        if(ComputerRocks.IsValidIndex(ClearedNodes))
        {
            auto* Rock=ComputerRocks[ClearedNodes].Get();
            const FVector Local=Rock->GetActorRotation().UnrotateVector(P-Rock->GetActorLocation());
            if(Rock->SuccessfulCuts>0 && Local.X>Rock->RockSize.X*.5f && Local.X<Rock->RockSize.X*.5f+600 &&
                FMath::Abs(Local.Y)<TunnelHalfWidth && FMath::Abs(Local.Z)<TunnelHalfHeight)++ClearedNodes;
        }
        const auto* Character=Cast<ACharacter>(Player->GetPawn());
        const float Head=Character?Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*.75f:50.f;
        // Hazards win a same-frame race with the exit.
        if(bComputerCheckpoint && ComputerElapsed>=FMath::Max(1.f,ComputerSeconds))Fail(TEXT("水涌入电视，电路短路！"));
        else if(P.Z+Head<WaterZ || P.Z<SourceLevelOffset.Z)Fail(TEXT("被上涨的水淹没了！"));
        else if(!bComputerCheckpoint && Elapsed>=FMath::Max(1.f,FloodSeconds))Fail(TEXT("房间被灌满，未能及时逃出！"));
        else if(bEnteredComputer && !ComputerRocks.IsEmpty() && ClearedNodes==ComputerRocks.Num() && FVector::Dist(P,Exit)<ExitRadius)
        {Phase=EMiningEscapePhase::Escaped;PhaseTime=0;Player->EncounterPolicy->SetMiningAllowed(false);SetTelevisionMining(false);}
        UpdateWater();
    }
}
void AMiningRoomEscape::EndPlay(const EEndPlayReason::Type Reason)
{
    SetTelevisionMining(false);
    if(CardStage){CardStage->OnOpponentWallImpact.RemoveAll(this);CardStage->OnCollectibleClaimed.RemoveAll(this);}
    if(Player.IsValid())
    {
        if(bFailureLock)Player->SetIgnoreMoveInput(false);
        if(Player->EchoInteraction->RestartOverride.IsBoundToObject(this))Player->EchoInteraction->RestartOverride.Unbind();
        Player->EchoInteraction->OnRestarted.RemoveAll(this);
    }
    if(Entrance)Entrance->bExternalEncounterActive=false;
    if(ReplacedWall.IsValid()){ReplacedWall->SetActorHiddenInGame(false);ReplacedWall->SetActorEnableCollision(true);}
    for(const auto& Actor:GeneratedActors)if(IsValid(Actor))Actor->Destroy();
    if(View && GEngine && GEngine->GameViewport)GEngine->GameViewport->RemoveViewportWidgetContent(View.ToSharedRef());
    View.Reset();Super::EndPlay(Reason);
}
