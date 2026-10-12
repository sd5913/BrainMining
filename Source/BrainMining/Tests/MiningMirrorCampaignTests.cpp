#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Showcase/MiningExhibitHall.h"
#include "WenSi/WenSiMirrorPuzzle.h"
#include "MiningComponent.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"

namespace MirrorCampaignTests
{
class FCheck : public IAutomationLatentCommand
{
public:
 explicit FCheck(FAutomationTestBase* InTest):Test(InTest){}
 bool Update() override
 {
  if(FPlatformTime::Seconds()-Started>90){Test->AddError(TEXT("Mirror campaign startup timed out"));return true;}
  auto World=GEditor->PlayWorld.Get();auto Hall=AMiningExhibitHall::Find(World);
  if(!Hall || !Hall->bReady || World->GetTimeSeconds()<3)return false;
  AWenSiMirrorPuzzle* Museum=nullptr;
  for(TActorIterator<AWenSiMirrorPuzzle> It(World);It;++It)Museum=*It;
  if(!Test->TestNotNull(TEXT("Latest mirror museum is loaded"),Museum))return true;
  auto PC=World->GetFirstPlayerController();auto Pawn=PC->GetPawn();
  const FVector Scale=Pawn->GetActorScale3D();
  Test->TestTrue(TEXT("Latest museum runs from its source sublevel"),Museum->GetLevel()==Hall->Areas[5].SourceLevel->GetLoadedLevel());
  Test->TestFalse(TEXT("Museum input is inactive in the mine"),Museum->bAreaActive);
  Hall->UnlockedArea=6;Hall->EnterArea(5);
  Test->TestTrue(TEXT("Shared pawn can solve every translated mirror mechanism"),Museum->ValidateMirrorPuzzle().StartsWith(TEXT("PASS")));
  Pawn->SetActorLocation(Museum->RoomPoint(FVector(1150,80,89)));
  Test->TestTrue(TEXT("Shared miner can become small"),Museum->SetPlayerWord(EWenSiMirrorWord::Small));
  Hall->SkipCurrentRoom();
  Test->TestTrue(TEXT("Skipping restores player scale and the same pawn"),PC->GetPawn()==Pawn && Pawn->GetActorScale3D().Equals(Scale));
  Test->TestFalse(TEXT("Museum stops accepting input after exit"),Museum->bAreaActive);
  Hall->EnterArea(5);
  Test->TestTrue(TEXT("Revisiting preserves the player word without losing a puzzle token"),Museum->GetPlayerWord()==EWenSiMirrorWord::Small && Pawn->GetActorScale3D().X<Scale.X);
  Museum->bComplete=true;
  Test->TestTrue(TEXT("Mirror reward completion opens campaign progression"),Hall->CanAdvanceCampaign());
  Hall->EnterArea(0);
  Test->TestFalse(TEXT("Returning to mining restores tool input"),Pawn->FindComponentByClass<UMiningComponent>()->IsInputSuspended());
  return true;
 }
private:
 FAutomationTestBase* Test;
 double Started=FPlatformTime::Seconds();
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMirrorCampaignTest,"BrainMining.Campaign.MirrorMuseumIntegration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMirrorCampaignTest::RunTest(const FString&)
{
 AutomationOpenMap(TEXT("/Game/Campaign/Maps/Lvl_FullJourney"));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(MirrorCampaignTests::FCheck(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
