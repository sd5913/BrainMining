#include "WenSi/WenSiPuzzle.h"
#include "MiningHapticsSubsystem.h"
#include "MiningCharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
 UMaterialInterface* MuseumSurface(const TCHAR* Name)
 {
  const FString Path=FString(TEXT("/Game/WenSi/Museum/Materials/"))+Name+TEXT(".")+Name;
  return LoadObject<UMaterialInterface>(nullptr,*Path);
 }
 void Shape(UStaticMeshComponent* Part,UStaticMesh* Mesh,FVector Position,FVector Size,UMaterialInterface* Material,bool bSolid)
 {
  Part->SetStaticMesh(Mesh);
  Part->SetRelativeLocation(Position);
  Part->SetRelativeScale3D(Size/100.f);
  Part->SetMaterial(0,Material);
  Part->SetCollisionProfileName(bSolid ? TEXT("BlockAll") : TEXT("NoCollision"));
  Part->SetCastShadow(bSolid);
  Part->SetVisibility(true);
 }
 FBox BoxAt(const AActor* Actor,FVector Center,FVector HalfSize)
 {
  const FVector WorldCenter=Actor->GetActorTransform().TransformPosition(Center);
  return FBox(WorldCenter-HalfSize,WorldCenter+HalfSize);
 }
}

AWenSiCircuitMechanism::AWenSiCircuitMechanism()
{
 PrimaryActorTick.bCanEverTick=true;
 auto Root=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));SetRootComponent(Root);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
 CubeMesh=Cube.Object;SphereMesh=Sphere.Object;CylinderMesh=Cylinder.Object;
 for(int32 I=0;I<40;++I)
 {
  auto Part=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("BenchPart%d"),I));
  Part->SetupAttachment(Root);Part->SetMobility(EComponentMobility::Movable);Parts.Add(Part);
 }
}

