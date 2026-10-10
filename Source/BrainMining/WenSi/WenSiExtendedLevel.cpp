#include "WenSi/WenSiPuzzle.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#if WITH_EDITOR
#include "Editor.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#endif

FString AWenSiPuzzle::ValidateExtendedPuzzle()
{
 if (!bExtendedPuzzle || Objects.Num()!=5 || bEditing || !GetWorld()->IsGameWorld()) return TEXT("FAIL: requires fresh extended PIE");
 AWenSiObject* O[5]={};
 for (auto Object:Objects) if(Object->ObjectKind>=0 && Object->ObjectKind<5) O[Object->ObjectKind]=Object;
 for (auto Object:O) if(!Object) return TEXT("FAIL: missing puzzle object");
 auto PC=GetWorld()->GetFirstPlayerController();
 auto Pawn=PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
 if(!Pawn) return TEXT("FAIL: missing character");
 const FVector OriginalPosition=Pawn->GetActorLocation();
 const FRotator OriginalRotation=PC->GetControlRotation();
 int32 Failures=0;
 auto Require=[&](bool Condition,const TCHAR* Name)
 { if(!Condition){++Failures;UE_LOG(LogTemp,Error,TEXT("WenSi extended validation failed: %s"),Name);} };
 auto Reset=[&]()
 {
  for(int32 I=0;I<5;++I)
  {
   O[I]->Rule=(I==1 || I==4) ? EWenSiRule::Light : I==2 ? EWenSiRule::Intangible : EWenSiRule::Heavy;
   O[I]->SetActorLocation(O[I]->Rest); O[I]->ApplyRule();
  }
  Thoughts[0].Reset();Thoughts[1].Reset();bEditing=false;bComplete=false;
  Pawn->SetActorLocation(FVector(-370,0,90));Pawn->GetCharacterMovement()->StopMovementImmediately();
 };
 auto Settle=[&](){for(int32 T=0;T<100;++T)for(auto Object:O)Object->Tick(.05f);};
 auto SwapObjects=[&](int32 A,int32 B)
 {Thoughts[0]=O[A];Thoughts[1]=O[B];bEditing=true;SwapRules(0,1);bEditing=false;Settle();};
 auto Blocked=[&](float From,float To)
 {
  FHitResult Hit;FCollisionQueryParams Query;Query.AddIgnoredActor(Pawn);
  return GetWorld()->SweepSingleByChannel(Hit,FVector(From,0,90),FVector(To,0,90),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(32,88),Query);
 };
 auto FloorAt=[&](float X,float Y,AWenSiObject* Expected)
 {
  FHitResult Hit;FCollisionQueryParams Query;Query.AddIgnoredActor(Pawn);
  return GetWorld()->LineTraceSingleByChannel(Hit,FVector(X,Y,40),FVector(X,Y,-50),ECC_Pawn,Query) && Hit.GetActor()==Expected;
 };
 Reset();Settle();
 Require(Blocked(60,460),TEXT("initial stone and cage block route"));
 Require(Blocked(360,760),TEXT("initial screen blocks route"));
 Require(!FloorAt(1020,0,O[4]),TEXT("floating plank cannot support crossing"));
 SwapObjects(3,4);
 Require(Blocked(360,760),TEXT("light screen is stopped by low ceiling"));
 Reset();SwapObjects(1,2);SwapObjects(0,2);
 Require(!Blocked(60,460),TEXT("original two swaps open first route"));
 Require(Blocked(360,760),TEXT("screen remains a separate obstacle"));
 Pawn->SetActorLocation(FVector(380,0,90));
 SwapObjects(1,3);
 Require(!Blocked(360,760),TEXT("void screen allows passage"));
 Require(O[1]->Rule==EWenSiRule::Heavy,TEXT("cage supplies recovered heavy adjective"));
 // All thinking still uses the same nearby camera trace, including the hovering plank.
 Pawn->SetActorLocation(FVector(780,0,90));Thoughts[0]=O[1];Thoughts[1].Reset();
 PC->SetControlRotation((FVector(850,0,O[4]->GetActorLocation().Z)-FVector(780,0,155)).Rotation());
 if(PC->PlayerCameraManager)PC->PlayerCameraManager->UpdateCamera(.01f);
 Tick(0);Require(Aim!=INDEX_NONE && Objects[Aim]==O[4],TEXT("floating plank can be thought about from bank"));
 Think();Require(Thoughts[1]==O[4] && Thoughts[0]==O[1],TEXT("keep cage thought while selecting plank"));
 bEditing=true;SwapRules(0,1);bEditing=false;Settle();
 Require(O[4]->Rule==EWenSiRule::Heavy && O[1]->Rule==EWenSiRule::Light,TEXT("final swap recycles heavy and light"));
 Require(FMath::IsNearlyEqual(O[4]->Body->Bounds.GetBox().Max.Z,0.0,1.0),TEXT("heavy plank rests on both banks"));
 for(float X:{910.f,1020.f,1130.f})for(float Y:{-75.f,0.f,75.f})Require(FloorAt(X,Y,O[4]),TEXT("bridge supports full crossing width"));
 Require(!Blocked(820,1220),TEXT("walking capsule has clear bridge passage"));
 // Passing the first obstacle must not prematurely end the extended puzzle.
 Pawn->SetActorLocation(FVector(780,0,90));Tick(0);Require(!bComplete,TEXT("completion waits for far bank"));
 Pawn->SetActorLocation(FVector(1020,0,-100));Tick(0);
 Require(Pawn->GetActorLocation().Equals(FVector(780,0,90),1),TEXT("pit recovers at near bank"));
 Require(O[4]->Rule==EWenSiRule::Heavy,TEXT("pit recovery preserves puzzle progress"));
 Pawn->SetActorLocation(FVector(1400,0,90));Tick(0);Require(bComplete,TEXT("far bank completes puzzle"));
 Reset();Settle();Pawn->SetActorLocation(OriginalPosition);PC->SetControlRotation(OriginalRotation);if(UI)UI->Refresh();
 const FString Result=Failures==0 ? TEXT("PASS: five objects, full solution, low ceiling, near-bank thinking, bridge collision, recovery and completion") : FString::Printf(TEXT("FAIL: %d checks"),Failures);
 UE_LOG(LogTemp,Display,TEXT("WenSi extended %s"),*Result);return Result;
}

