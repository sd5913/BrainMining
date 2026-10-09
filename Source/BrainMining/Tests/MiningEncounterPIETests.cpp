#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "MiningEncounterPlayer.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningComponent.h"
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

namespace
{
class FEncounterPlaythrough : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Step=0;
    double Since=FPlatformTime::Seconds();
    FVector Start;
    float InitialYaw=0;
    void Next() {++Step;Since=FPlatformTime::Seconds();}
    void Key(APlayerController* PC,FKey K,bool Down)
    {PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f));}
public:
    explicit FEncounterPlaythrough(FAutomationTestBase* InTest):Test(InTest){}
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
            if(Age<1 || !Echo->Stage)return false;
            Test->TestEqual(TEXT("PIE uses original Blueprint pawn"),P->GetClass()->GetName(),FString(TEXT("BP_FirstPersonCharacter_C")));
            Test->TestFalse(TEXT("PIE flight blocked"),Move->IsFlightAvailable());
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
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("EncounterPreview/EchoSharedPlayer.png"),false,false);
            Key(PC,EKeys::LeftMouseButton,false);Key(PC,EKeys::R,true);Next();break;
        case 7:
            if(Age<.3)return false;
            Test->TestFalse(TEXT("R resets collapse"),Echo->Stage->bCollapsed);
            Test->TestEqual(TEXT("R clears echoes"),Echo->Stage->KnockCount,0);
            Test->TestTrue(TEXT("R returns to entry"),P->GetActorLocation().Z>900);
            Key(PC,EKeys::R,false);
            // Exercise real destination trigger/level travel after the input checks.
            Echo->Stage->bLanded=true;
            P->SetActorLocation(Echo->Stage->GetActorTransform().TransformPosition(FVector(4100,1600,100)+EchoPassageLayout::ExitOffset(true)),false,nullptr,ETeleportType::TeleportPhysics);
            Next();break;
        case 8:
            if(!Card->Stage || Age<1)return false;
            Test->TestFalse(TEXT("Card room flight blocked too"),Move->IsFlightAvailable());
            Test->TestTrue(TEXT("Travel replaced echo with card interaction"),!Echo->Stage);
            Test->TestFalse(TEXT("Travel restored look input"),PC->IsLookInputIgnored());
            P->SetActorLocation(Card->Stage->GetApproachLocation()+FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics);
            Key(PC,EKeys::E,true);Next();break;
        case 9:
            if(Age<.3)return false;
            Test->TestTrue(TEXT("E seats at table"),Card->Stage->Run.Phase==ECardRoomPhase::Seated);
            Test->TestTrue(TEXT("Seat locks movement and view"),PC->IsMoveInputIgnored()&&PC->IsLookInputIgnored());
            Test->TestTrue(TEXT("Seated camera sees table from expected height"),FMath::Abs(P->FindComponentByClass<UCameraComponent>()->GetComponentLocation().Z-(Card->Stage->GetSeatLocation().Z+88))<25);
            Key(PC,EKeys::E,false);Card->Stage->Run.Match();Card->Stage->Run.Tick(10);Card->Stage->Run.Tick(10);Card->Stage->Run.Stage(0);
            Key(PC,EKeys::SpaceBar,true);Next();break;
        case 10:
            if(Age<.2)return false;
            Test->TestTrue(TEXT("Space confirms selected card"),Card->Stage->Run.Phase==ECardRoomPhase::Resolving);
            Test->TestTrue(TEXT("Space does not jump out of seat"),Move->MovementMode==MOVE_None);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("EncounterPreview/CardSharedPlayer.png"),false,false);
            Key(PC,EKeys::SpaceBar,false);Key(PC,EKeys::Q,true);Next();break;
        case 11:
            if(Age<.3)return false;
            Test->TestTrue(TEXT("Q returns to exploration"),Card->Stage->Run.Phase==ECardRoomPhase::Exploring);
            Test->TestFalse(TEXT("Q restores move input"),PC->IsMoveInputIgnored());Test->TestFalse(TEXT("Q restores look input"),PC->IsLookInputIgnored());
            Test->TestTrue(TEXT("Q restores collision"),P->GetActorEnableCollision());Key(PC,EKeys::Q,false);
            Start=P->GetActorLocation();Key(PC,EKeys::S,true);Next();break;
        case 12:
            if(Age<.4)return false;
            Test->TestTrue(TEXT("Original movement works after leaving seat"),FVector::Dist2D(Start,P->GetActorLocation())>30);
            Key(PC,EKeys::S,false);return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterPIETest,"BrainMining.Encounters.PIE.InputAndJourney",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEncounterPIETest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEncounterPlaythrough>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
