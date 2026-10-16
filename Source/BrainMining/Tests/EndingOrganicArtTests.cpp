#if WITH_DEV_AUTOMATION_TESTS
#include "EndingEmergence.h"
#include "DiggableRock.h"
#include "MiningComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"
#include "ShaderCompiler.h"
#include "TextureCompiler.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UDynamicMesh.h"
#include "Engine/StaticMesh.h"

class FEndingOrganicPreview : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int Step=0,ShotIndex=0;
    double Start=FPlatformTime::Seconds(),At=0;FVector CellBefore;bool ShotCaptured=false;
    double ThoughtFrameAt=0;TArray<double> ThoughtFrames;
    const int Shots[8]={2,3,3,3,4,4,5,6};
    const float ShotTimes[8]={4,1.5f,6,13.5f,2,17,16,6};
    void Key(APlayerController* P,FKey K,bool Down){P->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f));}
    void Capture(const FString& Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("EndingEmergencePreview")/(TEXT("organic-")+Name+TEXT(".png")),true,false);}
public:
    explicit FEndingOrganicPreview(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>160){Test->AddError(FString::Printf(TEXT("Organic art route timeout at step %d"),Step));return true;}
        if((GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())||FTextureCompilingManager::Get().GetNumRemainingTextures()>0)return false;
        UWorld* W=AutomationCommon::GetAnyGameWorld();if(!W)return false;
        AEndingEmergence* D=nullptr;for(TActorIterator<AEndingEmergence> I(W);I;++I){D=*I;break;}
        auto* P=W->GetFirstPlayerController();auto* Pawn=P?Cast<ACharacter>(P->GetPawn()):nullptr;
        if(!D||!Pawn||!D->bReady||D->Age<3)return false;
        // Sample the real first brain-to-city cut after screenshot I/O has settled.
        if(Step==15&&(D->Shot==0||D->BeatAt-D->Age<.4f))
        {
            const double Now=FPlatformTime::Seconds();
            if(ThoughtFrameAt>0)ThoughtFrames.Add((Now-ThoughtFrameAt)*1000);
            ThoughtFrameAt=Now;
        }
        if(Step==0)
        {
            if(FParse::Param(FCommandLine::Get(),TEXT("EndingEarthOnly")))
            {
                D->PreviewFilm();D->FilmSpeed=1;D->SetShot(3);ShotIndex=1;
                D->ShotTime=ShotTimes[ShotIndex];At=D->Age;Step=10;return false;
            }
            Test->TestTrue(TEXT("Cell organelles have cached motion groups"),D->CellBreathers.Num()>8);
            CellBefore=D->CellBreathers[0]->GetRelativeLocation();
            Test->TestEqual(TEXT("Neural branching route retains 47 hosts"),D->NeuronPositions.Num(),47);
            Test->TestTrue(TEXT("First host is lowered toward cortex"),D->NeuronPositions[0].Z<500);
            for(const FVector& Host:D->NeuronPositions)
                Test->TestTrue(TEXT("Every neuron lies inside the displaced brain envelope"),((Host-FVector(22900,0,1200))/FVector(8700,5800,4200)).SizeSquared()<.90);
            TArray<UStaticMeshComponent*> Parts;D->GetComponents(Parts);int Lines=0,Arbors=0;
            for(auto* C:Parts){Lines+=C->GetName().StartsWith(TEXT("FollowLight"));Arbors+=C->GetName().StartsWith(TEXT("NeuronDendriticArbor"));}
            Test->TestEqual(TEXT("No road-like floor guidance remains"),Lines,0);
            Test->TestEqual(TEXT("Every host has a continuous branching dendritic arbor"),Arbors,D->NeuronPositions.Num());
            Test->TestNull(TEXT("No long rod-shaped pulse wake remains"),D->PulseTrail.Get());
            for(auto Nerve:D->Nerves)Test->TestTrue(TEXT("Neural routes use translucent filaments rather than flat glow"),Nerve->GetMaterial(0)&&Nerve->GetMaterial(0)->GetBlendMode()==BLEND_Translucent);
            for(auto Core:D->AmbientNeuronCores)Test->TestTrue(TEXT("Decorative neural cores use glass contours rather than solid dots"),Core->GetMaterial(0)&&Core->GetMaterial(0)->GetBlendMode()==BLEND_Translucent);
            for(auto Nerve:D->Nerves)Test->TestTrue(TEXT("Neural fibres use organic meshes rather than uniform cylinders"),Nerve->GetStaticMesh()&&(Nerve->GetStaticMesh()->GetName().Contains(TEXT("NeuronBranch"))||Nerve->GetStaticMesh()->GetName().Contains(TEXT("NeuronArbor"))));
            for(auto Core:D->NeuronCores)Test->TestTrue(TEXT("Possessable neurons have irregular soma meshes"),Core->GetStaticMesh()&&Core->GetStaticMesh()->GetName().Contains(TEXT("NeuronSoma")));
            for(auto* Part:Parts)if(Part->GetName().StartsWith(TEXT("BackgroundNeuronBodies")))
                Test->TestTrue(TEXT("Decorative neurons densely fill the brain without changing the route"),CastChecked<UInstancedStaticMeshComponent>(Part)->GetInstanceCount()>900);
            Test->TestTrue(TEXT("Goal neuron is markedly larger than ordinary hosts"),D->NeuronCores.Last()->GetRelativeScale3D().X>D->NeuronCores[0]->GetRelativeScale3D().X*4);
            TSet<UStaticMesh*> ArborMeshes;int Axons=0;
            for(auto Nerve:D->Nerves)
            {
                if(Nerve->GetName().StartsWith(TEXT("NeuronDendriticArbor")))ArborMeshes.Add(Nerve->GetStaticMesh());
                Axons+=Nerve->GetName().StartsWith(TEXT("NeuralAxonLink"));
            }
            Test->TestEqual(TEXT("Neurons use four distinct branching morphologies"),ArborMeshes.Num(),4);
            Test->TestTrue(TEXT("Fine axons connect the neuronal network"),Axons>47);
            Test->TestTrue(TEXT("Soma retains rounded volume"),D->NeuronCores[3]->GetRelativeScale3D().Z>D->NeuronCores[3]->GetRelativeScale3D().X*.8f);
            TArray<UDynamicMeshComponent*> Shells;D->GetComponents(Shells);
            for(auto* Shell:Shells)if(Shell->GetName().StartsWith(TEXT("RedSpirallingVessel")))
                Shell->GetDynamicMesh()->ProcessMesh([this](const UE::Geometry::FDynamicMesh3& Mesh){double MinX=1e9;for(int V:Mesh.VertexIndicesItr())MinX=FMath::Min(MinX,Mesh.GetVertex(V).X);Test->TestTrue(TEXT("Vessel shell never projects back through the cell seam"),MinX>=1599);});
            bool HasBed=false;
            for(auto* Shell:Shells)if(Shell->GetName().StartsWith(TEXT("InteriorCorticalBed")))
            {
                HasBed=true;
                Test->TestTrue(TEXT("Interior brain folds use hollow translucent contours"),Shell->GetMaterial(0)&&Shell->GetMaterial(0)->GetBlendMode()==BLEND_Translucent);
                Shell->GetDynamicMesh()->ProcessMesh([this,D,Shell](const UE::Geometry::FDynamicMesh3& Mesh)
                {
                    for(const FVector& Host:D->NeuronPositions)
                    {
                        double Nearest=1e20,Clearance=0;
                        for(int V:Mesh.VertexIndicesItr())
                        {
                            const FVector3d P=Mesh.GetVertex(V)+FVector3d(Shell->GetRelativeLocation());const double XY=FMath::Square(P.X-Host.X)+FMath::Square(P.Y-Host.Y);
                            if(XY<Nearest){Nearest=XY;Clearance=Host.Z-P.Z;}
                        }
                        Test->TestTrue(TEXT("Cortex is close beneath every host without burying it"),Clearance>250&&Clearance<1000);
                    }
                });
            }
            Test->TestTrue(TEXT("Brain has nearby interior cortical contours"),HasBed);
            Capture(TEXT("cell-front"));At=D->Age;Step=1;
        }
        else if(Step==1&&D->Age-At>.6f){P->SetControlRotation(FRotator(0,-45,0));At=D->Age;Step=2;}
        else if(Step==2&&D->Age-At>.6f){Test->TestTrue(TEXT("Cell organelles move over time"),FVector::Dist(CellBefore,D->CellBreathers[0]->GetRelativeLocation())>.1f);Capture(TEXT("cell-nucleus"));P->SetControlRotation(FRotator(0,40,0));At=D->Age;Step=3;}
        else if(Step==3&&D->Age-At>.6f){Capture(TEXT("cell-organelles"));Key(P,EKeys::W,true);Key(P,EKeys::LeftMouseButton,true);Step=4;}
        else if(Step==4)
        {
            if(D->GateIndex==0)
            {
                FVector Eye;FRotator Rot;P->GetPlayerViewPoint(Eye,Rot);
                const FVector Target=D->Gates[0]->GetActorLocation()+FVector(0,0,-70);
                Rot=(D->GetGateApproach(0)+FVector(580,0,0)-Pawn->GetActorLocation()).Rotation();
                Rot.Pitch=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Target.Z-Eye.Z,FMath::Max(180.,FMath::Abs(Target.X-Eye.X)))),-20.,25.);
                Rot.Roll=0;P->SetControlRotation(Rot);
            }
            else if(D->GateIndex==1){Key(P,EKeys::LeftMouseButton,false);At=D->Age;Step=5;}
        }
        else if(Step==5&&D->Age-At>2)
        {
            Test->TestTrue(TEXT("Compact cell and resized neck remain traversable"),D->VesselDistance>1200);
            Capture(TEXT("blood-entry"));Key(P,EKeys::W,false);
            D->VesselDistance=D->VesselDistances.Last()*.43f;
            D->Camera->SetActorRotation(FRotator(0,180,0));P->SetControlRotation(FRotator(0,180,0));At=D->Age;Step=55;
        }
        else if(Step==55&&D->Age-At>2.2f)
        {
            const FVector Ahead=(D->GetActorTransform().TransformPosition(D->VesselPoint(D->VesselDistance+2200))-D->Camera->GetActorLocation()).GetSafeNormal();
            Test->TestTrue(TEXT("Blood camera frames the approaching passage farther down the bend"),FVector::DotProduct(D->Camera->GetActorForwardVector(),Ahead)>.96);
            for(float Reach:{800.f,1300.f})
            {
                const FVector Ray=D->GetActorTransform().InverseTransformPosition(D->Camera->GetActorLocation()+D->Camera->GetActorForwardVector()*Reach);
                double Nearest=1e20;for(const FVector& Centre:D->VesselPath)Nearest=FMath::Min(Nearest,FVector::Dist(Ray,Centre));
                Test->TestTrue(TEXT("Default view ray stays within the open blood passage"),Nearest<700);
            }
            Test->TestTrue(TEXT("Blood camera maintains a level horizon"),FMath::Abs(D->Camera->GetActorRotation().Roll)<.01f);
            Test->TestEqual(TEXT("Neural cores stay hidden while still in the vessel"),CastChecked<UMaterialInstanceDynamic>(D->NeuronCores[3]->GetMaterial(0))->K2_GetScalarParameterValue(TEXT("Reveal")),0.f);
            Capture(TEXT("blood-bend"));At=D->Age;Step=56;
        }
        else if(Step==56&&D->Age-At>.6f&&!FScreenshotRequest::IsScreenshotRequested())
        {
            P->SetControlRotation(D->Camera->GetActorRotation()+FRotator(12,42,0));D->UpdatePulse(1.f/60);
            Test->TestTrue(TEXT("Mouse look is accepted immediately rather than overwritten"),D->VesselLookOffset.Yaw>40&&D->VesselLookOffset.Pitch>10);
            At=D->Age;Step=57;
        }
        else if(Step==57&&D->Age-At>.8f)
        {
            Test->TestTrue(TEXT("Player can hold a side view during the idle grace period"),D->VesselLookOffset.Yaw>35);
            Capture(TEXT("blood-free-look"));Step=58;
        }
        else if(Step==58&&D->Age-At>4.6f)
        {
            Test->TestTrue(TEXT("Default heading returns gently after mouse inactivity"),FMath::Abs(D->VesselLookOffset.Yaw)<1);
            D->VesselDistance=D->VesselDistances.Last()-650;At=D->Age;Step=59;
        }
        else if(Step==59&&D->Age-At>.65f)
        {
            Test->TestTrue(TEXT("First host is separated from the vessel by a visible approach gap"),D->NeuronPositions[0].X-D->VesselPath.Last().X>4000);
            Capture(TEXT("vessel-brain-approach"));At=D->Age;Step=60;
        }
        else if(Step==60&&D->Age-At>.3f){if(!D->bEnteringBrain)D->EnterBrain();At=D->Age;Step=6;}
        else if(Step==6&&D->GateIndex==2&&!D->bEnteringBrain)
        {P->SetControlRotation((D->NeuronWorldPosition(3)-D->Camera->GetActorLocation()).Rotation());At=D->Age;Step=7;}
        else if(Step==7&&D->Age-At>.8f)
        {
            Test->TestEqual(TEXT("Aimed neuron is selected"),D->TargetNeuron,3);
            const auto Selected=CastChecked<UMaterialInstanceDynamic>(D->NeuronCores[3]->GetMaterial(0))->K2_GetVectorParameterValue(TEXT("Tint"));
            const auto Idle=CastChecked<UMaterialInstanceDynamic>(D->NeuronCores[2]->GetMaterial(0))->K2_GetVectorParameterValue(TEXT("Tint"));
            Test->TestTrue(TEXT("Selection differs visibly from the cool idle state"),Selected.R>Selected.B&&Idle.B>Idle.R*2);
            for(auto Nerve:D->Nerves)if(Nerve->GetName().StartsWith(TEXT("NeuronDendriticArbor"))&&Nerve->GetRelativeLocation()==D->NeuronPositions[3])
            {
                const auto Tint=CastChecked<UMaterialInstanceDynamic>(Nerve->GetMaterial(0))->K2_GetVectorParameterValue(TEXT("Tint"));
                Test->TestTrue(TEXT("Selected dendrites match the selected soma"),Tint.Equals(Selected,.01f));
            }
            Capture(TEXT("brain-neurons"));Key(P,EKeys::SpaceBar,true);Step=8;
        }
        else if(Step==8&&D->TravellingToNeuron!=INDEX_NONE)
        {
            Key(P,EKeys::SpaceBar,false);
            if(D->Age-D->PulseAt>D->TransitDuration*.42f)
            {
                Test->TestNotNull(TEXT("Pulse post process is loaded"),D->TransitMaterial.Get());
                if(D->TransitMaterial)Test->TestTrue(TEXT("Warp is active during the actual flight"),D->TransitMaterial->K2_GetScalarParameterValue(TEXT("Strength"))>.6f);
                Test->TestTrue(TEXT("Pulse flight visibly expands the field of view"),D->Camera->GetCameraComponent()->FieldOfView>130);
                Capture(TEXT("brain-transit"));Step=9;
            }
        }
        else if(Step==9&&D->CurrentNeuron==3&&D->TravellingToNeuron==INDEX_NONE)
        {At=D->Age;Step=91;}
        else if(Step==91&&D->Age-At>.12f)
        {
            Test->TestTrue(TEXT("Arrival triggers a strong local neural pulse"),D->NeuralFlash(3)>.4f);
            Test->TestTrue(TEXT("Arrival wave reaches nearby neurons"),D->NeuralFlash(2)>.15f);
            Test->TestTrue(TEXT("Warp clears after landing"),D->TransitMaterial->K2_GetScalarParameterValue(TEXT("Strength"))<.01f);
            Capture(TEXT("brain-arrival"));Step=12;
        }
        else if(Step==12&&D->Age-At>.25f)
        {
            D->CurrentNeuron=43;
            D->Camera->SetActorLocation(D->NeuronWorldPosition(43)+D->GetActorUpVector()*380);
            P->SetControlRotation((D->NeuronWorldPosition(46)-D->Camera->GetActorLocation()).Rotation());
            At=D->Age;Step=120;
        }
        else if(Step==120&&D->Age-At>.5f)
        {
            const auto Tint=CastChecked<UMaterialInstanceDynamic>(D->NeuronCores.Last()->GetMaterial(0))->K2_GetVectorParameterValue(TEXT("Tint"));
            Test->TestTrue(TEXT("Goal remains a distinct golden destination"),Tint.R>Tint.B*2);
            Capture(TEXT("brain-goal"));At=D->Age;Step=121;
        }
        else if(Step==121&&D->Age-At>.5f)
        {
            D->CurrentNeuron=46;D->Camera->SetActorLocation(D->NeuronWorldPosition(46)+D->GetActorUpVector()*600);
            D->BeginThought();D->NeuralWave();At=D->Age;Step=14;
        }
        else if(Step==14&&D->Age-At>2.8f)
        {
            const float Height=D->GetActorTransform().InverseTransformPosition(D->Camera->GetActorLocation()).Z;
            Test->TestTrue(TEXT("Thought remains a close, brief rise"),Height>4500&&Height<10000);
            Test->TestEqual(TEXT("Brain overview stays unobscured by transition white"),D->OverlayAlpha(),0.f);
            Test->TestTrue(TEXT("Thought keeps emitting pulses from multiple origins"),D->NeuralWaveTimes.Num()>8);
            TArray<USceneComponent*> Parts;D->GetComponents(Parts);int BloodParts=0;
            for(auto* Part:Parts)if(Part->ComponentHasTag(TEXT("EndingBloodVisual"))){++BloodParts;Test->TestFalse(TEXT("Blood vessel and blood cells are hidden before the overview"),Part->IsVisible());}
            Test->TestTrue(TEXT("Vessel shell, ribs, exit strands and cells are covered"),BloodParts>300);
            for(auto Nerve:D->Nerves)if(Nerve->GetName().StartsWith(TEXT("NeuronDendriticArbor"))&&Nerve->GetRelativeLocation()==D->NeuronPositions[3])
                Test->TestTrue(TEXT("Activated soma and dendrites share one colour state"),CastChecked<UMaterialInstanceDynamic>(Nerve->GetMaterial(0))->K2_GetVectorParameterValue(TEXT("Tint")).Equals(CastChecked<UMaterialInstanceDynamic>(D->NeuronCores[3]->GetMaterial(0))->K2_GetVectorParameterValue(TEXT("Tint")),.01f));
            Capture(TEXT("brain-envelope"));Step=15;
        }
        else if(Step==15&&D->Shot==0&&D->ShotTime>.3f)
        {
            ThoughtFrames.Sort();
            if(!ThoughtFrames.IsEmpty())Test->AddInfo(FString::Printf(TEXT("First thought-to-street cut: %d frames, p95 %.1f ms, peak %.1f ms (1080p offscreen; excludes screenshot writing)"),ThoughtFrames.Num(),ThoughtFrames[FMath::Min(ThoughtFrames.Num()-1,FMath::FloorToInt(ThoughtFrames.Num()*.95))],ThoughtFrames.Last()));
            Test->TestTrue(TEXT("Thought cuts to street within four seconds"),D->Age-At<4.5f);
            Test->TestEqual(TEXT("Street arrival contains no white screen"),D->OverlayAlpha(),0.f);
            Test->TestTrue(TEXT("Brain image is carried into the street transition"),D->bScaleBridgeReady&&D->ScaleZoomMaterial->K2_GetScalarParameterValue(TEXT("Bridge"))>.1f);
            Capture(TEXT("thought-street-bridge"));Step=16;
        }
        else if(Step==16&&D->ShotTime>1.4f)
        {D->FilmSpeed=1;D->SetShot(2);D->ShotTime=4;At=D->Age;Step=10;}
        else if(Step==10&&D->Age-At>1)
        {
            if(!ShotCaptured)
            {
                Test->TestEqual(TEXT("Preview remains in the requested shot before its end transition"),D->Shot,Shots[ShotIndex]);
                if(ShotIndex==4||ShotIndex==5)
                {
                    Test->TestFalse(TEXT("Petri specks stay hidden throughout the universe shot"),D->FinaleCellSpecks->IsVisible());
                    TArray<USceneComponent*> Layers;D->NebulaUniverse->GetChildrenComponents(false,Layers);
                    bool CheckedLayer=false;
                    for(auto* Layer:Layers)if(Layer->GetName().StartsWith(TEXT("ParallaxNebula")))
                    {
                        const FVector View=D->Camera->GetActorTransform().InverseTransformPosition(Layer->GetComponentLocation());
                        const bool InFrame=View.X>0&&FMath::Abs(View.Y)<View.X*FMath::Tan(FMath::DegreesToRadians(31.f));
                        Test->TestEqual(TEXT("Camera pullback physically brings the near nebula into frame"),InFrame,ShotIndex==5);
                        auto* Mat=Cast<UMaterialInstanceDynamic>(CastChecked<UStaticMeshComponent>(Layer)->GetMaterial(0));
                        Test->TestEqual(TEXT("Nebula opacity remains fully visible in world space"),Mat->K2_GetScalarParameterValue(TEXT("Reveal")),1.f);
                        CheckedLayer=true;break;
                    }
                    Test->TestTrue(TEXT("A spatial nebula layer was inspected"),CheckedLayer);
                }
                Capture(ShotIndex==1?TEXT("earth-close"):ShotIndex==2?TEXT("shot-3"):ShotIndex==3?TEXT("earth-wide"):ShotIndex==4?TEXT("solar-system"):ShotIndex==5?TEXT("purple-universe"):FString::Printf(TEXT("shot-%d"),Shots[ShotIndex]));ShotCaptured=true;At=D->Age;
            }
            else if(ShotIndex==3&&FParse::Param(FCommandLine::Get(),TEXT("EndingEarthOnly")))return true;
            else if(ShotIndex==7){FTransform T;D->TitleBatches[0]->GetInstanceTransform(0,T);CellBefore=T.GetLocation();At=D->Age;Step=11;}
            else{++ShotIndex;D->SetShot(Shots[ShotIndex]);D->ShotTime=ShotTimes[ShotIndex];At=D->Age;ShotCaptured=false;}
        }
        else if(Step==11&&D->Age-At>1.4f)
        {
            FTransform T;D->TitleBatches[0]->GetInstanceTransform(0,T);
            Test->TestTrue(TEXT("Cell cloud drifts before title assembly"),FVector::Dist(CellBefore,T.GetLocation())>100);
            Capture(TEXT("cells-drift"));At=D->Age;Step=13;
        }
        else if(Step==13&&D->Age-At>.25f)
        {
            D->RestartDemo();Test->TestEqual(TEXT("Replay returns to the compact cell"),D->GateIndex,0);
            TArray<USceneComponent*> Parts;D->GetComponents(Parts);
            for(auto* Part:Parts)if(Part->ComponentHasTag(TEXT("EndingBloodVisual")))Test->TestTrue(TEXT("Replay restores blood visuals"),Part->IsVisible());
            return true;
        }
        return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndingOrganicTest,"BrainMining.EndingEmergence.OrganicArtRoute",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FEndingOrganicTest::RunTest(const FString&){FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEndingOrganicPreview>(this));return true;}
#endif
