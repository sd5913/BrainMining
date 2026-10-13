#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "EndingEmergence.h"
#include "DiggableRock.h"
#include "MiningComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
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
    float ReleaseSpeed=0,EarlyRate=0,ReleaseRate=0,LastRadius=0,BrainMin=10000,BrainMax=-10000,TakeoffZ=0;bool Captured=false,BlackSeen=false,EyeSeen=false,BloodCaptured=false,BrainCaptured=false,AscentSeen=false,FadeSeen=false;
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
        if(Now-Started>220){Capture(TEXT("failure-timeout"));Test->AddError(FString::Printf(TEXT("Ending route timeout, step %d, gate %d, shot %d"),Step,LastGate,LastShot));return true;}
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
            Test->TestEqual(TEXT("Saved five actual rock boundaries"),D->Gates.Num(),5);
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
                for(int I=0;I<5;++I)Test->TestTrue(FString::Printf(TEXT("Gate %d really excavated"),I),D->Gates[I]->SuccessfulCuts>0);
                Test->TestTrue(TEXT("Pawn movement suspended in film"),Pawn->GetCharacterMovement()->MovementMode==MOVE_None);
                Test->TestTrue(TEXT("Mining suspended in film"),Mining->IsInputSuspended());
                Test->TestTrue(TEXT("Holding W during the light bloom cannot fall off the route"),Pawn->GetActorLocation().Z>0&&Pawn->GetActorLocation().X<10400);
                Test->TestTrue(TEXT("Brain traversal follows a genuinely curved floor"),BrainMax-BrainMin>200);
                Test->TestTrue(TEXT("Camera rises before leaving the brain"),AscentSeen);
                Test->TestTrue(TEXT("Ascent overlaps the gradual transition"),FadeSeen);
                Test->TestTrue(TEXT("Blood keeps flowing forward throughout the route"),D->BloodTravel>10000);
                D->FilmSpeed=4;Next();break;
            }
            if(D->GateIndex<5)
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
            if(Pawn->GetActorLocation().X>3000&&!BloodCaptured){Capture(TEXT("blood-cylinder"));BloodCaptured=true;}
            if(Pawn->GetActorLocation().X>6500&&Pawn->GetActorLocation().X<10000)
            {
                BrainMin=FMath::Min(BrainMin,float(Pawn->GetActorLocation().Z));BrainMax=FMath::Max(BrainMax,float(Pawn->GetActorLocation().Z));
                if(Pawn->GetActorLocation().X>8000&&!BrainCaptured){Capture(TEXT("brain-surface"));BrainCaptured=true;}
            }
            if(D->GateIndex==5)
            {
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
            if(D->Shot==2&&BoostStep==2){D->FilmSpeed=1;Key(PC,EKeys::S,true);DriftAt=Now;BoostStep=3;}
            if(D->Shot==2&&BoostStep==3&&Now-DriftAt>1)
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
                if(DriftStep==0){D->FilmSpeed=1;Key(PC,EKeys::S,true);Key(PC,EKeys::D,true);DriftAt=Now;DriftStep=1;}
                else if(DriftStep==1&&Now-DriftAt>1)
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
            if(D->Shot==7){BlackSeen=true;Test->TestEqual(TEXT("Hard blackout fully covers the frame"),D->OverlayAlpha(),1.f);}
            if(D->Shot==8){EyeSeen=true;Test->TestTrue(TEXT("Eye follows the blackout"),BlackSeen);}
            if(D->bComplete)
            {
                Test->TestEqual(TEXT("Automatic ending reaches developers"),D->Shot,10);
                Test->TestTrue(TEXT("Blackout then eye close-up played"),BlackSeen&&EyeSeen);
                Test->TestEqual(TEXT("Requested final line"),D->Subtitle(),FString(TEXT("“欸，要不，我们就做挖！”")));
                Test->TestEqual(TEXT("Scale-dependent input checks completed"),BoostStep,4);
                Test->TestEqual(TEXT("Space steering and release checks completed"),DriftStep,5);
                Capture(TEXT("14-afternoon"));Key(PC,EKeys::LeftMouseButton,false);Next();
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
            Test->TestFalse(TEXT("R clears completion"),D->bComplete);
            Test->TestFalse(TEXT("R restores mining input"),Mining->IsInputSuspended());
            Test->TestEqual(TEXT("R restores view target"),PC->GetViewTarget(),static_cast<AActor*>(Pawn));
            for(int I=0;I<5;++I)Test->TestEqual(TEXT("R restores real rock geometry"),D->Gates[I]->SuccessfulCuts,0);
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