#if WITH_EDITOR
void AWenSiPuzzle::CreateExtendedLevel()
{
 const FString Path=TEXT("/Game/WenSi/Maps/Lvl_WenSi_ScreenBridge");
 if(!GEditor || FPackageName::DoesPackageExist(Path)) return;
 UWorld* World=UEditorLoadingAndSavingUtils::NewBlankMap(false);if(!World)return;
 auto Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
 auto Block=[&](const TCHAR* Name,FVector Position,FVector Size,FLinearColor Color)
 {
  auto Actor=World->SpawnActor<AActor>();Actor->SetActorLabel(Name);
  auto Mesh=NewObject<UStaticMeshComponent>(Actor);Actor->SetRootComponent(Mesh);Actor->AddInstanceComponent(Mesh);
  Mesh->SetStaticMesh(Cube);Mesh->SetCollisionProfileName(TEXT("BlockAll"));Mesh->RegisterComponent();Actor->SetActorLocation(Position);Mesh->SetWorldScale3D(Size/100);
  auto Material=UMaterialInstanceDynamic::Create(Base,Actor);Material->SetVectorParameterValue(TEXT("Color"),Color);Mesh->SetMaterial(0,Material);
 };
 const FLinearColor Wall(.64f,.66f,.68f),Floor(.32f,.35f,.39f);
 Block(TEXT("NearFloor"),FVector(170,0,-15),FVector(1420,800,30),Floor);
 Block(TEXT("FarFloor"),FVector(1330,0,-15),FVector(340,800,30),Floor);
 Block(TEXT("PitBottom"),FVector(1020,0,-230),FVector(280,800,20),FLinearColor(.12f,.13f,.15f));
 Block(TEXT("Ceiling"),FVector(480,0,510),FVector(2040,800,20),Wall);
 Block(TEXT("LeftWall"),FVector(480,-410,135),FVector(2040,20,730),Wall);
 Block(TEXT("RightWall"),FVector(480,410,135),FVector(2040,20,730),Wall);
 Block(TEXT("BackWall"),FVector(-550,0,250),FVector(20,840,500),Wall);
 Block(TEXT("EndWall"),FVector(1510,0,250),FVector(20,840,500),Wall);
 Block(TEXT("PartitionLeft"),FVector(250,-260,250),FVector(25,280,500),Wall);
 Block(TEXT("PartitionRight"),FVector(250,260,250),FVector(25,280,500),Wall);
 Block(TEXT("PartitionHeader"),FVector(250,0,375),FVector(25,240,250),Wall);
 Block(TEXT("CageStopBeam"),FVector(140,0,247),FVector(80,240,20),FLinearColor(.22f,.25f,.28f));
 Block(TEXT("PassageLeft"),FVector(900,-130,135),FVector(1200,20,730),Wall);
 Block(TEXT("PassageRight"),FVector(900,130,135),FVector(1200,20,730),Wall);
 Block(TEXT("LowCeiling"),FVector(1000,0,240),FVector(1000,260,20),Wall);
 const FVector Positions[]={FVector(250,0,95),FVector(220,0,213),FVector(-40,220,58),FVector(600,0,105),FVector(1020,0,-8)};
 for(int32 I=0;I<5;++I)
 {
  FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto Object=World->SpawnActor<AWenSiObject>(Positions[I],FRotator::ZeroRotator,Params);Object->Configure(I);
  Object->Rule=(I==1 || I==4) ? EWenSiRule::Light : I==2 ? EWenSiRule::Intangible : EWenSiRule::Heavy;
  Object->ApplyRule();Object->SetActorLabel(Object->Noun());
 }
 for(FVector Position:{FVector(-220,0,420),FVector(400,0,215),FVector(800,0,205),FVector(1320,0,205)})
 {
  auto Actor=World->SpawnActor<AActor>();auto Light=NewObject<UPointLightComponent>(Actor);
  Actor->SetRootComponent(Light);Actor->AddInstanceComponent(Light);Light->SetMobility(EComponentMobility::Movable);
  Light->SetIntensity(12000);Light->SetAttenuationRadius(1100);Light->SetCastShadows(false);Light->RegisterComponent();Actor->SetActorLocation(Position);
 }
 World->SpawnActor<APlayerStart>(FVector(-370,0,90),FRotator::ZeroRotator)->SetActorLabel(TEXT("PlayerStart"));
 World->SpawnActor<AWenSiPuzzle>()->bExtendedPuzzle=true;
 World->GetWorldSettings()->DefaultGameMode=AWenSiGameMode::StaticClass();
 const bool Saved=UEditorLoadingAndSavingUtils::SaveMap(World,Path);
 UE_LOG(LogTemp,Display,TEXT("WenSi.CreateExtendedLevel saved=%d"),Saved);
}
static FAutoConsoleCommand CreateExtendedWenSi(TEXT("WenSi.CreateExtendedLevel"),TEXT("Create the separate screen and bridge puzzle level"),FConsoleCommandDelegate::CreateStatic(&AWenSiPuzzle::CreateExtendedLevel));
#endif
