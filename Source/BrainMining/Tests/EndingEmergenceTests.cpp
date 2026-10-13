#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "EndingEmergence.h"
#include "DiggableRock.h"
#include "MiningComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "UnrealClient.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "PlayInEditorDataTypes.h"

namespace
{
class FStartEndingPIE : public IAutomationLatentCommand
{
public:
    bool Update() override
    {
        FRequestPlaySessionParams P;
        auto* Settings=NewObject<ULevelEditorPlaySettings>(GetTransientPackage(),NAME_None,RF_Transient);
        Settings->NewWindowWidth=1280;Settings->NewWindowHeight=720;P.EditorPlaySettings=Settings;P.bAllowOnlineSubsystem=false;
        GEditor->RequestPlaySession(P);return true;
    }
};
class FEndingRoute : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int Step=0,LastShot=-2,LastGate=-1,DriftStep=0,BoostStep=0;
    double Started=FPlatformTime::Seconds(),Since=Started,DriftAt=0,TelemetryAt=0;
    float ReleaseSpeed=0,EarlyRate=0,ReleaseRate=0,LastRadius=0,TakeoffZ=0;bool Captured=false,BlackSeen=false,EyeSeen=false,AscentSeen=false,FadeSeen=false;
    int BloodCheck=0,BrainCheck=0,JumpIndex=0;double CheckAt=0;FVector CheckPosition=FVector::ZeroVector;
    const int JumpRoute[7]={2,0,4,9,14,19,21};
    bool TargetCaptured=false,BranchesCaptured=false,TransitChecked=false,EntrySeen=false,CurveCaptured=false,ThresholdCaptured=false;
    float HighestBloodZ=0,WidestBloodY=0,CheckDistance=0;double EntryAt=0;
    FVector PreviousEntryPosition=FVector::ZeroVector;float PreviousEntryTime=0;
    void Next(){++Step;Since=FPlatformTime::Seconds();}
    void Key(APlayerController* P,FKey K,bool Down){P->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f));}
    void Capture(const FString& Name)
    {
        const FString Dir=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("EndingEmergencePreview"));IFileManager::Get().MakeDirectory(*Dir,true);
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(Dir,Name+TEXT(".png")),true,false);
    }
