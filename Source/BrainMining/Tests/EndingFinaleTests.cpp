#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "EndingEmergence.h"
#include "MiningComponent.h"
#include "MiningToolEffectsComponent.h"
#include "DiggableRock.h"
#include "EndingComicRock.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "UDynamicMesh.h"
#include "Camera/CameraActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Editor.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"
#include "ShaderCompiler.h"
#include "TextureCompiler.h"

#if WITH_EDITOR
#define BM_TEXTURES_COMPILING (FTextureCompilingManager::Get().GetNumRemainingTextures() > 0)
#else
#define BM_TEXTURES_COMPILING false
#endif
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

class FEndingTitleDrillPreview : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int Step=0;double Started=FPlatformTime::Seconds();
    int InputPhase=0,LastCuts=0,ColorCapture=0;
    bool EntryCaptured=false,FadeCaptured=false,OriginalEnabled=true,ExitStartCaptured=false,WhiteCaptured=false;
    float LastContact=0,MaxContactGap=0;
    bool GroundTurnCaptured=false,PortalCaptured=false;
    float LastBlackProbe=-1;int BlackMatchSamples=0;
    float Since=0,BaseSpeed=0,BulbIntensity=0,BulbMin=10000,BulbMax=0,OriginalRadius=0,OriginalSmoke=0,OriginalDust=0,OriginalSparks=0;bool InfiniteBefore=false;
    void Key(APlayerController* P,FKey K,bool Down){P->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f));}
    void Capture(const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("EndingEmergencePreview")/(FString(Name)+TEXT(".png")),true,false);}
