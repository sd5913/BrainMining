#include "HeavenToilet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

AHeavenToilet::AHeavenToilet()
{
 PrimaryActorTick.bCanEverTick=true;
 Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));SetRootComponent(Body);
 Body->SetMobility(EComponentMobility::Movable);Body->SetCollisionProfileName(TEXT("BlockAll"));
 LidHinge=CreateDefaultSubobject<USceneComponent>(TEXT("LidHinge"));LidHinge->SetupAttachment(Body);LidHinge->SetRelativeLocation(FVector(-2.8739,-80,264.5));
 Lid=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lid"));Lid->SetupAttachment(LidHinge);Lid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 WaterOutlet=CreateDefaultSubobject<USceneComponent>(TEXT("WaterOutlet"));WaterOutlet->SetupAttachment(Body);WaterOutlet->SetRelativeLocation(FVector(0,60,260));
 WaterJet=CreateDefaultSubobject<UNiagaraComponent>(TEXT("RealWaterJet"));WaterJet->SetupAttachment(WaterOutlet);WaterJet->SetAutoActivate(false);WaterJet->SetAbsolute(false,false,true);WaterJet->SetRelativeScale3D(FVector(.65));
 OpeningSplash=CreateDefaultSubobject<UNiagaraComponent>(TEXT("RealOpeningSplash"));OpeningSplash->SetupAttachment(WaterOutlet);OpeningSplash->SetAutoActivate(false);OpeningSplash->SetAbsolute(false,false,true);OpeningSplash->SetRelativeScale3D(FVector(.45));
 SprayDuck=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SprayDuck"));SprayDuck->SetupAttachment(Body);SprayDuck->SetCollisionEnabled(ECollisionEnabled::NoCollision);SprayDuck->SetGenerateOverlapEvents(false);SprayDuck->SetVisibility(false);SprayDuck->SetRelativeScale3D(FVector(DuckScale));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyAsset(TEXT("/Game/HeavenPreview/Meshes/SM_HeavenToiletBody.SM_HeavenToiletBody"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> LidAsset(TEXT("/Game/HeavenPreview/Meshes/SM_HeavenToiletLid.SM_HeavenToiletLid"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> PorcelainAsset(TEXT("/Game/Fab/Toilet/Toilet.Toilet"));
 static ConstructorHelpers::FObjectFinder<UNiagaraSystem> JetAsset(TEXT("/Game/HeavenPreview/VFX/NS_HeavenToiletWater.NS_HeavenToiletWater"));
 static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SplashAsset(TEXT("/Game/HeavenPreview/VFX/NS_HeavenToiletOpeningSplash.NS_HeavenToiletOpeningSplash"));
 Body->SetStaticMesh(BodyAsset.Object);Lid->SetStaticMesh(LidAsset.Object);
 Body->SetMaterial(0,PorcelainAsset.Object);Lid->SetMaterial(0,PorcelainAsset.Object);
 WaterJet->SetAsset(JetAsset.Object);OpeningSplash->SetAsset(SplashAsset.Object);
 SprayDuck->SetRelativeScale3D(FVector(DuckScale));
 ApplyLidPose();
}