void AWenSiCircuitMechanism::Configure()
{
 Palette={MuseumSurface(TEXT("MI_Limestone")),MuseumSurface(TEXT("MI_Charcoal")),MuseumSurface(TEXT("MI_DeepBlue")),MuseumSurface(TEXT("MI_Brass")),MuseumSurface(TEXT("MI_Terracotta")),MuseumSurface(TEXT("MI_Patina"))};
 GlowMaterial=MuseumSurface(TEXT("M_InteractiveRim"));GlassMaterial=MuseumSurface(TEXT("M_DisplayGlass"));
 for(auto Part:Parts){Part->SetVisibility(false);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetStaticMesh(nullptr);}
 auto Cube=[this](int32 I,FVector P,FVector Size,int32 Color,bool bSolid=true){Shape(Parts[I],CubeMesh,P,Size,Palette[Color],bSolid);};
 // One base contains the two locks and their visibly connected brass routes.
 Cube(0,FVector(0,-200,5),FVector(130,130,10),1);
 Shape(Parts[1],CylinderMesh,FVector(0,-200,12),FVector(114,114,6),Palette[3],true);
 Cube(2,FVector(0,160,34),FVector(122,116,68),2);
 Cube(3,FVector(-32,160,100),FVector(8,30,40),3);
 Cube(4,FVector(32,160,100),FVector(8,30,40),3);
 Cube(5,FVector(75,-200,8),FVector(150,5,4),3,false);
 Cube(6,FVector(150,-110,8),FVector(5,185,4),3,false);
 Cube(7,FVector(150,-20,8),FVector(80,5,4),3,false);
 Cube(8,FVector(80,160,8),FVector(160,5,4),3,false);
 Cube(9,FVector(160,100,8),FVector(5,125,4),3,false);
 Cube(10,FVector(175,40,8),FVector(30,5,4),3,false);
 Cube(11,FVector(190,0,9),FVector(138,138,18),1);
 Cube(12,FVector(190,0,32),FVector(120,120,28),2);
 Cube(13,FVector(190,0,49),FVector(136,136,6),3);
 for(int32 I=0;I<4;++I)
  Cube(14+I,FVector(190+(I<2?-65:65),(I%2)?65:-65,141),FVector(3,3,180),3,false);
 Cube(18,FVector(190,0,233),FVector(140,140,8),1);
 Shape(Parts[19],CubeMesh,FVector(255,0,141),FVector(2,128,180),GlassMaterial,true);
 Shape(Parts[20],CubeMesh,FVector(190,-65,141),FVector(128,2,180),GlassMaterial,true);
 Shape(Parts[21],CubeMesh,FVector(190,65,141),FVector(128,2,180),GlassMaterial,true);
 Shape(Parts[22],CubeMesh,FVector(125,-32,141),FVector(2,64,180),GlassMaterial,true);
 Shape(Parts[23],CubeMesh,FVector(125,32,141),FVector(2,64,180),GlassMaterial,true);
 for(int32 I=19;I<=23;++I)Parts[I]->SetCastShadow(false);
 Cube(24,FVector(123,-32,141),FVector(3,3,182),3,false);
 Cube(25,FVector(123,32,141),FVector(3,3,182),3,false);
 Shape(Parts[26],CylinderMesh,FVector(190,0,59),FVector(48,48,12),Palette[0],false);
 Shape(Parts[27],CylinderMesh,FVector(190,0,84),FVector(12,12,42),Palette[3],false);
 Shape(Parts[28],SphereMesh,FVector(190,0,117),FVector(48),Palette[3],false);
 Cube(29,FVector(190,0,145),FVector(12,12,26),3,false);
 Parts[29]->SetRelativeRotation(FRotator(0,0,45));
 for(int32 I=0;I<2;++I)
 {
  Shape(Parts[30+I],SphereMesh,FVector(122,I?35:-35,40),FVector(18),Palette[4],false);
  Lamps[I]=UMaterialInstanceDynamic::Create(Palette[4],this);Parts[30+I]->SetMaterial(0,Lamps[I]);
  Parts[30+I]->SetOverlayMaterial(UMaterialInstanceDynamic::Create(GlowMaterial,this));
 }
 ResetMechanism();
}

void AWenSiCircuitMechanism::BeginPlay(){Super::BeginPlay();Configure();}
void AWenSiCircuitMechanism::Tick(float Dt)
{
 Super::Tick(Dt);
 DoorSlide=FMath::FInterpConstantTo(DoorSlide,bRewardOpen?75.f:0.f,Dt,70.f);
 Parts[22]->SetRelativeLocation(FVector(125,-32-DoorSlide,141));
 Parts[23]->SetRelativeLocation(FVector(125,32+DoorSlide,141));
 Parts[24]->SetRelativeLocation(FVector(123,-32-DoorSlide,141));
 Parts[25]->SetRelativeLocation(FVector(123,32+DoorSlide,141));
 Parts[1]->SetRelativeLocation(FVector(0,-200,bPressureActive?9.f:12.f));
}
void AWenSiCircuitMechanism::SetState(bool bPressure,bool bElectricity,bool bOpen)
{
 bPressureActive=bPressure;bCircuitActive=bElectricity;bRewardOpen=bOpen;
 for(int32 I=0;I<2;++I)
 {
  const bool bOn=I?bElectricity:bPressure;
  if(Lamps[I])Lamps[I]->SetVectorParameterValue(TEXT("Tint"),bOn?FLinearColor(.2f,.65f,.12f):FLinearColor(.35f,.035f,.018f));
  if(auto Overlay=Cast<UMaterialInstanceDynamic>(Parts[30+I]->GetOverlayMaterial()))
  {
   Overlay->SetVectorParameterValue(TEXT("Tint"),bOn?FLinearColor(.6f,1.f,.35f):FLinearColor(.4f,.035f,.02f));
   Overlay->SetScalarParameterValue(TEXT("Strength"),bOn?1.5f:.4f);
   Overlay->SetScalarParameterValue(TEXT("OutlineWidth"),2.f);
  }
 }
}
void AWenSiCircuitMechanism::ResetMechanism(){DoorSlide=0.f;SetState(false,false,false);Tick(0.f);}
bool AWenSiCircuitMechanism::HasPressure() const
{
 const FBox Plate=BoxAt(this,FVector(0,-200,15),FVector(52,52,5));
 for(TActorIterator<AWenSiObject> It(GetWorld());It;++It)
  if(It->Rule==EWenSiRule::Heavy)
   for(auto Component:TInlineComponentArray<UStaticMeshComponent*>(*It))
    if(Component->IsVisible() && Component->GetStaticMesh() && Component->Bounds.GetBox().Intersect(Plate)) return true;
 return false;
}
bool AWenSiCircuitMechanism::HasConductiveBridge() const
{
 const FBox Left=BoxAt(this,FVector(-32,160,100),FVector(5,16,21));
 const FBox Right=BoxAt(this,FVector(32,160,100),FVector(5,16,21));
 for(TActorIterator<AWenSiObject> It(GetWorld());It;++It)
  if(It->Rule==EWenSiRule::Conductive)
   for(auto Component:TInlineComponentArray<UStaticMeshComponent*>(*It))
    if(Component->IsVisible() && Component->GetStaticMesh())
    {
     const FBox Bounds=Component->Bounds.GetBox();
     if(Bounds.Intersect(Left) && Bounds.Intersect(Right)) return true;
    }
 return false;
}

