#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "EndingEmergence.h"
#include "DiggableRock.h"
#include "MiningComponent.h"
#include "MiningDebrisBurst.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Components/SceneComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
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
    float ReleaseSpeed=0,EarlyRate=0,ReleaseRate=0,LastRadius=0,TakeoffZ=0;bool Captured=false,VoiceSeen=false,FractureSeen=false,AscentSeen=false,FadeSeen=false;
    int BloodCheck=0,BrainCheck=0,JumpIndex=0;double CheckAt=0;FVector CheckPosition=FVector::ZeroVector;
    const int JumpRoute[12]={2,0,4,9,14,19,24,29,34,39,44,46};
    int StrikeStep=0;double StrikeAt=0;bool CrackCaptured=false,PaperCaptured=false,WaterCaptured=false,FractureFaceCaptured=false;float PreviousRetreat=1.e9f;int PreviousPapers=0;
    bool TargetCaptured=false,BranchesCaptured=false,TransitChecked=false,EntrySeen=false,CurveCaptured=false,ThresholdCaptured=false;
    bool EntryLensSeen=false,FastWaveSeen=false;double FirstArrivalAt=-1;float BoostGameAt=0;
    float HighestBloodZ=0,WidestBloodY=0,CheckDistance=0;double EntryAt=0;
    FVector PreviousEntryPosition=FVector::ZeroVector;float PreviousEntryTime=0;
    bool FlightLookInjected=false,FlightLookVerified=false,ArrivalAimSet=false;
    FRotator BeforeFlightLook=FRotator::ZeroRotator,ArrivalAim=FRotator::ZeroRotator,BreakRotation=FRotator::ZeroRotator;
    FVector BreakPosition=FVector::ZeroVector;int RippleHits=0;
    bool PaperFallSeen=false,ImmediateWaterSeen=false;
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
            Test->TestEqual(TEXT("Expanded saved branching neuron network"),D->NeuronPositions.Num(),47);
            Test->TestEqual(TEXT("Route forward distance doubled"),D->NeuronPositions.Last().X-D->NeuronPositions[0].X,13000.);
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
                Test->TestEqual(TEXT("Largest neuron reached by real Space input"),D->CurrentNeuron,46);
                Test->TestEqual(TEXT("Branching route including a backtrack completed"),D->PossessionCount,12);
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
                    Test->TestTrue(TEXT("Brain approach advances continuously without a camera cut"),FVector::Dist(CameraPosition,PreviousEntryPosition)<2500*Dt+5);
                    Test->TestEqual(TEXT("No flash hides the brain approach"),D->OverlayAlpha(),0.f);
                    if(Now-EntryAt>.5&&Now-EntryAt<1&&!EntryLensSeen)
                    {
                        auto* Rail=Cast<ACameraActor>(PC->GetViewTarget());
                        Test->TestTrue(TEXT("First blood-to-neuron flight uses the warp FOV"),Rail&&Rail->GetCameraComponent()->FieldOfView>104);
                        Capture(TEXT("blood-brain-first-warp"));EntryLensSeen=true;
                    }
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
                    Test->TestTrue(TEXT("First neuron is reached by a short visible warp"),EntrySeen&&EntryLensSeen&&Now-EntryAt>1.3&&Now-EntryAt<1.8);
                    Test->TestTrue(TEXT("Entry finishes without snapping to host"),FVector::Dist(PC->GetViewTarget()->GetActorLocation(),PreviousEntryPosition)<30);
                    int Choices=0;for(int I=0;I<D->NeuronPositions.Num();++I)if(D->CanPossess(I))++Choices;
                    Test->TestTrue(TEXT("Entry offers multiple routes"),Choices>=3);
                    Test->TestFalse(TEXT("Final neuron cannot be skipped to from entry"),D->CanPossess(46));
                    CheckPosition=PC->GetViewTarget()->GetActorLocation();PC->SetControlRotation(FRotator(85,0,0));Key(PC,EKeys::SpaceBar,true);Key(PC,EKeys::W,true);CheckAt=Now;BrainCheck=1;
                }
                else if(BrainCheck==1&&Now-CheckAt>.4)
                {
                    Test->TestEqual(TEXT("Space without aimed target does nothing"),D->PossessionCount,0);
                    Test->TestTrue(TEXT("Movement input cannot walk off a neuron"),PC->GetViewTarget()->GetActorLocation().Equals(CheckPosition,.1));
                    Key(PC,EKeys::W,false);Key(PC,EKeys::SpaceBar,false);BrainCheck=2;CheckAt=Now;
                }
                else if(BrainCheck==2&&JumpIndex<12)
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
                        if(JumpIndex==0)
                        {
                            Test->TestFalse(TEXT("Mouse look stays enabled throughout possession flight"),PC->IsLookInputIgnored());
                            BeforeFlightLook=PC->GetControlRotation();PC->AddYawInput(15);PC->AddPitchInput(-12);FlightLookInjected=true;
                        }
                    }
                    if(JumpIndex==0&&FlightLookInjected&&!FlightLookVerified&&Now-CheckAt>.48)
                    {
                        const FRotator Change=(PC->GetViewTarget()->GetActorRotation()-BeforeFlightLook).GetNormalized();
                        Test->TestTrue(TEXT("Yaw and pitch input actually rotate the camera in mid-flight"),FMath::Abs(Change.Yaw)>2&&FMath::Abs(Change.Pitch)>2);
                        Test->TestTrue(TEXT("Looking around does not cancel or redirect the flight"),D->TravellingToNeuron==JumpRoute[0]);
                        FlightLookVerified=true;
                    }
                    if(JumpIndex==0&&FlightLookVerified&&!ArrivalAimSet&&Now-CheckAt>.70)
                    {
                        // Aim for the following host before this hop finishes.
                        const FVector Landing=D->NeuronWorldPosition(JumpRoute[0])+D->GetActorUpVector()*110;
                        ArrivalAim=(D->NeuronWorldPosition(JumpRoute[1])-Landing).Rotation();PC->SetControlRotation(ArrivalAim);ArrivalAimSet=true;
                    }
                    if(JumpIndex==0&&D->TravellingToNeuron==INDEX_NONE)
                    {
                        if(FirstArrivalAt<0)FirstArrivalAt=Now;
                        if(!FastWaveSeen&&Now-FirstArrivalAt>.16)
                        {
                            Test->TestTrue(TEXT("Strong pulse lights the host immediately"),D->NeuralFlash(D->CurrentNeuron)>.15f);
                            Test->TestTrue(TEXT("Nearby neurons light within 0.2 seconds of arrival"),D->NeuralFlash(0)>.15f);
                            Capture(TEXT("brain-fast-neural-wave"));FastWaveSeen=true;
                        }
                    }
                    if(Now-CheckAt>1.6)
                    {
                        Test->TestEqual(TEXT("Camera flight settles on the aimed host"),D->CurrentNeuron,JumpRoute[JumpIndex]);
                        Test->TestEqual(TEXT("Holding Space does not chain automatic hops"),D->PossessionCount,JumpIndex+1);
                        Test->TestEqual(TEXT("Flight state clears on arrival"),D->TravellingToNeuron,INDEX_NONE);
                        if(JumpIndex==0)
                        {
                            Test->TestTrue(TEXT("Fast arrival pulse was observed"),FastWaveSeen);
                            Test->TestTrue(TEXT("Flight preserves the player's pre-aim on landing"),ArrivalAimSet&&PC->GetControlRotation().Equals(ArrivalAim,.1f));
                            Test->TestEqual(TEXT("Next neuron is already targetable without correcting pitch"),D->TargetNeuron,JumpRoute[1]);
                        }
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
            if(D->Shot==2&&BoostStep==2){D->FilmSpeed=1;Key(PC,EKeys::S,true);BoostGameAt=W->GetTimeSeconds();BoostStep=3;}
            if(D->Shot==2&&BoostStep==3&&W->GetTimeSeconds()-BoostGameAt>1)
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
                if(DriftStep==0){D->FilmSpeed=1;Key(PC,EKeys::S,true);Key(PC,EKeys::D,true);BoostGameAt=W->GetTimeSeconds();DriftStep=1;}
                else if(DriftStep==1&&W->GetTimeSeconds()-BoostGameAt>1)
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
            if(D->Shot==7)
            {
                Key(PC,EKeys::D,true);Key(PC,EKeys::S,true);
                Test->TestEqual(TEXT("Retreat is no longer controlled by WASD"),D->PullbackRate,1.f);
                Test->TestTrue(TEXT("Retreat decelerates continuously"),D->RetreatSpeed<=PreviousRetreat+1);PreviousRetreat=D->RetreatSpeed;
                VoiceSeen=true;Test->TestEqual(TEXT("Cells continue into voices without blackout"),D->OverlayAlpha(),0.f);
                const float Radius=(PC->GetViewTarget()->GetActorLocation()-FVector(95000,0,0)).Size();
                if(LastRadius>0)Test->TestTrue(TEXT("Voice scene keeps pulling out"),Radius>=LastRadius-.1f);
                LastRadius=Radius;
            }
            if(D->Shot==8)
            {
                BreakPosition=PC->GetViewTarget()->GetActorLocation();BreakRotation=PC->GetViewTarget()->GetActorRotation();
            }
            if((D->Shot==8||D->Shot==9)&&D->RealityHits>0&&!(RippleHits&(1<<D->RealityHits)))
            {
                auto* FilmCamera=Cast<ACameraActor>(PC->GetViewTarget());
                for(const auto& Blend:FilmCamera->GetCameraComponent()->PostProcessSettings.WeightedBlendables.Array)
                {
                    auto* MID=Cast<UMaterialInstanceDynamic>(Blend.Object);
                    if(!MID||!MID->GetMaterial()->GetName().Contains(TEXT("Exhaustion")))continue;
                    float RippleAge=100,Shake=0;
                    MID->GetScalarParameterValue(FMaterialParameterInfo(TEXT("RippleAge")),RippleAge);
                    MID->GetScalarParameterValue(FMaterialParameterInfo(TEXT("ShakeStrength")),Shake);
                    if(RippleAge>=.10f&&RippleAge<.35f)
                    {
                        Test->TestTrue(TEXT("Every impact starts a fresh ripple with bounded shake"),Shake>0&&Shake<.012f);
                        Test->TestEqual(TEXT("No impact uses a full-screen flash overlay"),D->OverlayAlpha(),0.f);
                        int CosmicBursts=0;
                        for(TActorIterator<AMiningDebrisBurst> It(W);It;++It)if(It->GetOwner()==D&&It->SourceMaterial)
                        {Test->TestTrue(TEXT("Mining chips use the starfield material"),It->SourceMaterial->GetName().Contains(TEXT("CosmicDebris")));++CosmicBursts;}
                        Test->TestTrue(TEXT("Impact has visible cosmic mining chips"),CosmicBursts>0);
                        Capture(FString::Printf(TEXT("ending-ripple-hit-%d"),D->RealityHits));RippleHits|=1<<D->RealityHits;
                    }
                }
            }
            if(D->Shot==8)
            {
                if(StrikeStep==0)
                {
                    Test->TestTrue(TEXT("Camera still retreats after tool control returns"),D->RetreatSpeed>1000);
                    Test->TestTrue(TEXT("Exhaustion desaturates the cell view"),D->Exhaustion>.8f);
                    Test->TestTrue(TEXT("Cell universe extends across the final field of view"),D->UniverseCellCount>=600);
                    Key(PC,EKeys::D,false);Key(PC,EKeys::S,false);
                    TArray<UStaticMeshComponent*> Parts;D->GetComponents(Parts);
                    for(auto* Part:Parts)if(Part->GetName().StartsWith(TEXT("MainGamePickaxe")))Test->TestEqual(TEXT("Ending uses the main game's pickaxe mesh"),Part->GetStaticMesh()->GetPathName(),FString(TEXT("/Game/Pickaxe/SM_Pickaxe.SM_Pickaxe")));
                    StrikeAt=Now;StrikeStep=1;
                }
                else if(StrikeStep==1&&Now-StrikeAt>2)
                {
                    Test->TestEqual(TEXT("Ending waits for player instead of auto-breaking"),D->RealityHits,0);
                    Test->TestFalse(TEXT("Ending cannot finish without strikes"),D->bComplete);
                    Key(PC,EKeys::LeftMouseButton,true);StrikeAt=Now;StrikeStep=2;
                }
                else if(StrikeStep==2&&Now-StrikeAt>1.5)
                {
                    Test->TestEqual(TEXT("Holding mouse produces exactly one strike"),D->RealityHits,1);
                    Test->TestTrue(TEXT("First strike restores some colour"),D->Exhaustion<.65f);
                    Test->TestEqual(TEXT("First strike emits main mining debris"),D->EndingDebrisBursts,1);
                    Key(PC,EKeys::LeftMouseButton,false);StrikeAt=Now;StrikeStep=3;
                }
                else if(StrikeStep==3&&Now-StrikeAt>.15)
                {Key(PC,EKeys::LeftMouseButton,true);StrikeAt=Now;StrikeStep=4;}
                else if(StrikeStep==4&&Now-StrikeAt>.32)
                {
                    Test->TestEqual(TEXT("Second deliberate strike grows cracks"),D->RealityHits,2);
                    Key(PC,EKeys::LeftMouseButton,false);StrikeAt=Now;StrikeStep=5;
                }
                else if(StrikeStep==5&&Now-StrikeAt>.08)
                {Key(PC,EKeys::LeftMouseButton,true);StrikeAt=Now;StrikeStep=6;}
                else if(StrikeStep==6&&Now-StrikeAt>.2)
                {
                    Test->TestEqual(TEXT("Rapid clicking cannot skip the recovery beat"),D->RealityHits,2);
                    Test->TestTrue(TEXT("Second strike restores more colour"),D->Exhaustion<.44f);
                    Key(PC,EKeys::LeftMouseButton,false);StrikeAt=Now;StrikeStep=7;
                }
                else if(StrikeStep==7&&D->StrikeReadiness()>=1&&D->ShotTime>4.1f&&Now-StrikeAt>.08)
                {
                    // Retreat uses game time; shader compilation can stall wall time.
                    Test->TestEqual(TEXT("Retreat eventually settles to rest"),D->RetreatSpeed,0.f);
                    Key(PC,EKeys::LeftMouseButton,true);StrikeAt=Now;StrikeStep=8;
                }
                else if(StrikeStep==8&&Now-StrikeAt>.4)
                {Key(PC,EKeys::LeftMouseButton,false);StrikeAt=Now;StrikeStep=7;}
                if(D->RealityHits==2&&StrikeStep==7&&!CrackCaptured){Capture(TEXT("ending-two-cracks-ripple"));CrackCaptured=true;}
            }
            if(D->Shot==9)
            {
                if(!ImmediateWaterSeen&&D->ShotTime>.055f)
                {
                    for(auto Jet:D->WaterJets)Test->TestTrue(TEXT("Water begins immediately when the hole opens"),Jet->IsVisible()&&Jet->GetMesh()->TriangleCount()>0);
                    ImmediateWaterSeen=true;
                }
                if(!FractureFaceCaptured&&D->ShotTime>.10f){Capture(TEXT("ending-fracture-universe-faces"));FractureFaceCaptured=true;}
                Test->TestEqual(TEXT("Three deliberate impacts shatter the view"),D->RealityHits,3);
                Test->TestTrue(TEXT("Reality follows voice and interactive cracks"),VoiceSeen&&CrackCaptured);
                Test->TestEqual(TEXT("Fracture is visible rather than a flash cut"),D->OverlayAlpha(),0.f);
                Test->TestTrue(TEXT("Opening the hole never cuts to another camera location"),PC->GetViewTarget()->GetActorLocation().Equals(BreakPosition,.1f)&&PC->GetViewTarget()->GetActorRotation().Equals(BreakRotation,.1f));
                if(!FractureSeen&&D->ShotTime>.45f)
                {
                    Test->TestEqual(TEXT("Each impact produces main mining debris"),D->EndingDebrisBursts,3);
                    Test->TestEqual(TEXT("Only the central hole breaks into solid pieces"),D->UniverseShards.Num(),24);
                    Test->TestNotNull(TEXT("Universe outside the hole remains intact"),D->UniverseSurround.Get());
                    if(D->UniverseSurround)
                    {
                        Test->TestTrue(TEXT("Surround remains visible during the burst"),D->UniverseSurround->IsVisible());
                        float Nearest=1.e9f;
                        for(int V:D->UniverseSurround->GetMesh()->VertexIndicesItr())
                        {const auto P=D->UniverseSurround->GetMesh()->GetVertex(V);if(P.X<36)Nearest=FMath::Min(Nearest,float(FMath::Sqrt(P.Y*P.Y+P.Z*P.Z)));}
                        Test->TestTrue(TEXT("Surround mesh has a genuine open central aperture"),Nearest>4);
                        Test->TestTrue(TEXT("The aperture never replaces the outside universe with an HDR screenshot"),D->UniverseSurround->GetMaterial(0)->GetName().Contains(TEXT("ApertureVoid")));
                    }
                    float Near=1.e9f,Far=-1.e9f;
                    for(auto Shard:D->UniverseShards)
                    {
                        Test->TestEqual(TEXT("Each fragment is a closed triangular prism"),Shard->GetMesh()->TriangleCount(),8);
                        Test->TestTrue(TEXT("Fragments have geometric thickness"),Shard->GetMesh()->GetBounds().Width()>1);
                        Near=FMath::Min(Near,float(Shard->GetRelativeLocation().X));Far=FMath::Max(Far,float(Shard->GetRelativeLocation().X));
                    }
                    Test->TestTrue(TEXT("Fragments separate along camera depth, not only screen XY"),Far-Near>10);
                    Capture(TEXT("ending-universe-shattering-3d"));FractureSeen=true;
                }
                Key(PC,EKeys::LeftMouseButton,false);
            }
            if(D->Shot==9&&D->ShotTime>1.1f&&!WaterCaptured)
            {
                Test->TestTrue(TEXT("Water wets and refracts the lens before paper covers it"),D->WetLens>.8f);
                Test->TestEqual(TEXT("Sheets have not landed before the water splash"),D->PapersLanded,0);
                Test->TestEqual(TEXT("Water has twelve continuous volumetric sheets"),D->WaterJets.Num(),12);
                for(auto Jet:D->WaterJets)Test->TestTrue(TEXT("Pressure jet is still spraying after one second"),Jet->IsVisible()&&Jet->GetMesh()->TriangleCount()>200);
                Capture(TEXT("ending-water-wet-lens"));WaterCaptured=true;
            }
            if(D->Shot==10)
            {
                Test->TestTrue(TEXT("Photographs arrive progressively"),D->PapersArrived>=PreviousPapers);PreviousPapers=D->PapersArrived;
                if(D->ShotTime>2&&!PaperFallSeen)
                {
                    Test->TestFalse(TEXT("The first photograph has slipped out of view"),D->EndingPapers[0]->IsVisible());
                    Test->TestTrue(TEXT("Loose photographs travel down after reaching the lens"),D->EndingPapers[1]->GetRelativeLocation().Z<-10);
                    PaperFallSeen=true;Capture(TEXT("ending-paper-sliding"));
                }
                if(D->PapersArrived>=4&&D->PapersArrived<10&&!PaperCaptured)
                {
                    Test->TestEqual(TEXT("Photographs fly as fifteen individual deforming sheets"),D->EndingPapers.Num(),15);
                    Test->TestFalse(TEXT("The final photograph arrives last"),D->bFinalPaperVisible);
                    for(int I=0;I<D->EndingPapers.Num();++I)
                    {
                        auto* M=Cast<UMaterialInstanceDynamic>(D->EndingPapers[I]->GetMaterial(0));UTexture* Photo=nullptr;
                        Test->TestTrue(TEXT("Every paper uses a supplied photograph"),M&&M->GetTextureParameterValue(FMaterialParameterInfo(TEXT("Idea")),Photo)&&Photo);
                        if(Photo)Test->TestEqual(TEXT("Idea10 is reserved for the last sheet"),Photo->GetName(),FString::Printf(TEXT("T_EE_Idea%d"),I==14?10:1+I%9));
                    }
                    Capture(TEXT("ending-paper-arrivals"));PaperCaptured=true;
                }
            }
            if(D->bComplete)
            {
                Test->TestEqual(TEXT("Ending reaches the real-world first-person hold"),D->Shot,11);
                Test->TestTrue(TEXT("Fracture, immediate spray, sliding photographs and final hold completed"),FractureSeen&&WaterCaptured&&ImmediateWaterSeen&&PaperCaptured&&PaperFallSeen&&D->bFinalPaperVisible);
                Test->TestEqual(TEXT("Only the last photograph sticks to the lens"),D->PapersLanded,1);
                for(int I=0;I<D->EndingPapers.Num();++I)Test->TestEqual(TEXT("Every earlier photograph leaves the view"),D->EndingPapers[I]->IsVisible(),I==D->EndingPapers.Num()-1);
                Test->TestTrue(TEXT("Idea10 rests in the centre"),FMath::Abs(D->EndingPapers.Last()->GetRelativeLocation().Y)<.5f&&FMath::Abs(D->EndingPapers.Last()->GetRelativeLocation().Z)<.5f);
                const auto PaperBounds=D->EndingPapers.Last()->GetMesh()->GetBounds();
                const auto PaperExtent=PaperBounds.Max-PaperBounds.Min;
                Test->TestTrue(TEXT("The left-rotated final photograph retains landscape proportions"),PaperExtent.Y/PaperExtent.Z>1.70&&PaperExtent.Y/PaperExtent.Z<1.85);
                auto* FinalMaterial=Cast<UMaterialInstanceDynamic>(D->EndingPapers.Last()->GetMaterial(0));UTexture* OldTitle=nullptr;
                Test->TestFalse(TEXT("No generated title ink is added to the photograph"),FinalMaterial->GetTextureParameterValue(FMaterialParameterInfo(TEXT("Ink")),OldTitle));
                Test->TestEqual(TEXT("All three strikes displayed their own ripple"),RippleHits,14);
                Test->TestTrue(TEXT("Mid-flight mouse look and pre-aim checks completed"),FlightLookVerified&&ArrivalAimSet);
                Test->TestEqual(TEXT("Old spoken ending removed"),D->Subtitle(),FString());
                Test->TestEqual(TEXT("Scale-dependent input checks completed"),BoostStep,4);
                Test->TestEqual(TEXT("Space steering and release checks completed"),DriftStep,5);
                Capture(TEXT("ending-idea10-final-paper"));Key(PC,EKeys::LeftMouseButton,false);Next();
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
            Test->TestEqual(TEXT("R repairs the broken universe"),D->RealityHits,0);
            Test->TestEqual(TEXT("R destroys all 3D fragments"),D->UniverseShards.Num(),0);
            Test->TestNull(TEXT("R removes the universe aperture"),D->UniverseSurround.Get());
            Test->TestEqual(TEXT("R clears sustained water jets"),D->WaterJets.Num(),0);
            Test->TestEqual(TEXT("R clears exhaustion filter"),D->Exhaustion,0.f);
            Test->TestEqual(TEXT("R resets impact effects"),D->EndingDebrisBursts,0);
            Test->TestFalse(TEXT("R clears final paper title"),D->bFinalPaperVisible);
            Test->TestEqual(TEXT("R removes all paper geometry"),D->EndingPapers.Num(),0);
            Test->TestEqual(TEXT("R dries the lens"),D->WetLens,0.f);
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
