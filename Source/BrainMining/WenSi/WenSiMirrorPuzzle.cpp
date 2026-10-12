#include "WenSi/WenSiMirrorPuzzle.h"
#include "WenSi/WenSiMirrorMechanismLayout.h"
#include "WenSi/WenSiPuzzle.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogWenSiMirror,Log,All);
namespace
{
 UMaterialInterface* Surface(const TCHAR* Name)
 {
  return LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/WenSi/Museum/Materials/"))+Name+TEXT(".")+Name));
 }
 FString WordText(EWenSiMirrorWord Word)
 {
  static const TCHAR* Names[]={TEXT("普通"),TEXT("沉重"),TEXT("悬浮"),TEXT("可穿过"),TEXT("导电"),TEXT("大"),TEXT("小")};
  return Names[int32(Word)];
 }
 float WordScale(EWenSiMirrorWord Word){return Word==EWenSiMirrorWord::Small?.28f:Word==EWenSiMirrorWord::Big?1.7f:1.f;}
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
}
FString AWenSiMirrorExhibit::Noun() const
{
 static const TCHAR* Names[]={TEXT("石块"),TEXT("罩子"),TEXT("雕像"),TEXT("气球"),TEXT("烟雾"),TEXT("屏风"),TEXT("压块"),TEXT("罩子"),TEXT("金属块"),TEXT("陶瓷球"),TEXT("矿工"),TEXT("矿工"),TEXT("摆件")};
 return DisplayNoun.IsEmpty()?FString(Names[FMath::Clamp(Kind,0,12)]):DisplayNoun;
}
FString AWenSiMirrorExhibit::Adjective() const{return WordText(Word);}
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
 case 0: Shape(0,FVector::ZeroVector,FVector(85,190,180));break;
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
  Shape(0,FVector::ZeroVector,FVector(55,55,70),Sphere);Shape(1,FVector(0,0,-40),FVector(10),Cylinder);
  Shape(2,FVector(0,0,-66),FVector(1.5,1.5,46),Cylinder);break;
 case 4:
  for(int32 I=0;I<7;++I)Shape(I,FVector((I%3-1)*15,(I%2?1:-1)*12,I*10),FVector(40,40,40),Sphere);break;
 case 5:
  Shape(0,FVector::ZeroVector,FVector(18,220,236));Shape(1,FVector(0,-114,0),FVector(30,12,240));Shape(2,FVector(0,114,0),FVector(30,12,240));break;
 case 6: Shape(0,FVector::ZeroVector,FVector(85,85,80));break;
 case 7:
  Shape(0,FVector(0,0,130),FVector(200,200,10));Shape(1,FVector(94,0,0),FVector(12,200,260));
  Shape(2,FVector(0,-94,0),FVector(200,12,260));Shape(3,FVector(0,94,0),FVector(200,12,260));
  Shape(4,FVector(-94,-60,0),FVector(12,80,260));Shape(5,FVector(-94,60,0),FVector(12,80,260));
  Shape(6,FVector(-94,0,33),FVector(12,40,194));break;
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
  UMaterialInterface* Use=(Word==EWenSiMirrorWord::Passable && !IsSelf(this))||Kind==4?Ghost:Kind==7&&I!=0?Glass:Parent;
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
 Highlight(false);
}
void AWenSiMirrorExhibit::Highlight(bool bAimed)
{
 for(auto Part:Parts)if(auto Glow=Cast<UMaterialInstanceDynamic>(Part->GetOverlayMaterial()))
 {Glow->SetScalarParameterValue(TEXT("Strength"),bThinkingEnabled?(bAimed?1.6f:.65f):0.f);Glow->SetScalarParameterValue(TEXT("OutlineWidth"),bAimed?1.2f:.65f);Glow->SetVectorParameterValue(TEXT("Tint"),FLinearColor(1.f,.89f,.7f));}
}
void AWenSiMirrorExhibit::BeginPlay()
{
 Super::BeginPlay();Rest=GetActorLocation();Configure();
 if(IsSelf(this))
 {
  MirrorTarget=NewObject<UTextureRenderTarget2D>(this);MirrorTarget->InitAutoFormat(24,36);MirrorTarget->Filter=TF_Bilinear;MirrorTarget->ClearColor=FLinearColor(.03f,.04f,.055f);
  Capture->TextureTarget=MirrorTarget;Capture->HiddenActors.Add(this);Apply();
 }
}
void AWenSiMirrorExhibit::UpdateMirror()
{
 auto PC=GetWorld()->GetFirstPlayerController();if(!PC)return;
 FVector Eye;FRotator Rot;PC->GetPlayerViewPoint(Eye,Rot);
 const FVector Normal=GetActorForwardVector();const FVector Plane=GetActorLocation()+Normal*3.f;
 const float Distance=FVector::DotProduct(Eye-Plane,Normal);
 if(Distance<=5.f || Distance>450.f)return;
 const FVector Reflected=Eye-2.f*Distance*Normal;
 // A frosted reflection preserves the room's broad shapes without showing the miner.
 const FVector Forward=FMath::GetReflectionVector(Rot.Vector(),Normal);
 const FVector Up=FMath::GetReflectionVector(FRotationMatrix(Rot).GetUnitAxis(EAxis::Z),Normal);
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
 if(IsSelf(this)){MirrorClock+=Dt;if(MirrorClock>.066f){MirrorClock=0;UpdateMirror();}return;}
 float Target=Rest.Z;
 if(Word==EWenSiMirrorWord::Floating)
 {
  Target=Rest.Z+(Kind==0?230.f:Kind==2?125.f:Kind==3?0.f:90.f);
  if(Kind==0)
   for(TActorIterator<AWenSiMirrorExhibit> It(GetWorld());It;++It)
    if(It->Kind==1 && It->Word!=EWenSiMirrorWord::Passable)Target=FMath::Min(Target,It->GetActorLocation().Z+120.f-7.f-90.f-2.f);
 }
 if(Kind==3 && Word!=EWenSiMirrorWord::Floating)Target=Rest.Z-65.f;
 if(Kind==6 && Word==EWenSiMirrorWord::Heavy)Target=Rest.Z-WenSiMuseumMechanisms::BalanceTravel;
 if(Kind==9)
 {
  if(Word==EWenSiMirrorWord::Small || bBallLanded)Target=80.f;
  if(bBallLanded && Word==EWenSiMirrorWord::Floating)Target=165.f;
 }
 FVector P=GetActorLocation();P.Z=FMath::FInterpConstantTo(P.Z,Target,Dt,Kind==9?130.f:100.f);
 if(!P.Equals(GetActorLocation(),.01f))
 {
  auto PC=GetWorld()->GetFirstPlayerController();auto Character=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
  const bool bBlocks=Word!=EWenSiMirrorWord::Passable && Kind!=4;
  if(!Character || !bBlocks || !ExhibitBounds(this).ShiftBy(P-GetActorLocation()).Intersect(Character->GetCapsuleComponent()->Bounds.GetBox()))SetActorLocation(P);
 }
 if(Kind==9 && Word==EWenSiMirrorWord::Small && GetActorLocation().Z<=80.1f)bBallLanded=true;
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
 const float Scale=NewWord==EWenSiMirrorWord::Small?.28f:NewWord==EWenSiMirrorWord::Big?1.45f:1.f;
 const FVector Feet=GetActorLocation()-FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
 const FVector Next=Feet+FVector(0,0,88.f*Scale+.5f);
 FCollisionQueryParams Query;Query.AddIgnoredActor(this);
 if(NewWord!=EWenSiMirrorWord::Passable && GetWorld()->OverlapBlockingTestByChannel(Next,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(32.f*Scale,88.f*Scale-.2f),Query))return false;
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
 AddMovementInput(GetActorForwardVector(),float(PC->IsInputKeyDown(EKeys::W))-float(PC->IsInputKeyDown(EKeys::S)));
 AddMovementInput(GetActorRightVector(),float(PC->IsInputKeyDown(EKeys::D))-float(PC->IsInputKeyDown(EKeys::A)));
 if(!PC->IsInputKeyDown(EKeys::SpaceBar))bRequireFlightRelease=false;
 if((PC->IsInputKeyDown(EKeys::SpaceBar)&&!bRequireFlightRelease)||Word==EWenSiMirrorWord::Floating)
 {auto Movement=GetCharacterMovement();if(Movement->IsMovingOnGround())Movement->SetMovementMode(MOVE_Falling);Movement->Velocity.Z=FMath::Min(350.f,Movement->Velocity.Z+(1700.f+FMath::Abs(GetWorld()->GetGravityZ()))*Dt);}
}
AWenSiMirrorGameMode::AWenSiMirrorGameMode(){DefaultPawnClass=AWenSiMirrorPawn::StaticClass();}

