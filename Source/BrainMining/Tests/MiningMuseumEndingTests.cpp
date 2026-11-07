#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "EndingEmergence.h"
#include "Showcase/MiningExhibitHall.h"
#include "WenSi/WenSiMirrorPuzzle.h"
#include "WenSi/MouseMuseumPortal.h"
#include "MiningSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "StartRoom/MiningOpening.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "DiggableRock.h"
#include "ShaderCompiler.h"
#include "InputKeyEventArgs.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
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
 double Started=FPlatformTime::Seconds(),Next=0,EntryStarted=0;
 double HintCheckStarted=-1;
 bool bMobileInput=false;
public:
 explicit FMuseumRewardTravel(FAutomationTestBase* InTest,bool MobileInput=false):Test(InTest),bMobileInput(MobileInput){}
 bool Update() override
 {
  const double Now=FPlatformTime::Seconds();
  if(Now-Started>300){Test->AddError(FString::Printf(TEXT("Museum ending travel timeout at step %d"),Step));return true;}
  if(GShaderCompilingManager && GShaderCompilingManager->IsCompiling())return false;
  if(Now<Next)return false;
  UWorld* World=GEditor->PlayWorld;
  auto* PC=World?World->GetFirstPlayerController():nullptr;
  auto* Pawn=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
  if(!Pawn || World->GetTimeSeconds()<2)return false;
  if(Step==0)
  {
   // MainGame now starts on a separate menu camera. Complete its real release
   // before teleporting the pawn, otherwise the aim ray stays in the bedroom.
   for(TActorIterator<AMiningOpening> It(World);It;++It)
    if(!It->bFinished){It->StartGame();return false;}
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
   if(!Test->TestNotNull(TEXT("Mouse gallery is built"),Puzzle->MousePortal.Get()))return true;
   Pawn->SetActorLocation(Puzzle->RoomPoint(FVector(1650,40,89)));
   if(!Test->TestTrue(TEXT("Shrink using the museum player transformation"),Puzzle->SetPlayerWord(EWenSiMirrorWord::Small)))return true;
   Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
   Pawn->SetActorLocation(Puzzle->RoomPoint(FVector(2300,-350,35)));
  }
  else if(Step==1)
  {
   if(!Test->TestTrue(TEXT("Locked reward cannot be collected or trigger travel"),Puzzle.IsValid() && !Puzzle->bComplete && !Puzzle->bRewardUnlocked))return true;
   FVector Eye;FRotator Look;PC->GetPlayerViewPoint(Eye,Look);
   PC->SetControlRotation((Puzzle->MousePortal->CellCenter()-Eye).Rotation());PC->PlayerCameraManager->UpdateCamera(.016f);
   Test->TestFalse(TEXT("Locked mechanisms reject painting interaction"),Puzzle->MousePortal->TryEnter());
   Test->TestFalse(TEXT("Locked gallery does not activate the circle task"),World->GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>()->MuseumCircleTaskStarted);
   Pawn->SetActorLocation(Puzzle->RoomPoint(FVector(1650,40,35)));
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
   Test->TestFalse(TEXT("Interaction from outside the mouse hole is rejected"),Puzzle->MousePortal->TryEnter());
   Test->TestFalse(TEXT("Unlocking the gallery does not start its task before entry"),World->GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>()->MuseumCircleTaskStarted);
   if(!Test->TestTrue(TEXT("Restore ordinary size outside alcove"),Puzzle->SetPlayerWord(EWenSiMirrorWord::Ordinary)))return true;
   Pawn->SetActorLocation(Puzzle->RoomPoint(FVector(2300,-350,89)));
  }
  else if(Step==3)
  {
   if(!Test->TestTrue(TEXT("Ordinary-size player cannot collect the golden reward"),Puzzle.IsValid() && !Puzzle->bComplete))return true;
   PC->PlayerCameraManager->UpdateCamera(.016f);FVector Eye;FRotator Look;PC->GetPlayerViewPoint(Eye,Look);
   PC->SetControlRotation((Puzzle->MousePortal->CellCenter()-Eye).Rotation());PC->PlayerCameraManager->UpdateCamera(.016f);
   Test->TestFalse(TEXT("Ordinary size rejects painting interaction"),Puzzle->MousePortal->TryEnter());
   Pawn->SetActorLocation(Puzzle->RoomPoint(FVector(1650,40,89)));
   if(!Test->TestTrue(TEXT("Shrink again before collection"),Puzzle->SetPlayerWord(EWenSiMirrorWord::Small)))return true;
   Pawn->SetActorLocation(Puzzle->RoomPoint(FVector(2300,-350,35)));
   PC->PlayerCameraManager->UpdateCamera(.016f);
   PC->GetPlayerViewPoint(Eye,Look);
   PC->SetControlRotation((Puzzle->MousePortal->CellCenter()-Eye).Rotation());
  }
  else if(Step==4)
  {
   if(HintCheckStarted<0)
   {
    Puzzle->MousePortal->RefreshFocus();
    auto* Saves=World->GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>();
    Test->TestTrue(TEXT("Entering the mouse gallery immediately starts the circle task"),Saves->MuseumCircleTaskStarted);
    Test->TestFalse(TEXT("Seeing the circle does not complete its task"),Saves->MuseumCircleTaskCompleted);
    HintCheckStarted=World->GetTimeSeconds();
    Test->TestFalse(TEXT("Gallery hint does not appear immediately on entry"),Puzzle->MousePortal->HasShownGalleryHint());
   }
   if(World->GetTimeSeconds()-HintCheckStarted<3.1)return false;
   Test->TestTrue(TEXT("Gallery hint appears after three seconds inside"),Puzzle->MousePortal->HasShownGalleryHint());
   if(!Test->TestTrue(TEXT("Standing at the former egg does not complete or travel"),Puzzle.IsValid() && !Puzzle->bComplete && !Puzzle->MousePortal->IsEntering()))return true;
   FHitResult Obstruction;FCollisionQueryParams Q;Q.AddIgnoredActor(Pawn);
   const auto Capsule=Pawn->GetCapsuleComponent();
   const bool Blocked=World->SweepSingleByChannel(Obstruction,Puzzle->RoomPoint(FVector(2150,-350,35)),Puzzle->RoomPoint(FVector(2260,-350,35)),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Q);
   Test->TestFalse(TEXT("The actual .38-scale capsule fits the open mouse hatch"),Blocked);
   if(auto* Cover=Puzzle->Find(7))
   {
    const FVector Start=Cover->GetActorTransform().TransformPosition(FVector(-150,0,-95)),End=Cover->GetActorTransform().TransformPosition(FVector(-65,0,-95));
    const bool CoverBlocked=World->SweepSingleByChannel(Obstruction,Start,End,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Q);
    if(CoverBlocked)Test->AddInfo(FString::Printf(TEXT("Cover hatch obstruction: actor=%s component=%s point=%s cover=%s radius=%.2f halfheight=%.2f"),*GetNameSafe(Obstruction.GetActor()),*GetNameSafe(Obstruction.GetComponent()),*Obstruction.ImpactPoint.ToString(),*Cover->GetActorTransform().ToString(),Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()));
    Test->TestFalse(TEXT("Shrunken capsule also fits the exhibit cover hatch"),CoverBlocked);
   }
   Pawn->SetActorLocation(Puzzle->RoomPoint(FVector(2230,-350,35)));
   PC->PlayerCameraManager->UpdateCamera(.016f);
   FVector Eye;FRotator Look;PC->GetPlayerViewPoint(Eye,Look);
   PC->SetControlRotation((Puzzle->MousePortal->CellCenter()-Eye).Rotation());
   PC->PlayerCameraManager->UpdateCamera(.016f);
   Puzzle->MousePortal->RefreshFocus();
   if(FParse::Param(FCommandLine::Get(),TEXT("MouseMuseumScreenshots")))FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Validation/MouseMuseum/Gallery.png"),false,false);
   if(!Puzzle->MousePortal->IsFocused())
   {
    PC->GetPlayerViewPoint(Eye,Look);const FVector Target=Puzzle->MousePortal->CellCenter();
    const double RayT=(Target.X-Eye.X)/Look.Vector().X;
    const FVector Point=Eye+Look.Vector()*RayT;FHitResult Hit;
    World->LineTraceSingleByChannel(Hit,Eye,Point+Look.Vector()*2,ECC_Visibility,Q);
    Test->AddInfo(FString::Printf(TEXT("Portal focus diagnostic: eye=%s rotation=%s pawn=%s target=%s circle_error=%.2f hit=%s component=%s active=%d small=%d editing=%d swapping=%d"),*Eye.ToString(),*Look.ToString(),*Pawn->GetActorLocation().ToString(),*Target.ToString(),FVector::Dist(Point,Target),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),Puzzle->bAreaActive,Puzzle->GetPlayerWord()==EWenSiMirrorWord::Small,Puzzle->bEditing,Puzzle->IsSwapping()));
   }
   if(!Test->TestTrue(TEXT("Aiming at the painted cell highlights it"),Puzzle->MousePortal->IsFocused()))return true;
   PC->GetPlayerViewPoint(Eye,Look);
   auto Blocker=World->SpawnActor<AStaticMeshActor>();
   Blocker->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
   Blocker->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
   Blocker->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
   Blocker->SetActorLocation((Eye+Puzzle->MousePortal->CellCenter())*.5f);Blocker->SetActorScale3D(FVector(.1f));
   Test->TestFalse(TEXT("An occluding object blocks the painted portal"),Puzzle->MousePortal->TryEnter());
   Test->TestFalse(TEXT("Blocked interaction cannot complete the circle task"),World->GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>()->MuseumCircleTaskCompleted);
   Blocker->Destroy();Puzzle->MousePortal->RefreshFocus();
   if(FParse::Param(FCommandLine::Get(),TEXT("MouseMuseumScreenshots")))FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Validation/MouseMuseum/Gallery.png"),false,false);
  }
  else if(Step==5)
  {
   PC->SetControlRotation(FRotator(0,180,0));PC->PlayerCameraManager->UpdateCamera(.016f);
   Test->TestFalse(TEXT("Looking away cannot enter"),Puzzle->MousePortal->TryEnter());
   FVector Eye;FRotator Look;PC->GetPlayerViewPoint(Eye,Look);
   PC->SetControlRotation((Puzzle->MousePortal->CellCenter()-Eye).Rotation());
   PC->PlayerCameraManager->UpdateCamera(.016f);
   if(!Test->TestTrue(TEXT("E interaction starts the cell suction"),Puzzle->MousePortal->TryEnter()))return true;
   auto* Saves=World->GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>();
   Test->TestTrue(TEXT("Successful circle interaction completes the task before travel"),Saves->MuseumCircleTaskCompleted);
   const int32 Notices=Saves->CompletionNotices.Num();
   Test->TestTrue(TEXT("Suction locks movement and look"),PC->IsMoveInputIgnored() && PC->IsLookInputIgnored());
   Test->TestTrue(TEXT("Repeated E cannot start a second entry"),!Puzzle->MousePortal->TryEnter());
   Test->TestEqual(TEXT("Repeated interaction does not duplicate task completion"),Saves->CompletionNotices.Num(),Notices);
   // Exercise cancellation before the real travel; it must release camera/input.
   Puzzle->MousePortal->Cancel();
   Puzzle->MousePortal->RefreshFocus();
   Test->TestTrue(TEXT("Cancelling travel and re-entering cannot reactivate a completed task"),Saves->MuseumCircleTaskCompleted);
   Test->TestFalse(TEXT("Cancelled suction restores movement"),PC->IsMoveInputIgnored());
   Test->TestFalse(TEXT("Cancelled suction restores look"),PC->IsLookInputIgnored());
   Test->TestFalse(TEXT("Cancelled suction restores the visible pawn"),Pawn->IsHidden());
   Test->TestTrue(TEXT("Remote museum skip still reaches the local mouse gallery"),Puzzle->SkipToReward());
   Test->TestFalse(TEXT("Museum skip does not bypass painted-cell interaction"),Puzzle->bComplete || Puzzle->MousePortal->IsEntering());
   for(TActorIterator<AActor> It(World);It;++It)
    if(It->ActorHasTag(TEXT("MirrorReward")))Test->TestTrue(TEXT("Museum skip cannot reveal the obsolete golden egg"),It->IsHidden());
   PC->PlayerCameraManager->UpdateCamera(.016f);
   // Final travel exercises the actual campaign controller's E-key route.
   if(bMobileInput)
   {
    auto* Mobile=Cast<ABrainMiningPlayerController>(PC);
    if(!Test->TestNotNull(TEXT("Campaign supports mobile interaction"),Mobile))return true;
    Puzzle->Aim=Puzzle->Find(2);
    Mobile->HandleMobileInteract();
   }
   else PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1.f));
   if(!Test->TestTrue(TEXT("Campaign E input begins the painting transition"),Puzzle->MousePortal->IsEntering()))return true;
   PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0.f));
   EntryStarted=Now;
  }
  else
  {
   if(!World->GetName().Contains(TEXT("Lvl_EndingEmergence_Demo")))
   {
    if(Now-EntryStarted>12){Test->AddError(TEXT("Suction did not finish travel while its input lock was active"));return true;}
    return false;
   }
   AEndingEmergence* Ending=nullptr;
   for(TActorIterator<AEndingEmergence> It(World);It;++It){Ending=*It;break;}
   Test->TestNotNull(TEXT("Arrived in the authored emergence ending"),Ending);
   Test->TestTrue(TEXT("Museum world and reward actor were released"),!Puzzle.IsValid());
   Test->TestTrue(TEXT("Ending pawn has normal scale"),Pawn->GetActorScale3D().Equals(FVector::OneVector,.01));
   if(Step==6)
   {
    Test->TestFalse(TEXT("Ending movement input is enabled"),PC->IsMoveInputIgnored());
    Test->TestFalse(TEXT("Ending look input is enabled"),PC->IsLookInputIgnored());
   }
   if(!Ending)return true;
   auto Capture=[&](const TCHAR* Name)
   {if(FParse::Param(FCommandLine::Get(),TEXT("MouseMuseumScreenshots")))FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Validation/MouseMuseum")/(FString(Name)+TEXT(".png")),false,false);};
   auto PaintWeight=[](UCameraComponent* C)
   {
    float Weight=0;
    if(C)for(const auto& B:C->PostProcessSettings.WeightedBlendables.Array)
     if(auto M=Cast<UMaterialInterface>(B.Object))
      if(M->GetMaterial()->GetPathName().Contains(TEXT("/CelPaint/")))Weight=B.Weight;
    return Weight;
   };
   if(Step==6)
   {
    Test->TestEqual(TEXT("Ending starts at gameplay, before its film"),Ending->Shot,-1);
    Test->TestTrue(TEXT("Cell camera uses the imported oil paint shader"),PaintWeight(Pawn->FindComponentByClass<UCameraComponent>())>0);
    TArray<USkeletalMeshComponent*> Bodies;Pawn->GetComponents(Bodies);
    for(auto* Body:Bodies)Test->TestTrue(TEXT("Cell player body and arms stay hidden"),Body->bHiddenInGame && !Body->IsVisible());
    for(TActorIterator<ACameraActor> It(World);It;++It)
     Test->TestEqual(TEXT("Film camera never inherits opening-cell paint"),PaintWeight(It->GetCameraComponent()),0.f);
    Capture(TEXT("Cell"));
   }
   else if(Step==7)
   {
    // Approach fixture only: the uncut, pass-through gate performs the real handoff.
    auto Gate=Ending->Gates[0];
    Test->TestFalse(TEXT("Cell gate is pass-through"),Gate->GetActorEnableCollision());
    Test->TestEqual(TEXT("No excavation required at the cell gate"),Gate->SuccessfulCuts,0);
    Pawn->SetActorLocation(Gate->GetActorLocation()+Ending->GetActorForwardVector()*(Gate->RockSize.X*.5f+120));
   }
   else if(Step==8)
   {
    if(Ending->GateIndex!=1)return false;
    auto BloodCamera=PC->GetViewTarget()->FindComponentByClass<UCameraComponent>();
    Test->TestTrue(TEXT("Blood handoff switches to the cinematic camera"),PC->GetViewTarget()!=Pawn);
    Test->TestEqual(TEXT("Blood camera uses the authored vessel paint strength"),PaintWeight(BloodCamera),FMath::Clamp(Ending->BloodPaintWeight,0.f,1.f));
    Test->TestEqual(TEXT("Blood handoff clears paint from the inactive pawn camera"),PaintWeight(Pawn->FindComponentByClass<UCameraComponent>()),0.f);
    Capture(TEXT("Blood"));
   }
   else if(Step==9)
   {
    // Let the real end-of-vessel condition call EnterBrain on the next frame.
    Ending->VesselDistance=1000000;
   }
   else if(Step==10)
   {
    // The authored vessel-to-brain handoff fades paint over 1.25 seconds;
    // it must be fully clear when the 1.45-second camera transition completes.
    Test->TestTrue(TEXT("Vessel exit paint stays within the authored fade range"),FMath::IsFinite(Ending->ActivePaintWeight) && Ending->ActivePaintWeight>=0.f && Ending->ActivePaintWeight<=Ending->BloodPaintWeight);
    Test->TestEqual(TEXT("Pawn camera cannot leak paint into neurons"),PaintWeight(Pawn->FindComponentByClass<UCameraComponent>()),0.f);
    Test->TestEqual(TEXT("Cinematic camera follows the vessel exit fade"),PaintWeight(PC->GetViewTarget()->FindComponentByClass<UCameraComponent>()),Ending->ActivePaintWeight);
   }
   else if(Step==11)
   {
    if(Ending->GateIndex!=2)return false;
    Test->TestEqual(TEXT("Entering neurons has fully removed the vessel paint"),Ending->ActivePaintWeight,0.f);
    Test->TestEqual(TEXT("Pawn camera remains clear in neurons"),PaintWeight(Pawn->FindComponentByClass<UCameraComponent>()),0.f);
    Test->TestEqual(TEXT("Cinematic camera cannot leak paint into neurons"),PaintWeight(PC->GetViewTarget()->FindComponentByClass<UCameraComponent>()),0.f);
    Capture(TEXT("Neurons"));
   }
   else if(Step==12)
   {
    Ending->PreviewFilm();
    Test->TestEqual(TEXT("Film has no oil paint"),Ending->ActivePaintWeight,0.f);
    Ending->RestartDemo();
    Test->TestTrue(TEXT("Restart restores the opening cell paint"),Ending->ActivePaintWeight>0);
    return true;
   }
  }
  ++Step;Next=Now+.5;
  return false;
 }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMuseumEndingTravel,"BrainMining.Campaign.MousePaintingEndingTravel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningMuseumEndingTravel::RunTest(const FString&)
{
 AutomationOpenMap(TEXT("/Game/MainGame/Maps/Lvl_MainGame"));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStartMuseumEndingPIE>());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FMuseumRewardTravel>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMobileMuseumEndingTravel,"BrainMining.Campaign.MobileMousePaintingEndingTravel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningMobileMuseumEndingTravel::RunTest(const FString&)
{
 AutomationOpenMap(TEXT("/Game/MainGame/Maps/Lvl_MainGame"));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FStartMuseumEndingPIE>());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FMuseumRewardTravel>(this,true));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMuseumCircleTaskSave,"BrainMining.Campaign.MouseCircleTaskSave",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMuseumCircleTaskSave::RunTest(const FString&)
{
 auto* Data=NewObject<UMiningSaveGame>();
 TestFalse(TEXT("Old saves default to an undiscovered circle task"),Data->MuseumCircleTaskStarted || Data->MuseumCircleTaskCompleted);
 for(const bool Completed:{false,true})
 {
  Data->MuseumCircleTaskStarted=true;Data->MuseumCircleTaskCompleted=Completed;
  TArray<uint8> Bytes;
  if(!TestTrue(TEXT("Serialize circle task without touching player files"),UGameplayStatics::SaveGameToMemory(Data,Bytes)))return false;
  auto* Restored=Cast<UMiningSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
  if(!TestNotNull(TEXT("Restore circle task snapshot"),Restored))return false;
  TestTrue(TEXT("Started task survives a save round trip"),Restored->MuseumCircleTaskStarted);
  TestEqual(TEXT("Completion state survives a save round trip"),Restored->MuseumCircleTaskCompleted,Completed);
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndingPaintScope,"BrainMining.Ending.PainterlyStageScope",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEndingPaintScope::RunTest(const FString&)
{
 TestEqual(TEXT("Opening cell is painted"),AEndingEmergence::PaintWeightForStage(0,-1,false,.85f,.7f),.85f);
 TestEqual(TEXT("Vessel uses its own oil-paint strength"),AEndingEmergence::PaintWeightForStage(1,-1,false,.85f,.7f),.7f);
 TestEqual(TEXT("Vessel exit immediately removes paint"),AEndingEmergence::PaintWeightForStage(1,-1,true,.85f,.7f),0.f);
 for(int32 Gate=2;Gate<=5;++Gate)TestEqual(TEXT("Neurons never inherit the oil shader"),AEndingEmergence::PaintWeightForStage(Gate,-1,false,.85f,.7f),0.f);
 for(int32 Shot=0;Shot<=11;++Shot)TestEqual(TEXT("Film and finale do not inherit paint"),AEndingEmergence::PaintWeightForStage(0,Shot,false,.85f,.7f),0.f);
 return true;
}
#endif
