#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "MiningCellUniverse.h"
#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "DiggableRock.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Editor.h"
#include "UnrealClient.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "PlayInEditorDataTypes.h"

namespace
{
class FStartCellUniversePIE : public IAutomationLatentCommand
{
public:
    virtual bool Update() override
    {
        FRequestPlaySessionParams Params;
        auto* Settings=NewObject<ULevelEditorPlaySettings>(GetTransientPackage(),NAME_None,RF_Transient);
        Settings->NewWindowWidth=1280; Settings->NewWindowHeight=720;
        Params.EditorPlaySettings=Settings; Params.bAllowOnlineSubsystem=false;
        GEditor->RequestPlaySession(Params); return true;
    }
};
class FCellUniverseWalk : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Step=0, LastStage=-1;
    double Since=FPlatformTime::Seconds(), Started=Since, CaptureUntil=0;
    FVector Start;
    float FlightHeight=0, InitialYaw=0;
    double FlyingMineAt=0;
    bool bShot=false;
    void Next() { ++Step; Since=FPlatformTime::Seconds(); }
    void Key(APlayerController* PC,FKey K,bool Down)
    { PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f)); }
    void Shot(const FString& Name)
    {
        const FString Dir=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("CellUniversePreview"));
        IFileManager::Get().MakeDirectory(*Dir,true);
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(Dir,Name+TEXT(".png")),true,false);
    }