void AHeavenToilet::ApplyLidPose()
{
 const float Ease=FMath::SmoothStep(0.f,1.f,LidOpenAmount);
 LidHinge->SetRelativeRotation(FRotator(0,0,FMath::Lerp(ClosedLidRoll,0.f,Ease)));
}
void AHeavenToilet::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);ApplyLidPose();
}
void AHeavenToilet::BeginPlay()
{
 Super::BeginPlay();LidOpenAmount=bContinuousSpray?1.f:0.f;bPlayerNearby=bWaterRunning=false;
 if(SprayDuck&&!SprayDuck->GetStaticMesh()) SprayDuck->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/MainGame/Meshes/SM_HeavenRubberDuck.SM_HeavenRubberDuck")));
 DuckPool.Add(SprayDuck);
 for(int32 I=1;I<FMath::Clamp(MaxDucks,64,256);++I)
 {
  auto* Duck=NewObject<UStaticMeshComponent>(this);AddInstanceComponent(Duck);Duck->SetupAttachment(Body);
  Duck->SetStaticMesh(SprayDuck->GetStaticMesh());Duck->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Duck->SetGenerateOverlapEvents(false);Duck->SetCanEverAffectNavigation(false);
  Duck->SetRelativeScale3D(FVector(DuckScale));Duck->RegisterComponent();DuckPool.Add(Duck);
 }
 DuckMotion.SetNum(DuckPool.Num());
 WaterJet->DeactivateImmediate();OpeningSplash->DeactivateImmediate();ResetDuck();ApplyLidPose();
 if(bContinuousSpray){WaterJet->Activate(true);OpeningSplash->Activate(true);bWaterRunning=true;StartDuckBurst();}
}
void AHeavenToilet::ResetDuck()
{
 for(int32 I=0;I<DuckPool.Num();++I){DuckPool[I]->SetVisibility(false);DuckMotion[I]=FDuckMotion();}
 BurstAge=0.f;PendingDucks=0;NextDuckDelay=0;
 GushAge=0.f;bGushing=bGushNearby=false;
 DucksLaunched=DucksRecycled=ActiveDucks=FlyingDucks=0;
}
void AHeavenToilet::StartDuckBurst()
{
 PendingDucks=FMath::RandRange(5,7);NextDuckDelay=0;
}
void AHeavenToilet::StartGush(const FVector& PlayerLocation)
{
 UpdateGushAim(PlayerLocation,0.f,true);
 GushAge=0.f;NextDuckDelay=0.f;PendingDucks=0;bGushing=true;
 OpeningSplash->Activate(true);
}
void AHeavenToilet::UpdateGushAim(const FVector& PlayerLocation,float DeltaSeconds,bool bSnap)
{
 const FVector Offset=PlayerLocation-WaterOutlet->GetComponentLocation();
 const FVector TargetDirection=Offset.GetSafeNormal2D();
 if(GushDirection.IsNearlyZero())GushDirection=GetActorForwardVector().GetSafeNormal2D();
 if(!TargetDirection.IsNearlyZero())
 {
  if(bSnap)GushDirection=TargetDirection;
  else
  {
   const float Yaw=GushDirection.Rotation().Yaw;
   const float Error=FMath::FindDeltaAngleDegrees(Yaw,TargetDirection.Rotation().Yaw);
   const float MaxStep=FMath::Max(0.f,GushTurnSpeed)*DeltaSeconds;
   // Ease into the target with a capped turn rate, including across the +/-180 seam.
   const float Step=FMath::Clamp(Error*(1.f-FMath::Exp(-1.4f*DeltaSeconds)),-MaxStep,MaxStep);
   GushDirection=FRotator(0,Yaw+Step,0).Vector();
  }
 }
 const float Gravity=FMath::Max(1.f,DuckGravity);
 const float Height=FMath::Max(0.f,WaterOutlet->GetComponentLocation().Z-PlayerLocation.Z+80.f);
 const float FlightTime=(GushUpSpeed+FMath::Sqrt(FMath::Square(GushUpSpeed)+2.f*Gravity*Height))/Gravity;
 const float TargetSpeed=FMath::Clamp(Offset.Size2D()/FMath::Max(.5f,FlightTime),140.f,620.f);
 GushForwardSpeed=bSnap ? TargetSpeed : FMath::Lerp(GushForwardSpeed,TargetSpeed,1.f-FMath::Exp(-DeltaSeconds));
}
int32 AHeavenToilet::AcquireDuck()
{
 int32 Oldest=INDEX_NONE;
 for(int32 I=0;I<DuckPool.Num();++I)
 {
  if(!DuckPool[I]->IsVisible())return I;
  if(!DuckMotion[I].Flying && (Oldest==INDEX_NONE || DuckMotion[I].Age>DuckMotion[Oldest].Age))Oldest=I;
 }
 // Never steal an airborne duck. Only extreme edited settings can exhaust the flight budget.
 if(Oldest!=INDEX_NONE)++DucksRecycled;
 return Oldest;
}
void AHeavenToilet::LaunchDuck()
{
 if(!SprayDuck||!SprayDuck->GetStaticMesh()||!WaterOutlet)return;
 const int32 Slot=AcquireDuck();if(Slot==INDEX_NONE)return;
 auto* Duck=DuckPool[Slot].Get();auto& Motion=DuckMotion[Slot];Motion=FDuckMotion();
 Duck->SetRelativeScale3D(FVector(DuckScale*FMath::FRandRange(.88f,1.10f)));
 Motion.Scale=Duck->GetRelativeScale3D();
 Duck->SetWorldLocation(WaterOutlet->GetComponentLocation()+FVector(FMath::FRandRange(-14.f,14.f),FMath::FRandRange(-14.f,14.f),FMath::FRandRange(0.f,10.f)));
 // Scatter each burst around the basin, with different ranges and flight arcs.
 const float Azimuth=bGushing ? FMath::Atan2(GushDirection.Y,GushDirection.X)+FMath::DegreesToRadians(FMath::FRandRange(-9.f,9.f)) : FMath::FRandRange(-PI,PI);
 const FVector Direction(FMath::Cos(Azimuth),FMath::Sin(Azimuth),0);
 Duck->SetWorldRotation(FRotator(0,FMath::RadiansToDegrees(Azimuth)+90.f,0));Duck->SetVisibility(true);
 Motion.Velocity=Direction*(bGushing ? GushForwardSpeed*FMath::FRandRange(.88f,1.12f) : DuckForwardSpeed*FMath::FRandRange(.75f,1.25f))
     +FVector::UpVector*(bGushing ? GushUpSpeed*FMath::FRandRange(.92f,1.08f) : DuckUpSpeed*FMath::FRandRange(.85f,1.3f));
 Motion.Spin=FRotator(FMath::FRandRange(-100.f,100.f),FMath::FRandRange(-130.f,130.f),FMath::FRandRange(-65.f,65.f));
 Motion.Flying=true;++DucksLaunched;
}
void AHeavenToilet::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 // A hitch must not release an entire overdue burst in one frame.
 DeltaSeconds=FMath::Clamp(DeltaSeconds,0.f,.1f);
 const APawn* Player=UGameplayStatics::GetPlayerPawn(this,0);
 const float Distance=Player ? FVector::Distance(Player->GetActorLocation(),WaterOutlet->GetComponentLocation()) : BIG_NUMBER;
 bPlayerNearby=Player && Distance<(bPlayerNearby ? FMath::Max(OpenDistance,CloseDistance):OpenDistance);
 const bool Open=bContinuousSpray||bPlayerNearby;
 LidOpenAmount=FMath::FInterpConstantTo(LidOpenAmount,Open ? 1.f:0.f,DeltaSeconds,1.f/FMath::Max(.1f,Open ? OpenSeconds:CloseSeconds));
 ApplyLidPose();
 const bool bStartWater=Open && LidOpenAmount>=.85f;
 if(bStartWater && !bWaterRunning)
 {
  bGushNearby=false;
  WaterJet->Activate(true);OpeningSplash->Activate(true);bWaterRunning=true;StartDuckBurst();
 }
 else if(!Open && bWaterRunning)
 {
  // Finish the original droplets naturally instead of cutting the whole effect.
  WaterJet->Deactivate();OpeningSplash->Deactivate();bWaterRunning=false;PendingDucks=0;
 }
 const bool WasGushNearby=bGushNearby;
 const FVector BodyOffset=Player ? Player->GetActorLocation()-GetActorLocation() : FVector(BIG_NUMBER);
 bGushNearby=Player && BodyOffset.Size2D()<(bGushNearby ? FMath::Max(GushEnterDistance,GushExitDistance):GushEnterDistance)
     && FMath::Abs(BodyOffset.Z)<FMath::Max(GushEnterDistance,GushExitDistance);
 if(bWaterRunning && bGushNearby)
 {
  if(!WasGushNearby)StartGush(Player->GetActorLocation());
  const float Duration=FMath::Max(.1f,GushSeconds),Pause=FMath::Max(.1f,GushPauseSeconds);
  const float EmitTime=bGushing ? FMath::Min(DeltaSeconds,FMath::Max(0.f,Duration-GushAge)) : 0.f;
  GushAge+=DeltaSeconds;
  if(bGushing)
  {
   // Only new launches follow the player; airborne ducks keep their ballistic motion.
   UpdateGushAim(Player->GetActorLocation(),DeltaSeconds);
   NextDuckDelay-=EmitTime;
   while(NextDuckDelay<=0.f)
   {LaunchDuck();NextDuckDelay+=1.f/FMath::Clamp(GushDucksPerSecond,1.f,60.f);}
   if(GushAge>=Duration){bGushing=false;GushAge=0.f;}
  }
  else if(GushAge>=Pause)StartGush(Player->GetActorLocation());
 }
 else
 {
  if(WasGushNearby){bGushing=false;GushAge=0.f;BurstAge=0.f;PendingDucks=0;}
  if(bWaterRunning)
  {
   BurstAge+=DeltaSeconds;
   if(BurstAge>=FMath::Max(1.f,DuckBurstInterval))
   {BurstAge=0;OpeningSplash->Activate(true);StartDuckBurst();}
   NextDuckDelay-=DeltaSeconds;
   while(PendingDucks>0 && NextDuckDelay<=0)
   {LaunchDuck();--PendingDucks;NextDuckDelay+=FMath::FRandRange(.035f,.085f);}
  }
 }
 // Reserve free slots by shrinking the oldest grounded ducks before the next emissions.
 int32 Occupied=0,Retiring=0;
 for(int32 I=0;I<DuckPool.Num();++I)if(DuckPool[I]->IsVisible()){++Occupied;if(DuckMotion[I].Recycling)++Retiring;}
 int32 ToRetire=FMath::Max(0,Occupied-(DuckPool.Num()-20)-Retiring);
 while(ToRetire-->0)
 {
  int32 Oldest=INDEX_NONE;
  for(int32 I=0;I<DuckPool.Num();++I)
   if(DuckPool[I]->IsVisible() && !DuckMotion[I].Flying && !DuckMotion[I].Recycling
       && (Oldest==INDEX_NONE || DuckMotion[I].Age>DuckMotion[Oldest].Age))Oldest=I;
  if(Oldest==INDEX_NONE)break;
  DuckMotion[Oldest].Recycling=true;
 }
 ActiveDucks=FlyingDucks=0;
 for(int32 I=0;I<DuckPool.Num();++I)
 {
  auto* Duck=DuckPool[I].Get();auto& Motion=DuckMotion[I];if(!Duck->IsVisible())continue;
  Motion.Age+=DeltaSeconds;
  if(!Motion.Flying)
  {
   Motion.RestAge+=DeltaSeconds;
   if(Motion.RestAge>22.f)Motion.Recycling=true;
   if(Motion.Recycling)
   {
    Motion.RecycleAge+=DeltaSeconds;
    if(Motion.RecycleAge>=.35f){Duck->SetVisibility(false);++DucksRecycled;continue;}
    // Keep the mesh's lowest point on the floor while it shrinks into the shallow water.
    const float OldScale=Duck->GetComponentScale().Z;
    Duck->SetRelativeScale3D(Motion.Scale*(1.f-FMath::SmoothStep(0.f,.35f,Motion.RecycleAge)));
    Duck->AddWorldOffset(FVector(0,0,Duck->GetStaticMesh()->GetBoundingBox().Min.Z*(OldScale-Duck->GetComponentScale().Z)));
   }
   ++ActiveDucks;continue;
  }
  // Safety for a duck that escaped the authored floor; bounded even in an empty test map.
  if(Motion.Age>12.f){Duck->SetVisibility(false);Motion.Flying=false;++DucksRecycled;continue;}
  ++ActiveDucks;++FlyingDucks;
  Motion.Velocity.Z-=DuckGravity*DeltaSeconds;
  const FVector From=Duck->GetComponentLocation(),To=From+Motion.Velocity*DeltaSeconds;
  FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(HeavenDuckLanding),false,this);
  // The collectible's invisible selection box surrounds the nozzle as well.
  if(GetOwner())Query.AddIgnoredActor(GetOwner());
  if(Player)Query.AddIgnoredActor(Player);
  const float Radius=FMath::Clamp(Duck->Bounds.SphereRadius*.35f,3.f,14.f);
  if(GetWorld()->SweepSingleByChannel(Hit,From,To,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(Radius),Query))
  {
   Duck->SetWorldLocation(Hit.Location+Hit.ImpactNormal*2);
   if(Hit.ImpactNormal.Z>.45f)
   {
    // Preserve the authored pivot offset so the duck rests on, rather than in, the floor.
    const auto Bounds=Duck->GetStaticMesh()->GetBoundingBox();
    Duck->SetWorldRotation(FRotator(0,Duck->GetComponentRotation().Yaw,0));
    FVector Rest=Hit.ImpactPoint;Rest.Z-=Bounds.Min.Z*Duck->GetComponentScale().Z;Rest.Z+=1;
    Duck->SetWorldLocation(Rest);Motion.Flying=false;
   }
   else Motion.Velocity=Motion.Velocity.MirrorByVector(Hit.ImpactNormal)*.35f;
  }
  else {Duck->SetWorldLocation(To);Duck->AddLocalRotation(Motion.Spin*DeltaSeconds);}
 }
}
