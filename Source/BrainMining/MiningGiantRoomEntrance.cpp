#include "MiningGiantRoomEntrance.h"
#include "MiningEncounterPlayer.h"
#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "EchoPassagePlayer.h"
#include "EchoPassageStage.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/World.h"

AMiningGiantRoomEntrance::AMiningGiantRoomEntrance()
{
    PrimaryActorTick.bCanEverTick=true;RockSize=FVector(240,400,380);
    RoomBounds=CreateDefaultSubobject<UBoxComponent>(TEXT("GiantRoomBounds"));RoomBounds->SetupAttachment(RootComponent);
    RoomBounds->SetRelativeLocation(FVector(3780,0,1220));RoomBounds->SetBoxExtent(FVector(3780,2820,1410));
    RoomBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    RoomBounds->SetCollisionResponseToAllChannels(ECR_Ignore);RoomBounds->SetGenerateOverlapEvents(false);
    RoomExposure=CreateDefaultSubobject<UPostProcessComponent>(TEXT("GiantRoomExposure"));RoomExposure->SetupAttachment(RoomBounds);
    RoomExposure->bUnbound=false;RoomExposure->Priority=10;RoomExposure->BlendRadius=180;
    auto& S=RoomExposure->Settings;S.bOverride_AutoExposureMethod=true;S.AutoExposureMethod=AEM_Manual;
    S.bOverride_AutoExposureBias=true;S.AutoExposureBias=-6;
    S.bOverride_AutoExposureApplyPhysicalCameraExposure=true;S.AutoExposureApplyPhysicalCameraExposure=false;
}
void AMiningGiantRoomEntrance::ResetRock()
{
    Super::ResetRock();
    // A narrow real opening lets the room's light through, but cannot fit the player's height.
    for(float X=-160;X<=160;X+=40)
        ADiggableRock::MineSphere(GetActorTransform().TransformPosition(FVector(X,0,-40)),42);
    SuccessfulCuts=0;LastStatus=TEXT("A warm pinhole; excavate the surrounding plaster to enter");
}
void AMiningGiantRoomEntrance::ResetJourney()
{
    if(Player.IsValid())
    {
        Player->CardInteraction->LeaveSeat();
        if(APawn* Pawn=Player->GetPawn())
        {
            if(auto* Mine=Pawn->FindComponentByClass<UMiningComponent>())Mine->CancelMining();
            if(auto* Move=Pawn->FindComponentByClass<UMiningCharacterMovementComponent>())Move->SetFlightInputHeld(false);
        }
    }
    ResetRock();
}
void AMiningGiantRoomEntrance::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* PC=Cast<AMiningEncounterPlayerController>(GetWorld()->GetFirstPlayerController());
    if(!PC || !PC->GetPawn())return;
    if(Player.Get()!=PC)
    {
        if(Player.IsValid())Player->EchoInteraction->OnRestarted.RemoveAll(this);
        Player=PC;PC->EchoInteraction->OnRestarted.AddUObject(this,&AMiningGiantRoomEntrance::ResetJourney);
    }
    if(bExternalEncounterActive)return;
    const FVector P=GetActorTransform().InverseTransformPosition(PC->GetPawn()->GetActorLocation());
    const bool InRoom=P.X>RockSize.X*.5+35;
    const bool NearWall=P.X> -MiningZoneDepth && FMath::Abs(P.Y)<470;
    const bool Seated=PC->CardInteraction->Stage && PC->CardInteraction->Stage->Run.Phase!=ECardRoomPhase::Exploring;
    PC->EncounterPolicy->SetMiningAllowed((NearWall || InRoom) && !Seated);
    PC->EchoInteraction->SetExplorationActive(!NearWall && !InRoom);
    PC->CardInteraction->SetRoomActive(NearWall || InRoom || Seated);
    PC->CardInteraction->ExplorationHint=InRoom?TEXT("沿左侧的纸箱、书本和尺子上桌，或按住空格飞行；在光圈处按 E 入座"):TEXT("洞里透着暖光……按住左键挖开洞口周围的墙");
    if(InRoom && PC->EchoInteraction->Stage)PC->EchoInteraction->Stage->Complete();
}
void AMiningGiantRoomEntrance::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Player.IsValid())Player->EchoInteraction->OnRestarted.RemoveAll(this);
    Super::EndPlay(Reason);
}