public:
    explicit FCellUniverseWalk(FAutomationTestBase* InTest):Test(InTest) {}
    virtual bool Update() override
    {
        const double Now=FPlatformTime::Seconds(),Age=Now-Since;
        if(Now-Started>480 || (Step!=9 && Age>30))
        { Test->AddError(FString::Printf(TEXT("Cell universe traversal timeout: step %d, stage %d"),Step,LastStage)); return true; }
        UWorld* W=GEditor?GEditor->PlayWorld:nullptr;
        auto* PC=W?W->GetFirstPlayerController():nullptr;
        auto* P=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
        if(!P) return false;
        AMiningCellUniverse* D=nullptr;
        for(TActorIterator<AMiningCellUniverse> It(W);It;++It) { D=*It; break; }
        auto* M=P->FindComponentByClass<UMiningComponent>();
        auto* Move=P->FindComponentByClass<UMiningCharacterMovementComponent>();
        if(!D||!M||!Move) { Test->AddError(TEXT("Original mining/flight or demo missing")); return true; }
        switch(Step)
        {
        case 0:
            if(Age<2 || !W->HasBegunPlay() || W->GetTimeSeconds()<2) return false;
            Test->TestEqual(TEXT("Original Blueprint pawn reused"),P->GetClass()->GetName(),FString(TEXT("BP_FirstPersonCharacter_C")));
            Test->TestEqual(TEXT("Eleven saved diggable boundaries"),D->Gates.Num(),D->StageCount);
            Test->TestEqual(TEXT("Tissue added in sequence"),D->BuiltStages[2].Name,FString(TEXT("组织")));
            Test->TestEqual(TEXT("Continent added in sequence"),D->BuiltStages[4].Name,FString(TEXT("大陆")));
            Test->TestEqual(TEXT("Galaxy distinct from universe"),D->BuiltStages[6].Name,FString(TEXT("星系")));
            Test->TestTrue(TEXT("Universe route much longer than cell"),D->BuiltStages[7].Length>D->BuiltStages[0].Length*5);
            Test->TestTrue(TEXT("Small membrane is a localized patch"),D->Gates[0]->RockSize.Z<400 && D->Gates[0]->RockSize.Y<500);
            Test->TestTrue(TEXT("Cosmic opening is much larger"),D->Gates[7]->RockSize.Y>D->Gates[0]->RockSize.Y*8);
            Test->TestTrue(TEXT("Distant planet keeps its immense size"),D->BuiltStages[5].EndScale==1.f);
            Test->TestTrue(TEXT("Underpath flight does not reset at old 20m threshold"),M->FallResetDistance>D->BuiltStages[7].BelowPathDepth);
            {
                FHitResult Hit;
                FCollisionQueryParams Q(SCENE_QUERY_STAT(SuspendedRoute),true,P);
                const bool Ground=W->LineTraceSingleByChannel(Hit,FVector(1200,900,50),FVector(1200,900,-2000),ECC_Visibility,Q);
                Test->TestTrue(TEXT("Off-path space really extends below the bridge"),Ground && Hit.ImpactPoint.Z < -1000);
                const auto& S=D->BuiltStages[0]; const float GX=D->Gates[0]->GetActorLocation().X;
                for(const float Z:{-500.f,S.OpeningHeight+160.f})
                    Test->TestTrue(TEXT("Boundary cannot be bypassed above or below the patch"),W->LineTraceSingleByChannel(Hit,FVector(GX-500,0,Z),FVector(GX+500,0,Z),ECC_Visibility,Q));
            }
            Test->TestTrue(TEXT("Existing flight enabled"),Move->IsFlightAvailable());
            Start=P->GetActorLocation(); InitialYaw=PC->GetControlRotation().Yaw;
            Shot(TEXT("01-cell")); Key(PC,EKeys::W,true); Next(); break;
        case 1:
            // The first screenshot can stall several frames on a cold shader cache.
            if(Age<.65 || (P->GetActorLocation().X<=Start.X+100 && Age<3)) return false;
            Test->TestTrue(TEXT("Real W mapping walks"),P->GetActorLocation().X>Start.X+100);
            Key(PC,EKeys::W,false);
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseX,IE_Axis,30,1)); Next(); break;
        case 2:
            if(Age<.35) return false;
            Test->TestTrue(TEXT("Real mouse mapping rotates view"),FMath::Abs(PC->GetControlRotation().Yaw-InitialYaw)>.1);
            PC->SetControlRotation(FRotator::ZeroRotator); FlightHeight=P->GetActorLocation().Z;
            Key(PC,EKeys::SpaceBar,true); Next(); break;
        case 3:
            if(Age<.85) return false;
            Test->TestTrue(TEXT("Holding space rises"),P->GetActorLocation().Z>FlightHeight+100);
            FlightHeight=P->GetActorLocation().Z; Key(PC,EKeys::SpaceBar,false); Next(); break;
        case 4:
            if(Age<1.0) return false;
            Test->TestTrue(TEXT("Releasing space coasts then falls"),Move->Velocity.Z<0 && P->GetActorLocation().Z<FlightHeight);
            Key(PC,EKeys::W,true); Next(); break;
        case 5:
            if(Age<7) return false;
            Test->TestTrue(TEXT("Uncut membrane blocks walking"),P->GetActorLocation().X<D->Gates[0]->GetActorLocation().X-D->Gates[0]->RockSize.X*.5);
            Test->TestEqual(TEXT("Standing at membrane cannot advance"),D->CurrentStage,0);
            Test->TestEqual(TEXT("No fake cuts from proximity"),D->Gates[0]->SuccessfulCuts,0);
            Key(PC,EKeys::W,false); Key(PC,EKeys::SpaceBar,true); Next(); break;
        case 6:
            if(Age<3.8) return false;
            Test->TestTrue(TEXT("Flight ceiling preserves collision"),P->GetActorLocation().Z<1420);
            Key(PC,EKeys::SpaceBar,false); Next(); break;
        case 7:
            if(FlyingMineAt==0)
            {
                if(P->GetActorLocation().Z>140) return false;
                Key(PC,EKeys::SpaceBar,true); Key(PC,EKeys::LeftMouseButton,true); FlyingMineAt=Now;
                return false;
            }
            if(Now-FlyingMineAt<.2) return false;
            Test->TestTrue(TEXT("Original left mouse mines while flying"),D->Gates[0]->SuccessfulCuts>0);
            Key(PC,EKeys::R,true); Next(); break;
        case 8:
            if(Age<.25) return false;
            Test->TestEqual(TEXT("R restores first membrane geometry"),D->Gates[0]->SuccessfulCuts,0);
            Test->TestEqual(TEXT("R restores stage presentation"),D->CurrentStage,0);
            Test->TestFalse(TEXT("R clears held flight"),Move->bFlightInputHeld);
            Test->TestTrue(TEXT("R restores spawn"),FVector::Dist2D(P->GetActorLocation(),Start)<10);
            Key(PC,EKeys::R,false); Key(PC,EKeys::SpaceBar,false); Key(PC,EKeys::LeftMouseButton,false);
            Key(PC,EKeys::W,true); Key(PC,EKeys::LeftMouseButton,true); Next(); break;
        case 9:
            // Walk the entire route using real input, real boolean holes and capsule collision.
            // No teleporting, collision disabling or direct stage manipulation is used.
            if(D->CurrentStage!=LastStage)
            {
                if(LastStage>=0)
                {
                    Test->TestEqual(TEXT("Stages advance in order"),D->CurrentStage,LastStage+1);
                    Test->TestTrue(TEXT("Previous boundary actually excavated"),D->Gates[LastStage]->SuccessfulCuts>0);
                }
                LastStage=D->CurrentStage; CaptureUntil=Now+1.2; bShot=false;
                Key(PC,EKeys::W,false); Key(PC,EKeys::LeftMouseButton,false);
                PC->SetControlRotation(FRotator(LastStage==5?-10:5,LastStage>=4&&LastStage<=7?38:15,0));
            }
            if(CaptureUntil>0)
            {
                if(Now<CaptureUntil) return false;
                if(!bShot) { Shot(FString::Printf(TEXT("stage-%02d"),LastStage+1)); bShot=true; CaptureUntil=Now+.3; return false; }
                PC->SetControlRotation(FRotator::ZeroRotator);
                Key(PC,EKeys::W,true); Key(PC,EKeys::LeftMouseButton,true); CaptureUntil=0;
            }
            if(!D->bComplete) return false;
            Key(PC,EKeys::W,false); Key(PC,EKeys::LeftMouseButton,false);
            Test->TestEqual(TEXT("All eleven stages reached"),D->CurrentStage,D->MouthStage);
            Test->TestTrue(TEXT("Universe membrane actually mined"),D->MembraneCuts>0);
            for(int32 I=0;I<D->StageCount;++I)
                Test->TestTrue(FString::Printf(TEXT("Gate %d excavated"),I),D->Gates[I]->SuccessfulCuts>0);
            Shot(TEXT("12-complete")); Next(); break;
        case 10:
            if(Age<.5) return false;
            Key(PC,EKeys::R,true); Next(); break;
        case 11:
            if(Age<.4) return false;
            Test->TestFalse(TEXT("Replay clears completion"),D->bComplete);
            Test->TestEqual(TEXT("Replay clears stage"),D->CurrentStage,0);
            Test->TestEqual(TEXT("Replay clears universe reveal"),D->MembraneCuts,0);
            for(int32 I=0;I<D->StageCount;++I) Test->TestEqual(FString::Printf(TEXT("Replay restores gate %d"),I),D->Gates[I]->SuccessfulCuts,0);
            Key(PC,EKeys::R,false); Key(PC,EKeys::D,true); Next(); break;
        case 12:
            if(Age<.9) return false;
            Key(PC,EKeys::D,false); Next(); break;
        case 13:
            if(Age<1.4) return false;
            Test->TestTrue(TEXT("Walking off the bridge descends into authored lower space"),P->GetActorLocation().Z < -300);
            Test->TestTrue(TEXT("Descent did not reset player to spawn"),P->GetActorLocation().Y>350);
            PC->SetControlRotation(FRotator(-24,0,0)); Shot(TEXT("13-below-cell"));
            Key(PC,EKeys::SpaceBar,true); Next(); break;
        case 14:
            if(P->GetActorLocation().Z<220) return false;
            Test->TestTrue(TEXT("Original flight climbs back from beneath bridge"),Move->Velocity.Z>0);
            Key(PC,EKeys::SpaceBar,false); Key(PC,EKeys::R,true); Next(); break;
        case 15:
            if(Age<.3) return false;
            Test->TestTrue(TEXT("Reset recovers from under-bridge exploration"),FVector::Dist2D(P->GetActorLocation(),Start)<10);
            Key(PC,EKeys::R,false); return true;
        }
        return false;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCellUniversePIETest,"BrainMining.CellUniverse.FullFirstPersonRoute",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCellUniversePIETest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/CellUniverse/Maps/Lvl_CellUniverse_Demo"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStartCellUniversePIE>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCellUniverseWalk>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

