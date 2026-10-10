#include "WenSi/WenSiPuzzle.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#if WITH_EDITOR
#include "Editor.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#endif

AWenSiObject::AWenSiObject()
{
 PrimaryActorTick.bCanEverTick = true;
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> Solid(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> Ghost(TEXT("/Engine/EngineDebugMaterials/M_SimpleTranslucent.M_SimpleTranslucent"));
 SolidMaterial = Solid.Object; GhostMaterial = Ghost.Object;
 auto Root=CreateDefaultSubobject<USceneComponent>(TEXT("Root")); SetRootComponent(Root);
 Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
 Body->SetupAttachment(Root);
 for (int32 I=0; I<8; ++I)
 {
  UStaticMeshComponent* Part = I==0 ? Body.Get() : CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Part%d"), I));
  if (I) Part->SetupAttachment(Root);
  Part->SetStaticMesh(Cube.Object);
  Part->SetMobility(EComponentMobility::Movable);
  Parts.Add(Part);
 }
}

void AWenSiObject::Configure(int32 Kind)
{
 ObjectKind=Kind;
 Body->SetRelativeScale3D(FVector::OneVector);
 for (auto Part : Parts) { Part->SetVisibility(false); Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
 auto Shape=[this](int32 I, FVector P, FVector Size)
 {
  Parts[I]->SetVisibility(true);
  Parts[I]->SetRelativeLocation(P);
  // Absolute scale prevents the root roof scale from distorting cage bars.
  Parts[I]->SetAbsolute(false, false, true);
  Parts[I]->SetWorldScale3D(Size/100.f);
 };
 if (Kind==0) Shape(0,FVector::ZeroVector,FVector(90,176,190));
 else if (Kind==1)
 {
  Shape(0,FVector::ZeroVector,FVector(170,210,12));
  for (int32 I=0; I<5; ++I) Shape(I+1,FVector(-70,(I-2)*49.f,-105),FVector(10,10,210));
  Shape(6,FVector(0,-100,-105),FVector(160,10,210));
  Shape(7,FVector(0,100,-105),FVector(160,10,210));
 }
 else if (Kind==2)
 {
  Shape(0,FVector::ZeroVector,FVector(58,58,75));
  Shape(1,FVector(0,0,58),FVector(40,40,40));
  Shape(2,FVector(0,0,-48),FVector(75,75,20));
 }
 else if (Kind==3)
 {
  Shape(0,FVector::ZeroVector,FVector(16,218,196));
  Shape(1,FVector(0,-112,0),FVector(26,12,210));
  Shape(2,FVector(0,112,0),FVector(26,12,210));
 }
 else if (Kind==4)
 {
  Shape(0,FVector::ZeroVector,FVector(360,236,16));
  for (int32 I=1;I<5;++I) Shape(I,FVector(0,(I-2.5f)*48,8.5f),FVector(350,2,1));
 }
 ApplyRule();
}

FString AWenSiObject::Noun() const
{
 static const TCHAR* Names[]={TEXT("石块"),TEXT("罩子"),TEXT("雕像"),TEXT("屏风"),TEXT("木板")};
 return ObjectKind>=0 && ObjectKind<UE_ARRAY_COUNT(Names) ? Names[ObjectKind] : TEXT("物体");
}
FString AWenSiObject::Adjective() const
{ return Rule==EWenSiRule::Heavy ? TEXT("沉重") : Rule==EWenSiRule::Light ? TEXT("轻盈") : TEXT("虚无"); }

void AWenSiObject::ApplyRule()
{
 const FLinearColor Color = ObjectKind==0 ? FLinearColor(.48f,.36f,.25f) : ObjectKind==1 ? FLinearColor(.25f,.48f,.58f) : ObjectKind==3 ? FLinearColor(.38f,.22f,.3f) : ObjectKind==4 ? FLinearColor(.55f,.32f,.15f) : FLinearColor(.8f,.72f,.5f);
 for (auto Part : Parts)
 {
  if (!Part->IsVisible()) continue;
  Part->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
  Part->SetCollisionObjectType(ECC_WorldDynamic);
  Part->SetCollisionResponseToAllChannels(ECR_Block);
  if (Rule==EWenSiRule::Intangible)
  {
   Part->SetCollisionResponseToAllChannels(ECR_Ignore);
   Part->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
  }
  UMaterialInstanceDynamic* M=UMaterialInstanceDynamic::Create(Rule==EWenSiRule::Intangible ? GhostMaterial.Get() : SolidMaterial.Get(),this);
  M->SetVectorParameterValue(TEXT("Color"),Rule==EWenSiRule::Intangible ? FLinearColor(.25,.7,1,.25) : Color);
  M->SetVectorParameterValue(TEXT("BaseColor"),Color);
  M->SetScalarParameterValue(TEXT("Opacity"),.25f);
  Part->SetMaterial(0,M);
 }
}

void AWenSiObject::BeginPlay()
{ Super::BeginPlay(); Rest=GetActorLocation(); Configure(ObjectKind); }

void AWenSiObject::Tick(float Dt)
{
 Super::Tick(Dt);
 float Lift = Rule==EWenSiRule::Light ? (ObjectKind==0 ? 210.f : ObjectKind==1 ? 18.f : 95.f) : 0.f;
 float TargetZ=Rest.Z+Lift;
 if (ObjectKind>=2 && Rule==EWenSiRule::Light)
 {
  // Keep the sculpture's head just below the actual room ceiling.
  float TopOffset=0.f;
  for (auto Part : Parts)
   if (Part->IsVisible()) TopOffset=FMath::Max(TopOffset,float(Part->Bounds.GetBox().Max.Z-GetActorLocation().Z));
  FCollisionQueryParams Query;
  for (TActorIterator<AWenSiObject> It(GetWorld());It;++It) Query.AddIgnoredActor(*It);
  if (auto PC=GetWorld()->GetFirstPlayerController()) Query.AddIgnoredActor(PC->GetPawn());
  const FVector Start=Rest+FVector(0,0,TopOffset);
  FHitResult Ceiling;
  if (GetWorld()->LineTraceSingleByChannel(Ceiling,Start,Start+FVector(0,0,1000),ECC_WorldStatic,Query))
   TargetZ=Ceiling.ImpactPoint.Z-TopOffset-4.f;
 }
 // The cage roof remains an obstacle even though it is a child-mesh assembly.
 if (ObjectKind==0 && Rule==EWenSiRule::Light)
  for (TActorIterator<AWenSiObject> It(GetWorld());It;++It)
   if (It->ObjectKind==1 && It->Rule!=EWenSiRule::Intangible)
    TargetZ=FMath::Min(TargetZ,It->GetActorLocation().Z-6.f-95.f-2.f);
 FVector P=GetActorLocation(); P.Z=FMath::FInterpConstantTo(P.Z,TargetZ,Dt,100.f);
 if (!P.Equals(GetActorLocation(),.01f))
 {
  // Never lower a solid object onto the player's capsule. Leave it suspended until clear.
  bool bOverlapsPawn=false;
  if (Rule!=EWenSiRule::Intangible)
  {
   APawn* Pawn=GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
   if (Pawn)
   {
    const FBox Bounds=GetComponentsBoundingBox(true);
    const FBox NextBounds=Bounds.ShiftBy(P-GetActorLocation());
    const ACharacter* Character=Cast<ACharacter>(Pawn);
    const FBox PawnBounds=Character ? Character->GetCapsuleComponent()->Bounds.GetBox() : Pawn->GetComponentsBoundingBox(true);
    bOverlapsPawn=NextBounds.Intersect(PawnBounds);
   }
  }
  if (!bOverlapsPawn) SetActorLocation(P);
 }
}

AWenSiPawn::AWenSiPawn()
{
 PrimaryActorTick.bCanEverTick=true;
 GetCapsuleComponent()->InitCapsuleSize(32,88);
 auto Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
 Camera->SetupAttachment(GetCapsuleComponent()); Camera->SetRelativeLocation(FVector(0,0,65)); Camera->bUsePawnControlRotation=true;
 bUseControllerRotationYaw=true;
 GetCharacterMovement()->MaxWalkSpeed=260;
 GetCharacterMovement()->MaxStepHeight=20;
}
void AWenSiPawn::Tick(float Dt)
{
 Super::Tick(Dt);
 auto PC=Cast<APlayerController>(Controller);
 if (!PC || PC->IsMoveInputIgnored()) return;
 float X=0,Y=0; PC->GetInputMouseDelta(X,Y);
 AddControllerYawInput(X); AddControllerPitchInput(-Y);
 AddMovementInput(GetActorForwardVector(),float(PC->IsInputKeyDown(EKeys::W))-float(PC->IsInputKeyDown(EKeys::S)));
 AddMovementInput(GetActorRightVector(),float(PC->IsInputKeyDown(EKeys::D))-float(PC->IsInputKeyDown(EKeys::A)));
}
AWenSiGameMode::AWenSiGameMode() { DefaultPawnClass=AWenSiPawn::StaticClass(); }

AWenSiPuzzle::AWenSiPuzzle() { PrimaryActorTick.bCanEverTick=true; }
void AWenSiPuzzle::BeginPlay()
{
 Super::BeginPlay();
 for (TActorIterator<AWenSiObject> It(GetWorld());It;++It) Objects.Add(*It);
 auto PC=GetWorld()->GetFirstPlayerController();
 if (PC)
 {
  UI=CreateWidget<UWenSiPuzzleWidget>(PC); UI->Puzzle=this; UI->AddToViewport(50); UI->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
  PC->SetInputMode(FInputModeGameOnly()); PC->bShowMouseCursor=false;
 }
}
void AWenSiPuzzle::Tick(float Dt)
{
 Super::Tick(Dt);
 auto PC=GetWorld()->GetFirstPlayerController(); if (!PC) return;
 Aim=INDEX_NONE;
 if (!bEditing && !bComplete)
 {
  FVector P; FRotator R; PC->GetPlayerViewPoint(P,R);
  FHitResult Hit; FCollisionQueryParams Q; Q.AddIgnoredActor(PC->GetPawn());
  if (GetWorld()->LineTraceSingleByChannel(Hit,P,P+R.Vector()*310.f,ECC_Visibility,Q))
  {
   auto Obj=Cast<AWenSiObject>(Hit.GetActor());
   if (Obj && PC->GetPawn() && FVector::Dist(PC->GetPawn()->GetActorLocation(),Hit.ImpactPoint)<280.f) Aim=Objects.IndexOfByKey(Obj);
  }
  if (PC->WasInputKeyJustPressed(EKeys::E)) Think();
  if (PC->WasInputKeyJustPressed(EKeys::Q)) CloseThought(0);
  if (PC->WasInputKeyJustPressed(EKeys::R)) CloseThought(1);
 }
 if (PC->WasInputKeyJustPressed(EKeys::G)) ToggleEdit();
 if (bExtendedPuzzle && PC->GetPawn() && PC->GetPawn()->GetActorLocation().Z<-80.f)
 {
  // Recover at the near bank without resetting the player's solved rules or thoughts.
  auto Character=Cast<ACharacter>(PC->GetPawn());
  PC->GetPawn()->SetActorLocation(FVector(780,0,90));
  if (Character) Character->GetCharacterMovement()->StopMovementImmediately();
 }
 if (!bComplete && PC->GetPawn() && PC->GetPawn()->GetActorLocation().X>(bExtendedPuzzle ? 1350.f : 510.f))
 {
  bComplete=true; if (bEditing) ToggleEdit(); Thoughts[0].Reset(); Thoughts[1].Reset();
 }
 if (UI) UI->Refresh();
}
void AWenSiPuzzle::Think()
{
 if (!Objects.IsValidIndex(Aim)) return;
 auto Obj=Objects[Aim];
 if (Thoughts[0]==Obj || Thoughts[1]==Obj) return;
 if (!Thoughts[0].IsValid()) Thoughts[0]=Obj;
 else if (!Thoughts[1].IsValid()) Thoughts[1]=Obj;
}
void AWenSiPuzzle::CloseThought(int32 Index)
{ if (Index>=0 && Index<2) { Thoughts[Index].Reset(); if (UI) UI->Refresh(); } }
void AWenSiPuzzle::ToggleEdit()
{
 auto PC=GetWorld()->GetFirstPlayerController(); if (!PC) return;
 if (!bEditing && (bComplete || (!Thoughts[0].IsValid() && !Thoughts[1].IsValid()))) return;
 bEditing=!bEditing; PC->SetIgnoreMoveInput(bEditing); PC->SetIgnoreLookInput(bEditing); PC->bShowMouseCursor=bEditing;
 if (UI) UI->Refresh();
 if (bEditing)
 {
  FInputModeGameAndUI Mode; Mode.SetWidgetToFocus(UI->TakeWidget()); Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); PC->SetInputMode(Mode);
 }
 else PC->SetInputMode(FInputModeGameOnly());
}
void AWenSiPuzzle::SwapRules(int32 From,int32 To)
{
 if (!bEditing || From==To || From<0 || From>1 || To<0 || To>1 || !Thoughts[From].IsValid() || !Thoughts[To].IsValid()) return;
 // Do not make a ghost solid while the player is inside it.
 auto Pawn=GetWorld()->GetFirstPlayerController()->GetPawn();
 const ACharacter* Character=Cast<ACharacter>(Pawn);
 const FBox PawnBounds=Character ? Character->GetCapsuleComponent()->Bounds.GetBox() : (Pawn ? Pawn->GetComponentsBoundingBox(true) : FBox(ForceInit));
 for (int32 I=0;I<2;++I)
  if (Pawn && Thoughts[I]->Rule==EWenSiRule::Intangible && Thoughts[1-I]->Rule!=EWenSiRule::Intangible && Thoughts[I]->GetComponentsBoundingBox(true).Intersect(PawnBounds)) return;
 Swap(Thoughts[From]->Rule,Thoughts[To]->Rule); Thoughts[From]->ApplyRule(); Thoughts[To]->ApplyRule();
 if (UI) UI->Refresh();
}
void AWenSiPuzzle::EndPlay(const EEndPlayReason::Type Reason)
{
 if (bEditing) ToggleEdit(); if (UI) UI->RemoveFromParent(); Super::EndPlay(Reason);
}

FString AWenSiPuzzle::ValidatePuzzle()
{
 if (!GetWorld()->IsGameWorld() || Objects.Num()!=3 || bEditing) return TEXT("FAIL: validation requires a fresh PIE session");
 AWenSiObject* ByKind[3]={nullptr,nullptr,nullptr};
 for(auto O:Objects) if(O->ObjectKind>=0 && O->ObjectKind<3) ByKind[O->ObjectKind]=O;
 for(auto O:ByKind) if(!O) return TEXT("FAIL: missing object");
 auto PC=GetWorld()->GetFirstPlayerController(); if(!PC || !PC->GetPawn()) return TEXT("FAIL: missing player");
 auto Pawn=PC->GetPawn();
 const FVector PlayerPos=Pawn->GetActorLocation(); const FRotator PlayerRot=PC->GetControlRotation();
 Pawn->SetActorLocation(FVector(-370,0,90));
 int32 Failures=0;
 auto Require=[&Failures](bool Condition,const TCHAR* Name){if(!Condition){++Failures;UE_LOG(LogTemp,Error,TEXT("WenSi validation failed: %s"),Name);}};
 auto Reset=[&]()
 {
  for(int32 I=0;I<3;++I){ByKind[I]->Rule=I==0?EWenSiRule::Heavy:I==1?EWenSiRule::Light:EWenSiRule::Intangible;ByKind[I]->SetActorLocation(ByKind[I]->Rest);ByKind[I]->ApplyRule();}
  Thoughts[0].Reset();Thoughts[1].Reset();bEditing=false;
 };
 auto Settle=[&](){for(int32 T=0;T<100;++T)for(auto O:ByKind)O->Tick(.05f);};
 auto Blocked=[&]()
 {
  FHitResult H; FCollisionQueryParams Q; Q.AddIgnoredActor(Pawn);
  return GetWorld()->SweepSingleByChannel(H,FVector(60,0,90),FVector(460,0,90),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(32,88),Q);
 };
 Reset();Settle(); Require(Blocked(),TEXT("initial exit must be blocked"));
 bEditing=true;Thoughts[0]=ByKind[0];Thoughts[1]=ByKind[1];SwapRules(0,1);Settle();
 Require(Blocked(),TEXT("direct stone/cage swap must not open exit"));
 Reset();
 bEditing=true;Thoughts[0]=ByKind[1];Thoughts[1]=ByKind[2];SwapRules(0,1);Settle();
 Require(FMath::IsNearlyEqual(ByKind[2]->GetComponentsBoundingBox(true).Max.Z,496.0,1.0),TEXT("light statue must reach ceiling without clipping"));
 Require(Blocked(),TEXT("ghost cage alone must not open exit"));
 CloseThought(0);Thoughts[0]=ByKind[0];SwapRules(0,1);Settle();
 Require(!Blocked(),TEXT("main two-swap solution must open exit"));
 Require(ByKind[0]->GetActorLocation().Z>290,TEXT("stone must rise above player"));
 Require(ByKind[1]->Body->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Ignore,TEXT("ghost must ignore player collision"));
 Require(ByKind[1]->Body->GetCollisionResponseToChannel(ECC_Visibility)==ECR_Block,TEXT("ghost must remain targetable"));
 Reset(); bEditing=true;Thoughts[0]=ByKind[0];Thoughts[1]=ByKind[2];SwapRules(0,1);Settle();
 Require(Blocked(),TEXT("ghost stone alone must not open exit"));
 Thoughts[1]=ByKind[1];SwapRules(0,1);Settle();Require(!Blocked(),TEXT("alternative solution must open exit"));
 Reset();
 // Verify proximity plus aiming through an actual camera trace, and reject remote thinking.
 Pawn->SetActorLocation(FVector(-180,220,90)); PC->SetControlRotation((ByKind[2]->GetActorLocation()-FVector(-180,220,155)).Rotation());
 if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(.01f);
 Tick(0);Require(Aim!=INDEX_NONE && Objects[Aim]==ByKind[2],TEXT("near ghost can be aimed at"));
 Think();Require(Thoughts[0]==ByKind[2],TEXT("E thinking selects left slot"));
 Pawn->SetActorLocation(FVector(-500,220,90));if(PC->PlayerCameraManager)PC->PlayerCameraManager->UpdateCamera(.01f);Tick(0);Require(Aim==INDEX_NONE,TEXT("remote thinking rejected"));
 Reset();Pawn->SetActorLocation(PlayerPos);PC->SetControlRotation(PlayerRot);if(UI)UI->Refresh();
 FString Result=Failures==0?TEXT("PASS: initial block, wrong swap, both solutions, ghost targeting, proximity and thinking"):FString::Printf(TEXT("FAIL: %d checks"),Failures);
 UE_LOG(LogTemp,Display,TEXT("WenSi %s"),*Result);return Result;
}

#if WITH_EDITOR
void AWenSiPuzzle::CreateLevel()
{
 const FString Path=TEXT("/Game/WenSi/Maps/Lvl_WenSi");
 if (!GEditor || FPackageName::DoesPackageExist(Path)) return;
 UWorld* W=UEditorLoadingAndSavingUtils::NewBlankMap(false); if (!W) return;
 auto Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
 auto Block=[W,Cube,Base](const TCHAR* Name,FVector P,FVector Size,FLinearColor Color)
 {
  AActor* A=W->SpawnActor<AActor>(); A->SetActorLabel(Name);
  auto M=NewObject<UStaticMeshComponent>(A); A->SetRootComponent(M); A->AddInstanceComponent(M); M->SetStaticMesh(Cube); M->SetCollisionProfileName(TEXT("BlockAll")); M->RegisterComponent(); A->SetActorLocation(P); M->SetWorldScale3D(Size/100);
  auto Mat=UMaterialInstanceDynamic::Create(Base,A); Mat->SetVectorParameterValue(TEXT("Color"),Color); M->SetMaterial(0,Mat);
 };
 FLinearColor Wall(.64,.66,.68), Floor(.32,.35,.39);
 Block(TEXT("Floor"),FVector(80,0,-15),FVector(1240,800,30),Floor);
 Block(TEXT("Ceiling"),FVector(80,0,510),FVector(1240,800,20),Wall);
 Block(TEXT("LeftWall"),FVector(80,-410,250),FVector(1240,20,500),Wall);
 Block(TEXT("RightWall"),FVector(80,410,250),FVector(1240,20,500),Wall);
 Block(TEXT("BackWall"),FVector(-550,0,250),FVector(20,840,500),Wall);
 Block(TEXT("ExitEndWall"),FVector(700,0,250),FVector(20,840,500),Wall);
 Block(TEXT("PartitionLeft"),FVector(250,-260,250),FVector(25,280,500),Wall);
 Block(TEXT("PartitionRight"),FVector(250,260,250),FVector(25,280,500),Wall);
 Block(TEXT("PartitionHeader"),FVector(250,0,375),FVector(25,240,250),Wall);
 Block(TEXT("CageStopBeam"),FVector(140,0,247),FVector(80,240,20),FLinearColor(.22,.25,.28));
 for(int32 I=0;I<3;++I)
 {
  FVector P=I==0 ? FVector(250,0,95) : I==1 ? FVector(220,0,213) : FVector(-40,220,58);
  FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto O=W->SpawnActor<AWenSiObject>(P,FRotator::ZeroRotator,Params); O->Configure(I);
  O->Rule=I==0 ? EWenSiRule::Heavy : I==1 ? EWenSiRule::Light : EWenSiRule::Intangible; O->ApplyRule(); O->SetActorLabel(O->Noun());
 }
 for (FVector P : {FVector(-220,0,420),FVector(420,0,420)})
 {
  auto A=W->SpawnActor<AActor>(); auto L=NewObject<UPointLightComponent>(A); A->SetRootComponent(L); A->AddInstanceComponent(L); L->SetMobility(EComponentMobility::Movable); L->SetIntensity(18000); L->SetAttenuationRadius(1400); L->SetCastShadows(false); L->RegisterComponent(); A->SetActorLocation(P);
 }
 auto Start=W->SpawnActor<APlayerStart>(FVector(-370,0,90),FRotator::ZeroRotator);
 Start->SetActorLabel(TEXT("PlayerStart"));
 W->SpawnActor<AWenSiPuzzle>(); W->GetWorldSettings()->DefaultGameMode=AWenSiGameMode::StaticClass();
 const bool bSaved=UEditorLoadingAndSavingUtils::SaveMap(W,Path);
 UE_LOG(LogTemp,Display,TEXT("WenSi.CreateLevel saved=%d"),bSaved);
}
static FAutoConsoleCommand CreateWenSi(TEXT("WenSi.CreateLevel"),TEXT("Create the independent WenSi puzzle level"),FConsoleCommandDelegate::CreateStatic(&AWenSiPuzzle::CreateLevel));
#endif
