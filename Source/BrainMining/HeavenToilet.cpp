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
 for(int32 I=1;I<8;++I)
 {
  auto* Duck=NewObject<UStaticMeshComponent>(this);AddInstanceComponent(Duck);Duck->SetupAttachment(Body);
  Duck->SetStaticMesh(SprayDuck->GetStaticMesh());Duck->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Duck->SetRelativeScale3D(FVector(DuckScale));Duck->RegisterComponent();DuckPool.Add(Duck);
 }
 DuckMotion.SetNum(DuckPool.Num());
 WaterJet->DeactivateImmediate();OpeningSplash->DeactivateImmediate();ResetDuck();ApplyLidPose();
 if(bContinuousSpray){WaterJet->Activate(true);OpeningSplash->Activate(true);bWaterRunning=true;LaunchDuck();}
}
void AHeavenToilet::ResetDuck()
{
 for(int32 I=0;I<DuckPool.Num();++I){DuckPool[I]->SetVisibility(false);DuckMotion[I]=FDuckMotion();}
 BurstAge=0.f;NextDuck=0;
}
void AHeavenToilet::LaunchDuck()
{
 if(!SprayDuck||!SprayDuck->GetStaticMesh()||!WaterOutlet)return;
 if(DuckPool.IsEmpty())return;
 auto* Duck=DuckPool[NextDuck].Get();auto& Motion=DuckMotion[NextDuck];
 Duck->SetRelativeScale3D(FVector(DuckScale));
 Duck->SetWorldLocation(WaterOutlet->GetComponentLocation());
 // Scatter each burst around the basin, with different ranges and flight arcs.
 const float Azimuth=FMath::FRandRange(-PI,PI);
 const FVector Direction(FMath::Cos(Azimuth),FMath::Sin(Azimuth),0);
 Duck->SetWorldRotation(FRotator(0,FMath::RadiansToDegrees(Azimuth),0));Duck->SetVisibility(true);
 Motion.Velocity=Direction*DuckForwardSpeed*FMath::FRandRange(.75f,1.25f)
     +FVector::UpVector*DuckUpSpeed*FMath::FRandRange(.85f,1.3f);
 Motion.Age=0;Motion.Flying=true;NextDuck=(NextDuck+1)%DuckPool.Num();
}
void AHeavenToilet::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 const APawn* Player=UGameplayStatics::GetPlayerPawn(this,0);
 const float Distance=Player ? FVector::Distance(Player->GetActorLocation(),WaterOutlet->GetComponentLocation()) : BIG_NUMBER;
 bPlayerNearby=Player && Distance<(bPlayerNearby ? FMath::Max(OpenDistance,CloseDistance):OpenDistance);
 const bool Open=bContinuousSpray||bPlayerNearby;
 LidOpenAmount=FMath::FInterpConstantTo(LidOpenAmount,Open ? 1.f:0.f,DeltaSeconds,1.f/FMath::Max(.1f,Open ? OpenSeconds:CloseSeconds));
 ApplyLidPose();
 const bool bStartWater=Open && LidOpenAmount>=.85f;
 if(bStartWater && !bWaterRunning)
 {
  WaterJet->Activate(true);OpeningSplash->Activate(true);bWaterRunning=true;LaunchDuck();
 }
 else if(!Open && bWaterRunning)
 {
  // Finish the original droplets naturally instead of cutting the whole effect.
  WaterJet->Deactivate();OpeningSplash->Deactivate();bWaterRunning=false;
 }
 if(bWaterRunning)
 {
  BurstAge+=DeltaSeconds;
  if(BurstAge>=FMath::Max(1.f,DuckBurstInterval))
  {BurstAge=0;OpeningSplash->Activate(true);LaunchDuck();}
 }
 for(int32 I=0;I<DuckPool.Num();++I)
 {
  auto* Duck=DuckPool[I].Get();auto& Motion=DuckMotion[I];if(!Duck->IsVisible())continue;
  Motion.Age+=DeltaSeconds;
  if(Motion.Age>24){Duck->SetVisibility(false);Motion.Flying=false;continue;}
  if(!Motion.Flying)continue;
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
  else {Duck->SetWorldLocation(To);Duck->AddLocalRotation(FRotator(80*DeltaSeconds,120*DeltaSeconds,35*DeltaSeconds));}
 }
}
