#if WITH_DEV_AUTOMATION_TESTS
#include "EndingEmergence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"
#include "ShaderCompiler.h"
#include "TextureCompiler.h"

class FEndingScaleMotionPreview : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int Step=0,Target=2;float At=0;FVector EarthBefore,PlanetBefore;
    double Started=FPlatformTime::Seconds();
    double LastFrameAt=0;bool bMeasureTransition=false;TArray<double> TransitionFrames;
    void Capture(const FString& Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("EndingEmergencePreview")/(TEXT("scale-")+Name+TEXT(".png")),true,false);}
public:
    explicit FEndingScaleMotionPreview(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>120){Test->AddError(TEXT("Scale motion validation timed out"));return true;}
        if((GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())||FTextureCompilingManager::Get().GetNumRemainingTextures())return false;
        UWorld* W=AutomationCommon::GetAnyGameWorld();if(!W)return false;
        AEndingEmergence* D=nullptr;for(TActorIterator<AEndingEmergence> I(W);I;++I){D=*I;break;}
        if(!D||!D->bReady||D->Age<3)return false;
        const double Now=FPlatformTime::Seconds();
        if(bMeasureTransition&&LastFrameAt>0)TransitionFrames.Add((Now-LastFrameAt)*1000);
        LastFrameAt=Now;
        if(Step==0)
        {
            Test->TestNotNull(TEXT("Dedicated scale zoom material exists"),D->ScaleZoomMaterial.Get());
            if(!D->ScaleZoomMaterial)return true;
            Test->TestEqual(TEXT("All eight planets have motion tracks"),D->OrbitingBodies.Num(),8);
            Test->TestNotNull(TEXT("Saturn ring follows its planet"),D->OrbitingRing.Get());
            D->PreviewFilm();D->FilmSpeed=1;Step=1;
        }
        if(Step==1)
        {
            D->SetShot(Target-1);D->ShotTime=D->Duration(Target-1)-1.1f;D->UpdateFilm(0);At=D->Age;Step=2;
        }
        else if(Step==2&&D->Age-At>.25f)
        {
            Test->TestEqual(TEXT("Outgoing scale change has no white overlay"),D->OverlayAlpha(),0.f);
            Test->TestTrue(TEXT("Outgoing radial acceleration is active"),D->ScaleZoomMaterial->K2_GetScalarParameterValue(TEXT("Strength"))>.05f);
            TransitionFrames.Reset();bMeasureTransition=true;
            D->SetShot(Target);D->UpdateFilm(0);
            Test->TestTrue(TEXT("Previous scene was captured before changing sets"),D->bScaleBridgeReady&&D->ScaleBridge);
            Test->TestEqual(TEXT("Incoming scale change has no white overlay"),D->OverlayAlpha(),0.f);
            At=D->Age;Step=3;
        }
        else if(Step==3&&D->Age-At>.42f)
        {
            bMeasureTransition=false;TransitionFrames.Sort();
            if(!TransitionFrames.IsEmpty())Test->AddInfo(FString::Printf(TEXT("Transition %d: %d frames, p95 %.1f ms, peak %.1f ms (1080p offscreen; excludes screenshot writing)"),Target,TransitionFrames.Num(),TransitionFrames[FMath::Min(TransitionFrames.Num()-1,FMath::FloorToInt(TransitionFrames.Num()*.95))],TransitionFrames.Last()));
            float Blend=D->ScaleZoomMaterial->K2_GetScalarParameterValue(TEXT("Bridge"));
            Test->TestTrue(TEXT("Transition overlaps old and new image content"),Blend>.1f&&Blend<.95f);
            Capture(FString::Printf(TEXT("bridge-%d"),Target));Step=4;
        }
        else if(Step==4&&D->Age-At>1.5f)
        {
            Test->TestEqual(TEXT("Zoom blur settles completely"),D->ScaleZoomMaterial->K2_GetScalarParameterValue(TEXT("Strength")),0.f);
            if(++Target<=4)Step=1;
            else {D->SetShot(3);D->ShotTime=10;D->UpdateFilm(0);EarthBefore=D->EarthMovingParts[0]->GetRelativeLocation();At=D->Age;Step=5;}
        }
        else if(Step==5&&D->Age-At>2)
        {
            Test->TestTrue(TEXT("Hero Earth has gentle orbital displacement"),FVector::Dist(EarthBefore,D->EarthMovingParts[0]->GetRelativeLocation())>10);
            auto* M=Cast<UMaterialInstanceDynamic>(D->EarthMovingParts[0]->GetMaterial(0));
            Test->TestTrue(TEXT("Hero Earth spins at a readable rate"),M&&FMath::IsNearlyEqual(M->K2_GetScalarParameterValue(TEXT("Spin")),D->CelestialAge*.012f,.0001f));
            Capture(TEXT("earth-dawn"));At=D->Age;Step=51;
        }
        else if(Step==51&&D->Age-At>.5f&&!FScreenshotRequest::IsScreenshotRequested())
        {D->SetShot(4);D->ShotTime=7;D->UpdateFilm(0);PlanetBefore=D->OrbitingBodies[0]->GetRelativeLocation();At=D->Age;Step=6;}
        else if(Step==6&&D->Age-At>2)
        {
            Test->TestTrue(TEXT("Solar planet moves around its orbit"),FVector::Dist(PlanetBefore,D->OrbitingBodies[0]->GetRelativeLocation())>30);
            Test->TestTrue(TEXT("Saturn rings track their host"),D->OrbitingRing->GetRelativeLocation().Equals(D->OrbitingBodies[5]->GetRelativeLocation(),.01));
            Capture(TEXT("solar-motion"));At=D->Age;Step=61;
        }
        else if(Step==61&&D->Age-At>.5f&&!FScreenshotRequest::IsScreenshotRequested()){D->ShotTime=31;D->UpdateFilm(0);At=D->Age;Step=7;}
        else if(Step==7&&D->Age-At>.5f)
        {
            Test->TestEqual(TEXT("Nebula passage now lasts 35 seconds"),D->Duration(4),35.f);
            Test->TestFalse(TEXT("Cell anatomy stays hidden even late in the nebula passage"),D->Cell->IsVisible());
            Test->TestFalse(TEXT("Other cells cannot intrude early"),D->OtherCells->IsVisible());
            Capture(TEXT("nebula-uninterrupted"));At=D->Age;Step=8;
        }
        else if(Step==8&&D->Age-At>.5f&&!FScreenshotRequest::IsScreenshotRequested())
        {
            D->RestartDemo();
            Test->TestEqual(TEXT("Replay resets celestial time"),D->CelestialAge,0.f);
            Test->TestEqual(TEXT("Replay clears transition overlay"),D->ScaleZoomMaterial->K2_GetScalarParameterValue(TEXT("Strength")),0.f);
            return true;
        }
        return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndingScaleMotionTest,"BrainMining.EndingEmergence.ScaleMotion",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FEndingScaleMotionTest::RunTest(const FString&){FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEndingScaleMotionPreview>(this));return true;}
#endif
