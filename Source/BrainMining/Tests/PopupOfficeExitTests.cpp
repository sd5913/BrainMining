#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "PopupClose/PopupCloseStage.h"
#include "PopupClose/PopupCloseFlyingWindow.h"
#include "MiningBombToolComponent.h"
#include "MiningBombActor.h"
#include "MiningWalletSubsystem.h"
#include "MiningProgressionComponent.h"
#include "BrainMiningPlayerController.h"
#include "MiningGUIWidgets.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "InputKeyEventArgs.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

namespace OfficeExitTests
{
class FStart : public IAutomationLatentCommand
{
 bool Update() override
 {
  FRequestPlaySessionParams P;P.EditorPlaySettings=DuplicateObject<ULevelEditorPlaySettings>(GetMutableDefault<ULevelEditorPlaySettings>(),GetTransientPackage());GEditor->RequestPlaySession(P);return true;
 }
};
class FRun : public IAutomationLatentCommand
{
public:
 explicit FRun(FAutomationTestBase* InTest):Test(InTest){}
 bool Update() override
 {
  if(FPlatformTime::Seconds()-Started>95){Test->AddError(FString::Printf(TEXT("Office exit timed out at step %d"),Step));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld:nullptr;if(!W || !W->HasBegunPlay() || W->GetTimeSeconds()<5.5)return false;
  auto PC=Cast<ABrainMiningPlayerController>(W->GetFirstPlayerController());auto Pawn=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
  APopupCloseStage* Stage=nullptr;for(TActorIterator<APopupCloseStage> It(W);It;++It){Stage=*It;break;}
  if(!PC || !Pawn || !Stage)return false;
  auto Tool=Pawn->FindComponentByClass<UMiningBombToolComponent>();auto Wallet=W->GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
  if(!Tool || !Wallet){Test->AddError(TEXT("Real bomb/skill account is missing"));return true;}
  auto Position=[&](FVector Local,FRotator R=FRotator::ZeroRotator)
  {
   Pawn->SetActorLocation(Stage->GetActorTransform().TransformPosition(Local),false,nullptr,ETeleportType::TeleportPhysics);
   Pawn->GetCharacterMovement()->StopMovementImmediately();PC->SetControlRotation(Stage->GetActorRotation()+R);PC->PlayerCameraManager->UpdateCamera(.01f);
  };
  auto Key=[&](FKey K,EInputEvent E){PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,E,E==IE_Pressed?1.f:0.f));};
  auto Body=[&](){return Tool->LastThrownBomb.IsValid()?Tool->LastThrownBomb->Collision.Get():nullptr;};
  auto FindMesh=[&](const TCHAR* Name)->UStaticMeshComponent*{TArray<UStaticMeshComponent*> Meshes;Stage->GetComponents(Meshes);for(auto Mesh:Meshes)if(Mesh->GetName()==Name)return Mesh;return nullptr;};
  auto PickupBody=[&](){auto Mesh=FindMesh(TEXT("OfficeBombSkillPickup"));return Mesh?Cast<USphereComponent>(Mesh->GetAttachParent()):nullptr;};
  auto Airborne=[&](){int32 Count=0;for(TActorIterator<APopupCloseFlyingWindow> It(W);It;++It)if(It->GetOwner()==Stage && !It->IsHidden())++Count;return Count;};
  auto GroundCount=[&]()
  {
   int32 Count=0;TArray<UInstancedStaticMeshComponent*> Meshes;Stage->GetComponents(Meshes);
   for(auto Mesh:Meshes)if(Mesh->GetName().StartsWith(TEXT("FloodFloor")))Count+=Mesh->GetInstanceCount();return Count;
  };
  switch(Step)
  {
  case 0:
   if(auto Progression=Pawn->FindComponentByClass<UMiningProgressionComponent>())Progression->InitializeProgression();
   Test->TestFalse(TEXT("Bomb is locked before pickup"),Tool->IsSkillUnlocked());
   Test->TestFalse(TEXT("Locked bomb cannot be equipped"),Tool->ToggleEquipped());
   Test->TestEqual(TEXT("Office does not complete on startup"),Stage->GetCompletedSessions(),0);
   Stage->StartOfficeRound();W->GetWorldSettings()->SetTimeDilation(6);break;
  case 1:
   if(auto Physics=PickupBody();Physics && Physics->IsSimulatingPhysics() && !bObservedLaunch)
   {
    bObservedLaunch=true;LaunchPosition=Physics->GetComponentLocation();PhysicalLaunchTime=W->GetTimeSeconds();
    W->GetWorldSettings()->SetTimeDilation(1);
    GroundAtHandoff=GroundCount();
    Test->TestTrue(TEXT("Airborne popups and papers survive the switch to the skill bomb"),Airborne()>0);
    Test->TestTrue(TEXT("Existing floor heaps survive the skill-bomb handoff"),GroundAtHandoff>0);
    Test->TestTrue(TEXT("Skill pickup has an independent physical actor"),Physics->GetOwner()!=Stage);
    Test->TestTrue(TEXT("Skill pickup uses gravity and CCD for the physical ejection"),Physics->IsGravityEnabled() && Physics->BodyInstance.bUseCCD);
    Test->TestTrue(TEXT("Skill pickup starts with a real physical velocity"),Physics->GetPhysicsLinearVelocity().Size()>50.f);
   }
   if(!Stage->GetExitStatus().Contains(TEXT("Exit phase=2")))return false;
   if(W->GetTimeSeconds()-PhysicalLaunchTime<2)return false;
   Test->TestEqual(TEXT("Last popups and papers finish landing after emission stops"),Airborne(),0);
   GroundAfterLanding=GroundCount();
   Test->TestTrue(TEXT("The final airborne batch adds persistent floor models"),GroundAfterLanding>GroundAtHandoff);
   Test->TestTrue(TEXT("Skill pickup remains a physical body after becoming collectible"),PickupBody() && PickupBody()->IsSimulatingPhysics());
   if(PickupBody())
   {
    const FVector Local=Stage->GetActorTransform().InverseTransformPosition(PickupBody()->GetComponentLocation());
    Test->TestTrue(TEXT("Ejected pickup lands on the floor without tunnelling below it"),Local.Z>=11.f && Local.Z<35.f);
   }
   Test->TestTrue(TEXT("Central bomb appears after timed eruption"),FindMesh(TEXT("OfficeBombSkillPickup"))->IsVisible());
   Test->TestEqual(TEXT("Spray completion does not automatically open the exit"),Stage->GetCompletedSessions(),0);
   Test->TestTrue(TEXT("Popup/paper eruption has stopped before the pickup"),Stage->GetSessionStatus().Contains(TEXT("Current emission rate=0.00/s")));
   Position(FVector(-600,0,98),FRotator(-20,0,0));Key(EKeys::E,IE_Pressed);break;
  case 2:
   Key(EKeys::E,IE_Released);Test->TestFalse(TEXT("Distant E cannot collect the bomb"),Tool->IsSkillUnlocked());
   {const FVector Target=PickupBody()->GetComponentLocation();
    Test->TestTrue(TEXT("The pickup moved under physics rather than staying at its launch point"),FVector::Dist(Target,LaunchPosition)>5.f);
    FVector Local=Stage->GetActorTransform().InverseTransformPosition(Target);Local.X-=110.f;Local.Z=98;Position(Local);
    FVector Eye;FRotator R;PC->GetPlayerViewPoint(Eye,R);PC->SetControlRotation((Target-Eye).Rotation());PC->PlayerCameraManager->UpdateCamera(.01f);}
   Key(EKeys::E,IE_Pressed);break;
  case 3:
   Key(EKeys::E,IE_Released);
   if(!Test->TestTrue(TEXT("Real near/aim/E unlocks the shared bomb skill"),Wallet->bBombSkillUnlocked))return true;
   Test->TestFalse(TEXT("Bomb pickup disappears after collection"),FindMesh(TEXT("OfficeBombSkillPickup"))->IsVisible());
   Test->TestTrue(TEXT("Collection stops the pickup's physics and collision"),PickupBody() && !PickupBody()->IsSimulatingPhysics() && PickupBody()->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
   Test->TestTrue(TEXT("Existing HUD bomb skill becomes available"),PC->MiningGUI && PC->MiningGUI->AbilityG && PC->MiningGUI->AbilityG->bAvailable);
   InitialInventory=Tool->BombCount;Key(EKeys::E,IE_Pressed);break;
  case 4:
   Key(EKeys::E,IE_Released);Test->TestEqual(TEXT("Repeated E cannot duplicate the pickup"),Tool->BombCount,InitialInventory);
   PC->SetControlRotation(Stage->GetActorRotation());Key(EKeys::G,IE_Pressed);break;
  case 5:
   Key(EKeys::G,IE_Released);Test->TestTrue(TEXT("G equips the real bomb component"),Tool->bEquipped);
   Key(EKeys::LeftMouseButton,IE_Pressed);break;
  case 6:
   Key(EKeys::LeftMouseButton,IE_Released);
   if(!Test->TestTrue(TEXT("Left mouse throws a real physics bomb"),Tool->LastThrownBomb.IsValid()))return true;
   Tool->LastThrownBomb->SetActorLocation(Stage->GetActorTransform().TransformPosition(FVector(0,0,50)),false,nullptr,ETeleportType::TeleportPhysics);
   if(Body())Body()->SetPhysicsLinearVelocity(FVector::ZeroVector);ExplosionCount=Tool->ExplosionCount;break;
  case 7:
   if(Tool->ExplosionCount<=ExplosionCount)return false;
   Test->TestEqual(TEXT("A distant real explosion does not break the door"),Stage->GetCompletedSessions(),0);
   PC->SetControlRotation(Stage->GetActorRotation());Key(EKeys::G,IE_Pressed);break;
  case 8:
   Key(EKeys::G,IE_Released);Key(EKeys::LeftMouseButton,IE_Pressed);break;
  case 9:
   Key(EKeys::LeftMouseButton,IE_Released);
   if(!Test->TestTrue(TEXT("Second bomb uses the same ability"),Tool->LastThrownBomb.IsValid()))return true;
   Tool->LastThrownBomb->SetActorLocation(Stage->GetActorTransform().TransformPosition(FVector(968,0,85)),false,nullptr,ETeleportType::TeleportPhysics);
   if(Body())Body()->SetPhysicsLinearVelocity(FVector::ZeroVector);ExplosionCount=Tool->ExplosionCount;break;
  case 10:
   if(Tool->ExplosionCount<=ExplosionCount)return false;
   if(!Test->TestEqual(TEXT("Nearby player bomb opens the completion gate"),Stage->GetCompletedSessions(),1))return true;
   Test->TestFalse(TEXT("Breaking the door alone does not evacuate or destroy the room"),Stage->IsOfficeEscaped());
   Test->TestFalse(TEXT("Door model no longer blocks"),FindMesh(TEXT("OfficeBlastDoor"))->IsVisible());
   Position(FVector(930,0,98));break;
  case 11:
   {FHitResult Hit;Pawn->SetActorLocation(Stage->GetActorTransform().TransformPosition(FVector(1135,0,98)),true,&Hit);
    Test->TestFalse(TEXT("Player capsule can sweep through the breached door"),Hit.bBlockingHit);}
   break;
  case 12:
   if(!Test->TestTrue(TEXT("Crossing the doorway triggers evacuation"),Stage->IsOfficeEscaped()))return true;
   Test->TestEqual(TEXT("Floor heaps remain when all devices switch to finale bombs"),GroundCount(),GroundAfterLanding);
   Stage->SetAreaActive(false);Stage->SetActorTickEnabled(false);
   Test->TestTrue(TEXT("Return shutter is visible immediately"),FindMesh(TEXT("OfficeEvacuationShutter"))->IsVisible());
   {FHitResult Hit;Pawn->SetActorLocation(Stage->GetActorTransform().TransformPosition(FVector(970,0,98)),true,&Hit);Test->TestTrue(TEXT("Swept return is blocked before detonation"),Hit.bBlockingHit);}
   Position(FVector(1320,0,98),FRotator(0,180,0));break;
  case 13:
   if(!Stage->IsOfficeDestroyed())return false;
   Test->TestEqual(TEXT("Popup/paper floor models clear only when the room collapses"),GroundCount(),0);
   Test->TestTrue(TEXT("Collapse continues after the campaign disables office tick"),Stage->GetExitStatus().Contains(TEXT("return_blocked=1")));
   Test->TestTrue(TEXT("Existing bomb skill stays unlocked outside the office"),Tool->IsSkillUnlocked());
   {const FVector P=Stage->GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation());
    Test->TestTrue(TEXT("Exit corridor retains its floor after the room collapses"),P.Z>60.f && P.Z<140.f);}
   Test->TestTrue(TEXT("A new bomb component sees the game-instance unlock"),NewObject<UMiningBombToolComponent>(Pawn)->IsSkillUnlocked());
   Test->TestTrue(TEXT("Cinematic bombs award no office coins"),Wallet->Balance==0);
   {
    auto Rubble=Cast<UInstancedStaticMeshComponent>(FindMesh(TEXT("OfficeRubble")));
    auto Bombs=Cast<UInstancedStaticMeshComponent>(FindMesh(TEXT("OfficeFinaleBombs")));
    Test->TestTrue(TEXT("Collapsed room has actual rubble models"),Rubble && Rubble->GetInstanceCount()>=200);
    Test->TestTrue(TEXT("Finale bomb instances are cleared on explosion"),Bombs && Bombs->GetInstanceCount()==0);
    FHitResult Hit;Test->TestTrue(TEXT("Full-height ruin collision blocks a flying return"),W->LineTraceSingleByChannel(Hit,Stage->GetActorTransform().TransformPosition(FVector(1200,0,330)),Stage->GetActorTransform().TransformPosition(FVector(980,0,330)),ECC_Pawn));
   }
   FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Validation/popup_office_exit_ruins.png"),false,false);
   Position(FVector(1500,0,98),FRotator(0,180,0));break;
  case 14:
   W->GetWorldSettings()->SetTimeDilation(1);Stage->SetActorTickEnabled(true);Stage->SetAreaActive(true);Stage->ResetCompletedSessions();
   Test->TestEqual(TEXT("Reset restores the closed exit and initial round"),Stage->GetCompletedSessions(),0);
   Test->TestFalse(TEXT("Reset clears permanent rubble"),Stage->IsOfficeDestroyed());
   Test->TestTrue(TEXT("Reset retains acquired skill ownership"),Tool->IsSkillUnlocked());
   Test->TestTrue(TEXT("Door collision is restored on reset"),FindMesh(TEXT("OfficeBlastDoor"))->GetCollisionEnabled()!=ECollisionEnabled::NoCollision);
   Test->TestTrue(TEXT("Reset leaves no hidden active pickup rigid body"),PickupBody() && !PickupBody()->IsSimulatingPhysics() && PickupBody()->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
   return true;
  }
  ++Step;return false;
 }
private:
 FAutomationTestBase* Test;double Started=FPlatformTime::Seconds();int32 Step=0,InitialInventory=0,ExplosionCount=0;
 bool bObservedLaunch=false;FVector LaunchPosition=FVector::ZeroVector;double PhysicalLaunchTime=0;
 int32 GroundAtHandoff=0,GroundAfterLanding=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPopupOfficeExitFlowTest,"BrainMining.Office.ExitFlow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPopupOfficeExitFlowTest::RunTest(const FString&)
{
 AutomationOpenMap(TEXT("/Game/PopupClose/Maps/Lvl_PopupClose"));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<OfficeExitTests::FStart>());
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<OfficeExitTests::FRun>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
