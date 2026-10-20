#include "WenSi/WenSiMirrorPuzzle.h"
#include "MiningHapticsSubsystem.h"
#include "MiningGamepadSubsystem.h"
#include "BrainMiningFonts.h"
#include "MiningInteractionTrace.h"
#include "WenSi/MouseMuseumPortal.h"
#include "Components/WidgetComponent.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "WenSi/WenSiExhibitArt.h"
#include "MiningRoomAudioSubsystem.h"
#include "WenSi/WenSiMirrorMechanismLayout.h"
#include "WenSi/WenSiPuzzle.h"
#include "MiningComponent.h"
#include "MiningBombToolComponent.h"
#include "Showcase/MiningExhibitHall.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogWenSiMirror,Log,All);
namespace
{
 bool IsCampaignMirrorMap(const UWorld* World,bool bAreaManaged)
 {
  return bAreaManaged || (World && World->GetMapName().Contains(TEXT("Lvl_WenSi_MirrorMuseum1")));
 }
 void TuneCampaignMirrorGeometry(UWorld* World,const FTransform& PuzzleTransform,bool bAreaManaged)
 {
  if(!IsCampaignMirrorMap(World,bAreaManaged))return;
  // Open the pocket directly above the first cage in the MainGame copy. The
  // source level contains a named cap for the pocket; older saved copies do
  // not always preserve that label, so retain the local-space fallback.
  for(TActorIterator<AActor> It(World);It;++It)
  {
   AActor* Actor=*It;if(!Actor)continue;
   const FVector Local=PuzzleTransform.InverseTransformPosition(Actor->GetActorLocation());
   const FString Label=
#if WITH_EDITOR
    Actor->GetActorLabel();
#else
    FString();
#endif
   const bool bPocketCap=Label.Contains(TEXT("LiftPocketCeiling")) ||
    (FMath::Abs(Local.X-650.f)<85.f && FMath::Abs(Local.Y)<145.f && FMath::Abs(Local.Z-430.f)<35.f);
   if(bPocketCap)
   {
    Actor->SetActorEnableCollision(false);
    Actor->SetActorHiddenInGame(true);
    Actor->Tags.AddUnique(TEXT("WenSiStageOneLiftPocket"));
   }
  }
 }
 UMaterialInterface* Surface(const TCHAR* Name)
 {
  return LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/WenSi/Museum/Materials/"))+Name+TEXT(".")+Name));
 }
 FString WordText(EWenSiMirrorWord Word)
 {
  static const TCHAR* Names[]={TEXT("普通"),TEXT("沉重"),TEXT("悬浮"),TEXT("可穿过"),TEXT("导电"),TEXT("大"),TEXT("小")};
  return Names[int32(Word)];
 }
 float WordScale(EWenSiMirrorWord Word){return Word==EWenSiMirrorWord::Small?.38f:Word==EWenSiMirrorWord::Big?1.45f:1.f;}
 bool IsSelf(const AWenSiMirrorExhibit* Exhibit){return Exhibit && (Exhibit->Kind==10 || Exhibit->Kind==11);}
 FBox ExhibitBounds(const AWenSiMirrorExhibit* Exhibit)
 {
  FBox Box(ForceInit);
  for(auto Part:Exhibit->Parts)if(Part->IsVisible() && Part->GetStaticMesh()){Part->UpdateBounds();Box+=Part->Bounds.GetBox();}
  return Box;
 }
}

