#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "MiningEncounterPlayer.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningComponent.h"
#include "MiningGiantRoomEntrance.h"
#include "EngineUtils.h"
#include "EchoPassagePlayer.h"
#include "EchoPassageStage.h"
#include "EchoPassageLayout.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Editor.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Components/StaticMeshComponent.h"

namespace
{
class FEncounterPlaythrough : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Step=0;
    double Since=FPlatformTime::Seconds();
    FVector Start;
    float InitialYaw=0;
    bool bPresentationCaptured=false;
    bool bHighSpawnRegression=false,bHighSpawnInitialized=false;
    void Next() {++Step;Since=FPlatformTime::Seconds();}
    void Key(APlayerController* PC,FKey K,bool Down)
    {PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f));}
public:
    explicit FEncounterPlaythrough(FAutomationTestBase* InTest,bool bHighSpawn=false):Test(InTest),bHighSpawnRegression(bHighSpawn){}
    virtual bool Update() override
    {
        const double Age=FPlatformTime::Seconds()-Since;
        if(Age>20) {Test->AddError(FString::Printf(TEXT("PIE playthrough timeout at step %d"),Step));return true;}
        UWorld* World=GEditor?GEditor->PlayWorld:nullptr;if(!World)return false;
        auto* PC=Cast<AMiningEncounterPlayerController>(World->GetFirstPlayerController());
        auto* P=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;if(!P)return false;
        auto* Echo=PC->EchoInteraction.Get();auto* Card=PC->CardInteraction.Get();
        auto* Move=Cast<UMiningCharacterMovementComponent>(P->GetCharacterMovement());
        if(!Move) {Test->AddError(TEXT("Main character movement missing"));return true;}
        switch(Step)
        {
        case 0:
            if(Age<1 || !Echo->Stage || !Card->Stage)return false;
            if(bHighSpawnRegression && !bHighSpawnInitialized)
            {
                // BeginPlay must really cache a high spawn, as with the desk-side preview.
                // Moving an existing pawn up would not reproduce the safety-reset bug.
                FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                auto* HighPawn=World->SpawnActor<ACharacter>(P->GetClass(),Card->Stage->GetApproachLocation()+FVector(0,0,100),FRotator::ZeroRotator,Params);
                if(!HighPawn){Test->AddError(TEXT("Could not create high-spawn regression pawn"));return true;}
                HighPawn->FindComponentByClass<UMiningComponent>()->FallResetDistance=2300.f;
                PC->Possess(HighPawn);P->Destroy();
                bHighSpawnInitialized=true;Echo->Restart();Since=FPlatformTime::Seconds();return false;
            }
            Test->TestEqual(TEXT("PIE uses original Blueprint pawn"),P->GetClass()->GetName(),FString(TEXT("BP_FirstPersonCharacter_C")));
            Test->TestTrue(TEXT("Original flight enabled in the passage"),Move->IsFlightAvailable());
            Test->TestTrue(TEXT("Mining blocked in echo"),P->FindComponentByClass<UMiningComponent>()->IsEncounterBlocked());
            Start=P->GetActorLocation();Key(PC,EKeys::W,true);Next();break;
        case 1:
            if(!Echo->Stage->bCollapsed)return false;
            Test->TestTrue(TEXT("Real W mapping walked to trap"),P->GetActorLocation().X-Start.X>250);
            Key(PC,EKeys::W,false);Next();break;
        case 2:
            if(!Echo->Stage->bLanded)return false;
            Test->TestFalse(TEXT("Landing restores move input"),PC->IsMoveInputIgnored());
            InitialYaw=PC->GetControlRotation().Yaw;
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseX,IE_Axis,40,1));Next();break;
        case 3:
            if(Age<.85)return false;
            Test->TestTrue(TEXT("Original mouse mapping rotates camera"),FMath::Abs(PC->GetControlRotation().Yaw-InitialYaw)>.1f);
            Key(PC,EKeys::LeftMouseButton,true);Next();break;
        case 4:
            if(Age<1.1)return false;
            Test->TestEqual(TEXT("Holding left mouse causes exactly one strike"),Echo->Stage->KnockCount,1);
            Key(PC,EKeys::LeftMouseButton,false);Next();break;
        case 5:
            if(Age<.15)return false;
            Key(PC,EKeys::LeftMouseButton,true);Next();break;
        case 6:
            if(Age<.4)return false;
            Test->TestEqual(TEXT("A second click causes second strike"),Echo->Stage->KnockCount,2);
            Key(PC,EKeys::LeftMouseButton,false);Key(PC,EKeys::R,true);Next();break;
        case 7:
            if(Age<.3)return false;
            Test->TestFalse(TEXT("R resets collapse"),Echo->Stage->bCollapsed);
            Test->TestEqual(TEXT("R clears echoes"),Echo->Stage->KnockCount,0);
            Test->TestTrue(TEXT("R returns to entry"),P->GetActorLocation().Z>900);
            Key(PC,EKeys::R,false);
            // Move only the test setup to the real diggable wall; no world travel is permitted.
            Echo->Stage->bLanded=true;
            P->SetActorLocation(FVector(6000,3680,100),false,nullptr,ETeleportType::TeleportPhysics);
            PC->SetControlRotation(FRotator(0,0,0));Next();break;
        case 8:
            if(Age<1)return false;
            Test->TestTrue(TEXT("Both encounters remain in the same world"),Card->Stage && Echo->Stage && Echo->Stage->bRoomInSameLevel);
            Test->TestFalse(TEXT("No map travel pending"),Echo->bChangingLevel);
            Test->TestTrue(TEXT("Mining available at the breach"),PC->EncounterPolicy->IsMiningAllowed());
            {
                const bool AtWall=FMath::Abs(P->GetActorLocation().X-6000)<100 && P->GetActorLocation().Z<400;
                Test->TestTrue(TEXT("Enabling mining at the breach must not teleport to the spawn/table"),AtWall);
                if(!AtWall)return true;
            }
            Test->TestTrue(TEXT("Arrival at the breach does not auto-seat"),Card->Stage->Run.Phase==ECardRoomPhase::Exploring);
            Key(PC,EKeys::LeftMouseButton,true);Next();break;
        case 9:
            if(Age<.8)return false;
            {
                AMiningGiantRoomEntrance* Wall=nullptr;
                for(TActorIterator<AMiningGiantRoomEntrance> It(World);It;++It){Wall=*It;break;}
                Test->TestTrue(TEXT("Left mouse excavates the actual entrance mesh"),Wall && Wall->SuccessfulCuts>0);
            }
            Key(PC,EKeys::LeftMouseButton,false);
            Test->TestTrue(TEXT("Digging preserves the player's location at the breach"),FMath::Abs(P->GetActorLocation().X-6000)<100 && P->GetActorLocation().Z<400);
            Card->Interact();
            Test->TestTrue(TEXT("E at the breach cannot seat through the wall"),Card->Stage->Run.Phase==ECardRoomPhase::Exploring);
            P->SetActorLocation(Card->Stage->GetApproachLocation()+FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics);
            Next();break;
        case 10:
            if(Age<.4)return false;
            Test->TestTrue(TEXT("Flight remains available in giant room"),Move->IsFlightAvailable());
            Key(PC,EKeys::E,true);Next();break;
        case 11:
            if(Age<.3)return false;
            Test->TestTrue(TEXT("E seats at table"),Card->Stage->Run.Phase==ECardRoomPhase::Seated);
            Test->TestTrue(TEXT("Seat locks movement and view"),PC->IsMoveInputIgnored()&&PC->IsLookInputIgnored());
            Test->TestTrue(TEXT("Seated camera sees table from expected height"),FMath::Abs(P->FindComponentByClass<UCameraComponent>()->GetComponentLocation().Z-Card->Stage->GetSeatEyeLocation().Z)<25);
            Key(PC,EKeys::E,false);Card->Stage->Run.Match();Card->Stage->Run.Tick(10);Card->Stage->Run.Tick(10);Card->Stage->Run.Stage(0);
            Key(PC,EKeys::SpaceBar,true);Next();break;
        case 12:
            if(Age<.9)
            {
                if(Age>.35 && !bPresentationCaptured)
                {
                    FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("CardRoomPreview/GiantBackgroundV03-cat.png")),true,false);
                    bPresentationCaptured=true;
                }
                return false;
            }
            Test->TestTrue(TEXT("Space confirms selected card"),Card->Stage->Run.Phase==ECardRoomPhase::Resolving);
            Test->TestTrue(TEXT("Space does not jump out of seat"),Move->MovementMode==MOVE_None);
            Key(PC,EKeys::SpaceBar,false);Key(PC,EKeys::Q,true);Next();break;
        case 13:
            if(Age<.3)return false;
            Test->TestTrue(TEXT("Q returns to exploration"),Card->Stage->Run.Phase==ECardRoomPhase::Exploring);
            Test->TestFalse(TEXT("Q restores move input"),PC->IsMoveInputIgnored());Test->TestFalse(TEXT("Q restores look input"),PC->IsLookInputIgnored());
            Test->TestTrue(TEXT("Q restores collision"),P->GetActorEnableCollision());Key(PC,EKeys::Q,false);
            Start=P->GetActorLocation();Key(PC,EKeys::S,true);Next();break;
        case 14:
            if(Age<.4)return false;
            Test->TestTrue(TEXT("Original movement works after leaving seat"),FVector::Dist2D(Start,P->GetActorLocation())>30);
            if(FVector::Dist2D(Start,P->GetActorLocation())<=30)
            {
                Test->AddInfo(FString::Printf(TEXT("Movement diagnosis: start=%s end=%s mode=%d velocity=%s"),*Start.ToString(),*P->GetActorLocation().ToString(),int32(Move->MovementMode),*Move->Velocity.ToString()));
                TArray<FOverlapResult> Hits;FCollisionQueryParams Query;Query.AddIgnoredActor(P);
                World->OverlapMultiByChannel(Hits,P->GetActorLocation(),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(60,96),Query);
                for(const auto& Hit:Hits)if(Hit.GetComponent())Test->AddInfo(FString::Printf(TEXT("Nearby collision: %s blocking=%d"),*Hit.GetComponent()->GetPathName(),int32(Hit.bBlockingHit)));
            }
            Key(PC,EKeys::S,false);
            if(bHighSpawnRegression)
            {
                PC->EncounterPolicy->Release();
                Test->TestEqual(TEXT("Leaving encounter policy restores authored mining fall reset"),P->FindComponentByClass<UMiningComponent>()->FallResetDistance,2300.f);
                PC->EncounterPolicy->Apply(P);
            }
            return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterPIETest,"BrainMining.Encounters.PIE.GiantRoomInput",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEncounterPIETest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEncounterPlaythrough>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterHighSpawnTest,"BrainMining.Encounters.PIE.HighSpawnBreachNoTeleport",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEncounterHighSpawnTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEncounterPlaythrough>(this,true));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
