#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "EndingCrowd.h"
#include "EndingEmergence.h"
#include "Camera/CameraActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"
#include "ShaderCompiler.h"
#include "TextureCompiler.h"

class FEndingOptimizationPreview : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Step=0,EndingStrikes=0,RoomPreview=0;
    double RoomCaptureAt=0;
    double Started=FPlatformTime::Seconds(),LastFrame=0;
    TArray<double> StreetFrames;
    FVector BeforeChain=FVector::ZeroVector;
    float LastRadius=0;
    void Capture(const TCHAR* Name)
    {
        if(FParse::Param(FCommandLine::Get(),TEXT("EndingNoScreenshots")))return;
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("EndingEmergencePreview")/(FString(Name)+TEXT(".png")),false,false);
    }
    void Key(APlayerController* P,bool Down){P->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar,Down?IE_Pressed:IE_Released,Down?1.f:0.f));}
public:
    explicit FEndingOptimizationPreview(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Started>180){Test->AddError(FString::Printf(TEXT("Optimization preview timed out at step %d"),Step));return true;}
        if((GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())||FTextureCompilingManager::Get().GetNumRemainingTextures()>0)return false;
        UWorld* W=GIsEditor?GEditor->PlayWorld.Get():AutomationCommon::GetAnyGameWorld();
        if(!W||W->GetTimeSeconds()<2)return false;
        AEndingEmergence* D=nullptr;
        for(TActorIterator<AEndingEmergence> I(W);I;++I){D=*I;break;}
        if(!D||!D->StreetCrowd){Test->AddError(TEXT("Lightweight crowd did not initialize"));return true;}
        auto* C=D->StreetCrowd.Get();auto* PC=W->GetFirstPlayerController();
        const bool EnvironmentOnly=FParse::Param(FCommandLine::Get(),TEXT("EndingEnvironmentOnly"));
        if(EnvironmentOnly&&RoomPreview<6)
        {
            if(RoomPreview%2==0)
            {
                PC->SetControlRotation(FRotator(0,RoomPreview==0?0:RoomPreview==2?-55:55,0));
                RoomCaptureAt=Now+.6;++RoomPreview;
            }
            else if(Now>=RoomCaptureAt)
            {
                Capture(RoomPreview==1?TEXT("cell-room-front"):RoomPreview==3?TEXT("cell-room-left"):TEXT("cell-room-right"));
                ++RoomPreview;
            }
            return false;
        }
        if(D->Shot>=0&&D->Shot<=1&&LastFrame>0)
        {
            StreetFrames.Add(Now-LastFrame);
            if(Now-LastFrame>.1)Test->AddInfo(FString::Printf(TEXT("Street hitch %.1fms at shot=%d time=%.3f step=%d"),1000*(Now-LastFrame),D->Shot,D->ShotTime,Step));
        }
        LastFrame=Now;
        if(Step==0)
        {
            Test->TestEqual(TEXT("Dense street has 1200 walkers and one central person"),C->PersonCount(),1201);
            Test->TestEqual(TEXT("Merged crowd uses nine instanced batches"),C->BatchCount(),9);
            Test->TestEqual(TEXT("No skeletal pose drivers run for baked crowd"),C->PoseDrivers().Num(),0);
            Test->TestEqual(TEXT("Runtime asset does not load original heavy characters"),D->CrowdAssets->People.Num(),0);
            Test->TestEqual(TEXT("Four acquired city building types are assigned"),D->CityBuildingMeshes.Num(),4);
            Test->TestEqual(TEXT("User cell meshes assigned"),D->CellVisualMeshes.Num(),5);
            TArray<UInstancedStaticMeshComponent*> Buildings;D->GetComponents(Buildings);int32 BuildingCount=0,CellCount=0;
            for(auto* B:Buildings)
            {
                if(B->GetName().StartsWith(TEXT("CityAssetBuildings")))BuildingCount+=B->GetInstanceCount();
                if(B->GetName().StartsWith(TEXT("DenseCellMembranes"))){CellCount+=B->GetInstanceCount();Test->TestTrue(TEXT("Outer cells stay abstract spheres"),B->GetStaticMesh()!=D->CellVisualMeshes[1]);}
                if(B->GetName().StartsWith(TEXT("DenseCellNuclei")))Test->TestEqual(TEXT("Abstract nuclei preserved"),B->GetInstanceCount(),666);
                if(B->GetName().StartsWith(TEXT("Buildings"))||B->GetName().StartsWith(TEXT("CityWindows")))Test->TestEqual(TEXT("Old cube buildings/windows replaced"),B->GetInstanceCount(),0);
            }
            Test->TestEqual(TEXT("Acquired buildings fill city blocks"),BuildingCount,196);
            Test->TestEqual(TEXT("All 666 abstract surrounding cells retained"),CellCount,666);
            Test->TestFalse(TEXT("Crowd hidden during cell route"),C->IsVisible());
            if(EnvironmentOnly){D->FilmSpeed=.5f;D->PreviewFilm();Step=7;return false;}
            // Skip to the first host; exercise the real input and update path.
            D->GateIndex=2;D->CurrentNeuron=0;D->VisitedNeurons.Add(0);PC->SetViewTarget(D->Camera);PC->ResetIgnoreLookInput();
            D->Camera->SetActorLocation(D->NeuronWorldPosition(0)+D->GetActorUpVector()*110);
            PC->SetControlRotation((D->NeuronWorldPosition(4)-D->Camera->GetActorLocation()).Rotation());
            ++Step;return false;
        }
        if(Step==1){Key(PC,true);++Step;return false;}
        if(Step==2&&D->TravellingToNeuron==4){Key(PC,false);++Step;return false;}
        if(Step==3)
        {
            PC->SetControlRotation((D->NeuronWorldPosition(9)-D->Camera->GetActorLocation()).Rotation());
            if((D->Age-D->PulseAt)/D->TransitDuration>=.8f)
            {BeforeChain=D->Camera->GetActorLocation();Key(PC,true);++Step;}
            return false;
        }
        if(Step==4&&D->TravellingToNeuron==9)
        {
            Test->TestEqual(TEXT("First host registers before chained transit"),D->PossessionCount,1);
            Test->TestTrue(TEXT("Second transit cancels the first settling tail"),FVector::Dist(D->TransitStart,D->NeuronWorldPosition(4)+D->GetActorUpVector()*110)>5);
            Test->TestTrue(TEXT("Chained camera does not snap to the host"),FVector::Dist(D->TransitStart,BeforeChain)<100);
            ++Step;return false;
        }
        if(Step==5&&D->CurrentNeuron==9&&D->TravellingToNeuron==INDEX_NONE)
        {
            Test->TestEqual(TEXT("Two jumps register"),D->PossessionCount,2);
            Test->TestEqual(TEXT("No arrival shell can cross the camera"),D->NeuralWaveShells.Num(),0);
            Test->TestEqual(TEXT("No arrival overlay flash"),D->OverlayAlpha(),0.f);Capture(TEXT("neuron-arrival-no-shell"));
            PC->SetControlRotation((D->NeuronWorldPosition(14)-D->Camera->GetActorLocation()).Rotation());
            D->BeatAt=D->Age+.4f;++Step;return false;
        }
        if(Step==6&&D->Age>D->BeatAt)
        {
            Test->TestEqual(TEXT("Holding space does not repeatedly trigger jumps"),D->PossessionCount,2);
            Test->TestEqual(TEXT("Holding space stays on the host"),D->TravellingToNeuron,INDEX_NONE);
            Key(PC,false);D->FilmSpeed=.5f;D->PreviewFilm();++Step;return false;
        }
        if(Step==7&&D->ShotTime>.45f){Test->TestTrue(TEXT("Crowd becomes visible"),C->IsVisible());Capture(TEXT("crowd-lite-close"));++Step;return false;}
        if(Step==8&&D->ShotTime>1.1f){Capture(TEXT("crowd-lite-walk"));D->FilmSpeed=2;++Step;return false;}
        if(Step==9&&D->Shot==1&&D->ShotTime>1){Capture(TEXT("crowd-lite-wide"));++Step;return false;}
        if(Step==10&&D->Shot==1&&D->ShotTime>14){Capture(TEXT("city-lite-skyline"));++Step;return false;}
        if(Step==11&&D->Shot==2)
        {
            Test->TestFalse(TEXT("Crowd hides before the continental shot"),C->IsVisible());
            if(EnvironmentOnly)return true;
            StreetFrames.Sort();double Sum=0;for(double F:StreetFrames)Sum+=F;
            if(!StreetFrames.IsEmpty())Test->AddInfo(FString::Printf(TEXT("Street samples=%d mean=%.1fms p95=%.1fms max=%.1fms (screenshots=%s; includes first reveal)"),StreetFrames.Num(),1000*Sum/StreetFrames.Num(),1000*StreetFrames[FMath::FloorToInt((StreetFrames.Num()-1)*.95f)],1000*StreetFrames.Last(),FParse::Param(FCommandLine::Get(),TEXT("EndingNoScreenshots"))?TEXT("off"):TEXT("on")));
            ++Step;return false;
        }
        if(D->Shot==3)
        {
            const FVector ToEarth=D->GetActorTransform().TransformPosition(FVector(50000,0,0))-D->Camera->GetActorLocation();
            const float Radius=ToEarth.Size(),Alignment=FVector::DotProduct(D->Camera->GetActorForwardVector(),ToEarth.GetSafeNormal());
            if(LastRadius>0)Test->TestTrue(TEXT("Earth pullback never descends"),Radius>=LastRadius-.1f);LastRadius=Radius;
            if(Step==12&&D->ShotTime>.4f){Test->TestTrue(TEXT("Initial Earth shot faces globe"),Alignment>.995f);Capture(TEXT("earth-globe"));++Step;}
            else if(Step==13&&D->ShotTime>D->Duration(3)*.9f){Test->TestTrue(TEXT("Earth remains facing the planet centre"),Alignment>.995f);Capture(TEXT("earth-pullback"));++Step;}
        }
        if(Step==14&&D->Shot==5&&D->ShotTime>D->Duration(5)*.85f){Capture(TEXT("cell-acquired-hero"));++Step;return false;}
        if(Step==15&&D->Shot==6&&D->ShotTime>D->Duration(6)*.85f){Capture(TEXT("cell-acquired-crowd"));++Step;return false;}
        if(Step==16&&D->Shot==7){D->RestartDemo();Step=24;return false;}
        if(Step==24){Test->TestFalse(TEXT("Restart hides crowd"),C->IsVisible());Test->TestEqual(TEXT("Restart does not duplicate people"),C->PersonCount(),1201);return true;}
        return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndingCrowdTest,"BrainMining.EndingEmergence.AcquiredCrowd",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FEndingCrowdTest::RunTest(const FString&)
{
    if(GIsEditor)AutomationOpenMap(TEXT("/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEndingOptimizationPreview>(this));
    if(GIsEditor){ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());}return true;
}
#endif

