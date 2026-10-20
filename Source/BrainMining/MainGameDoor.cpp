#include "MainGameDoor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "MiningInteractionTrace.h"
#include "StartRoom/MiningOpening.h"
#include "StartRoom/MiningStartRoom.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"

AMainGameDoor::AMainGameDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	SetRootComponent(DoorMesh);
	DoorMesh->SetCollisionProfileName(TEXT("BlockAll"));

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(DoorMesh);
	InteractionBox->SetBoxExtent(FVector(260.0f, 180.0f, 140.0f));
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AMainGameDoor::BeginPlay()
{
	Super::BeginPlay();
	ClosedLocation=DoorMesh->GetComponentLocation();
	ClosedRotation=DoorMesh->GetComponentRotation();
	const FBox Bounds=DoorMesh->GetStaticMesh()?DoorMesh->GetStaticMesh()->GetBoundingBox():FBox(FVector(-10,-100,0),FVector(10,100,240));
	const FVector ScaledSize=Bounds.GetSize()*DoorMesh->GetComponentScale().GetAbs();
	const FVector Hinge=ScaledSize.X<ScaledSize.Y?FVector(Bounds.GetCenter().X,Bounds.Min.Y,Bounds.Min.Z):FVector(Bounds.Min.X,Bounds.GetCenter().Y,Bounds.Min.Z);
	HingeLocation=DoorMesh->GetComponentTransform().TransformPosition(Hinge);
	if(const auto* Room=AMiningStartRoom::Find(GetWorld()))
	{
		// Choose the swing whose initial arc moves away from the room interior.
		const FVector Outward=ClosedLocation-Room->GetActorLocation();
		const FVector PositiveArc=FVector::CrossProduct(FVector::UpVector,ClosedLocation-HingeLocation);
		OpenDirection=FVector::DotProduct(PositiveArc,Outward)>=0.f?1.f:-1.f;
	}
	DoorMesh->SetMobility(EComponentMobility::Movable);
	Highlight=NewObject<UPostProcessComponent>(this);AddInstanceComponent(Highlight);
	Highlight->SetupAttachment(RootComponent);Highlight->bUnbound=true;Highlight->bEnabled=false;
	if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionSilhouette")))Highlight->Settings.AddBlendable(Material,1);
	Highlight->RegisterComponent();
	AnimationTime = 0.0f;
}

bool AMainGameDoor::IsPlayerInRange(const APawn* Pawn) const
{
	if (!Pawn || !InteractionBox) return false;
	return DoorMesh && DoorMesh->Bounds.GetBox().ComputeSquaredDistanceToPoint(Pawn->GetPawnViewLocation())<=FMath::Square(350.f);
}

AMainGameDoor* AMainGameDoor::FindInteractionTarget(APlayerController* Controller)
{
	if(!Controller || !Controller->GetPawn() || Controller->IsPaused() || Controller->bShowMouseCursor || Controller->IsLookInputIgnored())return nullptr;
	FVector Eye;FRotator View;Controller->GetPlayerViewPoint(Eye,View);
	FHitResult Hit;
	if(MiningInteraction::Trace(Controller->GetWorld(),Hit,Eye,View,350,FCollisionQueryParams(SCENE_QUERY_STAT(StartDoor),true,Controller->GetPawn()),[&](const FHitResult& H)
	{
		auto* Door=Cast<AMainGameDoor>(H.GetActor());if(!Door && H.GetActor())Door=Cast<AMainGameDoor>(H.GetActor()->GetAttachParentActor());return Door && !Door->bIsOpen && !Door->IsHidden() && Door->IsPlayerInRange(Controller->GetPawn());
	}))
	{
		if(auto* Door=Cast<AMainGameDoor>(Hit.GetActor()))return Door;
		return Hit.GetActor()?Cast<AMainGameDoor>(Hit.GetActor()->GetAttachParentActor()):nullptr;
	}
	return nullptr;
}

void AMainGameDoor::OpenDoor()
{
	if (bIsOpen) return;
    // The authored room actor's origin is not its interior. Use the actual
    // opening seat (or the interacting player), and ignore a saved angle sign.
    FVector Inside=ClosedLocation-GetActorForwardVector()*300;
    if(auto* PC=GetWorld()->GetFirstPlayerController();PC&&PC->GetPawn())Inside=PC->GetPawn()->GetActorLocation();
    for(TActorIterator<AMiningOpening> It(GetWorld());It;++It){Inside=It->SeatEye;break;}
    const FVector Centre=DoorMesh->Bounds.Origin;
    const FVector Outward=(Centre-Inside).GetSafeNormal2D();
    const FQuat Probe(FVector::UpVector,FMath::DegreesToRadians(FMath::Abs(OpenAngle)*.25f));
    const FVector Arc=HingeLocation+Probe.RotateVector(Centre-HingeLocation)-Centre;
    OpenDirection=FVector::DotProduct(Arc,Outward)>=0?1.f:-1.f;
	bIsOpen = true;
	DoorMesh->SetRenderCustomDepth(false);if(Highlight)Highlight->bEnabled=false;
	AnimationTime = 0.0f;
	if (DoorMesh) DoorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

}

void AMainGameDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const bool Focused=!bIsOpen && FindInteractionTarget(GetWorld()->GetFirstPlayerController())==this;
	DoorMesh->SetRenderCustomDepth(Focused);DoorMesh->SetCustomDepthStencilValue(1);
	TArray<AActor*> Parts;GetAttachedActors(Parts);
	for(auto* Part:Parts){TArray<UStaticMeshComponent*> Meshes;Part->GetComponents(Meshes);for(auto* Mesh:Meshes){Mesh->SetRenderCustomDepth(Focused);Mesh->SetCustomDepthStencilValue(1);}}
	if(Highlight)Highlight->bEnabled=Focused;
	if (!bIsOpen || !DoorMesh) return;
	AnimationTime = FMath::Min(OpenDuration, AnimationTime + FMath::Max(0.0f, DeltaSeconds));
	const float T = OpenDuration > KINDA_SMALL_NUMBER ? AnimationTime / OpenDuration : 1.0f;
	const float Smooth = T * T * (3.0f - 2.0f * T);
	const FQuat Turn(FVector::UpVector,FMath::DegreesToRadians(FMath::Abs(OpenAngle)*OpenDirection*Smooth));
	DoorMesh->SetWorldLocationAndRotation(HingeLocation+Turn.RotateVector(ClosedLocation-HingeLocation),Turn*ClosedRotation.Quaternion());
}
