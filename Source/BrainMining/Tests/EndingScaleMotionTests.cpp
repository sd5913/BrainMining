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
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if WITH_EDITOR
#define BM_TEXTURES_COMPILING (FTextureCompilingManager::Get().GetNumRemainingTextures() > 0)
#else
#define BM_TEXTURES_COMPILING false
#endif

class FEndingScaleMotionPreview : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int Step=0,Target=2;float At=0;FVector EarthBefore,PlanetBefore;
    double Started=FPlatformTime::Seconds();
    double LastFrameAt=0;bool bMeasureTransition=false;TArray<double> TransitionFrames;
    double RailAt=0,LastRadius=0,LastRailAge=0,MinLogRate=100,MaxRailFrame=0;
    int RailFrames=0,RailReversals=0,RailStops=0,SheetIntersections=0;
    FVector Cosmos,LastRailEye;
    FVector DepartureEye;
    double SnapshotAt=0;
    const bool bJumpMovie=FParse::Param(FCommandLine::Get(),TEXT("ScaleJumpMovie"));
    int MovieFrame=0;
    void Capture(const FString& Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("EndingEmergencePreview")/(TEXT("scale-")+Name+TEXT(".png")),true,false);}
public:
    explicit FEndingScaleMotionPreview(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>210){Test->AddError(TEXT("Scale motion validation timed out"));return true;}
        if((GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())||BM_TEXTURES_COMPILING)return false;
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
            TArray<USceneComponent*> RestoredFields;D->NebulaUniverse->GetChildrenComponents(false,RestoredFields);
            int RestoredCount=0;
            for(auto* Field:RestoredFields)
            {
                const FVector P=Field->GetRelativeLocation();
                if(Field->GetName().StartsWith(TEXT("DeepStarField")))
                {const int L=FMath::RoundToInt((-P.X-19000)/4300);Test->TestEqual(TEXT("Original star-field placement is restored"),float(P.Y),float((L%2?1:-1)*12500));++RestoredCount;}
                else if(Field->GetName().StartsWith(TEXT("ParallaxNebula")))
                {const int L=FMath::RoundToInt((-P.X-12500)/4300);Test->TestEqual(TEXT("Original nebula placement is restored"),float(P.Y),float((L%2?1:-1)*(6200+L*1400)));++RestoredCount;}
            }
            Test->TestEqual(TEXT("All original texture layers are present"),RestoredCount,7);
            if(bJumpMovie)
            {
                D->PreviewFilm();D->SetShot(1);D->ShotTime=D->Duration(1)-1;D->SetActorTickEnabled(false);
                FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1.0/30);Step=100;return false;
            }
            D->PreviewFilm();D->FilmSpeed=1;Step=1;
        }
        if(Step==100)
        {
            const float Dt=1.f/30;D->Age+=Dt;D->ShotTime+=Dt*1.5f;
            if(D->ShotTime>=D->Duration(D->Shot))
            {
                const float Rest=D->ShotTime-D->Duration(D->Shot);
                D->ShotTime=D->Duration(D->Shot);D->UpdateFilm(0);D->SetShot(D->Shot+1);D->ShotTime=Rest;
            }
            D->UpdateFilm(Dt);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("ScaleJumpMovie")/FString::Printf(TEXT("frame-%04d.png"),MovieFrame++),true,false);
            if(D->Shot==4&&D->ShotTime>1.2f)
            {
                Test->AddInfo(FString::Printf(TEXT("Captured %d production scale-transition frames at 30 fps"),MovieFrame));
                FApp::SetUseFixedTimeStep(false);D->SetActorTickEnabled(true);return true;
            }
            return false;
        }
        if(Step==1)
        {
            D->SetActorTickEnabled(false);
            D->SetShot(Target-1);D->ShotTime=D->Duration(Target-1)-.48f;D->UpdateFilm(0);
            DepartureEye=D->Camera->GetActorLocation();Capture(FString::Printf(TEXT("depart-%d"),Target));SnapshotAt=Now;Step=2;
        }
        else if(Step==2&&Now-SnapshotAt>.12&&!FScreenshotRequest::IsScreenshotRequested())
        {
            D->ShotTime=D->Duration(Target-1)-.22f;D->UpdateFilm(0);
            Test->TestEqual(TEXT("Outgoing scale change has no white overlay"),D->OverlayAlpha(),0.f);
            Test->TestTrue(TEXT("Outgoing radial acceleration is active"),D->ScaleZoomMaterial->K2_GetScalarParameterValue(TEXT("Strength"))>.05f);
            Test->TestTrue(TEXT("Spatial jump anticipation starts before the old set leaves"),D->ScaleZoomMaterial->K2_GetScalarParameterValue(TEXT("Handoff"))>0);
            Test->TestTrue(TEXT("The jump raises the actual 3D camera, not the image plane"),D->Camera->GetActorLocation().Z>DepartureEye.Z+25);
            Capture(FString::Printf(TEXT("jump-%d"),Target));
            SnapshotAt=Now;Step=20;
        }
        else if(Step==20&&Now-SnapshotAt>.12&&!FScreenshotRequest::IsScreenshotRequested())
        {
            // Screenshot encoding can block a short transition. Measure a
            // separate natural crossing after those files are completely saved.
            D->ShotTime=D->Duration(Target-1)-.45f;D->UpdateFilm(0);D->SetActorTickEnabled(true);
            TransitionFrames.Reset();LastFrameAt=Now;bMeasureTransition=true;Step=21;
        }
        else if(Step==21&&D->Shot==Target)
        {
            Test->TestTrue(TEXT("Scale resources are warmed before the jump"),D->bScaleWarmupComplete);
            Test->TestEqual(TEXT("Incoming scale change has no white overlay"),D->OverlayAlpha(),0.f);
            // Sample the short overlap deterministically: a first-use render hitch
            // can otherwise skip its entire 0.55-second lifetime between test ticks.
            D->ShotTime=.08f;D->UpdateFilm(0);
            const float ExpectedFov=Target==2?60.f:Target==3?FMath::Lerp(68.f,66.f,.08f/D->Duration(3)):62.f;
            Test->TestTrue(TEXT("Arrival lens cannot zoom back in and cancel retreat"),FMath::IsNearlyEqual(D->Camera->GetCameraComponent()->FieldOfView,ExpectedFov,.01f));
            const float Blend=D->ScaleZoomMaterial->K2_GetScalarParameterValue(TEXT("Bridge"));
            Test->TestTrue(TEXT("A short old exposure blends into the live arrival"),Blend>.1f&&Blend<.65f);
            if(Target==2)
            {
                // Dolly along the authored sightline must not pitch the planar
                // continent down and then expand it vertically on arrival.
                const FVector Start(-110000,-30000,130000),End(-180000,-45000,220000);
                const FVector Aim(15000,0,-1000);
                const float ExpectedPitch=(Aim-Start).Rotation().Pitch;
                Test->TestTrue(TEXT("Incoming continent retains its authored pitch"),FMath::Abs(D->Camera->GetActorRotation().Pitch-ExpectedPitch)<1.f);
            }
            Test->TestTrue(TEXT("The new scene rises into view"),D->ScaleZoomMaterial->K2_GetScalarParameterValue(TEXT("Rise"))>.1f);
            D->ShotTime=0;D->UpdateFilm(0);
            At=D->Age;Step=3;
        }
        else if(Step==3&&D->Age-At>.18f)
        {
            bMeasureTransition=false;TransitionFrames.Sort();
            if(!TransitionFrames.IsEmpty())Test->AddInfo(FString::Printf(TEXT("Transition %d: %d frames, p95 %.1f ms, peak %.1f ms (offscreen; excludes screenshot writing)"),Target,TransitionFrames.Num(),TransitionFrames[FMath::Min(TransitionFrames.Num()-1,FMath::FloorToInt(TransitionFrames.Num()*.95))],TransitionFrames.Last()));
            Capture(FString::Printf(TEXT("bridge-%d"),Target));Step=4;
        }
        else if(Step==4&&D->Age-At>.65f)
        {
            Test->TestEqual(TEXT("Old scene is gone within 0.65 seconds"),D->ScaleZoomMaterial->K2_GetScalarParameterValue(TEXT("Bridge")),0.f);
            Test->TestEqual(TEXT("Zoom blur settles completely"),D->ScaleZoomMaterial->K2_GetScalarParameterValue(TEXT("Strength")),0.f);
            if(++Target<=4)Step=1;
            else {D->SetShot(3);D->ShotTime=3;D->UpdateFilm(0);EarthBefore=D->EarthMovingParts[0]->GetRelativeLocation();At=D->Age;Step=5;}
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
            Test->TestEqual(TEXT("Earth holds one extra second before the nebula cut"),D->Duration(3),11.f);
            Test->TestFalse(TEXT("Cell anatomy stays hidden even late in the nebula passage"),D->Cell->IsVisible());
            Test->TestFalse(TEXT("Other cells cannot intrude early"),D->OtherCells->IsVisible());
            Capture(TEXT("nebula-uninterrupted"));At=D->Age;Step=8;
        }
        else if(Step==8&&D->Age-At>.5f&&!FScreenshotRequest::IsScreenshotRequested())
        {
            // Inspect both sides of each seam, including velocity and orientation.
            D->bComfortMode=false;D->DriftOffset=FVector::ZeroVector;
            for(int Seam=4;Seam<=5;++Seam)
            {
                D->SetShot(Seam);D->ShotTime=D->Duration(Seam)-.01f;D->UpdateFilm(0);
                const FVector Before=D->Camera->GetActorLocation();
                D->ShotTime=D->Duration(Seam);D->UpdateFilm(0);
                const FVector End=D->Camera->GetActorLocation();const FQuat Facing=D->Camera->GetActorQuat();
                D->SetShot(Seam+1);D->UpdateFilm(0);
                Test->TestTrue(TEXT("Cosmic seam preserves position"),End.Equals(D->Camera->GetActorLocation(),.1));
                Test->TestTrue(TEXT("Cosmic seam preserves facing"),Facing.AngularDistance(D->Camera->GetActorQuat())<.001);
                D->ShotTime=.01f;D->UpdateFilm(0);const FVector Next=D->Camera->GetActorLocation()-End;
                Test->TestTrue(TEXT("Cosmic seam preserves velocity"),(Next-(End-Before)).Size()<Next.Size()*.03);
                Test->TestEqual(TEXT("Continuous cosmic rail never overlays an old screenshot"),D->ScaleZoomMaterial->K2_GetScalarParameterValue(TEXT("Bridge")),0.f);
            }
            auto* P=W->GetFirstPlayerController();
            for(FKey Key:{EKeys::S,EKeys::W,EKeys::A,EKeys::D})P->InputKey(FInputKeyEventArgs::CreateSimulated(Key,IE_Released,0.f));
            D->PullbackBoost=0;D->FilmSpeed=1;D->SetShot(4);D->UpdateFilm(0);
            Cosmos=D->GetActorTransform().TransformPosition(FVector(95000,0,0));LastRailEye=D->Camera->GetActorLocation();
            LastRadius=FVector::Dist(D->Camera->GetActorLocation(),Cosmos);LastRailAge=D->Age;RailAt=D->Age;Step=9;
        }
        else if(Step==9)
        {
            TArray<USceneComponent*> Fields;D->NebulaUniverse->GetChildrenComponents(false,Fields);
            for(auto* Field:Fields)if(Field->GetName().StartsWith(TEXT("ParallaxNebula"))||Field->GetName().StartsWith(TEXT("DeepStarField")))
            {
                const FTransform Transform=Field->GetComponentTransform();
                const FVector A=Transform.InverseTransformPosition(LastRailEye),B=Transform.InverseTransformPosition(D->Camera->GetActorLocation());
                if(A.Z*B.Z<=0&&!FMath::IsNearlyEqual(A.Z,B.Z))
                {
                    const FVector Crossing=FMath::Lerp(A,B,-A.Z/(B.Z-A.Z));
                    SheetIntersections+=FMath::Abs(Crossing.X)<52&&FMath::Abs(Crossing.Y)<52;
                }
            }
            LastRailEye=D->Camera->GetActorLocation();
            const double Radius=FVector::Dist(D->Camera->GetActorLocation(),Cosmos),Delta=D->Age-LastRailAge;
            if(D->Shot<=6&&Delta>0)
            {
                const double Rate=FMath::Loge(Radius/LastRadius)/Delta;
                MinLogRate=FMath::Min(MinLogRate,Rate);RailReversals+=Rate<0;RailStops+=Rate<.01;
                MaxRailFrame=FMath::Max(MaxRailFrame,Delta*1000);++RailFrames;
            }
            LastRadius=Radius;LastRailAge=D->Age;
            if(D->Shot==7&&D->ShotTime>.3f)
            {
                Test->TestTrue(TEXT("No-input rail runs at 1.5x speed from solar system to title"),D->Age-RailAt>48&&D->Age-RailAt<51);
                Test->TestTrue(TEXT("Default outward speed stays at least 1.5x the previous rail"),MinLogRate>.038);
                Test->TestEqual(TEXT("No-input rail never reverses"),RailReversals,0);
                Test->AddInfo(FString::Printf(TEXT("Restored original nebula layout: %d camera-plane crossings"),SheetIntersections));
                Test->TestEqual(TEXT("No-input rail never brakes to a stop at chapter ends"),RailStops,0);
                Test->TestTrue(TEXT("Title inherits the moving camera velocity"),D->TitleCameraVelocity.Size()>1000&&D->FilmCameraVelocity.Size()>1000);
                Test->AddInfo(FString::Printf(TEXT("No-input cosmic rail: %d frames, minimum log-distance rate %.4f/s, peak frame %.1f ms"),RailFrames,MinLogRate,MaxRailFrame));
                At=D->Age;Step=10;
            }
        }
        else if(Step==10&&D->Shot==7&&D->ShotTime>10.5f)
        {Capture(TEXT("continuous-wooow"));At=D->Age;Step=11;}
        else if(Step==11&&D->Age-At>.5f&&!FScreenshotRequest::IsScreenshotRequested())
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
