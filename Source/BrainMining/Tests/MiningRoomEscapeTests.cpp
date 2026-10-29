#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "MiningRoomEscape.h"
#include "InputKeyEventArgs.h"
#include "MiningRoomAudioSubsystem.h"
#include "MiningAudioComponent.h"
#include "Showcase/MiningExhibitHall.h"
#include "StartRoom/MiningStartRoom.h"
#include "MiningEncounterPlayer.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningComponent.h"
#include "MiningGUIWidgets.h"
#include "MiningCollection.h"
#include "MiningCollectionEffects.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "CardRoomUISettings.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "EchoPassagePlayer.h"
#include "DiggableRock.h"
#include "GeneratedMiningRock.h"
#include "Components/DynamicMeshComponent.h"
#include "MeshQueries.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Editor.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PostProcessComponent.h"

namespace
{
void CheckStageEnclosure(FAutomationTestBase* Test,AMiningRoomEscape* E)
{
    UDynamicMeshComponent* Shell=nullptr;
    TArray<UDynamicMeshComponent*> Meshes;E->GetComponents(Meshes);
    for(auto* Mesh:Meshes)if(Mesh->ComponentHasTag(TEXT("EscapeStageBedrock")))Shell=Mesh;
    if(!Test->TestNotNull(TEXT("Ramp and stage have permanent bedrock"),Shell))return;
    Shell->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Mesh)
    {
        Test->TestTrue(TEXT("Stage shell is a closed solid without edge cracks"),Mesh.IsClosed());
        Test->TestTrue(TEXT("Stage shell topology is valid"),Mesh.CheckValidity());
        Test->TestTrue(TEXT("Bedrock winding encloses positive solid volume"),UE::Geometry::TMeshQueries<UE::Geometry::FDynamicMesh3>::GetVolumeArea(Mesh).X>0);
    });
    Test->TestEqual(TEXT("Stage shell blocks flying player capsules"),Shell->GetCollisionResponseToChannel(ECC_Pawn),ECR_Block);
    Test->TestFalse(TEXT("Stage shell is not a mineable actor"),Shell->GetOwner()->IsA<ADiggableRock>());
    const FCollisionShape Capsule=FCollisionShape::MakeCapsule(42,96);
    auto Point=[&](float X,float Y,float Z){return FVector(X+E->SourceLevelOffset.X,E->ComputerEntry.Y+Y,Z+E->SourceLevelOffset.Z);};
    int32 Blocked=0,Probes=0;
    for(float X:{30400.f,31500.f,32500.f})
        for(int32 Angle=0;Angle<360;Angle+=30)
            for(float Along:{-.25f,0.f,.25f})
            {
                const float R=FMath::DegreesToRadians(float(Angle));
                const FVector From=Point(X,0,10400),To=From+FVector(Along,FMath::Cos(R),FMath::Sin(R))*6000;
                FHitResult Hit;++Probes;
                if(Shell->SweepComponent(Hit,From,To,FQuat::Identity,Capsule,true))++Blocked;
            }
    Test->TestEqual(TEXT("Ramp/stage side, roof, floor and diagonal escape sweeps all blocked"),Blocked,Probes);
    // The shell itself must leave a full player capsule clear along both joins.
    const FRotator Frame=(E->TunnelEnd-E->ComputerEntry).Rotation();
    auto TunnelPoint=[&](float Along){return E->ComputerEntry+Frame.RotateVector(FVector(Along,0,-E->TunnelHalfHeight+110));};
    const TArray<FVector> Route={TunnelPoint(6850),TunnelPoint(7100),Point(30600,0,10000),
        Point(31100,0,10000),Point(32600,0,10000),Point(33200,0,10000)};
    for(int32 I=1;I<Route.Num();++I)
    {
        FHitResult Hit;
        Test->TestFalse(*FString::Printf(TEXT("Bedrock leaves walking route segment %d open"),I),
            Shell->SweepComponent(Hit,Route[I-1],Route[I],FQuat::Identity,Capsule,true));
    }
    // Above / beside the stage doorway must not provide a way around its collar.
    for(const FVector P:{Point(32500,0,11000),Point(32500,-1200,10300),Point(32500,1200,10300)})
    {
        FHitResult Hit;
        Test->TestTrue(TEXT("Stage rear wall seals the space outside the third-passage doorway"),
            Shell->SweepComponent(Hit,P,P+FVector(3000,0,0),FQuat::Identity,Capsule,true));
    }
    AGeneratedMiningRock* Facing=nullptr;
    for(TActorIterator<AGeneratedMiningRock> It(E->GetWorld());It;++It)
        if(It->GetOwner()==E && It->ActorHasTag(TEXT("EscapeStageRedFacing")))Facing=*It;
    if(!Test->TestNotNull(TEXT("Visible stage enclosure uses real mineable red rock"),Facing))return;
    Test->TestTrue(TEXT("Stage facing shares the third passage's textured red material"),Facing->RockMaterial && Facing->RockMaterial->GetName()==TEXT("M_DarkRed"));
    Facing->RockMesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Mesh)
    {
        Test->TestTrue(TEXT("Mineable facing is a closed solid"),Mesh.IsClosed());
        Test->TestTrue(TEXT("Red facing winding encloses positive solid volume"),UE::Geometry::TMeshQueries<UE::Geometry::FDynamicMesh3>::GetVolumeArea(Mesh).X>0);
    });
    for(int32 I=1;I<Route.Num();++I)
    {
        FHitResult Hit;
        Test->TestFalse(TEXT("Red facing leaves the full walking route open"),
            Facing->RockMesh->SweepComponent(Hit,Route[I-1],Route[I],FQuat::Identity,Capsule,true));
    }
}

