#include "PopupClose/PopupCloseFlyingWindow.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

APopupCloseFlyingWindow::APopupCloseFlyingWindow()
{
	PrimaryActorTick.bCanEverTick = false;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetCollisionObjectType(ECC_WorldDynamic);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Body->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Body->SetSimulatePhysics(true);
	Body->SetEnableGravity(true);
	Body->SetMassOverrideInKg(NAME_None, 0.35f, true);
	Body->SetLinearDamping(0.35f);
	Body->SetAngularDamping(0.6f);
	Body->SetRelativeScale3D(FVector(0.02f, 0.46f, 0.62f));

	TitleBar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TitleBar"));
	TitleBar->SetupAttachment(Body);
	TitleBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TitleBar->SetRelativeLocation(FVector(0.f, 0.f, 38.f));
	TitleBar->SetRelativeScale3D(FVector(1.4f, 1.f, 0.16f));

	CloseButton = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CloseButton"));
	CloseButton->SetupAttachment(Body);
	CloseButton->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CloseButton->SetRelativeLocation(FVector(0.f, 18.f, 38.f));
	CloseButton->SetRelativeScale3D(FVector(1.8f, 0.12f, 0.12f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Body->SetStaticMesh(Cube.Object);
		TitleBar->SetStaticMesh(Cube.Object);
		CloseButton->SetStaticMesh(Cube.Object);
	}
}

void APopupCloseFlyingWindow::Launch(const FLinearColor& Color, const FVector& Velocity)
{
	static UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BaseMaterial)
	{
		auto Paint = [this](UStaticMeshComponent* Mesh, const FLinearColor& PaintColor)
		{
			if (!Mesh || !BaseMaterial)
			{
				return;
			}
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			Material->SetVectorParameterValue(TEXT("Color"), PaintColor);
			Mesh->SetMaterial(0, Material);
		};
		Paint(Body, Color);
		Paint(TitleBar, Color * 0.55f);
		Paint(CloseButton, FLinearColor(0.85f, 0.16f, 0.14f));
	}

	Body->SetPhysicsLinearVelocity(Velocity);
	Body->SetPhysicsAngularVelocityInDegrees(FVector(
		FMath::FRandRange(-80.f, 80.f),
		FMath::FRandRange(-40.f, 40.f),
		FMath::FRandRange(-120.f, 120.f)));
}