AWenSiResetButton::AWenSiResetButton()
{
 auto Root=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));SetRootComponent(Root);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));CylinderMesh=Cylinder.Object;
 for(int32 I=0;I<12;++I)
 {
  auto Part=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("ResetPart%d"),I));
  Part->SetupAttachment(Root);Parts.Add(Part);
 }
 Button=Parts[2];
}
void AWenSiResetButton::Configure()
{
 auto Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto Dark=MuseumSurface(TEXT("MI_Charcoal"));auto Brass=MuseumSurface(TEXT("MI_Brass"));auto Red=MuseumSurface(TEXT("MI_Burgundy"));
 GlowMaterial=MuseumSurface(TEXT("M_InteractiveRim"));
 Shape(Parts[0],Cube,FVector(0,0,44),FVector(40,38,88),Dark,true);
 Shape(Parts[1],Cube,FVector(0,0,91),FVector(50,46,8),Brass,true);
 Shape(Button,CylinderMesh,FVector(-4,0,100),FVector(30,30,10),Red,true);
 Button->SetOverlayMaterial(UMaterialInstanceDynamic::Create(GlowMaterial,this));
 // A curved arrow icon identifies reset without adding puzzle-description text.
 for(int32 I=0;I<8;++I)
 {
  const float Angle=FMath::DegreesToRadians(float(I)*35.f);
  Shape(Parts[3+I],Cube,FVector(-4+9*FMath::Cos(Angle),9*FMath::Sin(Angle),106),FVector(4,2,1),Brass,false);
  Parts[3+I]->SetRelativeRotation(FRotator(0,float(I)*35.f+90.f,0));
 }
 Shape(Parts[11],Cube,FVector(5,1,106),FVector(5,5,1),Brass,false);Parts[11]->SetRelativeRotation(FRotator(0,45,0));
 SetAimed(false);
}
void AWenSiResetButton::BeginPlay(){Super::BeginPlay();Configure();}
void AWenSiResetButton::SetAimed(bool bAimed)
{
 if(auto Overlay=Cast<UMaterialInstanceDynamic>(Button->GetOverlayMaterial()))
 {
  Overlay->SetScalarParameterValue(TEXT("Strength"),bAimed?1.8f:1.f);
  Overlay->SetScalarParameterValue(TEXT("OutlineWidth"),bAimed?1.2f:.8f);
  Overlay->SetVectorParameterValue(TEXT("Tint"),FLinearColor(1.f,.88f,.68f));
 }
}