AWenSiMirrorPuzzle::AWenSiMirrorPuzzle(){PrimaryActorTick.bCanEverTick=true;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));}
AWenSiMirrorExhibit* AWenSiMirrorPuzzle::Find(int32 Kind) const{for(auto Exhibit:Exhibits)if(Exhibit->Kind==Kind)return Exhibit;return nullptr;}
void AWenSiMirrorPuzzle::BeginPlay()
{
 Super::BeginPlay();
 for(TActorIterator<AWenSiMirrorExhibit> It(GetWorld());It;++It){Exhibits.Add(*It);InitialTransforms.Add(It->GetActorTransform());InitialWords.Add(It->Word);}
 for(TActorIterator<APlayerStart> It(GetWorld());It;++It){StartTransform=It->GetActorTransform();break;}
 for(TActorIterator<AActor> It(GetWorld());It;++It)
 {
  if(It->ActorHasTag(TEXT("MirrorRewardShutter")))RewardShutter=*It;
  if(It->ActorHasTag(TEXT("MirrorReward")))Reward=*It;
  for(int32 I=0;I<2;++I)if(It->ActorHasTag(I?TEXT("MirrorPowerLamp"):TEXT("MirrorPressureLamp")))
   if(auto Mesh=It->FindComponentByClass<UStaticMeshComponent>()){LampMeshes.Add(Mesh);auto Mat=UMaterialInstanceDynamic::Create(Surface(TEXT("MI_Terracotta")),this);Mesh->SetMaterial(0,Mat);Lamps.Add(Mat);}
 }
 auto PC=GetWorld()->GetFirstPlayerController();if(PC){UI=CreateWidget<UWenSiMirrorWidget>(PC);UI->Puzzle=this;UI->AddToViewport(50);PC->SetInputMode(FInputModeGameOnly());PC->bShowMouseCursor=false;if(auto Pawn=Cast<AWenSiMirrorPawn>(PC->GetPawn()))Pawn->SetWord(EWenSiMirrorWord::Ordinary);}
 if(Reward)Reward->SetActorHiddenInGame(true);
 UE_LOG(LogWenSiMirror,Display,TEXT("Initialized %d exhibits; realistic initial states; exterior thinking mirror"),Exhibits.Num());
}
void AWenSiMirrorPuzzle::RefreshSelfThoughts()
{
 auto PC=GetWorld()->GetFirstPlayerController();auto Pawn=PC?Cast<AWenSiMirrorPawn>(PC->GetPawn()):nullptr;
 if(Pawn)for(auto Exhibit:Exhibits)if(IsSelf(Exhibit))Exhibit->Word=Pawn->Word;
}
void AWenSiMirrorPuzzle::Think()
{
 if(!Aim.IsValid() || bComplete)return;RefreshSelfThoughts();
 for(int32 I=0;I<2;++I)if(Thoughts[I]==Aim || (IsSelf(Thoughts[I].Get()) && IsSelf(Aim.Get())))return;
 for(int32 I=0;I<2;++I)if(!Thoughts[I].IsValid()){Thoughts[I]=Aim;break;}
}
void AWenSiMirrorPuzzle::CloseThought(int32 Index){if(Index>=0&&Index<2)Thoughts[Index].Reset();if(UI)UI->Refresh();}
void AWenSiMirrorPuzzle::ToggleEdit()
{
 auto PC=GetWorld()->GetFirstPlayerController();if(!PC || (!bEditing&&(bComplete||(!Thoughts[0].IsValid()&&!Thoughts[1].IsValid()))))return;
 if(bEditing && FSlateApplication::IsInitialized())FSlateApplication::Get().CancelDragDrop();
 bEditing=!bEditing;PC->SetIgnoreMoveInput(bEditing);PC->SetIgnoreLookInput(bEditing);PC->bShowMouseCursor=bEditing;
 if(bEditing){FInputModeGameAndUI Mode;Mode.SetWidgetToFocus(UI->TakeWidget());Mode.SetHideCursorDuringCapture(false);Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);PC->SetInputMode(Mode);}
 else PC->SetInputMode(FInputModeGameOnly());if(UI)UI->Refresh();
}
void AWenSiMirrorPuzzle::SwapWords(int32 From,int32 To)
{
 if(!bEditing || bComplete || From==To || From<0 || From>1 || To<0 || To>1 || !Thoughts[From].IsValid() || !Thoughts[To].IsValid())return;
 RefreshSelfThoughts();auto A=Thoughts[From].Get();auto B=Thoughts[To].Get();if(IsSelf(A)&&IsSelf(B))return;
 auto PC=GetWorld()->GetFirstPlayerController();auto Pawn=PC?Cast<AWenSiMirrorPawn>(PC->GetPawn()):nullptr;if(!Pawn)return;
 // Validate solidification and growth before changing either word, preserving atomic swaps.
 for(auto Pair:{TPair<AWenSiMirrorExhibit*,EWenSiMirrorWord>(A,B->Word),TPair<AWenSiMirrorExhibit*,EWenSiMirrorWord>(B,A->Word)})
 {
  if(IsSelf(Pair.Key) || Pair.Value==EWenSiMirrorWord::Passable || Pair.Key->Kind==4)continue;
  const FBox Current=ExhibitBounds(Pair.Key);const FVector Extent=Current.GetExtent()*WordScale(Pair.Value)/WordScale(Pair.Key->Word);
  const FBox Next(Current.GetCenter()-Extent,Current.GetCenter()+Extent);
  if(Next.Intersect(Pawn->GetCapsuleComponent()->Bounds.GetBox()))return;
 }
 const EWenSiMirrorWord OldA=A->Word,OldB=B->Word;
 if(IsSelf(A)&&!Pawn->SetWord(OldB))return;if(IsSelf(B)&&!Pawn->SetWord(OldA))return;
 A->Word=OldB;B->Word=OldA;if(!IsSelf(A))A->Apply();if(!IsSelf(B))B->Apply();RefreshSelfThoughts();
 UE_LOG(LogWenSiMirror,Display,TEXT("Swap: %s=%s; %s=%s"),*A->Noun(),*A->Adjective(),*B->Noun(),*B->Adjective());if(UI)UI->Refresh();
}
void AWenSiMirrorPuzzle::UpdateMechanism()
{
 auto Block=Find(6);auto Ball=Find(9);
 const float BalanceAlpha=Block?FMath::Clamp(float((Block->Rest.Z-Block->GetActorLocation().Z)/WenSiMuseumMechanisms::BalanceTravel),0.f,1.f):0.f;
 WenSiMuseumMechanisms::UpdateBalance(GetWorld(),BalanceAlpha);
 bPressure=Block && Block->Word==EWenSiMirrorWord::Heavy && BalanceAlpha>=.99f;
 bPower=Ball && Ball->bBallLanded && Ball->Word==EWenSiMirrorWord::Conductive && FMath::Abs(Ball->GetActorLocation().Z-80.f)<1.f;
 if(bPressure && bPower && !bRewardUnlocked){bRewardUnlocked=true;if(Reward)Reward->SetActorHiddenInGame(false);UE_LOG(LogWenSiMirror,Display,TEXT("Both physical locks active; reward hatch unlocked"));}
 if(RewardShutter)RewardShutter->SetActorLocation(FVector(2190,-350,FMath::FInterpConstantTo(RewardShutter->GetActorLocation().Z,bRewardUnlocked?112.f:32.f,GetWorld()->GetDeltaSeconds(),90.f)));
 for(int32 I=0;I<Lamps.Num();++I){const bool On=I?bPower:bPressure;Lamps[I]->SetVectorParameterValue(TEXT("Tint"),On?FLinearColor(.12,.75,.2):FLinearColor(.55,.025,.015));if(auto Overlay=Cast<UMaterialInstanceDynamic>(LampMeshes[I]->GetOverlayMaterial())){Overlay->SetVectorParameterValue(TEXT("Tint"),On?FLinearColor(.35,1,.45):FLinearColor(.5,.04,.025));Overlay->SetScalarParameterValue(TEXT("Strength"),On?2.f:.45f);}}
 auto PC=GetWorld()->GetFirstPlayerController();auto Pawn=PC?Cast<AWenSiMirrorPawn>(PC->GetPawn()):nullptr;
 if(!bComplete && bRewardUnlocked && Pawn && Pawn->Word==EWenSiMirrorWord::Small && Pawn->GetActorLocation().X>2210.f && FVector::Dist2D(Pawn->GetActorLocation(),FVector(2300,-350,0))<58.f)
 {bComplete=true;if(bEditing)ToggleEdit();Thoughts[0].Reset();Thoughts[1].Reset();if(Reward)Reward->SetActorHiddenInGame(true);UE_LOG(LogWenSiMirror,Display,TEXT("Puzzle complete: small miner entered reward alcove and collected prize"));}
}
void AWenSiMirrorPuzzle::Tick(float Dt)
{
 Super::Tick(Dt);auto PC=GetWorld()->GetFirstPlayerController();if(!PC)return;
 RefreshSelfThoughts();UpdateMechanism();Aim.Reset();bAimReset=false;
 if(!bEditing)
 {
  FVector P;FRotator R;PC->GetPlayerViewPoint(P,R);FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(PC->GetPawn());
  // Passing through an object never removes it from the thinking trace.
  if(GetWorld()->LineTraceSingleByChannel(Hit,P,P+R.Vector()*310.f,ECC_Visibility,Q))
  {
   auto Exhibit=Cast<AWenSiMirrorExhibit>(Hit.GetActor());
   const bool Nearby=PC->GetPawn() && FVector::Dist(PC->GetPawn()->GetActorLocation(),Hit.ImpactPoint)<280.f;
   if(Nearby){bAimReset=Cast<AWenSiResetButton>(Hit.GetActor())!=nullptr;if(Exhibit && Exhibit->bThinkingEnabled && !bComplete)Aim=Exhibit;}
  }
  if(PC->WasInputKeyJustPressed(EKeys::E)){if(bAimReset)ResetPuzzle();else Think();}
  if(PC->WasInputKeyJustPressed(EKeys::Q))CloseThought(0);if(PC->WasInputKeyJustPressed(EKeys::R))CloseThought(1);
 }
 if(PC->WasInputKeyJustPressed(EKeys::G))ToggleEdit();
 if(PC->GetPawn() && PC->GetPawn()->GetActorLocation().X>1000.f)bSecondRoom=true;
 for(auto Exhibit:Exhibits)Exhibit->Highlight(Exhibit==Aim.Get());if(UI)UI->Refresh();
}
void AWenSiMirrorPuzzle::ResetPuzzle()
{
 if(FSlateApplication::IsInitialized())FSlateApplication::Get().CancelDragDrop();if(bEditing)ToggleEdit();
 auto PC=GetWorld()->GetFirstPlayerController();auto Pawn=PC?Cast<AWenSiMirrorPawn>(PC->GetPawn()):nullptr;
 if(Pawn){Pawn->SetActorLocation(StartTransform.GetLocation());Pawn->SetWord(EWenSiMirrorWord::Ordinary);Pawn->SetActorTransform(StartTransform);Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->bRequireFlightRelease=true;}
 if(PC){PC->ResetIgnoreInputFlags();PC->SetControlRotation(StartTransform.Rotator());PC->SetInputMode(FInputModeGameOnly());PC->bShowMouseCursor=false;}
 for(int32 I=0;I<Exhibits.Num();++I){Exhibits[I]->Word=InitialWords[I];Exhibits[I]->bBallLanded=false;Exhibits[I]->SetActorTransform(InitialTransforms[I]);Exhibits[I]->Rest=InitialTransforms[I].GetLocation();Exhibits[I]->Apply();}
 bComplete=false;bRewardUnlocked=false;bPressure=false;bPower=false;bSecondRoom=false;Thoughts[0].Reset();Thoughts[1].Reset();Aim.Reset();bAimReset=false;
 if(RewardShutter)RewardShutter->SetActorLocation(FVector(2190,-350,32));if(Reward)Reward->SetActorHiddenInGame(true);RefreshSelfThoughts();if(UI)UI->Refresh();
 UpdateMechanism();
 UE_LOG(LogWenSiMirror,Display,TEXT("Reset: all words, transforms, miner size, mirrors, ball, balance, locks and reward restored"));
}
void AWenSiMirrorPuzzle::EndPlay(const EEndPlayReason::Type Reason){if(bEditing)ToggleEdit();if(UI)UI->RemoveFromParent();Super::EndPlay(Reason);}
FString AWenSiMirrorPuzzle::MirrorStatus()
{
 auto PC=GetWorld()->GetFirstPlayerController();auto Pawn=PC?Cast<AWenSiMirrorPawn>(PC->GetPawn()):nullptr;
 FString Result=FString::Printf(TEXT("second_room=%d pressure=%d power=%d reward=%d complete=%d miner=%s scale=%.2f"),bSecondRoom,bPressure,bPower,bRewardUnlocked,bComplete,Pawn?*WordText(Pawn->Word):TEXT("missing"),Pawn?Pawn->GetActorScale3D().X:0.f);
 for(auto Exhibit:Exhibits)Result+=FString::Printf(TEXT("\nkind=%d %s=%s location=%s landed=%d"),Exhibit->Kind,*Exhibit->Noun(),*Exhibit->Adjective(),*Exhibit->GetActorLocation().ToCompactString(),Exhibit->bBallLanded);
 UE_LOG(LogWenSiMirror,Display,TEXT("%s"),*Result);return Result;
}
