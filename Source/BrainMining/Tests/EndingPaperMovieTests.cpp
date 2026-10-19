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
#include "Misc/FileHelper.h"
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
#include "EngineUtils.h"
#include "DiggableRock.h"

// Record the actual map's director and Tick/SetShot path, including visibility
// changes and media readiness. A parallel preview rail misses boundary flashes.
class FEndingPaperMoviePreview : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<AEndingEmergence> Director;
    int Frame=0;
    double Started=FPlatformTime::Seconds();
    bool Entered=false;
    double RecordingStarted=0,LastRecordedAt=0;
    FString FrameTimes=TEXT("frame,time,shot,shot_time,paper_visible\n");
public:
    explicit FEndingPaperMoviePreview(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>240){FApp::SetUseFixedTimeStep(false);Test->AddError(TEXT("Paper movie timed out"));return true;}
        if(!Entered&&((GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())||BM_TEXTURES_COMPILING))return false;
        UWorld* W=AutomationCommon::GetAnyGameWorld();if(!W||!W->GetFirstPlayerController())return false;
        if(!Director.IsValid())
        {
            AEndingEmergence* D=nullptr;for(TActorIterator<AEndingEmergence> I(W);I;++I){D=*I;break;}
            if(!D||!D->bReady)return false;
            Director=D;D->PreviewFilm();D->SetShot(9);D->FilmSpeed=1;
            // Start in the last half-second of the already excavated tunnel,
            // retaining the real drilling-to-exit chapter boundary in the film.
            D->DrillDistance=7600;D->FinaleDrillSpeed=370;
            for(auto Rock:D->FinaleRocks)if(Rock){Rock->SetActorHiddenInGame(true);Rock->SetActorEnableCollision(false);}
            Entered=true;RecordingStarted=FPlatformTime::Seconds();
            return false;
        }
        auto* D=Director.Get();
        const double Now=FPlatformTime::Seconds();
        if(D->Shot==11||(D->Shot==12&&D->ShotTime<2.f))
            Test->TestTrue(TEXT("Desktop remains visible through the rock approach and impact"),D->PaperScene->IsVisible());
        FrameTimes+=FString::Printf(TEXT("%d,%.6f,%d,%.6f,%d\n"),Frame,Now-RecordingStarted,D->Shot,D->ShotTime,D->PaperScene->IsVisible());
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("PaperFinaleMovie")/FString::Printf(TEXT("frame-%04d.png"),Frame++),true,false);
        if(D->Shot==13&&D->ShotTime>6.f)
        {
            Test->TestTrue(TEXT("Production paper sequence enters the video aperture"),D->bComplete);
            Test->TestTrue(TEXT("The production finale completes its drill sequence"),D->FinaleDrillCuts>=0);
            FFileHelper::SaveStringToFile(FrameTimes,*(FPaths::ProjectSavedDir()/TEXT("PaperFinaleMovie/frame-times.csv")));
            Test->AddInfo(FString::Printf(TEXT("Captured %d real-time production frames over %.3f seconds"),Frame,Now-RecordingStarted));
            FApp::SetUseFixedTimeStep(false);return true;
        }
        return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndingPaperMovieTest,"BrainMining.EndingEmergence.PaperMovie",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FEndingPaperMovieTest::RunTest(const FString&){FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEndingPaperMoviePreview>(this));return true;}
#endif
