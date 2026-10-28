#include "WenSi/MouseMuseumPortal.h"
#include "MiningAudioComponent.h"
#include "MiningLocalizationSubsystem.h"
#include "MiningAchievementSubsystem.h"
#include "Engine/GameInstance.h"
#include "WenSi/WenSiMirrorPuzzle.h"
#include "Showcase/MiningExhibitHall.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Subtitles/SubtitleSubsystem.h"

AMouseMuseumPortal::AMouseMuseumPortal()
{
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("GalleryRoot")));
}

FVector AMouseMuseumPortal::CellCenter() const
{
 return GetActorLocation()+FVector(2432,-350+(CellU-.5f)*CanvasWidth,100+(.5f-CellV)*CanvasHeight);
}

void AMouseMuseumPortal::Initialize(AWenSiMirrorPuzzle* OwnerPuzzle)
{
 Puzzle=OwnerPuzzle;SetActorLocation(Puzzle->GetActorLocation());
 // Leave 90 cm clear for the actual 66.9 cm shrunken capsule plus floor clearance.
 for(TActorIterator<AActor> It(GetWorld());It;++It)
 {
  if(It->GetActorLocation().Equals(Puzzle->RoomPoint(FVector(2190,-350,242)),1.f) || It->GetActorLocation().Equals(Puzzle->RoomPoint(FVector(2190,-350,250)),1.f))
   if(auto Mesh=It->FindComponentByClass<UStaticMeshComponent>())
   {auto Scale=Mesh->GetRelativeScale3D();Scale.Z=3.3f;Mesh->SetRelativeScale3D(Scale);It->SetActorLocation(Puzzle->RoomPoint(FVector(2190,-350,255)));}
  if(It->ActorHasTag(TEXT("MirrorRewardShutter")))
   if(auto Mesh=It->FindComponentByClass<UStaticMeshComponent>())
   {auto Scale=Mesh->GetRelativeScale3D();Scale.Z=.9f;Mesh->SetRelativeScale3D(Scale);}
 }
 auto Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 auto Plane=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane"));
 auto Mat=[](const TCHAR* Name){return LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/WenSi/Museum/Materials/"))+Name));};
 auto Gold=Mat(TEXT("MI_Brass"));auto Dark=Mat(TEXT("MI_DeepBlue"));auto Cream=Mat(TEXT("MI_Limestone"));
 // The miniature endpoint gallery uses the same finish as the surrounding museum.
 auto Wall=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Museum/Materials/MI_Stucco01.MI_Stucco01"));
 if(!Wall)Wall=Cream;
 auto Shape=[&](const TCHAR* Name,FVector P,FVector Size,UStaticMesh* Mesh,UMaterialInterface* M)
 {
  auto C=NewObject<UStaticMeshComponent>(this,FName(Name));AddInstanceComponent(C);C->SetupAttachment(RootComponent);
  C->SetMobility(EComponentMobility::Movable);C->SetStaticMesh(Mesh);C->SetRelativeLocation(P);C->SetRelativeScale3D(Size/100);C->SetMaterial(0,M);
  C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->RegisterComponent();return C;
 };
 Shape(TEXT("GalleryBack"),FVector(2434,-350,100),FVector(2,258,200),Cube,Wall);
 auto Ceiling=Shape(TEXT("GalleryCeiling"),FVector(2320,-350,204),FVector(240,258,6),Cube,Dark);
 Ceiling->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Ceiling->SetCollisionResponseToAllChannels(ECR_Block);
 Shape(TEXT("GalleryLeftWall"),FVector(2320,-477,100),FVector(240,4,200),Cube,Wall);
 Shape(TEXT("GalleryRightWall"),FVector(2320,-223,100),FVector(240,4,200),Cube,Wall);
 Shape(TEXT("GalleryCarpet"),FVector(2320,-350,1),FVector(236,210,1),Cube,Mat(TEXT("MI_Burgundy")));
 for(int I=0;I<2;++I)
 {
  const float Side=I?1.f:-1.f;
  Shape(I?TEXT("FrameRight"):TEXT("FrameLeft"),FVector(2431,-350+Side*66,100),FVector(6,5,176),Cube,Gold);
  Shape(I?TEXT("FrameTop"):TEXT("FrameBottom"),FVector(2431,-350,100+Side*87),FVector(6,137,5),Cube,Gold);
  const float Y=-350+Side*105;
  Shape(I?TEXT("PlinthRight"):TEXT("PlinthLeft"),FVector(2385,Y,17),FVector(28,28,34),Cube,Cream);
  Shape(I?TEXT("MouseBodyRight"):TEXT("MouseBodyLeft"),FVector(2385,Y,43),FVector(16,12,20),Sphere,Gold);
  Shape(I?TEXT("MouseHeadRight"):TEXT("MouseHeadLeft"),FVector(2382,Y,55),FVector(12,12,12),Sphere,Gold);
  for(int E=0;E<2;++E)Shape(*FString::Printf(TEXT("MouseEar%d%d"),I,E),FVector(2384,Y+(E?5:-5),61),FVector(3,7,8),Sphere,Gold);
  auto Light=NewObject<UPointLightComponent>(this);AddInstanceComponent(Light);Light->SetupAttachment(RootComponent);
  Light->SetRelativeLocation(FVector(2370,Y,140));Light->SetIntensity(300);Light->SetAttenuationRadius(240);Light->SetLightColor(FLinearColor(1,.7,.36));Light->SetCastShadows(false);Light->RegisterComponent();
 }
 auto Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MouseMuseum/M_MouseKiss.M_MouseKiss"));
 if(Base)Painting=UMaterialInstanceDynamic::Create(Base,this);
 auto Canvas=Shape(TEXT("MouseKissCanvas"),FVector(2432,-350,100),FVector(CanvasWidth,CanvasHeight,100),Plane,Painting);
 // Engine Plane U runs along +Y and V downward after this rotation.
 // The painting material is two-sided so it is visible from the entrance.
 Canvas->SetRelativeRotation(FRotator(0,90,90));
 Canvas->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Canvas->SetCollisionResponseToAllChannels(ECR_Ignore);Canvas->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 auto Label=[&](const TCHAR* Name,const TCHAR* Text,FVector P,float Size)
 {
  auto C=NewObject<UTextRenderComponent>(this,FName(Name));AddInstanceComponent(C);C->SetupAttachment(RootComponent);
  C->SetText(FText::FromString(Text));C->SetWorldSize(Size);C->SetHorizontalAlignment(EHTA_Center);C->SetTextRenderColor(FColor(220,183,111));C->SetRelativeLocation(P);C->SetRelativeRotation(FRotator(0,180,0));C->RegisterComponent();
 };
 Label(TEXT("GalleryName"),TEXT("THE MOUSE MUSEUM"),FVector(2431,-350,194),5);
 Label(TEXT("PaintingLabel"),TEXT("THE KISS"),FVector(2430,-350,8),3);
}

void AMouseMuseumPortal::RefreshFocus()
{
 bFocused=false;
 bInsideGallery=false;
 auto PC=GetWorld()->GetFirstPlayerController();auto Pawn=PC?PC->GetPawn():nullptr;
 if(Puzzle && Pawn && Painting && Puzzle->bAreaActive && Puzzle->bRewardUnlocked && !Puzzle->bEditing && !Puzzle->bComplete && !Puzzle->IsSwapping() && !IsEntering() && Puzzle->GetPlayerWord()==EWenSiMirrorWord::Small)
 {
  const FVector Local=Pawn->GetActorLocation()-GetActorLocation();
  if(Local.X>2210 && Local.X<2440 && FMath::Abs(Local.Y+350)<125 && Local.Z<200)
  {
   bInsideGallery=true;
   if(GalleryEnteredAt<0)GalleryEnteredAt=GetWorld()->GetTimeSeconds();
   if(!bGalleryHintShown && GetWorld()->GetTimeSeconds()-GalleryEnteredAt>=3.f)
   {bGalleryHintShown=true;if(auto* Sub=USubtitleSubsystem::Get(this)){Sub->StopAll();Sub->PlayStatusLine(MiningText::Get(TEXT("UI.Museum.PaintingHint")),4.f);}}
   FVector Eye;FRotator R;PC->GetPlayerViewPoint(Eye,R);const FVector D=R.Vector(),Center=CellCenter();
   if(D.X>.05f)
   {
    const float T=(Center.X-Eye.X)/D.X;const FVector HitPoint=Eye+D*T;
    if(T>0 && T<280 && FVector::DistSquared(HitPoint,Center)<=FMath::Square(CellRadius))
    {
     FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(Pawn);
     bFocused=!GetWorld()->LineTraceSingleByChannel(Hit,Eye,HitPoint+D*2,ECC_Visibility,Q) || Hit.GetActor()==this;
    }
   }
  }
 }
 if(!bInsideGallery)GalleryEnteredAt=-1;
 if(Painting)
 {
  Painting->SetScalarParameterValue(TEXT("Highlight"),bFocused?1.f:0.f);
 }
}

bool AMouseMuseumPortal::TryEnter()
{
 RefreshFocus();if(!bFocused)return false;
 auto PC=GetWorld()->GetFirstPlayerController();auto Pawn=Cast<ACharacter>(PC->GetPawn());if(!Pawn)return false;
 Puzzle->CloseThoughts();PC->GetPlayerViewPoint(EntryEye,EntryRotation);EntryFOV=PC->PlayerCameraManager->GetFOVAngle();
 EntryCamera=GetWorld()->SpawnActor<ACameraActor>(EntryEye,EntryRotation);if(!EntryCamera)return false;
 EntryCamera->GetCameraComponent()->SetFieldOfView(EntryFOV);
 PC->SetViewTarget(EntryCamera);PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);bInputLocked=true;
 EntryPawn=Pawn;bPawnWasHidden=Pawn->IsHidden();Pawn->SetActorHiddenInGame(true);
 PreviousMovementMode=Pawn->GetCharacterMovement()->MovementMode;PreviousCustomMode=Pawn->GetCharacterMovement()->CustomMovementMode;
 Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->DisableMovement();
 if(auto* Sub=USubtitleSubsystem::Get(this)){Sub->StopAll();Sub->PlayStatusLine(MiningText::Get(TEXT("UI.Museum.LifeHint")),5.f);}
 EntryAge=0;bFocused=false;bTravelRequested=false;return true;
}