public:
    explicit FEndingRoute(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds(),Age=Now-Since;
        if(Now-Started>300){Capture(TEXT("failure-timeout"));Test->AddError(FString::Printf(TEXT("Ending route timeout, step %d, gate %d, shot %d"),Step,LastGate,LastShot));return true;}
        UWorld* W=GEditor?GEditor->PlayWorld:nullptr;
        auto* PC=W?W->GetFirstPlayerController():nullptr;
        auto* Pawn=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
        if(!Pawn||W->GetTimeSeconds()<2)return false;
        AEndingEmergence* D=nullptr;for(TActorIterator<AEndingEmergence> It(W);It;++It){D=*It;break;}
        if(!D){Test->AddError(TEXT("Saved ending actor missing"));return true;}
        auto* Mining=Pawn->FindComponentByClass<UMiningComponent>();
        if(!Mining){Test->AddError(TEXT("Original mining component missing"));return true;}
        if(Now-TelemetryAt>5)
        {
            TelemetryAt=Now;
            UE_LOG(LogTemp,Display,TEXT("ENDING_CHECK step=%d gate=%d shot=%d pos=%s velocity=%s target=%s radius=%.1f"),Step,D->GateIndex,D->Shot,*Pawn->GetActorLocation().ToString(),*Pawn->GetVelocity().ToString(),*GetNameSafe(Mining->TargetRock),Mining->GetMiningRadius());
            FHitResult Hit;FCollisionQueryParams Params;Params.AddIgnoredActor(Pawn);
            W->SweepSingleByChannel(Hit,Pawn->GetActorLocation(),Pawn->GetActorLocation()+FVector(200,0,0),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),Params);
            UE_LOG(LogTemp,Display,TEXT("ENDING_FLOOR normal=%s component=%s sweep=%s normal=%s impact=%s"),*Pawn->GetCharacterMovement()->CurrentFloor.HitResult.ImpactNormal.ToString(),*GetNameSafe(Pawn->GetCharacterMovement()->CurrentFloor.HitResult.GetComponent()),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactNormal.ToString(),*Hit.ImpactPoint.ToString());
        }
        switch(Step)
        {
        case 0:
            Test->TestEqual(TEXT("Only the cell membrane requires mining"),D->Gates.Num(),1);
            Test->TestEqual(TEXT("Expanded saved branching neuron network"),D->NeuronPositions.Num(),22);
            Test->TestEqual(TEXT("Original first person pawn"),Pawn->GetClass()->GetName(),FString(TEXT("BP_FirstPersonCharacter_C")));
            Capture(TEXT("01-cell"));Key(PC,EKeys::W,true);Next();break;
        case 1:
            if(Age<6)return false;
            Key(PC,EKeys::W,false);
            Test->TestTrue(TEXT("Walking input moves pawn toward first rock"),Pawn->GetActorLocation().X>400);
            Test->TestTrue(TEXT("Uncut rock still physically blocks passage"),Pawn->GetActorLocation().X<D->Gates[0]->GetActorLocation().X);
            Test->TestEqual(TEXT("No automatic progress without mining"),D->GateIndex,0);
            Key(PC,EKeys::W,true);Key(PC,EKeys::LeftMouseButton,true);Next();break;
        case 2:
            if(D->Shot>=0)
            {
                Key(PC,EKeys::W,false);
                Test->TestEqual(TEXT("All connections traversed with real input"),D->GateIndex,5);
                Test->TestTrue(TEXT("Cell membrane really excavated"),D->Gates[0]->SuccessfulCuts>0);
                Test->TestTrue(TEXT("Pawn movement suspended in film"),Pawn->GetCharacterMovement()->MovementMode==MOVE_None);
                Test->TestTrue(TEXT("Mining suspended in film"),Mining->IsInputSuspended());
                Test->TestEqual(TEXT("Automatic flight controls checked"),BloodCheck,5);
                Test->TestEqual(TEXT("Largest neuron reached by real Space input"),D->CurrentNeuron,21);
                Test->TestEqual(TEXT("Branching route including a backtrack completed"),D->PossessionCount,7);
                Test->TestTrue(TEXT("Vessel bends sideways and climbs around its axis"),WidestBloodY>1000&&HighestBloodZ>2800);
                Test->TestTrue(TEXT("Blood enters brain through a continuous approach"),EntrySeen);
                Test->TestTrue(TEXT("Camera rises before leaving the brain"),AscentSeen);
                Test->TestTrue(TEXT("Ascent overlaps the gradual transition"),FadeSeen);
                Test->TestTrue(TEXT("Blood keeps flowing forward throughout the route"),D->BloodTravel>10000);
                D->FilmSpeed=4;Next();break;
            }
            if(D->GateIndex==0)
            {
                const FVector Target=D->GetGateApproach(D->GateIndex)+FVector(580,0,0);
                FVector Eye;FRotator ViewRotation;PC->GetPlayerViewPoint(Eye,ViewRotation);
                const FVector Opening=D->Gates[D->GateIndex]->GetActorLocation()+FVector(0,0,-70);
                FRotator R=(Target-Pawn->GetActorLocation()).Rotation();
                // Aim at head clearance as the floor rises/falls, just as a player
                // adjusts the mouse while digging uphill; never edit rock state.
                R.Pitch=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Opening.Z-Eye.Z,FMath::Max(180.,FMath::Abs(Opening.X-Eye.X)))),-20.,25.);
                R.Roll=0;PC->SetControlRotation(R);
                if(D->GateIndex!=LastGate){LastGate=D->GateIndex;Capture(FString::Printf(TEXT("gameplay-%d"),LastGate));}
            }
            if(D->GateIndex==1)
            {
                const FVector CameraPosition=PC->GetViewTarget()->GetActorLocation();
                HighestBloodZ=FMath::Max(HighestBloodZ,float(CameraPosition.Z));WidestBloodY=FMath::Max(WidestBloodY,float(FMath::Abs(CameraPosition.Y)));
                if(D->VesselDistance>4500&&!CurveCaptured){Capture(TEXT("blood-spiral"));CurveCaptured=true;}
                if(D->VesselDistance>11600&&!ThresholdCaptured){Capture(TEXT("blood-opening-lattice"));ThresholdCaptured=true;}
                if(D->bEnteringBrain)
                {
                    if(!EntrySeen){EntrySeen=true;EntryAt=Now;PreviousEntryPosition=CameraPosition;PreviousEntryTime=W->GetTimeSeconds();Capture(TEXT("blood-brain-open-threshold"));}
                    const float Dt=W->GetTimeSeconds()-PreviousEntryTime;
                    Test->TestTrue(TEXT("Brain approach advances continuously without a camera cut"),FVector::Dist(CameraPosition,PreviousEntryPosition)<1200*Dt+5);
                    Test->TestEqual(TEXT("No flash hides the brain approach"),D->OverlayAlpha(),0.f);
                    PreviousEntryPosition=CameraPosition;PreviousEntryTime=W->GetTimeSeconds();
                }
                if(BloodCheck==0)
                {
                    Key(PC,EKeys::W,false);Key(PC,EKeys::LeftMouseButton,false);CheckAt=Now;CheckDistance=D->VesselDistance;BloodCheck=1;
                    Test->TestTrue(TEXT("Tool and body leave first person view in blood"),Pawn->IsHidden()&&Mining->IsInputSuspended());
                    Capture(TEXT("blood-tool-release"));
                }
                else if(BloodCheck==1&&Now-CheckAt>1)
                {
                    Test->TestTrue(TEXT("Blood automatically carries player along the curve without W"),D->VesselDistance-CheckDistance>350);
                    Key(PC,EKeys::S,true);CheckAt=Now;CheckDistance=D->VesselDistance;BloodCheck=2;
                }
                else if(BloodCheck==2&&Now-CheckAt>1.2)
                {
                    Test->TestTrue(TEXT("S cannot brake or reverse automatic flight"),D->VesselDistance-CheckDistance>500&&FMath::Abs(D->BloodSpeed-480)<10);
                    Test->TestTrue(TEXT("Player floats high above the vessel floor"),CameraPosition.Z>700);
                    Capture(TEXT("blood-floating"));Key(PC,EKeys::S,false);Key(PC,EKeys::W,true);CheckAt=Now;BloodCheck=3;
                }
                else if(BloodCheck==3&&Now-CheckAt>1)
                {
                    Test->TestTrue(TEXT("W accelerates blood flight"),D->BloodSpeed>850);Key(PC,EKeys::W,false);CheckAt=Now;BloodCheck=4;
                }
                else if(BloodCheck==4&&Now-CheckAt>1)
                {Test->TestTrue(TEXT("Releasing W returns to automatic forward flight"),D->BloodSpeed>470&&D->BloodSpeed<550);BloodCheck=5;}
            }
            if(D->GateIndex==2)
            {
                if(BrainCheck==0)
                {
                    Test->TestEqual(TEXT("Continuous approach settles on entry neuron"),D->CurrentNeuron,0);
                    Test->TestTrue(TEXT("Entry is a visible multi-second glide"),EntrySeen&&Now-EntryAt>3);
                    Test->TestTrue(TEXT("Entry finishes without snapping to host"),FVector::Dist(PC->GetViewTarget()->GetActorLocation(),PreviousEntryPosition)<30);
                    int Choices=0;for(int I=0;I<D->NeuronPositions.Num();++I)if(D->CanPossess(I))++Choices;
                    Test->TestTrue(TEXT("Entry offers multiple routes"),Choices>=3);
                    Test->TestFalse(TEXT("Final neuron cannot be skipped to from entry"),D->CanPossess(21));
                    CheckPosition=PC->GetViewTarget()->GetActorLocation();PC->SetControlRotation(FRotator(85,0,0));Key(PC,EKeys::SpaceBar,true);Key(PC,EKeys::W,true);CheckAt=Now;BrainCheck=1;
                }
                else if(BrainCheck==1&&Now-CheckAt>.4)
                {
                    Test->TestEqual(TEXT("Space without aimed target does nothing"),D->PossessionCount,0);
                    Test->TestTrue(TEXT("Movement input cannot walk off a neuron"),PC->GetViewTarget()->GetActorLocation().Equals(CheckPosition,.1));
                    Key(PC,EKeys::W,false);Key(PC,EKeys::SpaceBar,false);BrainCheck=2;CheckAt=Now;
                }
                else if(BrainCheck==2&&JumpIndex<7)
                {
                    const int Aim=JumpRoute[JumpIndex];PC->SetControlRotation((D->NeuronWorldPosition(Aim)-PC->GetViewTarget()->GetActorLocation()).Rotation());
                    if(Now-CheckAt>.3&&D->TargetNeuron==Aim)
                    {
                        if(JumpIndex==0&&!TargetCaptured){Capture(TEXT("brain-neuron-target"));TargetCaptured=true;CheckAt=Now;return false;}
                        if(JumpIndex==3&&!BranchesCaptured){Capture(TEXT("brain-neuron-branches"));BranchesCaptured=true;CheckAt=Now;return false;}
                        CheckPosition=PC->GetViewTarget()->GetActorLocation();TransitChecked=false;
                        Key(PC,EKeys::SpaceBar,true);CheckAt=Now;BrainCheck=3;
                    }
                }
                else if(BrainCheck==3)
                {
                    if(!TransitChecked&&Now-CheckAt>.28)
                    {
                        TransitChecked=true;
                        Test->TestEqual(TEXT("Space starts a physical flight toward the selected neuron"),D->TravellingToNeuron,JumpRoute[JumpIndex]);
                        Test->TestEqual(TEXT("Host transfer only completes after travelling"),D->PossessionCount,JumpIndex);
                        Test->TestTrue(TEXT("Camera leaves the host and crosses space before arrival"),FVector::Dist(PC->GetViewTarget()->GetActorLocation(),CheckPosition)>80&&FVector::Dist(PC->GetViewTarget()->GetActorLocation(),D->NeuronWorldPosition(JumpRoute[JumpIndex]))>250);
                        Test->TestEqual(TEXT("Possession has no full-screen flash"),D->OverlayAlpha(),0.f);
                        if(JumpIndex==0)Capture(TEXT("brain-first-person-transit"));
                    }
                    if(Now-CheckAt>1.6)
                    {
                        Test->TestEqual(TEXT("Camera flight settles on the aimed host"),D->CurrentNeuron,JumpRoute[JumpIndex]);
                        Test->TestEqual(TEXT("Holding Space does not chain automatic hops"),D->PossessionCount,JumpIndex+1);
                        Test->TestEqual(TEXT("Flight state clears on arrival"),D->TravellingToNeuron,INDEX_NONE);
                        Key(PC,EKeys::SpaceBar,false);++JumpIndex;BrainCheck=2;CheckAt=Now;
                    }
                }
            }
            if(D->GateIndex==5)
            {
                Key(PC,EKeys::SpaceBar,false);
                const float Z=PC->GetViewTarget()->GetActorLocation().Z;
                if(TakeoffZ==0)TakeoffZ=Z;
                if(Z-TakeoffZ>600)AscentSeen=true;
                if(AscentSeen&&D->OverlayAlpha()>0&&D->OverlayAlpha()<.85f&&!FadeSeen){FadeSeen=true;Capture(TEXT("brain-takeoff"));}
            }
            break;
        case 3:
            if(D->Shot!=LastShot){LastShot=D->Shot;Captured=false;LastRadius=0;}
            if(!Captured&&D->ShotTime>AEndingEmergence::Duration(D->Shot)*.62f&&D->Shot<10)
            {Capture(FString::Printf(TEXT("film-%02d"),D->Shot));Captured=true;}
            if(D->Shot==1&&BoostStep==0){Key(PC,EKeys::W,true);DriftAt=Now;BoostStep=1;}
            if(D->Shot==1&&BoostStep==1&&Now-DriftAt>.7)
            {Test->TestEqual(TEXT("W cannot accelerate or reverse the pullback"),D->PullbackRate,1.f);Key(PC,EKeys::W,false);BoostStep=2;}
            if(D->Shot==2&&BoostStep==2){D->FilmSpeed=1;Key(PC,EKeys::S,true);DriftAt=Now;BoostStep=3;}
            if(D->Shot==2&&BoostStep==3&&Now-DriftAt>1)
            {EarlyRate=D->PullbackRate;Test->TestTrue(TEXT("S accelerates the continental pullback"),EarlyRate>2);Key(PC,EKeys::S,false);D->FilmSpeed=4;BoostStep=4;}
            if(D->Shot>=4&&D->Shot<=6)
            {
                const float Radius=(PC->GetViewTarget()->GetActorLocation()-D->GetActorTransform().TransformPosition(FVector(95000,0,0))).Size();
                if(LastRadius>0)Test->TestTrue(TEXT("Universe and cell reveal always pull outward"),Radius>=LastRadius-.1f);
                LastRadius=Radius;
                Test->TestTrue(TEXT("A/D steering stays within four degrees"),D->DriftOffset.Size()<=4.01f);
                TArray<USceneComponent*> Components;D->GetComponents(Components);
                for(auto* C:Components)if(C->GetName().StartsWith(TEXT("RevealedCellMembrane")))
                    Test->TestTrue(TEXT("Enclosing cell is full sized throughout; never grows from centre"),C->GetRelativeScale3D().Equals(FVector(32)));
            }
            if(D->Shot==4)
            {
                if(DriftStep==0){D->FilmSpeed=1;Key(PC,EKeys::S,true);Key(PC,EKeys::D,true);DriftAt=Now;DriftStep=1;}
                else if(DriftStep==1&&Now-DriftAt>1)
                {
                    ReleaseRate=D->PullbackRate;Test->TestTrue(TEXT("S gains more acceleration at cosmic scale"),ReleaseRate>EarlyRate+.3f);
                    ReleaseSpeed=D->DriftVelocity.Size();Test->TestTrue(TEXT("D gently changes camera direction"),ReleaseSpeed>.5f);
                    Key(PC,EKeys::S,false);Key(PC,EKeys::D,false);DriftAt=Now;DriftStep=2;
                }
                else if(DriftStep==2&&Now-DriftAt>.45)
                {
                    Test->TestTrue(TEXT("Release retains but damps steering inertia"),D->DriftVelocity.Size()>.1f&&D->DriftVelocity.Size()<ReleaseSpeed);
                    Test->TestTrue(TEXT("S release smoothly returns toward automatic pullback"),D->PullbackRate>1&&D->PullbackRate<ReleaseRate);
                    Key(PC,EKeys::C,true);DriftStep=3;
                }
                else if(DriftStep==3)
                {Key(PC,EKeys::C,false);Test->TestTrue(TEXT("Comfort mode enabled by input"),D->bComfortMode);Test->TestTrue(TEXT("Comfort mode clears drift"),D->DriftVelocity.IsNearlyZero());Key(PC,EKeys::C,true);DriftStep=4;}
                else if(DriftStep==4){Key(PC,EKeys::C,false);D->FilmSpeed=4;DriftStep=5;}
            }
            if(D->Shot==7){BlackSeen=true;Test->TestEqual(TEXT("Hard blackout fully covers the frame"),D->OverlayAlpha(),1.f);}
            if(D->Shot==8){EyeSeen=true;Test->TestTrue(TEXT("Eye follows the blackout"),BlackSeen);}
            if(D->bComplete)
            {
                Test->TestEqual(TEXT("Automatic ending reaches developers"),D->Shot,10);
                Test->TestTrue(TEXT("Blackout then eye close-up played"),BlackSeen&&EyeSeen);
                Test->TestEqual(TEXT("Requested final line"),D->Subtitle(),FString(TEXT("“欸，要不，我们就做挖！”")));
                Test->TestEqual(TEXT("Scale-dependent input checks completed"),BoostStep,4);
                Test->TestEqual(TEXT("Space steering and release checks completed"),DriftStep,5);
                Capture(TEXT("14-afternoon"));Key(PC,EKeys::LeftMouseButton,false);Next();
            }
            break;
        case 4:
            if(Age<.8)return false;
            Key(PC,EKeys::R,true);Next();break;
        case 5:
            if(Age<.3)return false;
            Key(PC,EKeys::R,false);
            Test->TestEqual(TEXT("R restores playable route"),D->Shot,-1);
            Test->TestEqual(TEXT("R clears progress"),D->GateIndex,0);
            Test->TestFalse(TEXT("R clears completion"),D->bComplete);
            Test->TestFalse(TEXT("R restores mining input"),Mining->IsInputSuspended());
            Test->TestEqual(TEXT("R restores view target"),PC->GetViewTarget(),static_cast<AActor*>(Pawn));
            Test->TestEqual(TEXT("R restores real rock geometry"),D->Gates[0]->SuccessfulCuts,0);
            Test->TestEqual(TEXT("R clears neuron possession"),D->CurrentNeuron,INDEX_NONE);
            Test->TestEqual(TEXT("R clears pulse count"),D->PossessionCount,0);
            Test->TestEqual(TEXT("R clears in-flight possession"),D->TravellingToNeuron,INDEX_NONE);
            Test->TestFalse(TEXT("R clears brain entry"),D->bEnteringBrain);
            Test->TestFalse(TEXT("R restores visible hands and tool"),Pawn->IsHidden());
            Test->TestTrue(TEXT("R restores start location"),Pawn->GetActorLocation().X<200);
            Key(PC,EKeys::P,true);Next();break;
        case 6:
            if(Age<.3)return false;
            Key(PC,EKeys::P,false);Test->TestTrue(TEXT("P enters review preview"),D->Shot>=0);
            Key(PC,EKeys::R,true);Next();break;
        case 7:
            Key(PC,EKeys::R,false);return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndingEmergenceRoute,"BrainMining.EndingEmergence.PlayableRouteAndFilm",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEndingEmergenceRoute::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStartEndingPIE>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEndingRoute>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