AWenSiMirrorExhibit::AWenSiMirrorExhibit()
{
 PrimaryActorTick.bCanEverTick=true;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
 for(int32 I=0;I<12;++I)
 {
  auto Part=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Part%d"),I));
  Part->SetupAttachment(RootComponent);Part->SetMobility(EComponentMobility::Movable);Parts.Add(Part);
 }
 Capture=CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("MirrorCapture"));
 Capture->SetupAttachment(RootComponent);Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;
 Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;
 Capture->bAlwaysPersistRenderingState=true;
 Capture->bEnableClipPlane=true;Capture->bOverride_CustomNearClippingPlane=true;Capture->CustomNearClippingPlane=1.f;
 ArtVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArtVisual"));ArtVisual->SetupAttachment(RootComponent);ArtVisual->SetMobility(EComponentMobility::Movable);ArtVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 ConductiveFX=CreateDefaultSubobject<UNiagaraComponent>(TEXT("ConductiveParticles"));ConductiveFX->SetupAttachment(RootComponent);ConductiveFX->SetAutoActivate(false);
 if(auto* Spark=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Realistic_Starter_VFX_Pack_Niagara/Niagara/Impact/NS_Impact_Electric.NS_Impact_Electric")))ConductiveFX->SetAsset(Spark);
}
FString AWenSiMirrorExhibit::Noun() const
{
 static const TCHAR* Names[]={TEXT("石块"),TEXT("罩子"),TEXT("雕像"),TEXT("气球"),TEXT("烟雾"),TEXT("屏风"),TEXT("压块"),TEXT("罩子"),TEXT("金属块"),TEXT("陶瓷球"),TEXT("矿工"),TEXT("矿工"),TEXT("摆件")};
 return DisplayNoun.IsEmpty()?FString(Names[FMath::Clamp(Kind,0,12)]):DisplayNoun;
}
FString AWenSiMirrorExhibit::Adjective() const{return WordText(Word);}
void AWenSiMirrorExhibit::RefreshArtModel(){Configure();}
void AWenSiMirrorExhibit::Configure()
{
 auto Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 auto Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
 for(auto Part:Parts){Part->SetVisibility(false);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetStaticMesh(nullptr);Part->SetOverlayMaterial(nullptr);}
 auto Shape=[&](int32 I,FVector P,FVector Size,UStaticMesh* Mesh=nullptr)
 {
  auto Part=Parts[I];Part->SetStaticMesh(Mesh?Mesh:Cube);Part->SetRelativeLocation(P);Part->SetRelativeScale3D(Size/100.f);Part->SetVisibility(true);
 };
 switch(Kind)
 {
  // The stone remains wide enough to seal the corridor for a normal miner,
  // while its smaller silhouette leaves a visibly different gap once the
  // cage is lifted.  Keep the collision envelope and the authored read in
  // sync so the player cannot mistake the two openings for one another.
  case 0: Shape(0,FVector(0,0,-18),FVector(76,166,154));break;
 case 1:
  Shape(0,FVector(0,0,120),FVector(150,228,14));
  Shape(1,FVector(0,-108,0),FVector(150,12,240));Shape(2,FVector(0,108,0),FVector(150,12,240));
  for(int32 I=0;I<4;++I)Shape(3+I,FVector(68,-75+I*50,0),FVector(12,10,240));
  break;
 case 2:
 case 12:
  Shape(0,FVector(0,0,0),FVector(48,45,66));Shape(1,FVector(0,0,49),FVector(36),Sphere);
  Shape(2,FVector(0,-35,-2),FVector(22,20,50));Shape(3,FVector(0,35,-2),FVector(22,20,50));
  Shape(4,FVector(0,-14,-52),FVector(22,20,40));Shape(5,FVector(0,14,-52),FVector(22,20,40));
  Shape(6,FVector(0,0,-80),FVector(80,70,12));break;
 case 3:
  Shape(0,FVector::ZeroVector,FVector(95,95,110),Sphere);Shape(1,FVector(0,0,-40),FVector(10),Cylinder);
  Shape(2,FVector(0,0,-66),FVector(1.5,1.5,46),Cylinder);break;
 case 4:
  for(int32 I=0;I<7;++I)Shape(I,FVector((I%3-1)*15,(I%2?1:-1)*12,I*10),FVector(40,40,40),Sphere);break;
 case 5:
  Shape(0,FVector::ZeroVector,FVector(18,220,236));Shape(1,FVector(0,-114,0),FVector(30,12,240));Shape(2,FVector(0,114,0),FVector(30,12,240));break;
 case 6: Shape(0,FVector::ZeroVector,FVector(85,85,80));break;
 case 7:
  Shape(0,FVector(0,0,130),FVector(200,200,10));Shape(1,FVector(94,0,0),FVector(12,200,260));
  Shape(2,FVector(0,-94,0),FVector(200,12,260));Shape(3,FVector(0,94,0),FVector(200,12,260));
  Shape(4,FVector(-94,-67.5,0),FVector(12,65,260));Shape(5,FVector(-94,67.5,0),FVector(12,65,260));
  Shape(6,FVector(-94,0,-32.5),FVector(12,70,15));
  Shape(7,FVector(-94,0,85),FVector(12,70,90));
  Shape(8,FVector(-94,0,7.5),FVector(3,70,65));
  // 60 x 90 cm clear opening fits the shrunken player, including floor clearance.
  Shape(9,FVector(-94,-32.5,-85),FVector(12,5,90));
  Shape(10,FVector(-94,32.5,-85),FVector(12,5,90));break;
 case 8:Shape(0,FVector::ZeroVector,FVector(38,42,56));break;
 case 9:Shape(0,FVector::ZeroVector,FVector(72),Sphere);break;
 case 10:
 case 11:
  Shape(0,FVector::ZeroVector,FVector(3,125,245));
  Shape(1,FVector(0,-68,0),FVector(10,10,260));Shape(2,FVector(0,68,0),FVector(10,10,260));
  Shape(3,FVector(0,0,-127),FVector(10,145,10));Shape(4,FVector(0,0,127),FVector(10,145,10));break;
 }
 Apply();
}
void AWenSiMirrorExhibit::Apply()
{
 const float Scale=IsSelf(this)?1.f:WordScale(Word);
 SetActorScale3D(FVector(Scale));
 auto Parent=Surface(Kind==3?TEXT("MI_Burgundy"):Kind==8||Kind==1||Kind==7?TEXT("MI_Brass"):Kind==5?TEXT("MI_DeepBlue"):TEXT("MI_Limestone"));
 auto Glass=Surface(TEXT("M_DisplayGlass"));auto Rim=Surface(TEXT("M_InteractiveRim"));
 auto Ghost=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/EngineDebugMaterials/M_SimpleTranslucent.M_SimpleTranslucent"));
 for(int32 I=0;I<Parts.Num();++I)
 {
  auto Part=Parts[I];if(!Part->IsVisible())continue;
  Part->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Part->SetCollisionObjectType(ECC_WorldDynamic);Part->SetCollisionResponseToAllChannels(ECR_Block);
  if(Word==EWenSiMirrorWord::Passable && !IsSelf(this)){Part->SetCollisionResponseToAllChannels(ECR_Ignore);Part->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);}
  if(Kind==3 && I>0){Part->SetCollisionResponseToAllChannels(ECR_Ignore);Part->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);}
  if(Kind==4 && Word!=EWenSiMirrorWord::Heavy){Part->SetCollisionResponseToAllChannels(ECR_Ignore);Part->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);}
  UMaterialInterface* Use=(Word==EWenSiMirrorWord::Passable && !IsSelf(this))||Kind==4?Ghost:Parent;
  // A clear pane exposes the prize visually while retaining the puzzle's thinking/collision barrier.
  if(Kind==7 && I==8 && Word!=EWenSiMirrorWord::Passable && Glass)Use=Glass;
  if(Use)
  {
   auto Mat=UMaterialInstanceDynamic::Create(Use,this);Mat->SetVectorParameterValue(TEXT("Color"),FLinearColor(.68f,.72f,.78f,.2f));Mat->SetScalarParameterValue(TEXT("Opacity"),Kind==4?.18f:.15f);Part->SetMaterial(0,Mat);
  }
  if(Rim && !Part->GetOverlayMaterial())Part->SetOverlayMaterial(UMaterialInstanceDynamic::Create(Rim,this));
 }
 if(IsSelf(this)&&MirrorTarget)
 {
  auto Mat=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WenSi/Mirror/Materials/M_WenSiMirror.M_WenSiMirror")),this);
  if(Mat){Mat->SetTextureParameterValue(TEXT("MirrorTexture"),MirrorTarget);Parts[0]->SetMaterial(0,Mat);}
 }
 // Word scaling is applied around the actor origin. Re-seat the exhibit on
 // the same support plane after a big/small exchange so the enlarged mesh
 // does not sink through its plinth and the small mesh does not float.
 if(bRestBoundsInitialized && !(Kind==9 && bBallLanded))
 {
  const FBox Bounds=ExhibitBounds(this);
  if(Bounds.IsValid)
  {
   const float Delta=RestBoundsMinZ-Bounds.Min.Z;
   if(!FMath::IsNearlyZero(Delta,.01f)) AddActorWorldOffset(FVector(0,0,Delta),false,nullptr,ETeleportType::TeleportPhysics);
  }
 }
 WenSiExhibitArt::Apply(ArtVisual,Parts,ArtModel,ArtRotation,bUniformArtScale,Word==EWenSiMirrorWord::Passable,Ghost,Rim);
 if(Kind==7 && Word!=EWenSiMirrorWord::Passable && Parent)
  for(int32 I=0;I<ArtVisual->GetNumMaterials();++I)ArtVisual->SetMaterial(I,Parent);
 if(Kind==3 || Kind==7)
 {
  // The window and balloon tether are represented by the separate proxy parts.
  // The tether stretches independently; a fixed authored string cannot follow the ceiling balloon.
  ArtVisual->SetVisibility(false);
  for(auto Part:Parts)if(Part->GetStaticMesh()){Part->SetRenderInMainPass(true);Part->SetRenderInDepthPass(true);Part->SetCastShadow(true);if(Kind==7 && Rim && !Part->GetOverlayMaterial())Part->SetOverlayMaterial(UMaterialInstanceDynamic::Create(Rim,this));}
 }
 if(ConductiveFX)
 {
  ConductiveFX->SetVisibility(Word==EWenSiMirrorWord::Conductive);
  if(Word==EWenSiMirrorWord::Conductive)ConductiveFX->Activate(true);else ConductiveFX->Deactivate();
 }
 for(auto Arc:ConductiveArcs)Arc->SetVisibility(Word==EWenSiMirrorWord::Conductive);
 ConductiveClock=0;
 Highlight(false);
}
void AWenSiMirrorExhibit::Highlight(bool bAimed)
{
 WenSiExhibitArt::Highlight(ArtVisual,bThinkingEnabled ? (bAimed ? 2.2f:.8f):0.f,bAimed ? 4.0f:1.8f,FLinearColor(1.f,.89f,.7f));
 for(auto Part:Parts)if(auto Glow=Cast<UMaterialInstanceDynamic>(Part->GetOverlayMaterial()))
 {Glow->SetScalarParameterValue(TEXT("Strength"),bThinkingEnabled?(bAimed?4.5f:1.8f):0.f);Glow->SetScalarParameterValue(TEXT("OutlineWidth"),bAimed?4.0f:1.8f);Glow->SetVectorParameterValue(TEXT("Tint"),FLinearColor(1.f,.89f,.7f));}
}
void AWenSiMirrorExhibit::BeginPlay()
{
 Super::BeginPlay();Rest=GetActorLocation();Configure();
 if(Kind==3){Rest.Z+=195;SetActorLocation(Rest);}
 const FBox InitialBounds=ExhibitBounds(this);
 if(InitialBounds.IsValid){RestBoundsMinZ=InitialBounds.Min.Z;bRestBoundsInitialized=true;}
 if(Kind==2)
 {
  SetActorHiddenInGame(true);SetActorEnableCollision(false);bThinkingEnabled=false;
 }
 if(IsSelf(this))
 {
  MirrorTarget=NewObject<UTextureRenderTarget2D>(this);MirrorTarget->InitAutoFormat(384,576);MirrorTarget->Filter=TF_Bilinear;MirrorTarget->ClearColor=FLinearColor(.03f,.04f,.055f);
  Capture->TextureTarget=MirrorTarget;Capture->HiddenActors.Add(this);Apply();
  BuildMirrorPresence();
 }
}
void AWenSiMirrorExhibit::UpdateMirror()
{
 auto PC=GetWorld()->GetFirstPlayerController();if(!PC)return;
 FVector Eye;FRotator Rot;PC->GetPlayerViewPoint(Eye,Rot);
 const FVector Normal=GetActorForwardVector();const FVector Plane=GetActorLocation()+Normal*3.f;
 const float Distance=FVector::DotProduct(Eye-Plane,Normal);
 if(Distance<=5.f || Distance>1800.f)return;
 const FVector Reflected=Eye-2.f*Distance*Normal;
 // A capture-only glowing egg provides a presence for the first-person pawn.
 if(MirrorPresence && PC->GetPawn())
 {
  MirrorPresence->SetActorLocation(PC->GetPawn()->GetActorLocation());
  MirrorPresence->SetActorRotation(FRotator(0,Rot.Yaw,0));
  MirrorPresence->SetActorScale3D(PC->GetPawn()->GetActorScale3D());
  // Keep the light full and round, with a small, slow squash-and-stretch.
  // Animate the two soft shells only; the held pickaxe stays steady.
  const float Breath=FMath::Sin(GetWorld()->GetTimeSeconds()*2.4f);
  const FVector Squash(1.f+Breath*.025f,1.f+Breath*.025f,1.f-Breath*.04f);
  const FVector ShellSizes[]={FVector(1.04f,1.08f,1.20f),FVector(1.24f,1.28f,1.40f)};
  for(int32 I=0;I<2 && I<PresenceParts.Num();++I)
  {
   PresenceParts[I]->SetRelativeScale3D(ShellSizes[I]*Squash);
   PresenceParts[I]->SetRelativeLocation(FVector(0,0,-2.f+Breath*1.5f));
  }
 }
 const FVector Forward=Normal,Up=GetActorUpVector();
 const FVector Right=FVector::CrossProduct(Up,Forward).GetSafeNormal();
 // Off-axis projection pins the rendered window to the physical mirror plane.
 // A head-aimed symmetric camera otherwise makes the reflection follow the crosshair.
 const FVector Center=Plane-Reflected;const double Width=125,Height=245;
 FMatrix Projection=FMatrix::Identity;
 Projection.M[0][0]=2*Distance/Width;Projection.M[1][1]=2*Distance/Height;
 Projection.M[2][0]=-2*FVector::DotProduct(Center,Right)/Width;
 Projection.M[2][1]=-2*FVector::DotProduct(Center,Up)/Height;
 Projection.M[2][2]=0;Projection.M[2][3]=1;Projection.M[3][2]=1;Projection.M[3][3]=0;
 Capture->bUseCustomProjectionMatrix=true;Capture->CustomProjectionMatrix=Projection;
 // Stock projects can have shader clip planes disabled. Exclude geometry behind
 // the mirror so the reflected camera cannot be occluded by the museum wall.
 Capture->HiddenActors.Reset();Capture->HiddenActors.Add(this);
 if(PC->GetPawn())Capture->HiddenActors.Add(PC->GetPawn());
 for(TActorIterator<AActor> It(GetWorld());It;++It)
 {
  if(*It==this || *It==PC->GetPawn())continue;
  auto Exhibit=Cast<AWenSiMirrorExhibit>(*It);
  if(Kind==11 && Exhibit && Exhibit->Kind==7){Capture->HiddenActors.Add(*It);continue;}
  const FBox Bounds=Exhibit?ExhibitBounds(Exhibit):It->GetComponentsBoundingBox(true);
  if(!Bounds.IsValid)continue;
  const double Furthest=FVector::DotProduct(Bounds.GetCenter()-Plane,Normal)+FVector::DotProduct(Bounds.GetExtent(),Normal.GetAbs());
  if(Furthest<0.f)Capture->HiddenActors.Add(*It);
 }
 Capture->SetWorldLocationAndRotation(Reflected,FRotationMatrix::MakeFromXZ(Forward,Up).Rotator());
 Capture->ClipPlaneBase=Plane;Capture->ClipPlaneNormal=Normal;Capture->FOVAngle=90.f;
 Capture->CaptureScene();
}
void AWenSiMirrorExhibit::Tick(float Dt)
{
 Super::Tick(Dt);
 UpdateConductive(Dt);
 if(IsSelf(this)){MirrorClock+=Dt;if(MirrorClock>.066f){MirrorClock=0;UpdateMirror();}return;}
 float Target=Rest.Z;
 if(Word==EWenSiMirrorWord::Floating)
 {
  Target=Rest.Z+(Kind==0?230.f:Kind==2?125.f:Kind==3?0.f:90.f);
  if(Kind==0)
   for(TActorIterator<AWenSiMirrorExhibit> It(GetWorld());It;++It)
    if(It->Kind==1 && It->Word!=EWenSiMirrorWord::Passable)Target=FMath::Min(Target,It->GetActorLocation().Z+120.f-7.f-90.f-2.f);
 }
 if(Kind==3)
 {
  if(Word!=EWenSiMirrorWord::Floating)Target=Rest.Z-230.f;
  const float Anchor=Rest.Z-301.f,Bottom=GetActorLocation().Z-55.f*GetActorScale3D().Z;
  Parts[2]->SetWorldLocation(FVector(Rest.X,Rest.Y,(Anchor+Bottom)*.5f));
  Parts[2]->SetWorldScale3D(FVector(.015f,.015f,FMath::Max(1.f,Bottom-Anchor)/100.f));
 }
 if(Kind==6 && Word==EWenSiMirrorWord::Heavy)Target=Rest.Z-WenSiMuseumMechanisms::BalanceTravel;
 if(Kind==6 && Word==EWenSiMirrorWord::Ordinary)Target=Rest.Z-WenSiMuseumMechanisms::BalanceTravel*WenSiMuseumMechanisms::InitialBalanceAlpha;
 if(Kind==9)
 {
  if(Word==EWenSiMirrorWord::Small || bBallLanded)Target=Rest.Z-177.f;
  if(bBallLanded && Word==EWenSiMirrorWord::Floating)Target=Rest.Z-92.f;
 }
 FVector P=GetActorLocation();P.Z=FMath::FInterpConstantTo(P.Z,Target,Dt,Kind==9?130.f:100.f);
 if(!P.Equals(GetActorLocation(),.01f))
 {
  auto PC=GetWorld()->GetFirstPlayerController();auto Character=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
  const bool bBlocks=Word!=EWenSiMirrorWord::Passable && Kind!=4;
  if(!Character || !bBlocks || !ExhibitBounds(this).ShiftBy(P-GetActorLocation()).Intersect(Character->GetCapsuleComponent()->Bounds.GetBox()))
  {
   SetActorLocation(P);
   if(auto Hall=AMiningExhibitHall::Find(GetWorld());Hall && Hall->AreaKind(Hall->ActiveArea)==EMiningExperienceKind::Museum)
    UMiningRoomAudioSubsystem::Loop(this,TEXT("S_Mechanism_Loop"),.16f,true);
  }
 }
 if(Kind==9 && Word==EWenSiMirrorWord::Small && GetActorLocation().Z<=Rest.Z-176.9f && !bBallLanded)
 {bBallLanded=true;UMiningRoomAudioSubsystem::Emit(this,TEXT("S_Pottery"),.45f,true);}
}