bool AMouseMuseumPortal::Advance(float Dt)
{
 if(!IsEntering())return false;
 auto PC=GetWorld()->GetFirstPlayerController();if(!PC||!EntryCamera){Cancel();return false;}
 EntryAge+=Dt;const float T=FMath::Clamp(EntryAge/2.25f,0.f,1.f);
 const float Ease=T*T*T;
 const FVector Center=CellCenter(),End=Center-FVector(.6f,0,0);
 EntryCamera->SetActorLocation(FMath::Lerp(EntryEye,End,Ease));
 const FRotator Look=(Center-EntryEye).Rotation();
 EntryCamera->SetActorRotation(FQuat::Slerp(EntryRotation.Quaternion(),Look.Quaternion(),FMath::SmoothStep(0.f,.35f,T)));
 EntryCamera->GetCameraComponent()->SetFieldOfView(FMath::Lerp(EntryFOV,52.f,Ease));
 Painting->SetScalarParameterValue(TEXT("Highlight"),1.f+T*2);
 PC->PlayerCameraManager->SetManualCameraFade(FMath::SmoothStep(.68f,1.f,T),FLinearColor::Black,false);
 if(T>=1 && !bTravelRequested)
 {
  bTravelRequested=true;
  bool Campaign=false;
  if(auto Hall=AMiningExhibitHall::Find(GetWorld());Hall && Hall->Areas.IsValidIndex(Hall->ActiveArea))Campaign=Puzzle->bAreaManaged && Hall->Areas[Hall->ActiveArea].Tag==TEXT("BM_Campaign_MainGameMuseum");
  if(Campaign)
  {
   if(EntryPawn.IsValid())if(auto* Audio=EntryPawn->FindComponentByClass<UMiningAudioComponent>())Audio->BeginEndingMusicTransition();
   Puzzle->bComplete=true;GetGameInstance()->GetSubsystem<UMiningAchievementSubsystem>()->RecordMilestone(TEXT("ACH_MUSEUM_CLEAR"));
   UGameplayStatics::OpenLevel(this,TEXT("/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo"),true);
  }
  else {Puzzle->bComplete=true;Cancel();}
 }
 // A missing packaged destination must not leave the player permanently locked.
 if(EntryAge>8.f){if(Puzzle)Puzzle->bComplete=false;bTravelRequested=false;Cancel();}
 return true;
}

