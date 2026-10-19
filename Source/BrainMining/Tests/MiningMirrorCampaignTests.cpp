#include "StartRoom/MiningOpening.h"
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Showcase/MiningExhibitHall.h"
#include "WenSi/WenSiMirrorPuzzle.h"
#include "MiningComponent.h"
#include "MiningRoomAudioSubsystem.h"
#include "MiningAudioComponent.h"
#include "MiningExperienceSubsystem.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace MirrorCampaignTests
{
class FKeyboardCheck : public IAutomationLatentCommand
{
public:
 explicit FKeyboardCheck(FAutomationTestBase* InTest,bool RequireHall=true):Test(InTest),bRequireHall(RequireHall){}
 bool Update() override
 {
  if(FPlatformTime::Seconds()-Started>90){Test->AddError(TEXT("Mirror keyboard checks timed out"));return true;}
  auto* World=GEditor->PlayWorld.Get();auto* Hall=AMiningExhibitHall::Find(World);
  if(!World || (bRequireHall && (!Hall || !Hall->bReady)) || World->GetTimeSeconds()<3)return false;
  AWenSiMirrorPuzzle* Museum=nullptr;
  for(TActorIterator<AWenSiMirrorPuzzle> It(World);It;++It)Museum=*It;
  if(!Test->TestNotNull(TEXT("Mirror keyboard target exists"),Museum))return true;
  if(Step==0)for(TActorIterator<AMiningOpening> It(World);It;++It)if(!It->bFinished){It->StartGame();return false;}
  auto* Controller=World->GetFirstPlayerController();auto Pawn=Controller->GetPawn();
  if(FPlatformTime::Seconds()-LastStep<.2)return false;
  auto Key=[&](FKey Input,EInputEvent Event){Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Input,Event,Event==IE_Pressed?1.f:0.f));};
  auto SelectBoth=[&](){Museum->Thoughts[0]=Museum->Find(3);Museum->Thoughts[1]=Museum->Find(2);Museum->UI->Refresh();};
  auto CheckClosed=[&](){Test->TestTrue(TEXT("Both thought bubbles are dismissed"),!Museum->Thoughts[0].IsValid() && !Museum->Thoughts[1].IsValid());};
  auto Capture=[](const TCHAR* Name)
  {
   if(FParse::Param(FCommandLine::Get(),TEXT("WenSiSwapScreenshots")))
    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("MirrorSwapPreview")/(FString(Name)+TEXT(".png")),true,false);
  };
  switch(Step++)
  {
   case 0:
   {
    if(Hall)
    {
     FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::F4,FModifierKeysState(),0,false,0,0));
     Test->TestFalse(TEXT("MainGame F4 does not open a removed area menu"),World->GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->IsMenuOpen());
     Test->TestFalse(TEXT("MainGame F4 does not pause or capture input"),Controller->IsPaused() || Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored() || Controller->bShowMouseCursor);
     FString TravelError;
     if(!Test->TestTrue(TEXT("Developer travel prepares the completed tutorial equipment"),Hall->EnterDeveloperArea(5,TravelError)))return true;
    }
    Museum->ResetPuzzle();
    Pawn->SetActorLocation(Museum->RoomPoint(FVector(-550,0,89)));
    SelectBoth();LeftWord=Museum->Find(3)->Word;RightWord=Museum->Find(2)->Word;
    Key(EKeys::R,IE_Pressed);
    if(Hall)Test->TestTrue(TEXT("R immediately starts the word flight"),Museum->IsSwapping());
    Test->TestTrue(TEXT("Words remain unchanged while departing"),Museum->Find(3)->Word==LeftWord && Museum->Find(2)->Word==RightWord);
    break;
   }
   case 1:
   {
    Key(EKeys::R,IE_Released);
    Test->TestTrue(TEXT("R starts the word flight"),Museum->IsSwapping());
    Test->TestTrue(TEXT("Both bubbles remain visible during travel"),Museum->Thoughts[0].IsValid() && Museum->Thoughts[1].IsValid());
    Capture(TEXT("01-WordFlight"));
    const float Progress=Museum->GetSwapProgress();
    Key(EKeys::R,IE_Pressed);Key(EKeys::R,IE_Repeat);Key(EKeys::R,IE_Released);
    Museum->UI->NativeOnKeyDown(FGeometry(),FKeyEvent(EKeys::R,FModifierKeysState(),0,false,0,0));
    Test->TestEqual(TEXT("Repeated and UI input cannot restart the same exchange"),Museum->GetSwapProgress(),Progress);
    Test->TestFalse(TEXT("R does not enter drag editing"),Museum->bEditing);
    Test->TestFalse(TEXT("R keeps the mouse captured"),Controller->bShowMouseCursor);
    Test->TestFalse(TEXT("R preserves movement and camera control"),Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored());
    break;
   }
   case 2:
    if(Museum->IsSwapping() && !Museum->HasSwapCommitted()){--Step;return false;}
    Test->TestTrue(TEXT("Words exchange at the end of the flight"),Museum->Find(3)->Word==RightWord && Museum->Find(2)->Word==LeftWord);
    Test->TestTrue(TEXT("Updated bubbles remain during landing"),Museum->HasSwapCommitted() && Museum->Thoughts[0].IsValid() && Museum->Thoughts[1].IsValid());
    Capture(TEXT("02-LandedWords"));
    break;
   case 3:
    if(Museum->IsSwapping())
    {
     if(!bCapturedFade && Museum->GetSwapOpacity()<.8f){Capture(TEXT("03-BubbleFade"));bCapturedFade=true;}
     --Step;return false;
    }
    CheckClosed();Capture(TEXT("04-ClosedBubbles"));
    Test->TestTrue(TEXT("Floating word changes the actual statue height"),Museum->Find(2)->GetActorLocation().Z>Museum->Find(2)->Rest.Z+1.f);
    break;
   case 4:
    SelectBoth();Key(EKeys::G,IE_Pressed);break;
   case 5:
    Key(EKeys::G,IE_Released);
    Test->TestTrue(TEXT("G leaves both thoughts selected without starting a swap"),!Museum->IsSwapping() && Museum->Thoughts[0]==Museum->Find(3) && Museum->Thoughts[1]==Museum->Find(2));
    Key(EKeys::Q,IE_Pressed);break;
   case 6:
    Key(EKeys::Q,IE_Released);CheckClosed();
    Test->TestTrue(TEXT("Q does not exchange object words"),Museum->Find(3)->Word==RightWord && Museum->Find(2)->Word==LeftWord);
    Museum->Thoughts[0]=Museum->Find(3);Museum->UI->Refresh();Key(EKeys::R,IE_Pressed);break;
   case 7:
    Key(EKeys::R,IE_Released);
    Test->TestTrue(TEXT("R with one thought preserves it"),!Museum->IsSwapping() && Museum->Thoughts[0]==Museum->Find(3) && !Museum->Thoughts[1].IsValid());
    Key(EKeys::Q,IE_Pressed);break;
   case 8:
    Key(EKeys::Q,IE_Released);CheckClosed();
    Museum->Thoughts[1]=Museum->Find(2);Museum->UI->Refresh();Key(EKeys::Q,IE_Pressed);break;
   case 9:
    Key(EKeys::Q,IE_Released);CheckClosed();Key(EKeys::R,IE_Pressed);break;
   case 10:
    Key(EKeys::R,IE_Released);CheckClosed();
    SelectBoth();Key(EKeys::R,IE_Pressed);break;
   case 11:
    Key(EKeys::R,IE_Released);Key(EKeys::Q,IE_Pressed);break;
   case 12:
    Key(EKeys::Q,IE_Released);CheckClosed();
    Test->TestFalse(TEXT("Q cancels an in-flight exchange"),Museum->IsSwapping());
    Test->TestTrue(TEXT("Cancelled flight has no delayed world effect"),Museum->Find(3)->Word==RightWord && Museum->Find(2)->Word==LeftWord);
    Museum->ResetPuzzle();if(Hall)Hall->EnterArea(0);return true;
  }
  LastStep=FPlatformTime::Seconds();return false;
 }