AWenSiMirrorPawn::AWenSiMirrorPawn()
{
 PrimaryActorTick.bCanEverTick=true;GetCapsuleComponent()->InitCapsuleSize(32,88);
 auto Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));Camera->SetupAttachment(GetCapsuleComponent());Camera->SetRelativeLocation(FVector(0,0,65));Camera->bUsePawnControlRotation=true;
 bUseControllerRotationYaw=true;GetCharacterMovement()->MaxWalkSpeed=300;GetCharacterMovement()->MaxStepHeight=12;
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 for(int32 I=0;I<8;++I)
 {
  auto Part=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Miner%d"),I));Part->SetupAttachment(GetCapsuleComponent());Part->SetStaticMesh(I==1||I==6?Sphere.Object:Cube.Object);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetOwnerNoSee(true);MinerParts.Add(Part);
 }
 const FVector Positions[]={FVector(0,0,4),FVector(0,0,56),FVector(0,-32,2),FVector(0,32,2),FVector(0,-14,-52),FVector(0,14,-52),FVector(10,0,69),FVector(-26,0,0)};
 const FVector Sizes[]={FVector(42,44,65),FVector(35),FVector(22,18,60),FVector(22,18,60),FVector(22,20,66),FVector(22,20,66),FVector(37,37,15),FVector(18,38,48)};
 for(int32 I=0;I<8;++I){MinerParts[I]->SetRelativeLocation(Positions[I]);MinerParts[I]->SetRelativeScale3D(Sizes[I]/100.f);}
}
bool AWenSiMirrorPawn::SetWord(EWenSiMirrorWord NewWord)
{
 const float Scale=NewWord==EWenSiMirrorWord::Small?.38f:NewWord==EWenSiMirrorWord::Big?1.45f:1.f;
 const FVector Feet=GetActorLocation()-FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
 FVector Next=Feet+FVector(0,0,88.f*Scale+.5f);
 FCollisionQueryParams Query;Query.AddIgnoredActor(this);
 // Shrinking can only reduce the occupied volume, so it is always safe to
 // apply even when the old capsule is touching a nearby surface.  Keep the
 // overlap guard for growth/solidification where a new penetration is possible.
 if(NewWord!=EWenSiMirrorWord::Passable && NewWord!=EWenSiMirrorWord::Small)
 {
  const FCollisionShape Shape=FCollisionShape::MakeCapsule(32.f*Scale,88.f*Scale-.2f);
  bool bBlocked=GetWorld()->OverlapBlockingTestByChannel(Next,FQuat::Identity,ECC_Pawn,Shape,Query);
  // If the old small capsule was placed close to the floor, lift the growing
  // body until its feet clear the support plane. This keeps growth safe while
  // still allowing the intended exchange in a low maintenance alcove.
  for(int32 Attempt=0;bBlocked&&Attempt<24;++Attempt){Next.Z+=8.f;bBlocked=GetWorld()->OverlapBlockingTestByChannel(Next,FQuat::Identity,ECC_Pawn,Shape,Query);}
  if(bBlocked)return false;
 }
 Word=NewWord;SetActorScale3D(FVector(Scale));SetActorLocation(Next,false,nullptr,ETeleportType::TeleportPhysics);
 GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic,NewWord==EWenSiMirrorWord::Passable?ECR_Ignore:ECR_Block);
 GetCharacterMovement()->MaxWalkSpeed=Scale<1.f?220.f:300.f;
 for(int32 I=0;I<MinerParts.Num();++I)if(auto Mat=Surface(I==6?TEXT("MI_Brass"):I==1?TEXT("MI_Limestone"):TEXT("MI_DeepBlue")))MinerParts[I]->SetMaterial(0,Mat);
 return true;
}
void AWenSiMirrorPawn::Tick(float Dt)
{
 Super::Tick(Dt);auto PC=Cast<APlayerController>(Controller);if(!PC || PC->IsMoveInputIgnored())return;
 float X=0,Y=0;PC->GetInputMouseDelta(X,Y);AddControllerYawInput(X);AddControllerPitchInput(-Y);
 AddMovementInput(GetActorForwardVector(),UMiningGamepadSubsystem::ReadMove(PC).Y);
 AddMovementInput(GetActorRightVector(),UMiningGamepadSubsystem::ReadMove(PC).X);
 if(!UMiningGamepadSubsystem::ReadJumpHeld(PC))bRequireFlightRelease=false;
 if((UMiningGamepadSubsystem::ReadJumpHeld(PC)&&!bRequireFlightRelease)||Word==EWenSiMirrorWord::Floating)
 {auto Movement=GetCharacterMovement();if(Movement->IsMovingOnGround())Movement->SetMovementMode(MOVE_Falling);Movement->Velocity.Z=FMath::Min(350.f,Movement->Velocity.Z+(1700.f+FMath::Abs(GetWorld()->GetGravityZ()))*Dt);}
}
AWenSiMirrorGameMode::AWenSiMirrorGameMode(){DefaultPawnClass=AWenSiMirrorPawn::StaticClass();}

