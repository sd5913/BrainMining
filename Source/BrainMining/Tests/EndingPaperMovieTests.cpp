#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "EndingEmergence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"
#include "ShaderCompiler.h"
#include "TextureCompiler.h"

#if WITH_EDITOR
#define BM_TEXTURES_COMPILING (FTextureCompilingManager::Get().GetNumRemainingTextures() > 0)
#else
#define BM_TEXTURES_COMPILING false
#endif
#include "UnrealClient.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

// A lightweight visual capture harness; normal gameplay still loads its one
// complete ending map. This invokes the production paper geometry and animation.
class FEndingPaperMoviePreview : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<AEndingEmergence> Director;
    int Frame=0;
    double Started=FPlatformTime::Seconds();
    bool Entered=false;
    void Enter(int Shot)
    {
        auto* D=Director.Get();D->Shot=Shot;D->ShotTime=0;
        if(Shot==10){D->PaperScene->SetVisibility(false,true);}
        if(Shot==11){D->EndFinaleDrill();D->ExitInkMaterial->SetScalarParameterValue(TEXT("Strength"),0);D->PaperScene->SetRelativeRotation(FRotator::ZeroRotator);}
        if(Shot==14)
        {
            D->PaperScene->SetVisibility(false,true);D->FinaleReality->SetVisibility(true,true);
            D->CreditsScreenMaterial->SetScalarParameterValue(TEXT("Power"),1);D->CreditsScreenMaterial->SetScalarParameterValue(TEXT("CreditMix"),0);
            D->TransitEffects(0);D->ExitInkMaterial->SetScalarParameterValue(TEXT("Strength"),0);
            D->Camera->GetCameraComponent()->NotifyCameraCut();D->Player->PlayerCameraManager->SetGameCameraCutThisFrame();
        }
    }
public:
    explicit FEndingPaperMoviePreview(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>240){FApp::SetUseFixedTimeStep(false);Test->AddError(TEXT("Paper movie timed out"));return true;}
        if((GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())||BM_TEXTURES_COMPILING)return false;
        UWorld* W=AutomationCommon::GetAnyGameWorld();if(!W||!W->GetFirstPlayerController())return false;
        if(!Director.IsValid())
        {
            auto* D=W->SpawnActor<AEndingEmergence>();Director=D;D->SetActorTickEnabled(false);D->Player=W->GetFirstPlayerController();
            auto& PP=D->Camera->GetCameraComponent()->PostProcessSettings;
            PP.bOverride_AutoExposureMinBrightness=true;PP.AutoExposureMinBrightness=0;
            PP.bOverride_AutoExposureMaxBrightness=true;PP.AutoExposureMaxBrightness=0;
            PP.bOverride_AutoExposureBias=true;PP.AutoExposureBias=0;
            PP.bOverride_BloomIntensity=true;PP.BloomIntensity=.32f;
            D->BuildFinale();D->DrillDistance=7800;D->FinaleDrillSpeed=370;D->Player->SetViewTarget(D->Camera);
            FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1.0/30.0);
            return false;
        }
        auto* D=Director.Get();
        if(!Entered){Enter(10);Entered=true;}
        const float Dt=1.f/30;D->Age+=Dt;D->UpdatePaperFinale(Dt);
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("PaperFinaleMovie")/FString::Printf(TEXT("frame-%04d.png"),Frame++),true,false);
        D->ShotTime+=Dt;
        if(D->Shot<14&&D->ShotTime>=AEndingEmergence::Duration(D->Shot))Enter(D->Shot+1);
        if(D->Shot==14&&D->ShotTime>8.6f)
        {
            Test->TestTrue(TEXT("Production paper sequence reaches the computer credits"),D->bComplete);
            Test->TestTrue(TEXT("The production finale completes its drill sequence"),D->FinaleDrillCuts>=0);
            Test->AddInfo(FString::Printf(TEXT("Captured %d production animation frames at 30 fps"),Frame));
            FApp::SetUseFixedTimeStep(false);return true;
        }
        return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndingPaperMovieTest,"BrainMining.EndingEmergence.PaperMovie",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FEndingPaperMovieTest::RunTest(const FString&){FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEndingPaperMoviePreview>(this));return true;}
#endif