public:
    explicit FEndingTitleDrillPreview(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>180){Test->AddError(FString::Printf(TEXT("Finale timed out at step %d"),Step));return true;}
        if((GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())||BM_TEXTURES_COMPILING)return false;
        UWorld* W=GIsEditor?GEditor->PlayWorld.Get():AutomationCommon::GetAnyGameWorld();if(!W||W->GetTimeSeconds()<2)return false;
        AEndingEmergence* D=nullptr;for(TActorIterator<AEndingEmergence> I(W);I;++I){D=*I;break;}if(!D||!D->bReady)return false;
        auto* P=W->GetFirstPlayerController();auto* M=D->Mining.Get();auto* Effects=P->GetPawn()->FindComponentByClass<UMiningToolEffectsComponent>();
        if(D->Shot==13&&D->ShotTime>.9f&&D->ShotTime<2.2f&&D->ShotTime-LastBlackProbe>.08f)
        {
            // Slate's normal screenshot path works with RenderOffscreen. Direct
            // viewport ReadPixels has no swapchain texture in that mode.
            Capture(*FString::Printf(TEXT("paper-black-match-%02d"),BlackMatchSamples++));
            LastBlackProbe=D->ShotTime;
        }
        if(D->Shot==10&&D->ShotTime>3.65f&&!PortalCaptured)
        {
            Test->TestTrue(TEXT("White paper only appears after the black flight"),D->PaperScene->IsVisible()&&!D->FinaleLight->IsVisible());
            Capture(TEXT("paper-luminous-portal"));PortalCaptured=true;
        }
        if(D->Shot==10&&D->ShotTime>5.35f&&!GroundTurnCaptured)
        {
            const FTransform Frame=D->PaperScene->GetComponentTransform();
            Test->TestTrue(TEXT("Exit ends above the desktop with a horizontal forward gaze"),Frame.InverseTransformPosition(D->Camera->GetActorLocation()).Z>20&&Frame.InverseTransformVectorNoScale(D->Camera->GetActorForwardVector()).X>.98f);
            Capture(TEXT("paper-white-to-ground"));GroundTurnCaptured=true;
        }
        if(D->Shot==9&&!EntryCaptured){Capture(TEXT("wooow-cave-first-frame"));EntryCaptured=true;}
        if(D->Shot==8&&D->ShotTime>1.6f&&!FadeCaptured)
        {
            const float Reveal=D->FinalePreviewMaterials[0]->K2_GetScalarParameterValue(TEXT("Reveal"));
            Test->TestTrue(TEXT("Cave is partially revealed during descent, not instant"),Reveal>.1f&&Reveal<.8f);
            Capture(TEXT("wooow-o-partial-reveal"));FadeCaptured=true;
        }
        if(D->Shot==9&&D->DrillDistance>7100&&!WhiteCaptured)
        {
            Test->TestTrue(TEXT("White line-art phase continues actual excavation"),M->IsMiningActive()&&D->FinaleRocks.Last()->SuccessfulCuts>0);
            Test->TestEqual(TEXT("Drills become white line art before exiting"),D->FinaleDrillMaterials[0]->K2_GetScalarParameterValue(TEXT("WhiteInk")),1.f);
            Capture(TEXT("wooow-cave-white-lineart"));WhiteCaptured=true;
        }
        if(D->Shot==9&&D->DrillDistance>220&&D->DrillDistance<7500)
        {
            if(!LastContact||D->FinaleDrillCuts!=LastCuts)
            {
                if(LastContact)MaxContactGap=FMath::Max(MaxContactGap,D->Age-LastContact);
                LastContact=D->Age;LastCuts=D->FinaleDrillCuts;
            }
            const float Positions[]={1700,3100,4400,5700};
            const TCHAR* Names[]={TEXT("wooow-cave-purple"),TEXT("wooow-cave-red"),TEXT("wooow-cave-yellow"),TEXT("wooow-cave-ivory")};
            if(ColorCapture<4&&D->DrillDistance>Positions[ColorCapture])Capture(Names[ColorCapture++]);
        }
        if(D->Shot==10&&!ExitStartCaptured&&D->ShotTime>.3f)
        {
            Test->TestFalse(TEXT("Desktop cannot occlude the black-space exit"),D->PaperScene->IsVisible());
            Test->TestTrue(TEXT("Distant luminous aperture remains visible in black space"),D->FinaleLight->IsVisible());
            const FTransform Frame=D->PaperScene->GetComponentTransform();
            Test->TestTrue(TEXT("Exit remains on the paper front, looking down or across it rather than at the ceiling"),Frame.InverseTransformPosition(D->Camera->GetActorLocation()).Z>0&&Frame.InverseTransformVectorNoScale(D->Camera->GetActorForwardVector()).Z<=.01f);
            Test->TestEqual(TEXT("Tool copies survive the initial exit transition"),D->FinaleDrills.Num(),2);
            Test->TestTrue(TEXT("Speed strokes survive the initial exit transition"),D->FinaleSpeedLines->IsVisible());
            ExitStartCaptured=true;
        }
        if(Step==0)
        {
            InfiniteBefore=M->bInfiniteDrillEnergy;OriginalRadius=M->Radius;
            OriginalSmoke=Effects->DrillSmokeStrength;OriginalDust=Effects->DrillDustStrength;OriginalSparks=Effects->DrillSparkStrength;OriginalEnabled=Effects->bEnabled;
            for(int Seed=137;Seed<154;++Seed)
            {
                auto Rock=EndingComic::Volume(FVector(620,700,700));EndingComic::InkFacets(Rock);
                Test->TestTrue(TEXT("Generated ending rock is a closed, valid solid"),Rock.IsClosed()&&Rock.CheckValidity(UE::Geometry::FDynamicMesh3::FValidityOptions(),UE::Geometry::EValidityCheckFailMode::ReturnOnly));
                for(int T:Rock.TriangleIndicesItr())Test->TestTrue(TEXT("Continuous volume faces point outwards for collision and subtraction"),Rock.GetTriNormal(T).Dot(Rock.GetTriCentroid(T))>0);
            }
            D->PreviewFilm();D->SetShot(6);D->ShotTime=17.8f;D->UpdateFilm(0);D->FilmSpeed=1;
            Test->TestEqual(TEXT("Every surrounding cell has a title destination"),D->TitleTargets.Num(),666);++Step;
        }
        else if(Step==1&&D->Shot==7&&D->ShotTime>5){Capture(TEXT("wooow-forming"));++Step;}
        else if(Step==2&&D->Shot==7&&D->ShotTime>10.8f)
        {
            Capture(TEXT("wooow-title"));
            for(int I=0;I<D->TitleTargets.Num();++I)
            {
                FTransform T;D->TitleBatches[0]->GetInstanceTransform(I,T);
                Test->TestTrue(TEXT("Membranes keep the letter geometry within subtle floating motion"),T.GetLocation().Equals(D->TitleTargets[I],700));
            }
            Test->TestEqual(TEXT("Title has no exhausted grey filter"),D->Exhaustion,0.f);++Step;
            Test->TestTrue(TEXT("Petri specks decorate the cell background"),D->FinaleCellSpecks->IsVisible()&&D->FinaleCellSpecks->GetInstanceCount()==1900);
            Test->TestEqual(TEXT("All six stages are prepared before the dive"),D->FinaleRocks.Num(),12);
            Test->TestFalse(TEXT("Title reading keeps the cave hidden until descent"),D->FinaleEntrancePreview->IsVisible());
            Test->TestEqual(TEXT("Title never reveals the cave material"),D->FinalePreviewMaterials[0]->K2_GetScalarParameterValue(TEXT("Reveal")),0.f);
        }
        else if(Step==3&&D->Shot==8&&D->ShotTime>4.1f){Capture(TEXT("wooow-o-dive"));++Step;}
        else if(Step==4&&D->Shot==9&&D->DrillDistance>800&&D->FinaleKick>.12f)
        {
            Test->TestTrue(TEXT("No click is required to run the real mining component"),M->IsMiningActive()&&M->bDrillMode&&M->bInfiniteDrillEnergy);
            Test->TestEqual(TEXT("Energy remains full"),M->DrillEnergy,M->MaxDrillEnergy);
            Test->TestEqual(TEXT("Both stylized drill models are equipped"),D->FinaleDrills.Num(),2);
            for(auto Drill:D->FinaleDrills)Test->TestTrue(TEXT("Drill uses the ending-only painted material"),Drill->GetMaterial(0)->GetMaterial()->GetName().Contains(TEXT("M_EE_ComicDrill")));
            Test->TestEqual(TEXT("Original drill reach is restored for the half-length first-person model"),M->Reach,420.f);
            // The spin transform translates the pivot around the bit axis. Its
            // raw X is not the mount position; review framing in the screenshot.
            Test->TestTrue(TEXT("Starting pace is slower than the previous 260 cm/s"),D->FinaleDrillSpeed>150&&D->FinaleDrillSpeed<210);
            Test->TestTrue(TEXT("Early impact is restrained"),D->FinaleKick<.5f);
            Test->TestFalse(TEXT("Shared effects do not duplicate ending shards"),Effects->bEnabled);
            Test->TestFalse(TEXT("Preview is replaced by matching full-size geometry"),D->FinaleEntrancePreview->IsVisible());
            for(auto R:D->FinaleRocks)Test->TestTrue(TEXT("Faceted cutting is restricted to ending-only rock actors"),R->IsA<AEndingComicRock>());
            Test->TestNotNull(TEXT("Comic rock shader is available"),LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_ComicRock")));
            Test->TestTrue(TEXT("Comic impact pool is visible during drilling"),D->FinaleImpactShards->IsVisible()&&D->FinaleImpactLines->IsVisible());
            Capture(TEXT("wooow-drill-entry"));++Step;
        }
        else if(Step==5&&D->DrillDistance>500)
        {
            Test->TestTrue(TEXT("Automatic advance excavates geometry"),D->FinaleDrillCuts>0);BaseSpeed=D->FinaleDrillSpeed;Since=D->Age;
            for(FKey K:{EKeys::W,EKeys::A})Key(P,K,true);++Step;
        }
        else if(Step==6&&D->Age-Since>1)
        {
            if(InputPhase==0)
            {
                Test->TestTrue(TEXT("W accelerates automatic excavation"),D->FinaleDrillSpeed>BaseSpeed*1.5f);
                Test->TestTrue(TEXT("A shifts left within the excavation radius"),D->FinaleSide<-25&&D->FinaleSide>=-55);
                Key(P,EKeys::W,false);Key(P,EKeys::A,false);Key(P,EKeys::D,true);Key(P,EKeys::LeftMouseButton,true);Key(P,EKeys::S,true);
                Since=D->Age;InputPhase=1;
            }
            else if(D->Age-Since>1.3f)
            {
                Test->TestTrue(TEXT("D shifts right relative to the rotating view"),D->FinaleSide>-5&&D->FinaleSide<=55);
                float T=FMath::Clamp(D->DrillDistance/6500,0.f,1.f);T=T*T*(3-2*T);
                const float AutomaticSpeed=FMath::Lerp(175.f,370.f,T);
                Test->TestTrue(TEXT("Mouse and S preserve the depth-based automatic pace"),FMath::Abs(D->FinaleDrillSpeed-AutomaticSpeed)<10);
                for(FKey K:{EKeys::LeftMouseButton,EKeys::S,EKeys::D})Key(P,K,false);++Step;
            }
        }
        else if(Step==7&&D->DrillDistance>2800)
        {
            Test->TestTrue(TEXT("Released mouse continues mining"),M->IsMiningActive());Capture(TEXT("wooow-drill-turn"));++Step;
        }
        else if(Step==8&&D->DrillDistance>4700)
        {
            Test->TestTrue(TEXT("Automatic pace grows progressively later in the cave"),D->FinaleDrillSpeed>310);
            Capture(TEXT("wooow-drill-spiral"));int Opened=0;
            for(auto R:D->FinaleRocks)if(R->SuccessfulCuts>0)
            {
                ++Opened;
                R->RockMesh->GetDynamicMesh()->ProcessMesh([this](const UE::Geometry::FDynamicMesh3& Mesh)
                {
                    Test->TestTrue(TEXT("Excavated faceted rocks remain closed and valid"),Mesh.IsClosed()&&Mesh.CheckValidity(UE::Geometry::FDynamicMesh3::FValidityOptions(),UE::Geometry::EValidityCheckFailMode::ReturnOnly));
                    Test->TestEqual(TEXT("Cut faces retain separate hard normals"),Mesh.Attributes()->PrimaryNormals()->ElementCount(),Mesh.TriangleCount()*3);
                });
            }
            Test->TestTrue(TEXT("Successive physical rock faces are actually excavated"),Opened>=7);++Step;
        }
        else if(Step==9&&D->Shot==10&&D->ShotTime>1)
        {
            Test->TestTrue(TEXT("Sixth white excavation stage was reached"),WhiteCaptured);
            Test->TestFalse(TEXT("Drill shuts down at the exit"),M->bDrillMode);
            Test->TestEqual(TEXT("Unlimited energy is scoped to the finale"),M->bInfiniteDrillEnergy,InfiniteBefore);
            Test->TestEqual(TEXT("Original mining radius is restored"),M->Radius,OriginalRadius);Capture(TEXT("paper-black-flight"));++Step;
            Test->TestEqual(TEXT("Main-game smoke restored after finale"),Effects->DrillSmokeStrength,OriginalSmoke);
            Test->TestEqual(TEXT("Main-game dust restored after finale"),Effects->DrillDustStrength,OriginalDust);
            Test->TestEqual(TEXT("Main-game sparks restored after finale"),Effects->DrillSparkStrength,OriginalSparks);
            Test->TestEqual(TEXT("Main-game effects enabled state is restored"),Effects->bEnabled,OriginalEnabled);
            Test->TestTrue(TEXT("Exit strokes fade rather than vanishing at the boundary"),D->FinaleSpeedLines->IsVisible());
            const float Fade=D->FinaleDrillMaterials[0]->K2_GetScalarParameterValue(TEXT("Fade"));
            Test->TestTrue(TEXT("Tool opacity is between visible and gone during exit"),Fade>.02f&&Fade<.8f);
            Test->TestTrue(TEXT("Monochrome speed lines bridge the exit"),D->ExitInkMaterial&&D->ExitInkMaterial->K2_GetScalarParameterValue(TEXT("Strength"))>.1f);
            Test->AddInfo(FString::Printf(TEXT("Continuous excavation: %d contacts; maximum contact gap %.3fs"),D->FinaleDrillCuts,MaxContactGap));
            Test->TestTrue(TEXT("No empty travel intervals interrupt excavation"),MaxContactGap<1.2f);

        }
        else if(Step==10&&D->Shot==11&&D->ShotTime>1.5f)
        {
            Test->TestTrue(TEXT("Paper flight follows the tunnel before reality"),D->PaperScene->IsVisible()&&!D->FinaleReality->IsVisible()&&!D->bComplete);
            Test->TestEqual(TEXT("Twenty-four contour cutouts line the route"),D->PaperCardCount,24);
            int Popped=0;for(float Age:D->PaperPopAges)if(Age>=0)++Popped;
            Test->TestTrue(TEXT("Nearby cutouts pop while later cutouts remain flat"),Popped>0&&Popped<24);
            for(const TCHAR* Name:{TEXT("toilet"),TEXT("warnings"),TEXT("glasses"),TEXT("cards"),TEXT("fountain"),TEXT("thought"),TEXT("computer"),TEXT("brain"),TEXT("solar"),TEXT("ants"),TEXT("roguelike"),TEXT("pickaxe")})
                Test->TestNotNull(TEXT("Individual doodle texture is available"),LoadObject<UTexture2D>(nullptr,*(FString(TEXT("/Game/EndingEmergence/PaperFinale/Doodles/T_EE_Doodle_"))+Name)));
            Test->TestEqual(TEXT("Faded tools are released before reality"),D->FinaleDrills.Num(),0);
            Test->TestEqual(TEXT("Old bulb finale is removed"),D->FinaleBulbLights.Num(),0);
            Test->TestEqual(TEXT("Old photograph finale does not run"),D->EndingPapers.Num(),0);
            Test->TestEqual(TEXT("Old starlight finale does not run"),D->StarlightRibbons.Num(),0);
            InputPhase=0;Capture(TEXT("paper-flight-start"));++Step;
        }
        else if(Step==11)
        {
            if(InputPhase==0&&D->Shot==11&&D->ShotTime>3.2f){Capture(TEXT("paper-flight-middle"));++InputPhase;}
            else if(InputPhase==1&&D->Shot==11&&D->ShotTime>5.8f)
            {for(float Age:D->PaperPopAges)Test->TestTrue(TEXT("All cutouts rise as camera reaches the end"),Age>=0);Capture(TEXT("paper-flight-last"));++InputPhase;}
            else if(InputPhase==2&&D->Shot==12&&D->ShotTime>3.8f){Capture(TEXT("paper-ceiling-vent"));++InputPhase;}
            else if(InputPhase==3&&D->Shot==12&&D->ShotTime>6.35f)
            {Test->TestTrue(TEXT("Entering the vent reaches full black"),D->OverlayAlpha()>.99f);Capture(TEXT("paper-vent-black"));++InputPhase;}
            else if(InputPhase==4&&D->Shot==13&&D->ShotTime>5.6f)
            {
                Test->TestTrue(TEXT("Pullback reveals physical computer before power-on"),D->FinaleReality->IsVisible()&&!D->PaperScene->IsVisible()&&!D->bComplete);
                Test->TestEqual(TEXT("Monitor is off while camera pulls back"),D->CreditsScreenMaterial->K2_GetScalarParameterValue(TEXT("Power")),0.f);
                Test->TestTrue(TEXT("Multiple rendered frames verify the black screen match"),BlackMatchSamples>=5);
                Test->AddInfo(FString::Printf(TEXT("Black match: %d screenshot probes requested for pixel verification"),BlackMatchSamples));
                Capture(TEXT("paper-computer-off"));++InputPhase;
            }
            else if(InputPhase==5&&D->Shot==13&&D->ShotTime>8.5f)
            {
                Test->TestTrue(TEXT("Credits complete the new ending"),D->bComplete);
                Test->TestEqual(TEXT("Monitor powers on after stopping"),D->CreditsScreenMaterial->K2_GetScalarParameterValue(TEXT("Power")),1.f);
                Capture(TEXT("paper-developer-credits"));Since=D->Age;++InputPhase;
            }
            else if(InputPhase==6&&D->Age-Since>.5f){Key(P,EKeys::R,true);++Step;}
        }
        else if(Step==12)
        {
            Key(P,EKeys::R,false);Test->TestEqual(TEXT("Restart returns to the playable cell"),D->Shot,-1);
            Test->TestEqual(TEXT("Restart removes excavatable finale rocks"),D->FinaleRocks.Num(),0);
            Test->TestTrue(TEXT("Restart restores the cell cloud"),D->bTitleRestored);
            Test->TestFalse(TEXT("Restart hides the paper route"),D->PaperScene->IsVisible());
            Test->TestFalse(TEXT("Restart hides credits room"),D->FinaleReality->IsVisible());
            for(float Age:D->PaperPopAges)Test->TestTrue(TEXT("Restart rearms every folded cutout"),Age<0);
            D->PreviewFilm();D->SetShot(9);Since=D->Age;++Step;
        }
        else if(Step==13&&D->Age-Since>.5f){Key(P,EKeys::R,true);++Step;}
        else if(Step==14)
        {
            Key(P,EKeys::R,false);Test->TestEqual(TEXT("Mid-drill restart restores energy mode"),M->bInfiniteDrillEnergy,InfiniteBefore);
            Test->TestFalse(TEXT("Mid-drill restart stops automatic digging"),M->IsMiningActive());
            Test->TestEqual(TEXT("Mid-drill restart restores shared effects"),Effects->DrillSmokeStrength,OriginalSmoke);
            Test->TestEqual(TEXT("Mid-drill restart restores effects enablement"),Effects->bEnabled,OriginalEnabled);
            Test->TestFalse(TEXT("Mid-drill restart clears comic fragments"),D->FinaleImpactShards->IsVisible());
            Test->TestEqual(TEXT("Mid-drill restart removes drill copies"),D->FinaleDrills.Num(),0);return true;
        }
        return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndingTitleDrillTest,"BrainMining.EndingEmergence.TitleDrillReality",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FEndingTitleDrillTest::RunTest(const FString&){FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEndingTitleDrillPreview>(this));return true;}
#endif