AWenSiMirrorPuzzle::AWenSiMirrorPuzzle(){PrimaryActorTick.bCanEverTick=true;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));}
AWenSiMirrorExhibit* AWenSiMirrorPuzzle::Find(int32 Kind) const{for(auto Exhibit:Exhibits)if(Exhibit->Kind==Kind)return Exhibit;return nullptr;}
EWenSiMirrorWord AWenSiMirrorPuzzle::GetPlayerWord() const
{
 auto PC=GetWorld()->GetFirstPlayerController();
 if(auto Pawn=PC?Cast<AWenSiMirrorPawn>(PC->GetPawn()):nullptr)return Pawn->Word;
 return SharedPlayerWord;
}
bool AWenSiMirrorPuzzle::SetPlayerWord(EWenSiMirrorWord NewWord)
{
 auto PC=GetWorld()->GetFirstPlayerController();auto Pawn=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;if(!Pawn)return false;
 if(auto MirrorPawn=Cast<AWenSiMirrorPawn>(Pawn))return MirrorPawn->SetWord(NewWord);
 const float Scale=NewWord==EWenSiMirrorWord::Small?.38f:NewWord==EWenSiMirrorWord::Big?1.45f:1.f;
 auto Capsule=Pawn->GetCapsuleComponent();
 const float Half=Capsule->GetUnscaledCapsuleHalfHeight()*PlayerBaseScale.Z*Scale;
 const float Radius=Capsule->GetUnscaledCapsuleRadius()*PlayerBaseScale.X*Scale;
 FVector Next=Pawn->GetActorLocation()+FVector(0,0,Half-Capsule->GetScaledCapsuleHalfHeight()+.5f);
 FCollisionQueryParams Q;Q.AddIgnoredActor(Pawn);
 if(NewWord!=EWenSiMirrorWord::Passable && NewWord!=EWenSiMirrorWord::Small)
 {
  const FCollisionShape Shape=FCollisionShape::MakeCapsule(Radius,Half-.2f);
  bool bBlocked=GetWorld()->OverlapBlockingTestByChannel(Next,FQuat::Identity,ECC_Pawn,Shape,Q);
  for(int32 Attempt=0;bBlocked&&Attempt<24;++Attempt){Next.Z+=8.f;bBlocked=GetWorld()->OverlapBlockingTestByChannel(Next,FQuat::Identity,ECC_Pawn,Shape,Q);}
  if(bBlocked)return false;
 }
 SharedPlayerWord=NewWord;Pawn->SetActorScale3D(PlayerBaseScale*Scale);Pawn->SetActorLocation(Next,false,nullptr,ETeleportType::TeleportPhysics);
 Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic,NewWord==EWenSiMirrorWord::Passable?ECR_Ignore:PlayerBaseCollision);
 Pawn->GetCharacterMovement()->MaxWalkSpeed=Scale<1?220.f:300.f;
 return true;
}
void AWenSiMirrorPuzzle::SetAreaActive(bool Active)
{
 const bool WasActive=bAreaActive;bAreaActive=Active;
 if(!Active){CancelSwap();if(MousePortal)MousePortal->Cancel();}
 if(!Active && bEditing)ToggleEdit();
 if(UI)UI->SetVisibility(Active?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
 if(!Active){Aim.Reset();bAimReset=false;for(auto Exhibit:Exhibits)Exhibit->Highlight(false);}
 if(!bAreaManaged || WasActive==Active)return;
 auto PC=GetWorld()->GetFirstPlayerController();auto Pawn=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;if(!Pawn)return;
 if(Active)
 {
  if(auto* Bomb=Pawn->FindComponentByClass<UMiningBombToolComponent>())Bomb->Stow();
  if(auto* Tool=Pawn->FindComponentByClass<UMiningComponent>()){Tool->CancelMining();Tool->SelectPickaxe();}
 }
 if(Active){PlayerBaseRadius=Pawn->GetCapsuleComponent()->GetUnscaledCapsuleRadius();PlayerBaseHalfHeight=Pawn->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();PlayerBaseStepHeight=Pawn->GetCharacterMovement()->MaxStepHeight;Pawn->GetCapsuleComponent()->SetCapsuleSize(32,88);Pawn->GetCharacterMovement()->MaxStepHeight=12;PlayerBaseScale=Pawn->GetActorScale3D();PlayerBaseSpeed=Pawn->GetCharacterMovement()->MaxWalkSpeed;PlayerBaseCollision=Pawn->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_WorldDynamic);if(SharedPlayerWord!=EWenSiMirrorWord::Ordinary)SetPlayerWord(SharedPlayerWord);}
 else if(SharedPlayerWord!=EWenSiMirrorWord::Ordinary)
 {
  // The exit is full-height. Rescue/skip may leave from the tiny alcove, then teleport immediately.
  const float BeforeHalf=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  Pawn->SetActorScale3D(PlayerBaseScale);Pawn->AddActorWorldOffset(FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-BeforeHalf));Pawn->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic,PlayerBaseCollision);
 }
 if(!Active)
 {
  Pawn->GetCapsuleComponent()->SetCapsuleSize(PlayerBaseRadius,PlayerBaseHalfHeight);
  Pawn->GetCharacterMovement()->MaxWalkSpeed=PlayerBaseSpeed;Pawn->GetCharacterMovement()->MaxStepHeight=PlayerBaseStepHeight;
 }
 if(auto Mining=Pawn->FindComponentByClass<UMiningComponent>()){Mining->CancelMining();Mining->SetInputContextEnabled(!Active);Mining->SetInputSuspended(Active);}
 if(Active)UpdateMechanism();
}
void AWenSiMirrorPuzzle::BeginPlay()
{
 Super::BeginPlay();
 TuneCampaignMirrorGeometry(GetWorld(),GetActorTransform(),bAreaManaged);
 for(TActorIterator<AWenSiMirrorExhibit> It(GetWorld());It;++It){Exhibits.Add(*It);InitialTransforms.Add(It->GetActorTransform());InitialWords.Add(It->Word);}
 TActorIterator<APlayerStart> PlayerStartIt(GetWorld());
 if(PlayerStartIt)StartTransform=PlayerStartIt->GetActorTransform();
 for(TActorIterator<AActor> It(GetWorld());It;++It)
 {
  if(It->ActorHasTag(TEXT("WenSiPowerCable")) || It->ActorHasTag(TEXT("WenSiPlugSupplyCable")) || It->ActorHasTag(TEXT("WenSiBalanceCable")))
   if(auto* Mesh=It->FindComponentByClass<UStaticMeshComponent>())
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WenSi/Mirror/Materials/M_PoweredCable")))
    {auto* Mat=UMaterialInstanceDynamic::Create(Base,this);Mesh->SetMaterial(0,Mat);CableMaterials.Add(Mat);CablePressure.Add(It->ActorHasTag(TEXT("WenSiBalanceCable")));}
  if(It->ActorHasTag(TEXT("MirrorRewardShutter")))RewardShutter=*It;
  if(It->ActorHasTag(TEXT("MirrorReward")))Reward=*It;
  for(int32 I=0;I<2;++I)if(It->ActorHasTag(I?TEXT("MirrorPowerLamp"):TEXT("MirrorPressureLamp")))
   if(auto Mesh=It->FindComponentByClass<UStaticMeshComponent>()){LampMeshes.Add(Mesh);auto Mat=UMaterialInstanceDynamic::Create(Surface(TEXT("MI_Terracotta")),this);Mesh->SetMaterial(0,Mat);Lamps.Add(Mat);}
 }
 auto PC=GetWorld()->GetFirstPlayerController();if(PC){UI=CreateWidget<UWenSiMirrorWidget>(PC);UI->Puzzle=this;UI->AddToViewport(50);if(!bAreaManaged)PC->SetInputMode(FInputModeGameOnly());if(!bAreaManaged){PC->bShowMouseCursor=false;SetPlayerWord(EWenSiMirrorWord::Ordinary);}}
 if(bAreaManaged){StartTransform=AreaPlayerStart;bAreaActive=false;SetAreaActive(false);}
 if(Reward){Reward->SetActorHiddenInGame(true);Reward->SetActorEnableCollision(false);}
 MousePortal=GetWorld()->SpawnActor<AMouseMuseumPortal>();
 MousePortal->Initialize(this);
 for(TActorIterator<AActor> It(GetWorld());It;++It)
  if(It->GetActorLocation().Equals(RoomPoint(FVector(2300,-350,8)),1.f)){It->SetActorHiddenInGame(true);It->SetActorEnableCollision(false);}
 UE_LOG(LogWenSiMirror,Display,TEXT("Initialized %d exhibits; realistic initial states; exterior thinking mirror"),Exhibits.Num());
}
void AWenSiMirrorPuzzle::RefreshSelfThoughts()
{
 auto PC=GetWorld()->GetFirstPlayerController();auto Pawn=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
 if(Pawn)for(auto Exhibit:Exhibits)if(IsSelf(Exhibit))Exhibit->Word=GetPlayerWord();
}
void AWenSiMirrorPuzzle::Think()
{
 if(!Aim.IsValid() || bComplete || IsSwapping())return;RefreshSelfThoughts();
 for(int32 I=0;I<2;++I)if(Thoughts[I]==Aim || (IsSelf(Thoughts[I].Get()) && IsSelf(Aim.Get())))return;
 for(int32 I=0;I<2;++I)if(!Thoughts[I].IsValid()){Thoughts[I]=Aim;UMiningRoomAudioSubsystem::Emit(this,TEXT("S_Thought"),.45f);break;}
}
bool AWenSiMirrorPuzzle::HandleInteractionInput(APlayerController* PC)
{
 if(!PC || !bAreaActive || bEditing || PC->IsPaused())return false;
 if(bAimReset){ResetPuzzle();return true;}
 if(Aim.IsValid() && !bComplete){Think();return true;}
 return false;
}
void AWenSiMirrorPuzzle::CloseThought(int32 Index){CancelSwap();if(Index>=0&&Index<2 && Thoughts[Index].IsValid()){Thoughts[Index].Reset();UMiningRoomAudioSubsystem::Emit(this,TEXT("S_Close"),.25f);}if(UI)UI->Refresh();}
void AWenSiMirrorPuzzle::CloseThoughts()
{
 CancelSwap();
 const bool HadThoughts=Thoughts[0].IsValid() || Thoughts[1].IsValid();
 if(bEditing)ToggleEdit();
 Thoughts[0].Reset();Thoughts[1].Reset();
 if(HadThoughts)UMiningRoomAudioSubsystem::Emit(this,TEXT("S_Close"),.25f);
 if(UI)UI->Refresh();
}
void AWenSiMirrorPuzzle::SwapThoughts()
{
 auto* Controller=GetWorld()->GetFirstPlayerController();
 if(!bAreaActive || bComplete || IsSwapping() || !Controller || Controller->IsPaused())return;
 if(auto* Hall=AMiningExhibitHall::Find(GetWorld());Hall && Hall->IsInteractionOpen())return;
 if(!Thoughts[0].IsValid() || !Thoughts[1].IsValid())return;
 RefreshSelfThoughts();
 if(Thoughts[0]==Thoughts[1] || (IsSelf(Thoughts[0].Get()) && IsSelf(Thoughts[1].Get())))return;
 for(int32 Index=0;Index<2;++Index)
 {
  SwapTargets[Index]=Thoughts[Index];SwapInitialWords[Index]=Thoughts[Index]->Word;
  SwapWordsBefore[Index]=Thoughts[Index]->Adjective();
 }
 if(bEditing)ToggleEdit();
 SwapAge=0.f;bSwapCommitted=false;
 if(UI)UI->Refresh();
}
void AWenSiMirrorPuzzle::CancelSwap()
{
 SwapAge=-1.f;bSwapCommitted=false;
 for(int32 Index=0;Index<2;++Index){SwapTargets[Index].Reset();SwapWordsBefore[Index].Empty();}
}
void AWenSiMirrorPuzzle::AdvanceSwap(float DeltaTime)
{
 if(!IsSwapping())return;
 RefreshSelfThoughts();
 for(int32 Index=0;Index<2;++Index)
  if(!SwapTargets[Index].IsValid() || Thoughts[Index]!=SwapTargets[Index] ||
     (!bSwapCommitted && SwapTargets[Index]->Word!=SwapInitialWords[Index]))
  {CancelSwap();if(UI)UI->Refresh();return;}
 SwapAge+=FMath::Max(0.f,DeltaTime);
 if(!bSwapCommitted && SwapAge>=SwapTravelSeconds)
 {
  if(!SwapWords(0,1)){CancelSwap();if(UI)UI->Refresh();return;}
  bSwapCommitted=true;
 }
 if(SwapAge>=SwapTotalSeconds)CloseThoughts();
 else if(UI)UI->Refresh();
}
void AWenSiMirrorPuzzle::ToggleEdit()
{
 auto PC=GetWorld()->GetFirstPlayerController();if(!PC || (!bEditing&&(bComplete||(!Thoughts[0].IsValid()&&!Thoughts[1].IsValid()))))return;
 if(bEditing && FSlateApplication::IsInitialized())FSlateApplication::Get().CancelDragDrop();
 bEditing=!bEditing;PC->SetIgnoreMoveInput(bEditing);PC->SetIgnoreLookInput(bEditing);PC->bShowMouseCursor=bEditing;
 UMiningRoomAudioSubsystem::Emit(this,bEditing?TEXT("S_Open"):TEXT("S_Close"),.25f);
 if(bEditing){FInputModeGameAndUI Mode;Mode.SetWidgetToFocus(UI->TakeWidget());Mode.SetHideCursorDuringCapture(false);Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);PC->SetInputMode(Mode);}
 else PC->SetInputMode(FInputModeGameOnly());if(UI)UI->Refresh();
}
bool AWenSiMirrorPuzzle::SwapWords(int32 From,int32 To)
{
 if(bComplete || From==To || From<0 || From>1 || To<0 || To>1 || !Thoughts[From].IsValid() || !Thoughts[To].IsValid())return false;
 RefreshSelfThoughts();auto A=Thoughts[From].Get();auto B=Thoughts[To].Get();if(IsSelf(A)&&IsSelf(B))return false;
 auto PC=GetWorld()->GetFirstPlayerController();auto Pawn=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;if(!Pawn)return false;
 // Validate solidification and growth before changing either word, preserving atomic swaps.
 for(auto Pair:{TPair<AWenSiMirrorExhibit*,EWenSiMirrorWord>(A,B->Word),TPair<AWenSiMirrorExhibit*,EWenSiMirrorWord>(B,A->Word)})
 {
  if(IsSelf(Pair.Key) || Pair.Value==EWenSiMirrorWord::Passable || Pair.Key->Kind==4)continue;
  const float ScaleRatio=WordScale(Pair.Value)/WordScale(Pair.Key->Word);
  const FVector ProbePosition=Pair.Key->GetActorLocation()+(Pawn->GetCapsuleComponent()->GetComponentLocation()-Pair.Key->GetActorLocation())/ScaleRatio;
  const FCollisionShape ProbeShape=FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius()/ScaleRatio,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()/ScaleRatio);
  for(int32 PartIndex=0;PartIndex<Pair.Key->Parts.Num();++PartIndex)
  {
   auto* Part=Pair.Key->Parts[PartIndex].Get();
   if(!Part->IsVisible() || !Part->GetStaticMesh() || (Pair.Key->Kind==3 && PartIndex>0))continue;
   if(Part->OverlapComponent(ProbePosition,Pawn->GetActorQuat(),ProbeShape))
   {UMiningHapticsSubsystem::Emit(this, EMiningHapticCue::PuzzleRejected);UMiningRoomAudioSubsystem::Emit(this,TEXT("S_Reject"),.35f); return false;}
  }
 }
 const EWenSiMirrorWord OldA=A->Word,OldB=B->Word;
 if(IsSelf(A)&&!SetPlayerWord(OldB)){UMiningHapticsSubsystem::Emit(this, EMiningHapticCue::PuzzleRejected);UMiningRoomAudioSubsystem::Emit(this,TEXT("S_Reject"),.35f); return false;}if(IsSelf(B)&&!SetPlayerWord(OldA)){UMiningHapticsSubsystem::Emit(this, EMiningHapticCue::PuzzleRejected);UMiningRoomAudioSubsystem::Emit(this,TEXT("S_Reject"),.35f); return false;}
 A->Word=OldB;B->Word=OldA;if(!IsSelf(A))A->Apply();if(!IsSelf(B))B->Apply();RefreshSelfThoughts();
 UMiningHapticsSubsystem::Emit(this, EMiningHapticCue::PuzzleSwap);
 UMiningRoomAudioSubsystem::Emit(this,TEXT("S_Swap"),.45f);
 for(auto Exhibit:{A,B})
 {
  const FName Cue=Exhibit->Word==EWenSiMirrorWord::Small?TEXT("S_Shrink"):Exhibit->Word==EWenSiMirrorWord::Big?TEXT("S_Grow"):Exhibit->Word==EWenSiMirrorWord::Passable?TEXT("S_Phase"):Exhibit->Word==EWenSiMirrorWord::Floating?TEXT("S_Float"):TEXT("S_Transform");
  UMiningRoomAudioSubsystem::Emit(Exhibit,Cue,.2f,!IsSelf(Exhibit));
 }
 UE_LOG(LogWenSiMirror,Display,TEXT("Swap: %s=%s; %s=%s"),*A->Noun(),*A->Adjective(),*B->Noun(),*B->Adjective());if(UI)UI->Refresh();
 return true;
}
void AWenSiMirrorPuzzle::UpdateMechanism()
{
 const bool WasPressure=bPressure,WasPower=bPower;
 auto Block=Find(6);auto Ball=Find(9);
 float BalanceAlpha=Block?FMath::Clamp(float((Block->Rest.Z-Block->GetActorLocation().Z)/WenSiMuseumMechanisms::BalanceTravel),0.f,1.f):0.f;
 // The unassigned counterweight starts slightly heavy so the beam is visibly
 // out of balance before the pressure block is transferred.
 if(Block && Block->Word==EWenSiMirrorWord::Ordinary)BalanceAlpha=WenSiMuseumMechanisms::InitialBalanceAlpha;
 WenSiMuseumMechanisms::UpdateBalance(GetWorld(),BalanceAlpha,GetActorLocation());
 bPressure=Block && Block->Word==EWenSiMirrorWord::Heavy && BalanceAlpha>=.99f;
 bPower=Ball && Ball->bBallLanded && Ball->Word==EWenSiMirrorWord::Conductive && FMath::Abs(Ball->GetActorLocation().Z-RoomPoint(FVector(0,0,80)).Z)<1.f;
 if(bPressure!=WasPressure)UMiningRoomAudioSubsystem::Emit(Block,bPressure?TEXT("S_Pressure"):TEXT("S_PowerOff"),.6f,true);
 if(bPower!=WasPower)UMiningRoomAudioSubsystem::Emit(Ball,bPower?TEXT("S_Power"):TEXT("S_PowerOff"),.6f,true);
 if(bPressure && bPower && !bRewardUnlocked){bRewardUnlocked=true;UMiningHapticsSubsystem::Emit(this, EMiningHapticCue::PuzzleComplete);UMiningRoomAudioSubsystem::Emit(RewardShutter,TEXT("S_RewardDoor"),.6f,true);UE_LOG(LogWenSiMirror,Display,TEXT("Both physical locks active; mouse museum hatch unlocked"));}
 if(RewardShutter)RewardShutter->SetActorLocation(RoomPoint(FVector(2190,-350,FMath::FInterpConstantTo(RewardShutter->GetActorLocation().Z-GetActorLocation().Z,bRewardUnlocked?138.f:45.f,GetWorld()->GetDeltaSeconds(),90.f))));
 for(int32 I=0;I<CableMaterials.Num();++I)CableMaterials[I]->SetScalarParameterValue(TEXT("Powered"),(CablePressure[I]?bPressure:bPower)?1.f:0.f);
 for(int32 I=0;I<Lamps.Num();++I){const bool On=I?bPower:bPressure;Lamps[I]->SetVectorParameterValue(TEXT("Tint"),On?FLinearColor(.12,.75,.2):FLinearColor(.55,.025,.015));if(auto Overlay=Cast<UMaterialInstanceDynamic>(LampMeshes[I]->GetOverlayMaterial())){Overlay->SetVectorParameterValue(TEXT("Tint"),On?FLinearColor(.35,1,.45):FLinearColor(.5,.04,.025));Overlay->SetScalarParameterValue(TEXT("Strength"),On?2.f:.45f);}}
}
void AWenSiMirrorPuzzle::Tick(float Dt)
{
 Super::Tick(Dt);auto PC=GetWorld()->GetFirstPlayerController();if(!PC || !bAreaActive || PC->IsPaused())return;
 // Suction owns a look-input lock. Advance it before the hall interprets that
 // lock as an open UI, otherwise the transition would suspend itself forever.
 if(MousePortal && MousePortal->Advance(Dt)){if(UI)UI->Refresh();return;}
 if(auto Hall=AMiningExhibitHall::Find(GetWorld());Hall && !bEditing && Hall->IsInteractionOpen())return;
 AdvanceSwap(Dt);
 if(bAreaManaged && GetPlayerWord()==EWenSiMirrorWord::Floating && !bEditing)
  if(auto Character=Cast<ACharacter>(PC->GetPawn())){auto Move=Character->GetCharacterMovement();if(Move->IsMovingOnGround())Move->SetMovementMode(MOVE_Falling);Move->Velocity.Z=FMath::Min(350.f,Move->Velocity.Z+(1700.f+FMath::Abs(GetWorld()->GetGravityZ()))*Dt);}
 RefreshSelfThoughts();UpdateMechanism();Aim.Reset();bAimReset=false;
 if(MousePortal)
 {
  MousePortal->RefreshFocus();
  if(MousePortal->IsFocused() && PC->WasInputKeyJustPressed(EKeys::E) && MousePortal->TryEnter()){if(UI)UI->Refresh();return;}
 }
 if(!bEditing)
 {
  FVector P;FRotator R;PC->GetPlayerViewPoint(P,R);FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(PC->GetPawn());
  // Passing through an object never removes it from the thinking trace.
  if(MiningInteraction::Trace(GetWorld(),Hit,P,R,310.f,Q,[this,PC](const FHitResult& Candidate)
  {
   auto* Exhibit=Cast<AWenSiMirrorExhibit>(Candidate.GetActor());
   return PC->GetPawn() && FVector::Dist(PC->GetPawn()->GetActorLocation(),Candidate.ImpactPoint)<280.f
    && (Cast<AWenSiResetButton>(Candidate.GetActor()) || (Exhibit && Exhibit->bThinkingEnabled && !bComplete));
  }))
  {
   auto Exhibit=Cast<AWenSiMirrorExhibit>(Hit.GetActor());
   const bool Nearby=PC->GetPawn() && FVector::Dist(PC->GetPawn()->GetActorLocation(),Hit.ImpactPoint)<280.f;
   if(Nearby){bAimReset=Cast<AWenSiResetButton>(Hit.GetActor())!=nullptr;if(Exhibit && Exhibit->bThinkingEnabled && !bComplete)Aim=Exhibit;}
  }
  if(PC->WasInputKeyJustPressed(EKeys::Q))CloseThoughts();
 }
 if(PC->WasInputKeyJustPressed(EKeys::R))SwapThoughts();
 if(PC->GetPawn() && PC->GetPawn()->GetActorLocation().X>RoomPoint(FVector(1000,0,0)).X)bSecondRoom=true;
 for(auto Exhibit:Exhibits)Exhibit->Highlight(Exhibit==Aim.Get());if(UI)UI->Refresh();
}
void AWenSiMirrorPuzzle::ResetPuzzle()
{
 if(MousePortal)MousePortal->Cancel();
 CancelSwap();
 UMiningRoomAudioSubsystem::Emit(this,TEXT("S_Reset"),.4f);
 for(auto Exhibit:Exhibits)UMiningRoomAudioSubsystem::StopSource(Exhibit);
 if(FSlateApplication::IsInitialized())FSlateApplication::Get().CancelDragDrop();if(bEditing)ToggleEdit();
 auto PC=GetWorld()->GetFirstPlayerController();auto Pawn=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
 if(Pawn){Pawn->SetActorLocation(StartTransform.GetLocation());SetPlayerWord(EWenSiMirrorWord::Ordinary);Pawn->SetActorTransform(StartTransform);Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);Pawn->GetCharacterMovement()->StopMovementImmediately();if(auto MirrorPawn=Cast<AWenSiMirrorPawn>(Pawn))MirrorPawn->bRequireFlightRelease=true;}
 if(PC){PC->ResetIgnoreInputFlags();PC->SetControlRotation(StartTransform.Rotator());PC->SetInputMode(FInputModeGameOnly());PC->bShowMouseCursor=false;}
 for(int32 I=0;I<Exhibits.Num();++I){Exhibits[I]->Word=InitialWords[I];Exhibits[I]->bBallLanded=false;Exhibits[I]->SetActorTransform(InitialTransforms[I]);Exhibits[I]->Rest=InitialTransforms[I].GetLocation();Exhibits[I]->Apply();}
 bComplete=false;bRewardUnlocked=false;bPressure=false;bPower=false;bSecondRoom=false;Thoughts[0].Reset();Thoughts[1].Reset();Aim.Reset();bAimReset=false;
 if(RewardShutter)RewardShutter->SetActorLocation(RoomPoint(FVector(2190,-350,45)));if(Reward)Reward->SetActorHiddenInGame(true);RefreshSelfThoughts();if(UI)UI->Refresh();
 UpdateMechanism();
 UE_LOG(LogWenSiMirror,Display,TEXT("Reset: all words, transforms, miner size, mirrors, ball, balance, locks and reward restored"));
}
bool AWenSiMirrorPuzzle::SkipToReward()
{
 if(!bAreaManaged || !bAreaActive || bComplete || !MousePortal || MousePortal->IsEntering())return false;
 auto Hall=AMiningExhibitHall::Find(GetWorld());
 if(!Hall || !Hall->Areas.IsValidIndex(Hall->ActiveArea) ||
    Hall->Areas[Hall->ActiveArea].Tag!=TEXT("BM_Campaign_MainGameMuseum"))return false;
 auto PC=GetWorld()->GetFirstPlayerController();auto Pawn=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
 if(!Pawn)return false;
 CancelSwap();
 if(bEditing)ToggleEdit();
 Thoughts[0].Reset();Thoughts[1].Reset();Aim.Reset();bAimReset=false;
 // Finish inside the mouse gallery, facing the final painting.
 // Normal E interaction still enters the ending.
 if(!SetPlayerWord(EWenSiMirrorWord::Small))return false;
 if(auto Block=Find(6)){Block->Word=EWenSiMirrorWord::Heavy;Block->Apply();Block->SetActorLocation(Block->Rest-FVector(0,0,WenSiMuseumMechanisms::BalanceTravel));}
 if(auto Ball=Find(9)){Ball->Word=EWenSiMirrorWord::Conductive;Ball->bBallLanded=true;Ball->Apply();Ball->SetActorLocation(RoomPoint(FVector(1900,120,80)));}
 bPressure=true;bPower=true;bRewardUnlocked=true;bSecondRoom=true;
 if(Reward){Reward->SetActorHiddenInGame(true);Reward->SetActorEnableCollision(false);}
 if(RewardShutter)RewardShutter->SetActorLocation(RoomPoint(FVector(2190,-350,138)));
 // The tiny capsule is roughly 33 cm half-height; keep its feet just above
 // the alcove floor instead of dropping the scaled body into the slab.
 Pawn->SetActorLocation(RoomPoint(FVector(2230,-350,35)),false,nullptr,ETeleportType::TeleportPhysics);
 if(auto Movement=Pawn->FindComponentByClass<UCharacterMovementComponent>())Movement->StopMovementImmediately();
 const auto ViewCamera=Pawn->FindComponentByClass<UCameraComponent>();
 PC->SetControlRotation((MousePortal->CellCenter()-(ViewCamera?ViewCamera->GetComponentLocation():Pawn->GetPawnViewLocation())).Rotation());
 if(UI)UI->Refresh();
 UE_LOG(LogWenSiMirror,Display,TEXT("Hidden museum skip accepted: small miner moved inside mouse gallery"));
 return true;
}
void AWenSiMirrorPuzzle::EndPlay(const EEndPlayReason::Type Reason){if(MousePortal)MousePortal->Destroy();CancelSwap();if(bEditing)ToggleEdit();if(UI)UI->RemoveFromParent();Super::EndPlay(Reason);}
FString AWenSiMirrorPuzzle::MirrorStatus()
{
 auto PC=GetWorld()->GetFirstPlayerController();auto Pawn=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
 FString Result=FString::Printf(TEXT("second_room=%d pressure=%d power=%d reward=%d complete=%d miner=%s scale=%.2f"),bSecondRoom,bPressure,bPower,bRewardUnlocked,bComplete,Pawn?*WordText(GetPlayerWord()):TEXT("missing"),Pawn?Pawn->GetActorScale3D().X:0.f);
 for(auto Exhibit:Exhibits)Result+=FString::Printf(TEXT("\nkind=%d %s=%s location=%s landed=%d"),Exhibit->Kind,*Exhibit->Noun(),*Exhibit->Adjective(),*Exhibit->GetActorLocation().ToCompactString(),Exhibit->bBallLanded);
 UE_LOG(LogWenSiMirror,Display,TEXT("%s"),*Result);return Result;
}
