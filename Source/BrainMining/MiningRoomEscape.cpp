#include "MiningRoomEscape.h"
#include "MiningRoomAudioSubsystem.h"
#include "MiningEncounterPlayer.h"
#include "MiningGiantRoomEntrance.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningComponent.h"
#include "MiningCollection.h"
#include "MiningCollectionEffects.h"
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
#include "Subtitles/SubtitleSubsystem.h"
#include "MiningWalletSubsystem.h"
#include "Engine/GameInstance.h"
#include "Camera/PlayerCameraManager.h"

AMiningRoomEscape::AMiningRoomEscape()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("EscapeRoot"));
}
void AMiningRoomEscape::BeginPlay()
{
    Super::BeginPlay();
    ClearTelevisionPreview();
    InitializeRuntime();
}

void AMiningRoomEscape::InitializeRuntime()
{
    if (bRuntimeInitialized) return;
    Player=Cast<AMiningEncounterPlayerController>(GetWorld()->GetFirstPlayerController());
    if(!Player.IsValid() || !CardStage || !Entrance)return;
    Checkpoint+=SourceLevelOffset;ComputerEntry+=SourceLevelOffset;
    ComputerCheckpointPosition+=SourceLevelOffset;TunnelEnd+=SourceLevelOffset;Exit+=SourceLevelOffset;
    WaterStartZ+=SourceLevelOffset.Z;WaterEndZ+=SourceLevelOffset.Z;TableWaterZ+=SourceLevelOffset.Z;
    if(bBuildStudyLayout)BuildStudyLayout();
    CardStage->OnOpponentWallImpact.AddDynamic(this,&AMiningRoomEscape::OnWallImpact);
    CardStage->OnCollectibleClaimed.AddDynamic(this,&AMiningRoomEscape::OnRewardClaimed);
    // Explicit pickup owns this ability; never auto-claim a victory reward.
    bAutoClaimVictoryReward=false;
    if(auto* GI=GetGameInstance())GI->GetSubsystem<UMiningWalletSubsystem>()->bDrillSkillRequiresUnlock=true;
    if(auto* Mine=Player->GetPawn()?Player->GetPawn()->FindComponentByClass<UMiningComponent>():nullptr)Mine->RequireDrillUnlock();
    CardStage->SubtitleOverride.BindWeakLambda(this,[this](const FString& Text,float Duration)
    {if(auto* Subtitles=USubtitleSubsystem::Get(this))Subtitles->PlayStatusLine(FText::FromString(Text),Duration);});
    Player->EchoInteraction->RestartOverride.BindUObject(this,&AMiningRoomEscape::HandleRestart);
    Player->EchoInteraction->OnRestarted.AddUObject(this,&AMiningRoomEscape::ResetJourney);
    if(GEngine && GEngine->GameViewport){SAssignNew(View,SMiningEscapeView).Owner(this);GEngine->GameViewport->AddViewportWidgetContent(View.ToSharedRef(),60);}
    ResetJourney();
    if (bAreaManaged) SetAreaActive(false);
    bRuntimeInitialized=true;
}
void AMiningRoomEscape::SetAreaActive(bool bActive)
{
    bAreaActive = bActive;
    if(!bActive)UMiningRoomAudioSubsystem::StopSource(this);
    if (View) View->SetVisibility(bActive ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
    if (bActive && Phase != EMiningEscapePhase::Idle) ApplyExploration();
    if (!bActive) SetTelevisionMining(false);
    if(!bActive && bOwnsStageCollection && StageCollection.IsValid())StageCollection->SetAreaActive(false);
    if(StageCelebration)StageCelebration->SetActorTickEnabled(bActive && Phase==EMiningEscapePhase::Escaped);
    if (bActive && bEnteredComputer && Phase!=EMiningEscapePhase::Escaped) SetTelevisionMining(true);
    if (bActive && Phase==EMiningEscapePhase::Escaped && Player.IsValid())Player->EncounterPolicy->SetMiningAllowed(true);
    if (!bActive && bFailureLock && Player.IsValid()) { Player->SetIgnoreMoveInput(false); bFailureLock=false; }
}
void AMiningRoomEscape::OnWallImpact(FName RoomId)
{
    if(!CardStage || RoomId!=CardStage->RoomId || bWallBroken || !BreachWall)return;
    bWallBroken=BreachWall->MineSphere(BreachWall->GetActorLocation(),1150);
    if(bWallBroken)UMiningRoomAudioSubsystem::Emit(BreachWall,TEXT("S_DoorBreak"),.8f,true);
    BreachBurstTime=0;
    UpdateWater();
    if(Player.IsValid() && Player->PlayerCameraManager)Player->PlayerCameraManager->StartCameraFade(.18f,0,.35f,FLinearColor(1,.72f,.35f),false,false);
}
void AMiningRoomEscape::OnRewardClaimed(FName RoomId)
{
    if(!CardStage || RoomId!=CardStage->RoomId || Phase!=EMiningEscapePhase::Idle)return;
    if(Player.IsValid() && Player->GetPawn())
        if(auto* Mine=Player->GetPawn()->FindComponentByClass<UMiningComponent>()){Mine->UnlockDrill();}
    OnWallImpact(RoomId);Phase=EMiningEscapePhase::Warning;PhaseTime=0;Elapsed=0;
    ApplyExploration();
    if(Player.IsValid() && Player->GetPawn())
        if(auto* Mine=Player->GetPawn()->FindComponentByClass<UMiningComponent>();Mine && !Mine->bDrillMode)Mine->ActivateDrill();
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
    for(float& Age:FloatingSinkElapsed)Age=-1;
    Phase=EMiningEscapePhase::Flooding;PhaseTime=0;Elapsed=0;WaterZ=WaterStartZ;
    bEnteredComputer=false;bExitGuidance=false;bComputerCheckpoint=false;ComputerElapsed=0;ClearedNodes=0;++Attempts;
    UMiningRoomAudioSubsystem::StopSource(this);
    UMiningRoomAudioSubsystem::Emit(this,Attempts==1?TEXT("S_FloodStart"):TEXT("S_Retry"),.65f);
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
    // Manual restart is unavailable in the card room. Death still calls RetryEscape directly.
    return bAreaActive && Player.IsValid() && Player->IsCardRoomContext();
}
void AMiningRoomEscape::ResetJourney()
{
    SetTelevisionMining(false);
    if(bFailureLock && Player.IsValid())Player->SetIgnoreMoveInput(false);
    bFailureLock=false;Phase=EMiningEscapePhase::Idle;PhaseTime=Elapsed=ComputerElapsed=0;
    UMiningRoomAudioSubsystem::StopSource(this);
    Attempts=ClearedNodes=0;VictoryHoldTime=0;WaterZ=WaterStartZ;bWallBroken=bEnteredComputer=bComputerCheckpoint=bExitGuidance=false;
    for(float& Age:FloatingSinkElapsed)Age=-1;
    if(Entrance)Entrance->bExternalEncounterActive=false;
    if(BreachWall)BreachWall->ResetRock();
    for(const auto& Rock:ComputerRocks)if(IsValid(Rock))Rock->ResetRock();
    UpdateWater();
}
void AMiningRoomEscape::Fail(const FString& Reason)
{
    if(Phase!=EMiningEscapePhase::Flooding)return;
    Phase=EMiningEscapePhase::Failed;PhaseTime=0;FailureReason=Reason;
    UMiningRoomAudioSubsystem::StopSource(this);
    UMiningRoomAudioSubsystem::Emit(this,Reason.Contains(TEXT("短路"))?TEXT("S_ShortCircuit"):TEXT("S_Drown"),.7f);
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
    // The restored dark green CRT must keep its tone under the local tunnel
    // exposure, rather than becoming a flat lime panel as the player approaches.
    if(ScreenMaterial)ScreenMaterial->SetVectorParameterValue(TEXT("Color"),Signal?FLinearColor(.08f,.30f,.24f)*(.18f+.035f*FMath::Sin(Elapsed*5)):FLinearColor(.001f,.001f,.001f));
    if(Beacon)Beacon->SetVisibility(Signal);
    for(const auto& Light:InteriorLights)if(Light)Light->SetVisibility(Signal || bEnteredComputer);
    for(int32 i=0;i<FloatingProps.Num();++i)
    {
        auto* Prop=FloatingProps[i].Get();
        const int32 Pad=FloatingPadIndices[i];
        const float Sink=FMath::Clamp((FloatingSinkElapsed[Pad]-FloatingSinkDelay)/FMath::Max(.1f,FloatingSinkSeconds),0.f,1.f);
        const bool Available=WaterVisible && Sink<1;
        Prop->SetVisibility(Available);Prop->SetCollisionEnabled(Available?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        // Cap each complete stack below the fixed television shelf, even on a slow attempt.
        const float HeightLimit=ComputerCheckpointPosition.Z-200-FloatingTopOffsets[Pad];
        FVector P=Prop->GetComponentLocation();P.Z=FMath::Min(WaterZ,HeightLimit)+FloatingOffsets[i]-Sink*(FloatingTopOffsets[Pad]+450);Prop->SetWorldLocation(P);
    }

}
void AMiningRoomEscape::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bRuntimeInitialized)
    {
        InitializeRuntime();
        return;
    }
    if((bAreaManaged && !bAreaActive) || !Player.IsValid() || !Player->GetPawn())return;
    TickPresentation(DeltaSeconds);const float PreviousPhaseTime=PhaseTime;PhaseTime+=DeltaSeconds;
    if(Phase==EMiningEscapePhase::Warning && PreviousPhaseTime<.45f && PhaseTime>=.45f)
        UMiningRoomAudioSubsystem::Emit(this,TEXT("S_Warning"),.5f);
    if(Phase==EMiningEscapePhase::Warning && PhaseTime>=FMath::Max(.1f,WarningSeconds))
    {
        StartFlood();
    }
    else if(Phase==EMiningEscapePhase::Failed && PhaseTime>=FMath::Max(.1f,RetrySeconds))RetryEscape();
    else if(Phase==EMiningEscapePhase::Flooding)
    {
        if(!bEnteredComputer)
        {
            const float WaterProgress=FMath::Clamp((WaterZ-WaterStartZ)/FMath::Max(1.f,WaterEndZ-WaterStartZ),0.f,1.f);
            UMiningRoomAudioSubsystem::Loop(this,TEXT("S_Water_Loop"),.25f+.2f*WaterProgress);
            if(SecondsLeft()>0 && SecondsLeft()<=15)UMiningRoomAudioSubsystem::Emit(this,TEXT("S_Warning"),.5f,false,1.5f);
        }
        else UMiningRoomAudioSubsystem::Loop(this,TEXT("A_TV_Loop"),.18f);
        TickFloatingProps(DeltaSeconds);
        Elapsed+=DeltaSeconds;
        const FVector P=Player->GetPawn()->GetActorLocation();
        const bool InComputer=IsInsideComputer(P);
        // Direction changes before entering the screen; retreating a step must not flip the marker.
        // This presentation latch does not activate a checkpoint or grant safety.
        bExitGuidance|=bComputerCheckpoint || FVector::DistSquared(P,ComputerEntry)<=FMath::Square(TelevisionGuideRadius);
        const bool AtTelevision=FVector::Dist(P,ComputerCheckpointPosition)<280;
        const bool ReachedTelevision=AtTelevision || (InComputer && !ComputerRocks.IsEmpty() && ComputerRocks[0]->SuccessfulCuts>0);
        // Never save an already submerged respawn shelf: that would create a death loop.
        if(!bComputerCheckpoint && ReachedTelevision && WaterZ>=ComputerCheckpointPosition.Z-60)
        {Fail(TEXT("电视入口已经进水，没能及时赶到！"));UpdateWater();return;}
        if(!bComputerCheckpoint && ReachedTelevision)
        {bComputerCheckpoint=true;ComputerElapsed=0;CheckpointElapsed=Elapsed-DeltaSeconds;CheckpointWaterZ=ComputerStartWaterZ=WaterZ;SetTelevisionMining(true);UMiningRoomAudioSubsystem::Emit(this,TEXT("S_Checkpoint"),.55f);}
        if(!bEnteredComputer && InComputer && bComputerCheckpoint && TelevisionScreen && TelevisionScreen->SuccessfulCuts>0)
        {
            bEnteredComputer=true;UMiningRoomAudioSubsystem::StopSource(this);
        }
        // The intact screen is still a barrier. Only a real cut followed by entry grants safety.
        if(bEnteredComputer) { /* Keep the room water at the arrival height. */ }
        else if(Elapsed<RapidFillSeconds)WaterZ=FMath::Lerp(WaterStartZ,TableWaterZ,Elapsed/FMath::Max(.1f,RapidFillSeconds));
        else WaterZ=FMath::Lerp(TableWaterZ,WaterEndZ,FMath::Clamp((Elapsed-RapidFillSeconds)/FMath::Max(1.f,FloodSeconds-RapidFillSeconds),0.f,1.f));
        // Shelf top is Entry.Z - 700: stop 150 cm below it, retaining the original rise speed.
        WaterZ=FMath::Min(WaterZ,float(ComputerEntry.Z-850));
        // Sequentially cross the far face of each contiguous solid section.
        if(ComputerRocks.IsValidIndex(ClearedNodes))
        {
            auto* Rock=ComputerRocks[ClearedNodes].Get();
            const FVector Local=Rock->GetActorRotation().UnrotateVector(P-Rock->GetActorLocation());
            // The authored CRT has an offset mesh pivot, rotation and scale.
            // Its passage is defined by the TV entrance, not the asset's pivot.
            const bool Crossed = ClearedNodes==0 ? bEnteredComputer :
                Local.X>Rock->RockSize.X*.5f && Local.X<Rock->RockSize.X*.5f+600 &&
                FMath::Abs(Local.Y)<TunnelHalfWidth && FMath::Abs(Local.Z)<TunnelHalfHeight;
            if(Rock->SuccessfulCuts>0 && Crossed){++ClearedNodes;UMiningRoomAudioSubsystem::Emit(this,TEXT("S_Confirm"),.2f);}
        }
        const auto* Character=Cast<ACharacter>(Player->GetPawn());
        const float Head=Character?Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*.75f:50.f;
        // Hazards win a same-frame race with the exit.
        if(!bEnteredComputer && (P.Z+Head<WaterZ || P.Z<SourceLevelOffset.Z))Fail(TEXT("被上涨的水淹没了！"));
        else if(!bEnteredComputer && !bComputerCheckpoint && Elapsed>=FMath::Max(1.f,FloodSeconds))Fail(TEXT("未能及时到达电视平台！"));
        else if(bEnteredComputer && !ComputerRocks.IsEmpty() && ClearedNodes==ComputerRocks.Num() && FVector::Dist(P,Exit)<ExitRadius)
        {Phase=EMiningEscapePhase::Escaped;PhaseTime=0;Player->EncounterPolicy->SetMiningAllowed(true);SetTelevisionMining(false);UMiningRoomAudioSubsystem::StopSource(this);UMiningRoomAudioSubsystem::Emit(this,TEXT("S_Escape"),.65f);}
        UpdateWater();
    }
    TickExitStage();
}
void AMiningRoomEscape::EndPlay(const EEndPlayReason::Type Reason)
{
    SetTelevisionMining(false);
    if(CardStage){CardStage->OnOpponentWallImpact.RemoveAll(this);CardStage->OnCollectibleClaimed.RemoveAll(this);CardStage->SubtitleOverride.Unbind();}
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
