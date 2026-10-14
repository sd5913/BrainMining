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
 static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyAsset(TEXT("/Game/HeavenPreview/Meshes/SM_HeavenToiletBody.SM_HeavenToiletBody"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> LidAsset(TEXT("/Game/HeavenPreview/Meshes/SM_HeavenToiletLid.SM_HeavenToiletLid"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> PorcelainAsset(TEXT("/Game/Fab/Toilet/Toilet.Toilet"));
 static ConstructorHelpers::FObjectFinder<UNiagaraSystem> JetAsset(TEXT("/Game/HeavenPreview/VFX/NS_HeavenToiletWater.NS_HeavenToiletWater"));
 static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SplashAsset(TEXT("/Game/HeavenPreview/VFX/NS_HeavenToiletOpeningSplash.NS_HeavenToiletOpeningSplash"));
 Body->SetStaticMesh(BodyAsset.Object);Lid->SetStaticMesh(LidAsset.Object);
 Body->SetMaterial(0,PorcelainAsset.Object);Lid->SetMaterial(0,PorcelainAsset.Object);
 WaterJet->SetAsset(JetAsset.Object);OpeningSplash->SetAsset(SplashAsset.Object);
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
 Super::BeginPlay();LidOpenAmount=0.f;bPlayerNearby=bWaterRunning=false;
 WaterJet->DeactivateImmediate();OpeningSplash->DeactivateImmediate();ApplyLidPose();
}
void AHeavenToilet::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 const APawn* Player=UGameplayStatics::GetPlayerPawn(this,0);
 const float Distance=Player ? FVector::Distance(Player->GetActorLocation(),WaterOutlet->GetComponentLocation()) : BIG_NUMBER;
 bPlayerNearby=Player && Distance<(bPlayerNearby ? FMath::Max(OpenDistance,CloseDistance):OpenDistance);
 LidOpenAmount=FMath::FInterpConstantTo(LidOpenAmount,bPlayerNearby ? 1.f:0.f,DeltaSeconds,1.f/FMath::Max(.1f,bPlayerNearby ? OpenSeconds:CloseSeconds));
 ApplyLidPose();
 const bool bStartWater=bPlayerNearby && LidOpenAmount>=.85f;
 if(bStartWater && !bWaterRunning)
 {
  WaterJet->Activate(true);OpeningSplash->Activate(true);bWaterRunning=true;
 }
 else if(!bPlayerNearby && bWaterRunning)
 {
  // Finish the original droplets naturally instead of cutting the whole effect.
  WaterJet->Deactivate();OpeningSplash->Deactivate();bWaterRunning=false;
 }
}
