#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "EndingEmergence.h"
#include "Showcase/MiningExhibitHall.h"
#include "WenSi/WenSiMirrorPuzzle.h"
#include "WenSi/WenSiMirrorMechanismLayout.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "PlayInEditorDataTypes.h"

namespace
{
class FStartMuseumEndingPIE : public IAutomationLatentCommand
{
public:
 bool Update() override
 {
  FRequestPlaySessionParams Params;
  Params.EditorPlaySettings=NewObject<ULevelEditorPlaySettings>();
  Params.bAllowOnlineSubsystem=false;
  GEditor->RequestPlaySession(Params);
  return true;
 }
};

class FMuseumRewardTravel : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 TWeakObjectPtr<AWenSiMirrorPuzzle> Puzzle;
 int32 Step=0;
 double Started=FPlatformTime::Seconds(),Next=0;
public:
 explicit FMuseumRewardTravel(FAutomationTestBase* InTest):Test(InTest){}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();
  if(Now-Started>180){Test->AddError(FString::Printf(TEXT("Museum ending travel timeout at step %d"),Step));return true;}
  if(Now<Next)return false;
  UWorld* World=GEditor->PlayWorld;
  auto* PC=World?World->GetFirstPlayerController():nullptr;
  auto* Pawn=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
  if(!Pawn || World->GetTimeSeconds()<2)return false;
  if(Step==0)
  {
   auto* Hall=AMiningExhibitHall::Find(World);
   if(!Hall || !Hall->bReady)return false;
   int32 Museum=INDEX_NONE;
   for(int32 I=0;I<Hall->Areas.Num();++I)
    if(Hall->Areas[I].Tag==TEXT("BM_Campaign_MainGameMuseum"))Museum=I;
   if(!Test->TestTrue(TEXT("MainGame museum exists"),Museum!=INDEX_NONE))return true;
   Hall->bLinearCampaign=false;
   const bool Entered=Hall->EnterArea(Museum);
   Hall->bLinearCampaign=true;
   if(!Test->TestTrue(TEXT("Enter the actual campaign museum"),Entered))return true;
   Hall->SetActorTickEnabled(false);
   for(TActorIterator<AWenSiMirrorPuzzle> It(World);It;++It)if(It->bAreaManaged)Puzzle=*It;
   if(!Test->TestTrue(TEXT("Campaign puzzle is active"),Puzzle.IsValid() && Puzzle->bAreaActive))return true;
   Pawn->SetActorLocation(Puzzle->RoomPoint(FVector(1650,40,89)));
   if(!Test->TestTrue(TEXT("Shrink using the museum player transformation"),Puzzle->SetPlayerWord(EWenSiMirrorWord::Small)))return true;
   Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
   Pawn->SetActorLocation(Puzzle->RoomPoint(FVector(2300,-350,25)));
  }
  else if(Step==1)
  {
   if(!Test->TestTrue(TEXT("Locked reward cannot be collected or trigger travel"),Puzzle.IsValid() && !Puzzle->bComplete && !Puzzle->bRewardUnlocked))return true;
   Pawn->SetActorLocation(Puzzle->RoomPoint(FVector(1650,40,25)));
   auto* Block=Puzzle->Find(6);auto* Ball=Puzzle->Find(9);
   if(!Block || !Ball){Test->AddError(TEXT("Museum mechanisms missing"));return true;}
   Block->Word=EWenSiMirrorWord::Heavy;
   Block->SetActorLocation(Block->Rest-FVector(0,0,WenSiMuseumMechanisms::BalanceTravel));
   Ball->Word=EWenSiMirrorWord::Conductive;Ball->bBallLanded=true;
   Ball->SetActorLocation(Puzzle->RoomPoint(FVector(Ball->Rest.X-Puzzle->GetActorLocation().X,Ball->Rest.Y-Puzzle->GetActorLocation().Y,80)));
  }
  else if(Step==2)
  {
   if(!Test->TestTrue(TEXT("Unlocking both mechanisms alone does not travel"),Puzzle.IsValid() && Puzzle->bRewardUnlocked && !Puzzle->bComplete))return true;
   if(!Test->TestTrue(TEXT("Restore ordinary size outside alcove"),Puzzle->SetPlayerWord(EWenSiMirrorWord::Ordinary)))return true;
   Pawn->SetActorLocation(Puzzle->RoomPoint(FVector(2300,-350,89)));
  }
  else if(Step==3)
  {
   if(!Test->TestTrue(TEXT("Ordinary-size player cannot collect the golden reward"),Puzzle.IsValid() && !Puzzle->bComplete))return true;
   Pawn->SetActorLocation(Puzzle->RoomPoint(FVector(1650,40,89)));
   if(!Test->TestTrue(TEXT("Shrink again before collection"),Puzzle->SetPlayerWord(EWenSiMirrorWord::Small)))return true;
   Pawn->SetActorLocation(Puzzle->RoomPoint(FVector(2300,-350,25)));
   // The next real game tick must collect the reward and request travel.
  }
  else
  {
   if(!World->GetName().Contains(TEXT("Lvl_EndingEmergence_Demo")))return false;
   AEndingEmergence* Ending=nullptr;
   for(TActorIterator<AEndingEmergence> It(World);It;++It){Ending=*It;break;}
   Test->TestNotNull(TEXT("Arrived in the authored emergence ending"),Ending);
   Test->TestTrue(TEXT("Museum world and reward actor were released"),!Puzzle.IsValid());
   Test->TestTrue(TEXT("Ending pawn has normal scale"),Pawn->GetActorScale3D().Equals(FVector::OneVector,.01));
   Test->TestFalse(TEXT("Ending movement input is enabled"),PC->IsMoveInputIgnored());
   Test->TestFalse(TEXT("Ending look input is enabled"),PC->IsLookInputIgnored());
   if(Ending)Test->TestEqual(TEXT("Ending starts at gameplay, before its film"),Ending->Shot,-1);
   return true;
  }
  ++Step;Next=Now+.5;
  return false;
 }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMuseumEndingTravel,"BrainMining.Campaign.GoldenRewardEndingTravel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningMuseumEndingTravel::RunTest(const FString&)
{
 AutomationOpenMap(TEXT("/Game/MainGame/Maps/Lvl_MainGame"));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStartMuseumEndingPIE>());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FMuseumRewardTravel>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
