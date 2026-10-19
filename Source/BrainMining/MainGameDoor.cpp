#include "MainGameDoor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"

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
	ClosedRotation = DoorMesh ? DoorMesh->GetRelativeRotation() : FRotator::ZeroRotator;
	AnimationTime = 0.0f;
}

bool AMainGameDoor::IsPlayerInRange(const APawn* Pawn) const
{
	if (!Pawn || !InteractionBox) return false;
	const FVector Local = InteractionBox->GetComponentTransform().InverseTransformPosition(Pawn->GetActorLocation());
	const FVector Extent = InteractionBox->GetScaledBoxExtent() + FVector(120.0f, 80.0f, 60.0f);
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

void AMainGameDoor::OpenDoor()
{
	if (bIsOpen) return;
	bIsOpen = true;
	AnimationTime = 0.0f;
	if (DoorMesh) DoorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void AMainGameDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bIsOpen || !DoorMesh) return;
	AnimationTime = FMath::Min(OpenDuration, AnimationTime + FMath::Max(0.0f, DeltaSeconds));
	const float T = OpenDuration > KINDA_SMALL_NUMBER ? AnimationTime / OpenDuration : 1.0f;
	const float Smooth = T * T * (3.0f - 2.0f * T);
	FRotator Rotation = ClosedRotation;
	Rotation.Yaw += OpenAngle * OpenDirection * Smooth;
	DoorMesh->SetRelativeRotation(Rotation);
}