class FStageFacingCutChecks : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<AGeneratedMiningRock> Facing;
    TWeakObjectPtr<UDynamicMeshComponent> Backing;
    FVector From,To;
    double Started=0;
public:
    explicit FStageFacingCutChecks(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(!Started)
        {
            UWorld* World=GEditor?GEditor->PlayWorld:nullptr;if(!World)return true;
            for(TActorIterator<AGeneratedMiningRock> It(World);It;++It)
                if(It->ActorHasTag(TEXT("EscapeStageRedFacing")))Facing=*It;
            if(!Test->TestNotNull(TEXT("Mineable stage facing survives the completed journey"),Facing.Get()))return true;
            auto* E=Cast<AMiningRoomEscape>(Facing->GetOwner());
            TArray<UDynamicMeshComponent*> Meshes;E->GetComponents(Meshes);
            for(auto* M:Meshes)if(M->ComponentHasTag(TEXT("EscapeStageBedrock")))Backing=M;
            From=FVector(31500+E->SourceLevelOffset.X,E->ComputerEntry.Y,10500+E->SourceLevelOffset.Z);
            To=From-FVector(0,2500,0);
            FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(StageFacing),true);
            Test->TestTrue(TEXT("Red facing is present before digging"),Facing->RockMesh->LineTraceComponent(Hit,From,To,Query));
            Test->TestTrue(TEXT("Red facing accepts excavation through its full thickness"),Facing->MineSphere(From-FVector(0,1800,0),450));
            Started=FPlatformTime::Seconds();return false;
        }
        if(Facing->IsTerrainCutPending())
        {
            if(FPlatformTime::Seconds()-Started<20)return false;
            Test->AddError(TEXT("Stage facing excavation did not finish"));return true;
        }
        FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(StageFacing),true);
        Test->TestTrue(TEXT("Red facing excavation committed"),Facing->SuccessfulCuts>0);
        Test->TestFalse(TEXT("Digging opens a real hole through red rock"),Facing->RockMesh->LineTraceComponent(Hit,From,To,Query));
        Test->TestTrue(TEXT("Permanent backing still blocks flight after the red surface is removed"),
            Backing.IsValid() && Backing->SweepComponent(Hit,From,To,FQuat::Identity,FCollisionShape::MakeCapsule(42,96),true));
        Facing->ResetRock();return true;
    }
};

class FEscapeChecks : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Start=FPlatformTime::Seconds();
    bool bExhibitHall=false;