void AMouseMuseumPortal::Cancel()
{
 if(bInputLocked)
  if(auto PC=GetWorld()->GetFirstPlayerController())
  {
   PC->SetIgnoreMoveInput(false);PC->SetIgnoreLookInput(false);PC->SetViewTarget(PC->GetPawn());
   if(PC->PlayerCameraManager)PC->PlayerCameraManager->StopCameraFade();
  }
 if(EntryPawn.IsValid())
 {
  if(!bTravelRequested)if(auto* Audio=EntryPawn->FindComponentByClass<UMiningAudioComponent>())Audio->CancelEndingMusicTransition();
  EntryPawn->SetActorHiddenInGame(bPawnWasHidden);
  EntryPawn->GetCharacterMovement()->SetMovementMode(EMovementMode(PreviousMovementMode),PreviousCustomMode);
 }
 EntryPawn.Reset();
 bInputLocked=false;EntryAge=-1;bFocused=false;bTravelRequested=false;
 if(Painting)Painting->SetScalarParameterValue(TEXT("Highlight"),0);
 if(EntryCamera){EntryCamera->Destroy();EntryCamera=nullptr;}
}
void AMouseMuseumPortal::EndPlay(const EEndPlayReason::Type Reason){Cancel();Super::EndPlay(Reason);}