// Separate presentation checks: poses are set directly here, never in the walking test above.
namespace
{
class FCellUniverseVistas : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Pose=0;
    bool bPlaced=false;
    double Since=FPlatformTime::Seconds();
public:
    explicit FCellUniverseVistas(FAutomationTestBase* InTest):Test(InTest) {}
    virtual bool Update() override
    {
        if(FPlatformTime::Seconds()-Since>30) {Test->AddError(TEXT("Vista capture timed out")); return true;}
        UWorld* W=GEditor?GEditor->PlayWorld:nullptr;
        auto* PC=W?W->GetFirstPlayerController():nullptr;
        auto* P=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
        if(!P) return false;
        AMiningCellUniverse* D=nullptr;
        for(TActorIterator<AMiningCellUniverse> It(W);It;++It) {D=*It;break;}
        if(!D||!D->GetMining()) return false;
        // Let PIE finish its initial possession/restart before setting a capture pose.
        if(W->GetTimeSeconds()<2.5f) return false;
        if(Pose>=4) return true;
        const int32 Stage=Pose==3?0:5+Pose;
        if(!bPlaced)
        {
            P->SetActorLocation(FVector(D->GetStageStart(Stage)+1200,0,100),false,nullptr,ETeleportType::TeleportPhysics);
            PC->SetControlRotation(FRotator(Pose==0?-12:Pose==1?15:Pose==2?-12:0,Pose==3?15:38,0));
            D->CurrentStage=Stage; bPlaced=true; Since=FPlatformTime::Seconds(); return false;
        }
        if(FPlatformTime::Seconds()-Since<1.5) return false;
        TArray<USceneComponent*> Components; D->GetComponents(Components);
        int32 Checked=0;
        for(auto* C:Components)
        {
            const FString Name=C->GetName();
            if(Name.StartsWith(TEXT("PlanetLandmark")))
            {
                ++Checked;
                Test->TestEqual(TEXT("Giant planet only visible in its own section"),C->IsVisible(),Stage==5);
                Test->TestTrue(TEXT("Planet does not shrink"),FMath::IsNearlyEqual(C->GetRelativeScale3D().X,1.0));
            }
            if(Name.StartsWith(TEXT("GalaxyLandmark"))) {++Checked; Test->TestEqual(TEXT("Galaxy revealed without planet occlusion"),C->IsVisible(),Stage==6);}
            if(Name.StartsWith(TEXT("UniverseLandmark"))) {++Checked; Test->TestEqual(TEXT("Universe revealed in sequence"),C->IsVisible(),Stage==7);}
            if(Name.StartsWith(TEXT("OceanAndContinents"))) {++Checked; Test->TestTrue(TEXT("Planet diameter remains 840m"),C->GetComponentScale().X>=839);}
        }
        Test->TestEqual(TEXT("All saved hero components found"),Checked,4);
        const FString Dir=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("CellUniversePreview"));
        IFileManager::Get().MakeDirectory(*Dir,true);
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(Dir,FString::Printf(TEXT("V02-vista-%02d.png"),Stage+1)),true,false);
        ++Pose; bPlaced=false; return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCellUniverseVistaTest,"BrainMining.CellUniverse.ScaleVistas",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCellUniverseVistaTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/CellUniverse/Maps/Lvl_CellUniverse_Demo"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStartCellUniversePIE>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCellUniverseVistas>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