public:
    explicit FEscapeChecks(FAutomationTestBase* T,bool InExhibitHall=false):Test(T),bExhibitHall(InExhibitHall){}
    virtual bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>(bExhibitHall?90:25)){Test->AddError(TEXT("Escape PIE initialization timed out"));return true;}
        UWorld* World=GEditor?GEditor->PlayWorld:nullptr;
        if(bExhibitHall)
        {
            auto* Hall=AMiningExhibitHall::Find(World);
            if(!Hall || !Hall->bReady || World->GetTimeSeconds()<2.5) return false;
            // This isolated room fixture skips the starting bedroom. Grant only its
            // pickaxe/jetpack prerequisites; drill ownership still requires the card reward.
            if(auto* StartRoom=AMiningStartRoom::Find(World))StartRoom->UnlockDeveloperEquipment();
            const int32 JourneyIndex=Hall->bLinearCampaign?3:2;
            if(Hall->bLinearCampaign) Hall->UnlockedArea=JourneyIndex; // Isolated encounter fixture.
            if(Hall->ActiveArea!=JourneyIndex) Hall->EnterArea(JourneyIndex);
        }
        auto* PC=World?Cast<AMiningEncounterPlayerController>(World->GetFirstPlayerController()):nullptr;
        auto* Pawn=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
        if(!Pawn || !PC->CardInteraction->Stage)return false;
        AMiningRoomEscape* E=nullptr;
        for(TActorIterator<AMiningRoomEscape> It(World);It;++It){E=*It;break;}
        if(!Test->TestNotNull(TEXT("Study automatically assembles escape"),E))return true;
        int32 EscapeCount=0;for(TActorIterator<AMiningRoomEscape> It(World);It;++It)++EscapeCount;
        Test->TestEqual(TEXT("Placed escape does not also spawn a duplicate"),EscapeCount,1);
        TArray<UActorComponent*> EscapeParts;E->GetComponents(EscapeParts);
        for(auto* Part:EscapeParts)
            Test->TestFalse(TEXT("Editor TV preview cannot overlap the live diggable TV"),Part->ComponentHasTag(TEXT("TelevisionEditorPreview")));
        auto* Stage=PC->CardInteraction->Stage.Get();
        auto* RoomAudio=UMiningAudioComponent::IsMainGameWorld(World)?World->GetSubsystem<UMiningRoomAudioSubsystem>():nullptr;
        if(RoomAudio)RoomAudio->Tick(.01f);
        auto* Move=Cast<UMiningCharacterMovementComponent>(Pawn->GetCharacterMovement());
        auto* Miner=Pawn->FindComponentByClass<UMiningComponent>();const float OriginalRadius=Miner->Radius;
        Test->TestFalse(TEXT("Drill cannot activate before reward pickup"),Miner->IsDrillUnlocked() || Miner->ActivateDrill());
        Test->TestTrue(TEXT("Seat range covers the lamp pool"),Stage->IsPlayerNear(Stage->GetApproachLocation()+FVector(0,750,100)));
        Test->TestFalse(TEXT("Seat range ends outside the lamp pool"),Stage->IsPlayerNear(Stage->GetApproachLocation()+FVector(0,850,100)));
        Test->TestEqual(TEXT("Physical screen and fourteen thick dig sections"),E->ComputerRocks.Num(),15);
        Test->TestTrue(TEXT("Water, breach and route references exist"),E->Water && E->BreachWall);
        if(!E->AuthoredTelevisionScreen)
            Test->TestTrue(TEXT("TV coordinates follow the referenced room translation"),E->ComputerEntry.Equals(GetDefault<AMiningRoomEscape>()->ComputerEntry+E->SourceLevelOffset,1.));
        else
            Test->TestTrue(TEXT("Authored glass remains the real diggable television"),E->TelevisionScreen==E->AuthoredTelevisionScreen && E->TelevisionScreen->AuthoredRestMesh);
        Test->TestTrue(TEXT("Water heights follow the same room translation"),FMath::IsNearlyEqual(E->WaterStartZ,GetDefault<AMiningRoomEscape>()->WaterStartZ+float(E->SourceLevelOffset.Z),1.f));
        Test->TestTrue(TEXT("Breach is rebuilt at the translated source wall"),FMath::IsNearlyEqual(E->BreachWall->GetActorLocation().Z,9400.+E->SourceLevelOffset.Z,1.));
        Test->TestTrue(TEXT("No early flood"),E->Phase==EMiningEscapePhase::Idle);
        for(const auto& Prop:E->FloatingProps)Test->TestFalse(TEXT("No floating route before flood"),Prop->IsVisible());
        Test->TestTrue(TEXT("TV exit is behind original wall"),E->Exit.X>23700);
        Test->TestTrue(TEXT("Hole is above the room sign"),E->BreachWall->GetActorLocation().Z-1400>7480);
        Test->TestTrue(TEXT("Exit ramp is walkable without lift"),E->ExitRamp && FMath::Abs(E->ExitRamp->GetComponentRotation().Pitch)<15);
        Test->TestTrue(TEXT("Outburst includes a jet and spray"),E->JetStream!=nullptr && E->Foam.Num()>=48);
        for(int32 i=1;i<E->ComputerRocks.Num();++i)
        {
            auto* Rock=E->ComputerRocks[i].Get();
            Test->TestTrue(TEXT("Fill is thick solid material"),Rock->RockSize.X>=500);
            if(i>1)Test->TestTrue(TEXT("Continuous fill has no theatre gaps"),FMath::IsNearlyEqual(FVector::Dist(Rock->GetActorLocation(),E->ComputerRocks[i-1]->GetActorLocation()),500.f,1.f));
        }
        Pawn->SetActorLocation(Stage->GetApproachLocation()+FVector(0,0,100));
        PC->CardInteraction->SetRoomActive(true);PC->CardInteraction->Interact();
        Test->TestTrue(TEXT("Taking the seat automatically starts matchmaking"),Stage->Run.Phase==ECardRoomPhase::Matching);
        PC->PlayerTick(.01f);
        if(PC->MiningGUI)Test->TestFalse(TEXT("Mining HUD is hidden during the card encounter"),PC->MiningGUI->IsVisible());
        Stage->Tick(2.3f);Stage->Tick(1.5f);
        for(int32 Round=0;Round<4;++Round)
        {
            if(Round==3)
            {
                PC->CardInteraction->ConfirmCards();
                Test->TestTrue(TEXT("First final-round Space selects all cards without playing"),Stage->Run.CanConfirm() && Stage->Run.Phase==ECardRoomPhase::Choosing);
            }
            else for(int32 i=0;i<Stage->Run.Hand.Num();++i)Stage->Run.Stage(i);
            PC->CardInteraction->ConfirmCards();
            Test->TestTrue(TEXT("Confirm plays the selected hand"),Stage->Run.Phase==ECardRoomPhase::Resolving);
            Stage->Tick(.05f);
            TArray<UStaticMeshComponent*> CardParts;Stage->GetComponents(CardParts);
            int32 FaceCount=0;
            for(auto* Face:CardParts)
            {
                if(!Face->GetName().StartsWith(TEXT("CardMark")) || !Face->IsVisible())continue;
                const int32 Index=FCString::Atoi(*Face->GetName().RightChop(8));
                if(!Stage->Run.Hand.IsValidIndex(Index))continue;
                UTexture* Texture=nullptr;
                const bool HasArt=Face->GetMaterial(0) && Face->GetMaterial(0)->GetTextureParameterValue(FMaterialParameterInfo(TEXT("CardArt")),Texture);
                const UTexture2D* Art[]={Stage->UISettings->CardLaserArt.Get(),Stage->UISettings->CardPoisonArt.Get(),Stage->UISettings->CardFireArt.Get()};
                const int32 Type=int32(Stage->Run.Hand[Index]);
                if(Type<3)Test->TestTrue(TEXT("Played face uses the current hand artwork for its card type"),HasArt && Texture==Art[Type]);
                else Test->TestTrue(TEXT("New elemental card uses the matching paper-cut illustration"),HasArt && Texture && Texture->GetName()==TEXT("T_Element_")+FString(FCardRoomRun::CardKey(Stage->Run.Hand[Index])).RightChop(5));
                ++FaceCount;
            }
            Test->TestEqual(TEXT("Every played card has the updated face"),FaceCount,Stage->Run.Hand.Num());
            Stage->Tick(2.8f+(Round==3?FCardRoomRun::FusionSeconds:0.f));
            if(Round==3)
            {
                TArray<USceneComponent*> Parts;Stage->GetComponents(Parts);
                for(auto* Part:Parts)if(Part->GetFName()==TEXT("Opponent"))
                    Test->TestTrue(TEXT("Cat visual center ends at actual breach"),FVector::Dist(Part->GetComponentTransform().TransformPosition(Stage->OpponentExitCenterOffset),E->BreachWall->GetActorLocation())<1);
            }
            Stage->Tick(2.f);
        }
        Test->TestTrue(TEXT("Final cat impact creates wall hole"),E->bWallBroken && E->BreachWall->SuccessfulCuts>0);
        Test->TestTrue(TEXT("Impact waits for reward"),E->Phase==EMiningEscapePhase::Idle);
        for(const auto& Prop:E->FloatingProps)
        {
            Test->TestTrue(TEXT("Floating props appear with the first outburst"),Prop->IsVisible());
            Test->TestTrue(TEXT("Early floating props can be landed on"),Prop->GetCollisionEnabled()==ECollisionEnabled::QueryAndPhysics);
        }
        for(int32 Pad=0;Pad<6;++Pad)
        {
            double Bottom=MAX_dbl;
            for(const auto& Prop:E->FloatingProps)
                if(Prop->ComponentHasTag(FName(*FString::Printf(TEXT("EscapeFloatingPad%d"),Pad))))
                    Bottom=FMath::Min(Bottom,Prop->Bounds.Origin.Z-Prop->Bounds.BoxExtent.Z);
            Test->TestTrue(TEXT("Every floating stack extends below the water surface"),Bottom<E->WaterZ);
        }
        Test->TestTrue(TEXT("Victory reward claim succeeds"),Stage->CollectReward());
        Test->TestTrue(TEXT("Only the pickup unlocks and equips the drill"),Miner->IsDrillUnlocked() && Miner->bDrillMode);
        Test->TestTrue(TEXT("Reward starts warning and releases controls"),E->Phase==EMiningEscapePhase::Warning && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored());
        Test->TestFalse(TEXT("Duplicate reward ignored"),Stage->CollectReward());
        E->Tick(E->WarningSeconds+.1f);
        Test->TestTrue(TEXT("Warning transitions into flood"),E->Phase==EMiningEscapePhase::Flooding);
        Pawn->SetActorLocation(FVector(15000,3600,0));E->Tick(2.f);
        Test->TestTrue(TEXT("Submerged player fails"),E->Phase==EMiningEscapePhase::Failed);
        E->Tick(E->RetrySeconds+.1f);
        Test->TestTrue(TEXT("Automatic parkour retry"),E->Phase==EMiningEscapePhase::Flooding && FVector::Dist(Pawn->GetActorLocation(),E->Checkpoint)<1);
        Test->TestTrue(TEXT("Reward preserved on retry"),Stage->Run.Phase==ECardRoomPhase::Collected);
        Test->TestFalse(TEXT("Retry clears stale flight input"),Move->bFlightInputHeld);
        const float FloatOffset=E->FloatingProps[0]->GetComponentLocation().Z-E->WaterZ;
        E->Tick(E->RapidFillSeconds*.5f);
        Test->TestTrue(TEXT("Early surge remains below table"),E->WaterZ<E->TableWaterZ-100);
        for(const auto& Prop:E->FloatingProps)Test->TestTrue(TEXT("Props remain visible below tabletop"),Prop->IsVisible());
        Test->TestTrue(TEXT("Props rise with water during initial surge"),FMath::IsNearlyEqual(float(E->FloatingProps[0]->GetComponentLocation().Z),E->WaterZ+FloatOffset,1.f));
        E->Tick(E->RapidFillSeconds*.5f);
        Test->TestTrue(TEXT("Initial surge reaches just below tabletop"),FMath::IsNearlyEqual(E->WaterZ,E->TableWaterZ,1.f));
        Pawn->SetActorLocation(E->Checkpoint+FVector(0,0,2000));
        E->Tick(2.f);
        const float PreviousRisePerSecond=(E->WaterEndZ-E->TableWaterZ)/(120.f-E->RapidFillSeconds);
        Test->TestTrue(TEXT("Steady water rises twice as fast as the previous 120-second route"),FMath::IsNearlyEqual(E->WaterZ,E->TableWaterZ+PreviousRisePerSecond*4.f,1.f));
        Test->TestTrue(TEXT("HUD time matches the faster flood"),FMath::IsNearlyEqual(E->SecondsLeft(),56.f,.01f));
        Pawn->SetBase(E->FloatingProps[0]);E->Tick(.1f);Pawn->SetBase(static_cast<UPrimitiveComponent*>(nullptr));
        Test->TestTrue(TEXT("Standing contact starts only that floating step timer"),E->FloatingSinkElapsed[0]>=0 && E->FloatingSinkElapsed[1]<0);
        E->Tick(E->FloatingSinkDelay+E->FloatingSinkSeconds);
        Test->TestFalse(TEXT("Spent step sinks completely and loses collision"),E->FloatingProps[0]->IsVisible() || E->FloatingProps[0]->GetCollisionEnabled()!=ECollisionEnabled::NoCollision);
        E->RetryEscape();E->Tick(E->RapidFillSeconds);
        Test->TestTrue(TEXT("Automatic retry restores floating steps"),E->FloatingSinkElapsed[0]<0 && E->FloatingProps[0]->IsVisible());
        auto* LastPad=E->FloatingProps.Last().Get();
        Pawn->SetBase(LastPad);E->Tick(.1f);Pawn->SetBase(static_cast<UPrimitiveComponent*>(nullptr));
        Test->TestTrue(TEXT("Final TV platform stays stable on contact"),E->FloatingSinkElapsed.Last()<0);
        const FVector Axis=(E->TunnelEnd-E->ComputerEntry).GetSafeNormal();
        TArray<UPostProcessComponent*> ExposureComponents;E->GetComponents(ExposureComponents);
        Test->TestEqual(TEXT("Television and stage have local exposure"),ExposureComponents.Num(),2);
        if(ExposureComponents.Num())
        {
            auto* Exposure=ExposureComponents[0];
            Test->TestTrue(TEXT("Local exposure volume is enabled"),Exposure->GetProperties().bIsEnabled);
            Test->TestTrue(TEXT("Exposure bounds include first excavated section"),Exposure->EncompassesPoint(E->ComputerEntry+Axis*150,0,nullptr));
            Test->TestTrue(TEXT("Exposure bounds include screen transition"),Exposure->EncompassesPoint(E->ComputerEntry-FVector(300,0,0),0,nullptr));
            Test->TestFalse(TEXT("Television exposure does not cover room table"),Exposure->EncompassesPoint(E->Checkpoint,0,nullptr));
            Test->TestTrue(TEXT("Television disables motion blur"),Exposure->Settings.bOverride_MotionBlurAmount && Exposure->Settings.MotionBlurAmount==0);
            Test->TestTrue(TEXT("Television keeps bloom restrained"),Exposure->Settings.bOverride_BloomIntensity && Exposure->Settings.BloomIntensity<=.2f);
            Test->TestTrue(TEXT("Television overrides room manual exposure"),Exposure->Settings.bOverride_AutoExposureBias && Exposure->Settings.AutoExposureBias>-6);
        }
        // A position outside the shell cannot claim the computer checkpoint.
        Pawn->SetActorLocation(E->ComputerEntry+FVector(200,1000,0));E->Tick(.01f);
        Test->TestFalse(TEXT("Outside shell is not an entry"),E->bComputerCheckpoint);
        Test->TestTrue(TEXT("Far from television still guides to screen"),E->GuideTarget().Equals(E->ComputerEntry));
        Pawn->SetActorLocation(E->ComputerEntry-FVector(850,0,0));E->Tick(.01f);
        Test->TestTrue(TEXT("Near intact TV already points beyond screen to exit"),E->bExitGuidance && E->GuideTarget().Equals(E->TunnelEnd));
        Test->TestTrue(TEXT("Guidance does not start checkpoint or electrical timer"),!E->bComputerCheckpoint && !E->bEnteredComputer && E->ComputerElapsed==0 && E->TelevisionScreen->SuccessfulCuts==0);
        Pawn->SetActorLocation(E->ComputerEntry-FVector(1100,0,0));E->Tick(.01f);
        Test->TestTrue(TEXT("Brief retreat does not flip guidance back to screen"),E->GuideTarget().Equals(E->TunnelEnd));
        E->RetryEscape();
        Test->TestTrue(TEXT("Parkour retry resets guidance"),!E->bExitGuidance && E->GuideTarget().Equals(E->ComputerEntry));
        E->Tick(E->RapidFillSeconds);
        const float SavedFloatZ=E->FloatingProps[0]->GetComponentLocation().Z;
        Pawn->SetActorLocation(E->ComputerCheckpointPosition);E->Tick(.01f);
        const float SavedWater=E->CheckpointWaterZ,SavedElapsed=E->CheckpointElapsed;
        Test->TestTrue(TEXT("Checkpoint captured before digging TV"),E->bComputerCheckpoint);
        Test->TestTrue(TEXT("TV cuts have capsule clearance"),Miner->Radius>=E->TelevisionMineRadius);
        auto* Screen=E->ComputerRocks[0].Get();
        const FVector ScreenCenter=Screen->RockMesh->Bounds.Origin;
        Pawn->SetActorLocation(ScreenCenter-Axis*400);
        FHitResult ScreenHit;
        Pawn->SetActorLocation(ScreenCenter,true,&ScreenHit);
        // The authored cabinet may intercept the capsule before its glass.
        Test->TestTrue(TEXT("Intact television blocks walking into its interior"),ScreenHit.bBlockingHit);
        FHitResult GlassHit;
        Test->TestTrue(TEXT("Authored glass itself retains blocking collision"),Screen->ActorLineTraceSingle(GlassHit,ScreenCenter-Axis*400,ScreenCenter+Axis*400,ECC_Visibility,FCollisionQueryParams(SCENE_QUERY_STAT(TVGlassTest),true)));
        if(!ScreenHit.bBlockingHit || !GlassHit.bBlockingHit)
            Test->AddInfo(FString::Printf(TEXT("Screen collision diagnostic: center=%s start=%s axis=%s enabled=%d meshCollision=%d capsuleHit=%s"),*ScreenCenter.ToString(),*(ScreenCenter-Axis*400).ToString(),*Axis.ToString(),Screen->GetActorEnableCollision(),int32(Screen->RockMesh->GetCollisionEnabled()),*GetNameSafe(ScreenHit.GetActor())));
        Test->TestFalse(TEXT("An intact screen has not entered the television"),E->bEnteredComputer);
        Test->TestTrue(TEXT("Authored screen accepts excavation at its visible surface"),Screen->MineSphere(ScreenCenter,190));
        Pawn->SetActorLocation(E->ComputerEntry+Axis*180);E->Tick(.01f);
        Test->TestTrue(TEXT("Excavated screen crossing creates second checkpoint"),E->bComputerCheckpoint);
        const float SafeWater=E->WaterZ;
        E->Tick(E->ComputerSeconds*4);
        Test->TestTrue(TEXT("Real television entry grants permanent safety with no electrical timeout"),E->bEnteredComputer && E->Phase==EMiningEscapePhase::Flooding && E->ComputerElapsed==0);
        Test->TestEqual(TEXT("Safe entry stops room water rising"),E->WaterZ,SafeWater);
        // Before screen entry, drowning still returns to the saved television checkpoint.
        E->RetryEscape();
        Pawn->SetActorLocation(FVector(E->ComputerCheckpointPosition.X,E->ComputerCheckpointPosition.Y,E->WaterZ-200));E->Tick(.01f);
        Test->TestTrue(TEXT("Drowning before entering television still fails"),E->Phase==EMiningEscapePhase::Failed);
        E->Tick(E->RetrySeconds+.1f);
        Test->TestTrue(TEXT("Automatic retry is before computer"),FVector::Dist(Pawn->GetActorLocation(),E->ComputerCheckpointPosition)<1 && E->bComputerCheckpoint);
        Test->TestEqual(TEXT("Dig geometry restored"),Screen->SuccessfulCuts,0);
        Test->TestTrue(TEXT("TV retry retains distant exit guidance"),E->bExitGuidance && E->GuideTarget().Equals(E->TunnelEnd));
        Test->TestEqual(TEXT("Retry restores exact arrival water"),E->WaterZ,SavedWater);
        Test->TestEqual(TEXT("Retry restores room clock"),E->Elapsed,SavedElapsed);
        Test->TestTrue(TEXT("Floating props restore checkpoint height"),FMath::Abs(E->FloatingProps[0]->GetComponentLocation().Z-SavedFloatZ)<1);
        E->Tick(10);E->RetryEscape();
        Test->TestEqual(TEXT("Repeated retries do not accumulate water"),E->WaterZ,SavedWater);
        Pawn->SetActorLocation(E->ComputerCheckpointPosition);E->Tick(E->FloodSeconds*2);
        Test->TestTrue(TEXT("TV shelf remains safe past the room time limit"),E->Phase==EMiningEscapePhase::Flooding && !E->bEnteredComputer);
        Test->TestTrue(TEXT("Water stops below the TV landing"),E->WaterZ<=E->ComputerEntry.Z-850);
        E->RetryEscape();
        Test->TestTrue(TEXT("Computer retry receives full countdown"),E->ElectricalSecondsLeft()>=E->ComputerSeconds-.1f);
        const int32 AttemptsBeforeR=E->Attempts;
        const FVector PositionBeforeR=Pawn->GetActorLocation();
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::R,IE_Pressed,1));
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::R,IE_Released,0));
        PC->EchoInteraction->Restart();
        Test->TestTrue(TEXT("Manual restart cannot reset card-room progress"),E->Attempts==AttemptsBeforeR && Pawn->GetActorLocation().Equals(PositionBeforeR));
        // Success requires actual boolean cuts and crossing every slab, not an exterior teleport.
        Pawn->SetActorLocation(E->Exit);E->Tick(.01f);
        Test->TestTrue(TEXT("Cannot bypass sealed tunnel"),E->Phase==EMiningEscapePhase::Flooding);
        for(auto& Rock:E->ComputerRocks)
        {
            Test->TestTrue(TEXT("TV fill accepts real mesh excavation"),Rock->MineSphere(Rock->RockMesh->Bounds.Origin,190));
            Pawn->SetActorLocation(Rock==Screen ? E->ComputerEntry+Axis*180 : Rock->GetActorLocation()+Rock->GetActorForwardVector()*(Rock->RockSize.X*.5f+60));E->Tick(.01f);
        }
        Pawn->SetActorLocation(E->Exit);E->Tick(.01f);
        Test->TestTrue(TEXT("Upper exit completes escape"),E->Phase==EMiningEscapePhase::Escaped);
        Test->TestNotNull(TEXT("Stage celebration exists"),E->StageCelebration.Get());
        E->StageCelebration->Tick(.1f);
        Test->TestTrue(TEXT("Approaching stage opens its curtain"),E->StageCelebration->bCurtainOpen);
        Test->TestTrue(TEXT("Confetti starts immediately with the curtain"),E->StageCelebration->bStageActive);
        if(RoomAudio) Test->TestEqual(TEXT("Victory sound starts on the first confetti frame"),RoomAudio->EventCount(TEXT("S_TVEscapeVictory")),1);
        Test->TestTrue(TEXT("Collectible ribbon waits inside its cannon"),E->StageRibbon->IsHidden() && !E->StageRibbon->bReleased);
        Pawn->SetActorLocation(E->Exit+FVector(650,0,0));E->StageCelebration->Tick(.1f);
        Test->TestTrue(TEXT("Walking forward onto stage fires confetti"),E->StageCelebration->bStageActive);
        E->StageCelebration->Tick(.6f);E->Tick(.01f);
        Test->TestTrue(TEXT("Collectible ribbon launches visibly before becoming collectible"),!E->StageRibbon->IsHidden() && !E->StageRibbon->bReleased);
        E->StageCelebration->Tick(4.5f);E->Tick(.01f);
        Test->TestTrue(TEXT("Ribbon lands before enabling pickup"),E->StageRibbon->bReleased);
        CheckStageEnclosure(Test,E);
        if(RoomAudio) Test->TestEqual(TEXT("Victory sound does not repeat while confetti runs"),RoomAudio->EventCount(TEXT("S_TVEscapeVictory")),1);
        Pawn->SetActorLocation(E->Exit);
        Test->TestTrue(TEXT("Stage has one collectible ribbon using shared item ID"),E->StageRibbon && E->StageRibbon->ItemId==11 && !E->StageRibbon->bCollected);
        Test->TestTrue(TEXT("Mining input is restored after the card/TV escape"),PC->EncounterPolicy && PC->EncounterPolicy->IsMiningAllowed() && !Miner->IsEncounterBlocked());
        if(RoomAudio)
        {
            for(FName Cue:{FName(TEXT("S_CardDraw")),FName(TEXT("S_CardReward")),FName(TEXT("S_FloodStart")),FName(TEXT("S_Drown")),FName(TEXT("S_Retry")),FName(TEXT("S_Checkpoint")),FName(TEXT("S_Escape"))})
                Test->TestTrue(*FString::Printf(TEXT("Encounter triggers %s"),*Cue.ToString()),RoomAudio->EventCount(Cue)>0);
        }
        Test->TestEqual(TEXT("TV mining adjustment is restored on exit"),Miner->Radius,OriginalRadius);
        Test->TestTrue(TEXT("Reward still retained after completion"),Stage->Run.Phase==ECardRoomPhase::Collected);
        PC->EchoInteraction->Restart();
        Test->TestTrue(TEXT("Manual restart cannot replay completed escape"),E->Phase==EMiningEscapePhase::Escaped && FVector::Dist(Pawn->GetActorLocation(),E->Exit)<1);
        return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardEscapeTest,"BrainMining.Encounters.PIE.CardEscapeCheckpoints",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCardEscapeTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEscapeChecks>(this));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStageFacingCutChecks>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP2CardEscapeTest,"BrainMining.Encounters.PIE.P2CardEscapeCheckpoints",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP2CardEscapeTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2"));
    UWorld* EditorWorld=GEditor->GetEditorWorldContext().World();
    AMiningRoomEscape* Placed=nullptr;
    for(TActorIterator<AMiningRoomEscape> It(EditorWorld);It;++It){Placed=*It;break;}
    if(!TestNotNull(TEXT("P2 contains an authored television escape actor"),Placed))return false;
    TestTrue(TEXT("Authored television links to P2 cards and entrance"),Placed->CardStage && Placed->Entrance);
    TestTrue(TEXT("P2 television entrance matches P1"),Placed->ComputerEntry.Equals(GetDefault<AMiningRoomEscape>()->ComputerEntry,1));
    TestTrue(TEXT("P2 landing and final exit match P1"),Placed->ComputerCheckpointPosition.Equals(GetDefault<AMiningRoomEscape>()->ComputerCheckpointPosition,1) && Placed->Exit.Equals(GetDefault<AMiningRoomEscape>()->Exit,1));
    TArray<UActorComponent*> Parts;Placed->GetComponents(Parts);
    int32 PreviewParts=0;for(auto* Part:Parts)if(Part->ComponentHasTag(TEXT("TelevisionEditorPreview")))++PreviewParts;
    TestTrue(TEXT("P2 restores the P1 television and green diggable screen"),!Placed->AuthoredTelevisionScreen && PreviewParts>0);
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEscapeChecks>(this));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStageFacingCutChecks>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExhibitCardEscapeTest,"BrainMining.SixAreas.FullCardAndTelevisionEscape",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FExhibitCardEscapeTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Showcase/Maps/Lvl_MainExperiences"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEscapeChecks>(this,true));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStageFacingCutChecks>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCampaignCardEscapeTest,"BrainMining.Campaign.FullCardAndTelevisionEscape",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCampaignCardEscapeTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Campaign/Maps/Lvl_FullJourney"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEscapeChecks>(this,true));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStageFacingCutChecks>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainGameCardEscapeTest,"BrainMining.MainGame.FullCardAndTelevisionEscape",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainGameCardEscapeTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/MainGame/Maps/Lvl_MainGame"));
    int32 RobotStages=0;
    for(TActorIterator<ACardRoomStage> It(GEditor->GetEditorWorldContext().World());It;++It)
    {
        TestTrue(TEXT("MainGame references latest P2 source"),It->GetOutermost()->GetName().Contains(TEXT("Lvl_EchoMazeToCards_P2")));
        TestTrue(TEXT("MainGame uses the supplied robot opponent"),It->bUseRobotOpponent);
        ++RobotStages;
    }
    TestEqual(TEXT("MainGame has one P2 card encounter"),RobotStages,1);
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEscapeChecks>(this,true));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStageFacingCutChecks>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