private:
 FAutomationTestBase* Test;
 bool bRequireHall=true;
 bool bCapturedFade=false;
 int32 Step=0;
 double Started=FPlatformTime::Seconds(),LastStep=0;
 EWenSiMirrorWord LeftWord=EWenSiMirrorWord::Ordinary,RightWord=EWenSiMirrorWord::Ordinary;
};
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
  if(UMiningAudioComponent::IsMainGameWorld(World))
  {
   // Entering the tiny gallery no longer completes the room; explicit painting interaction owns that transition.
   auto* RoomAudio=World->GetSubsystem<UMiningRoomAudioSubsystem>();
   if(Test->TestNotNull(TEXT("MainGame museum has room audio"),RoomAudio))
    for(FName Cue:{FName(TEXT("S_Swap")),FName(TEXT("S_Pottery")),FName(TEXT("S_Pressure")),FName(TEXT("S_Power")),FName(TEXT("S_RewardDoor")),FName(TEXT("S_Reset"))})
     Test->TestTrue(*FString::Printf(TEXT("Physical museum solution triggers %s"),*Cue.ToString()),RoomAudio->EventCount(Cue)>0);
  }
  Pawn->SetActorLocation(Museum->RoomPoint(FVector(1150,80,89)));
  Test->TestTrue(TEXT("Shared miner can become small"),Museum->SetPlayerWord(EWenSiMirrorWord::Small));
  if(UMiningAudioComponent::IsMainGameWorld(World))Hall->EnterArea(0);else Hall->SkipCurrentRoom();
  Test->TestTrue(TEXT("Leaving museum restores player scale and the same pawn"),PC->GetPawn()==Pawn && Pawn->GetActorScale3D().Equals(Scale));
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainGameMirrorAudioTest,"BrainMining.MainGame.MirrorMuseumAudio",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainGameMirrorAudioTest::RunTest(const FString&)
{
 AutomationOpenMap(TEXT("/Game/MainGame/Maps/Lvl_MainGame"));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(MirrorCampaignTests::FKeyboardCheck(this));
 ADD_LATENT_AUTOMATION_COMMAND(MirrorCampaignTests::FCheck(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMirrorSwapVisualTest,"BrainMining.WenSi.MirrorSwapVisual",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMirrorSwapVisualTest::RunTest(const FString&)
{
 AutomationOpenMap(TEXT("/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum"));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(MirrorCampaignTests::FKeyboardCheck(this,false));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