// A focused visual playthrough for the giant study. Setup skips navigation; the
// original pawn, encounter controller, rules and camera run normally in PIE.
namespace
{
class FStudyPresentationV08 : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Step=0,Shots=0,LastRound=0;
    double Since=FPlatformTime::Seconds();
    void Next(){++Step;Since=FPlatformTime::Seconds();}
    void Shot(const FString& Name){FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("CardRoomPreview/StudyV08-")+Name+TEXT(".png")),true,false);}
public:
    explicit FStudyPresentationV08(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Since>35){Test->AddError(TEXT("V08 presentation timed out"));return true;}
        UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr;
        auto* PC=W?Cast<AMiningEncounterPlayerController>(W->GetFirstPlayerController()):nullptr;
        APawn* Pawn=PC?PC->GetPawn():nullptr;
        auto* Stage=PC&&PC->CardInteraction?PC->CardInteraction->Stage.Get():nullptr;
        if(!Pawn||!Stage)return false;
        auto Part=[&](const TCHAR* Name)->UStaticMeshComponent*
        {TArray<UStaticMeshComponent*> Parts;Stage->GetComponents(Parts);for(auto* P:Parts)if(P->GetFName()==FName(Name))return P;return nullptr;};
        auto InView=[&](FVector P,float Bottom=.85f)
        {FVector2D Screen;int32 X,Y;PC->GetViewportSize(X,Y);return PC->ProjectWorldLocationToScreen(P,Screen)&&Screen.X>0&&Screen.X<X&&Screen.Y>0&&Screen.Y<Y*Bottom;};
        if(Step==0)
        {
            if(FPlatformTime::Seconds()-Since<1)return false;
            Pawn->SetActorLocation(Stage->GetApproachLocation()+FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics);Next();return false;
        }
        if(Step==1)
        {
            if(FPlatformTime::Seconds()-Since<.4)return false;
            PC->CardInteraction->Interact();
            Test->TestTrue(TEXT("Giant high throw configured"),Stage->CardThrowHeight>100.f);
            Stage->Run.Match();Stage->Run.Tick(10);Stage->Run.Tick(10);Next();return false;
        }
        auto& Run=Stage->Run;
        if(Run.Phase==ECardRoomPhase::Choosing)
        {
            if(Run.Round!=LastRound)
            {
                LastRound=Run.Round;Shots=0;
                if(LastRound==1)
                {
                    auto* Mouth=Part(TEXT("Mouth"));auto* Head=Part(TEXT("RoundHead"));
                    Test->TestTrue(TEXT("Mouth follows the head pivot"),Mouth&&Head&&Mouth->GetAttachParent()==Head->GetAttachParent());
                    Test->TestTrue(TEXT("Mouth remains on the lower face"),Mouth&&Mouth->GetRelativeLocation().Z>-20&&Mouth->GetRelativeLocation().Z<0);
                    Test->TestTrue(TEXT("Bow tie no longer covers muzzle"),Part(TEXT("BowTie"))->GetRelativeScale3D().Y<.35f);
                }
            }
            for(int32 i=0;i<Run.Hand.Num();++i)Run.Stage(i);
            Run.Confirm();return false;
        }
        if(Run.Phase==ECardRoomPhase::Resolving)
        {
            const float T=Run.PhaseTime;
            if(T>=.30f&&!(Shots&1))
            {
                Shots|=1;
                Test->TestTrue(TEXT("Thrown card crosses visible camera area"),Part(TEXT("PlayedCard0"))&&InView(Part(TEXT("PlayedCard0"))->GetComponentLocation()));
                FVector Eye;FRotator Aim;PC->GetPlayerViewPoint(Eye,Aim);
                auto* Thrown=Part(TEXT("PlayedCard0"));
                Test->TestTrue(TEXT("Thrown card presents its illustrated face, not a thin edge"),FVector::DotProduct(Thrown->GetUpVector(),(Eye-Thrown->GetComponentLocation()).GetSafeNormal())>.35f);
                Shot(FString::Printf(TEXT("round%d-throw"),Run.Round));
            }
            else if(T>=.78f&&!(Shots&2))
            {
                Shots|=2;
                Test->TestTrue(TEXT("Card lands on the configured tabletop"),FVector::Dist(Part(TEXT("PlayedCard0"))->GetRelativeLocation(),Stage->GetCardLanding(0,Run.Hand.Num()))<2);
                Shot(FString::Printf(TEXT("round%d-spell"),Run.Round));
            }
            else if(T>=1.12f&&!(Shots&4)){Shots|=4;Shot(FString::Printf(TEXT("round%d-impact"),Run.Round));}
            else if(T>=2.02f&&!(Shots&8)){Shots|=8;Shot(FString::Printf(TEXT("round%d-reply"),Run.Round));}
            return false;
        }
        if(Run.Phase==ECardRoomPhase::Victory)
        {
            if(Step==2)
            {
                if(Run.PhaseTime<.4)return false;
                Test->TestEqual(TEXT("All four rounds completed"),LastRound,4);
                auto* Reward=Part(TEXT("CollectibleOrb"));
                Test->TestTrue(TEXT("Raised reward is visible above lower HUD"),Reward&&Reward->IsVisible()&&InView(Reward->GetComponentLocation(),.75f));
                Shot(TEXT("reward"));Next();return false;
            }
            if(FPlatformTime::Seconds()-Since<.3)return false;
            Test->TestTrue(TEXT("Raised reward remains collectable"),Stage->CollectReward());
            PC->CardInteraction->LeaveSeat();return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStudyPresentationV08Test,"BrainMining.Encounters.PIE.CardPresentationV08",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStudyPresentationV08Test::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStudyPresentationV08>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

#endif
