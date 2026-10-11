#include "WenSi/WenSiPuzzle.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "EngineUtils.h"

FString AWenSiPuzzle::ValidateCircuitPuzzle()
{
 if(!bCircuitPuzzle || !Circuit || !ResetButton || !GetWorld()->IsGameWorld())return TEXT("FAIL: requires circuit museum PIE");
 AWenSiObject* ByKind[9]={};
 for(auto Object:Objects)if(Object->ObjectKind>=0 && Object->ObjectKind<9)ByKind[Object->ObjectKind]=Object;
 for(int32 Kind:{0,1,2,3,5,6,7,8})if(!ByKind[Kind])return TEXT("FAIL: missing circuit exhibit");
 auto PC=GetWorld()->GetFirstPlayerController();auto Pawn=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
 if(!Pawn)return TEXT("FAIL: missing pawn");
 const auto Local=[this](FVector Position){return GetActorTransform().TransformPosition(Position);};
 int32 Failures=0,Checks=0;FString Report;
 auto Require=[&](bool bCondition,const TCHAR* Name)
 {
  ++Checks;Report+=FString::Printf(TEXT("%s: %s\n"),bCondition?TEXT("PASS"):TEXT("FAIL"),Name);
  if(!bCondition){++Failures;UE_LOG(LogTemp,Error,TEXT("WenSi circuit validation failed: %s"),Name);}
 };
 auto Settle=[&]()
 {
  for(int32 Frame=0;Frame<100;++Frame){for(auto Object:Objects)Object->Tick(.05f);UpdateCircuitProgress();Circuit->Tick(.05f);}
 };
 auto SwapObjects=[&](int32 A,int32 B)
 {
  Pawn->SetActorLocation(Local(FVector(-680,0,90)));Thoughts[0]=ByKind[A];Thoughts[1]=ByKind[B];
  if(!bEditing)ToggleEdit();SwapRules(0,1);if(bEditing)ToggleEdit();Settle();
 };
 auto AimAt=[&](FVector Position,FVector Target)
 {
  Pawn->SetActorLocation(Position);Pawn->GetCharacterMovement()->StopMovementImmediately();
  PC->SetControlRotation((Target-(Position+FVector(0,0,65))).Rotation());
  PC->PlayerCameraManager->UpdateCamera(.01f);Tick(0.f);
 };
 auto SolveEntrance=[&]()
 {
  SwapObjects(1,2);SwapObjects(0,2);Require(bStageOneSolved,TEXT("two original swaps solve stage one"));
  SwapObjects(1,3);Require(bStageTwoUnlocked && ByKind[1]->Rule==EWenSiRule::Heavy && ByKind[3]->Rule==EWenSiRule::Intangible,TEXT("void screen unlocks stage two while cage receives heavy"));
 };
 auto CheckReset=[&]()
 {
  Require(!bComplete && !bStageOneSolved && !bStageTwoUnlocked && !bEditing,TEXT("reset clears both stages and editing"));
  Require(!Thoughts[0].IsValid() && !Thoughts[1].IsValid(),TEXT("reset clears both thought cards"));
  Require(!PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && !PC->bShowMouseCursor,TEXT("reset restores game input and mouse mode"));
  Require(Pawn->GetActorLocation().Equals(PlayerResetTransform.GetLocation(),1.f) && Pawn->GetVelocity().IsNearlyZero(),TEXT("reset returns player to original stage-one spawn with zero flight velocity"));
  Require(!Circuit->bPressureActive && !Circuit->bCircuitActive && !Circuit->bRewardOpen,TEXT("reset clears both lamps and closes reward cabinet"));
  bool bAllRules=true,bAllTransforms=true;
  for(int32 I=0;I<Objects.Num();++I)
  {
   bAllRules &= Objects[I]->Rule==InitialObjectRules[I];
   bAllTransforms &= Objects[I]->GetActorTransform().Equals(InitialObjectTransforms[I],.01f);
  }
  Require(bAllRules,TEXT("every adjective returns to its original object"));
  Require(bAllTransforms,TEXT("every object returns to its original transform"));
 };
 ResetPuzzle();CheckReset();Settle();
 Require(Objects.Num()==8 && !ByKind[4],TEXT("bridge removed; eight thought objects remain"));
 Require(!FirstBarrierClear() && !bStageTwoUnlocked,TEXT("initial first-stage barrier remains closed"));
 Require(!Circuit->HasPressure() && !Circuit->HasConductiveBridge(),TEXT("initial light weight and ceramic ball satisfy neither lock"));
 Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);Pawn->SetActorLocation(Local(FVector(1400,0,230)));Tick(0.f);
 Require(!bComplete && !Circuit->bRewardOpen,TEXT("flying directly to reward does not complete the puzzle"));
 AimAt(Local(FVector(1050,-200,90)),ByKind[5]->GetActorLocation());
 Require(Aim==INDEX_NONE,TEXT("stage-two thinking is gated until entrance solution"));
 ResetPuzzle();Settle();SolveEntrance();
 AimAt(Local(FVector(1050,-200,90)),ByKind[5]->GetActorLocation());
 Require(Aim!=INDEX_NONE && Objects[Aim]==ByKind[5],TEXT("hovering weight can be thought about from nearby floor"));
 AimAt(Local(FVector(700,-210,90)),ByKind[8]->GetActorLocation());
 Require(Aim!=INDEX_NONE && Objects[Aim]==ByKind[7],TEXT("solid transparent cover blocks thinking about its contents"));
 // The electrical lock can be solved first, but must not substitute for the pressure lock.
 SwapObjects(3,7);
 CloseThought(0);CloseThought(1);
 AimAt(Local(FVector(775,-210,90)),ByKind[8]->GetActorLocation());
 Require(Aim!=INDEX_NONE && Objects[Aim]==ByKind[8],TEXT("void cover can be entered to think about conductor"));
 Think();Require(Thoughts[0]==ByKind[8] || Thoughts[1]==ByKind[8],TEXT("thinking selects conductor using actual camera trace"));
 SwapObjects(8,6);
 Require(Circuit->bCircuitActive && !Circuit->bPressureActive && !bComplete,TEXT("conductive ceramic ball alone lights only electrical lamp"));
 SwapObjects(1,5);
 Require(bComplete && Circuit->bRewardOpen && Circuit->bPressureActive && Circuit->bCircuitActive,TEXT("heavy pressure plus conductive contact opens reward cabinet"));
 ResetPuzzle();CheckReset();Settle();SolveEntrance();
 SwapObjects(1,5);
 Require(Circuit->bPressureActive && !Circuit->bCircuitActive && !bComplete,TEXT("heavy weight alone lights only mechanical lamp"));
 // Conditions are sustained, not latched individually.
 SwapObjects(1,5);
 Require(!Circuit->bPressureActive && !bComplete,TEXT("lightening weight releases pressure and relocks mechanical lock"));
 SwapObjects(1,5);SwapObjects(3,7);SwapObjects(8,6);
 Require(bComplete && Circuit->bRewardOpen,TEXT("pressure-first solution also completes puzzle"));
 ResetPuzzle();Settle();SolveEntrance();
 Thoughts[0]=ByKind[1];Thoughts[1]=ByKind[5];ToggleEdit();
 Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);Pawn->GetCharacterMovement()->Velocity=FVector(50,20,300);
 ResetPuzzle();CheckReset();
 Settle();
 int32 ResetButtonCount=0;
 for(TActorIterator<AWenSiResetButton> It(GetWorld());It;++It)
 {
  ++ResetButtonCount;
  AimAt(It->GetActorLocation()+FVector(0,-90,90),It->GetActorLocation()+FVector(0,0,100));
  Require(bAimReset && Aim==INDEX_NONE,TEXT("each physical reset button has distinct nearby E interaction"));
 }
 Require(ResetButtonCount==2,TEXT("both galleries contain a reset button"));
 Pawn->SetActorLocation(Local(FVector(20,0,90)));PC->PlayerCameraManager->UpdateCamera(.01f);Tick(0.f);
 Require(!bAimReset,TEXT("remote reset interaction is rejected"));
 ResetPuzzle();
 const FString Result=Failures?FString::Printf(TEXT("FAIL: %d/%d checks"),Failures,Checks):FString::Printf(TEXT("PASS: %d checks; both solutions, flight bypass, physical contacts, proximity, full reset and input recovery"),Checks);
 Report+=Result+TEXT("\n");IFileManager::Get().MakeDirectory(*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Validation")),true);
 FFileHelper::SaveStringToFile(Report,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Validation/wensi_circuit_runtime.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
 UE_LOG(LogTemp,Display,TEXT("WenSi circuit %s"),*Result);return Result;
}
