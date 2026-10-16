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
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"
#include "ShaderCompiler.h"
#include "TextureCompiler.h"

class FEndingOrganicPreview : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int Step=0,ShotIndex=0;
    double Start=FPlatformTime::Seconds(),At=0;FVector CellBefore;bool ShotCaptured=false;
    const int Shots[5]={2,3,4,5,6};
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
        if(Step==0)
        {
            Test->TestEqual(TEXT("Neural branching route retains 47 hosts"),D->NeuronPositions.Num(),47);
            Test->TestTrue(TEXT("First host is lowered toward cortex"),D->NeuronPositions[0].Z<500);
            TArray<UStaticMeshComponent*> Parts;D->GetComponents(Parts);int Lines=0,Boutons=0;
            for(auto* C:Parts){Lines+=C->GetName().StartsWith(TEXT("FollowLight"));Boutons+=C->GetName().StartsWith(TEXT("SynapticBouton"));}
            Test->TestEqual(TEXT("No road-like floor guidance remains"),Lines,0);
            Test->TestTrue(TEXT("Dendrites have visible terminal boutons"),Boutons>=600);
            Capture(TEXT("cell-front"));At=D->Age;Step=1;
        }
        else if(Step==1&&D->Age-At>.6f){P->SetControlRotation(FRotator(0,-45,0));At=D->Age;Step=2;}
        else if(Step==2&&D->Age-At>.6f){Capture(TEXT("cell-nucleus"));P->SetControlRotation(FRotator(0,40,0));At=D->Age;Step=3;}
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
            D->EnterBrain();At=D->Age;Step=6;
        }
        else if(Step==6&&D->GateIndex==2&&!D->bEnteringBrain)
        {P->SetControlRotation((D->NeuronWorldPosition(3)-D->Camera->GetActorLocation()).Rotation());At=D->Age;Step=7;}
        else if(Step==7&&D->Age-At>.8f){Capture(TEXT("brain-neurons"));Key(P,EKeys::SpaceBar,true);Step=8;}
        else if(Step==8&&D->TravellingToNeuron!=INDEX_NONE){Key(P,EKeys::SpaceBar,false);Step=9;}
        else if(Step==9&&D->CurrentNeuron==3&&D->TravellingToNeuron==INDEX_NONE)
        {Capture(TEXT("brain-arrival"));At=D->Age;Step=12;}
        else if(Step==12&&D->Age-At>.25f)
        {D->PreviewFilm();D->SetShot(2);D->ShotTime=4;At=D->Age;Step=10;}
        else if(Step==10&&D->Age-At>1)
        {
            if(!ShotCaptured){Capture(FString::Printf(TEXT("shot-%d"),Shots[ShotIndex]));ShotCaptured=true;At=D->Age;}
            else if(ShotIndex==4){FTransform T;D->TitleBatches[0]->GetInstanceTransform(0,T);CellBefore=T.GetLocation();At=D->Age;Step=11;}
            else{++ShotIndex;D->SetShot(Shots[ShotIndex]);D->ShotTime=Shots[ShotIndex]==5?16.f:6.f;At=D->Age;ShotCaptured=false;}
        }
        else if(Step==11&&D->Age-At>1.4f)
        {
            FTransform T;D->TitleBatches[0]->GetInstanceTransform(0,T);
            Test->TestTrue(TEXT("Cell cloud drifts before title assembly"),FVector::Dist(CellBefore,T.GetLocation())>100);
            Capture(TEXT("cells-drift"));At=D->Age;Step=13;
        }
        else if(Step==13&&D->Age-At>.25f){D->RestartDemo();Test->TestEqual(TEXT("Replay returns to the compact cell"),D->GateIndex,0);return true;}
        return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndingOrganicTest,"BrainMining.EndingEmergence.OrganicArtRoute",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FEndingOrganicTest::RunTest(const FString&){FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEndingOrganicPreview>(this));return true;}
#endif