bool AWenSiPuzzle::FirstBarrierClear() const
{
 auto PC=GetWorld()->GetFirstPlayerController();
 FHitResult Hit;FCollisionQueryParams Query;
 if(PC)Query.AddIgnoredActor(PC->GetPawn());
  return !GetWorld()->SweepSingleByChannel(Hit,GetActorTransform().TransformPosition(FVector(60,0,90)),GetActorTransform().TransformPosition(FVector(460,0,90)),GetActorQuat(),ECC_Pawn,FCollisionShape::MakeCapsule(32,88),Query);
}
bool AWenSiPuzzle::CanThinkAbout(const AWenSiObject* Object) const
{return Object && (!bCircuitPuzzle || Object->ObjectKind<5 || bStageTwoUnlocked);}
void AWenSiPuzzle::UpdateCircuitProgress()
{
 if(!bStageOneSolved) bStageOneSolved=FirstBarrierClear();
 if(!bStageTwoUnlocked)
 {
  bool bScreenVoid=false;
  for(auto Object:Objects)if(Object->ObjectKind==3 && Object->Rule==EWenSiRule::Intangible)bScreenVoid=true;
  bStageTwoUnlocked=bScreenVoid && bStageOneSolved;
 }
 if(!Circuit)return;
 const bool bPressure=bStageTwoUnlocked && Circuit->HasPressure();
 const bool bElectricity=bStageTwoUnlocked && Circuit->HasConductiveBridge();
 if(!bComplete && bPressure && bElectricity)
 {
  bComplete=true;if(bEditing)ToggleEdit();Thoughts[0].Reset();Thoughts[1].Reset();
  UMiningHapticsSubsystem::Emit(this, EMiningHapticCue::PuzzleComplete);
 }
 Circuit->SetState(bPressure,bElectricity,bComplete);
}
void AWenSiPuzzle::ResetPuzzle()
{
 auto PC=GetWorld()->GetFirstPlayerController();
 if(FSlateApplication::IsInitialized())FSlateApplication::Get().CancelDragDrop();
 if(bEditing)ToggleEdit();
 if(PC)
 {
  // Move the pawn out first, before a previously intangible barrier becomes solid.
  if(auto Pawn=PC->GetPawn())
  {
   if(auto Character=Cast<ACharacter>(Pawn))
   {
    if(auto Movement=Cast<UMiningCharacterMovementComponent>(Character->GetCharacterMovement())) Movement->ResetFlightInput();
    Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);Character->GetCharacterMovement()->StopMovementImmediately();
   }
   Pawn->SetActorTransform(PlayerResetTransform,false,nullptr,ETeleportType::TeleportPhysics);
   if(auto WenSiPawn=Cast<AWenSiPawn>(Pawn))WenSiPawn->bFlightNeedsRelease=true;
  }
  PC->SetControlRotation(PlayerResetTransform.Rotator());PC->ResetIgnoreInputFlags();PC->SetInputMode(FInputModeGameOnly());PC->bShowMouseCursor=false;
 }
 for(int32 I=0;I<Objects.Num();++I)
  if(InitialObjectTransforms.IsValidIndex(I) && InitialObjectRules.IsValidIndex(I))
  {
   Objects[I]->Rest=InitialObjectTransforms[I].GetLocation();
   Objects[I]->SetActorTransform(InitialObjectTransforms[I]);Objects[I]->Rule=InitialObjectRules[I];Objects[I]->ApplyRule();
  }
 bEditing=false;bComplete=false;bStageOneSolved=false;bStageTwoUnlocked=false;bAimReset=false;Aim=INDEX_NONE;
 Thoughts[0].Reset();Thoughts[1].Reset();if(Circuit)Circuit->ResetMechanism();if(UI)UI->Refresh();
 UE_LOG(LogTemp,Display,TEXT("WenSi reset: all initial words/transforms restored; stage=1; thoughts, locks and flight cleared"));
}
