#include "MiningDebrisBurst.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AMiningDebrisBurst::AMiningDebrisBurst()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("BurstRoot"));
}

void AMiningDebrisBurst::Initialize(const TArray<TObjectPtr<UStaticMesh>>& Meshes, UMaterialInterface* Material, const FVector& SurfaceNormal, float CutRadius, int32 Count, float Lifetime)
{
	if (Meshes.IsEmpty() || !Material) { Destroy(); return; }
	SourceMaterial = Material;
	const FVector Normal = SurfaceNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	FVector Tangent, Bitangent;
	Normal.FindBestAxisVectors(Tangent, Bitangent);
	const float SizeFactor = FMath::Clamp(CutRadius / 160.f, 0.55f, 1.8f);
	const float SpreadRadius = FMath::Clamp(CutRadius * 0.22f, 12.f, 45.f);
	Count = FMath::Clamp(Count, 12, 64);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// Large irregular chunks, medium clods and small crumbs, rather than identical chips.
		const float Fraction = float(Index) / Count;
		const bool bLarge = Fraction < 0.25f;
		const bool bMedium = Fraction < 0.7f;
		const int32 FirstMesh = bLarge ? 0 : FMath::Min(4, Meshes.Num() - 1);
		UStaticMesh* Mesh = Meshes[FMath::RandRange(FirstMesh, FMath::Min(FirstMesh + 3, Meshes.Num() - 1))];
		if (!Mesh) continue;
		const float Diameter = SizeFactor * (bLarge ? FMath::FRandRange(14.f, 24.f) : bMedium ? FMath::FRandRange(7.f, 13.f) : FMath::FRandRange(3.f, 6.f));
		const float Scale = Diameter / FMath::Max(1.f, static_cast<float>(Mesh->GetBounds().BoxExtent.GetMax() * 2));
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Distance = SpreadRadius * FMath::Sqrt(FMath::FRand());
		const FVector Radial = Tangent * FMath::Cos(Angle) + Bitangent * FMath::Sin(Angle);
		UStaticMeshComponent* Chunk = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("SoilChunk%d"), Index));
		AddInstanceComponent(Chunk);
		Chunk->SetupAttachment(RootComponent);
		Chunk->SetStaticMesh(Mesh);
		Chunk->SetRelativeScale3D(FVector(Scale, Scale * FMath::FRandRange(0.8f, 1.2f), Scale));
		Chunk->SetWorldLocation(GetActorLocation() + Normal * FMath::FRandRange(8.f, 20.f) + Radial * Distance);
		Chunk->SetWorldRotation(FRotator(FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-180.f, 180.f)));
		for (int32 Slot = 0; Slot < FMath::Max(1, Mesh->GetStaticMaterials().Num()); ++Slot) Chunk->SetMaterial(Slot, SourceMaterial);
		Chunk->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Chunk->SetCollisionObjectType(ECC_PhysicsBody);
		Chunk->SetCollisionResponseToAllChannels(ECR_Ignore);
		Chunk->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Chunk->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		Chunk->SetGenerateOverlapEvents(false);
		Chunk->SetLinearDamping(0.45f);
		Chunk->SetAngularDamping(1.4f);
		Chunk->RegisterComponent();
		Chunk->SetSimulatePhysics(true);
		Chunk->SetPhysicsLinearVelocity(Normal * FMath::FRandRange(170.f, 300.f) + Radial * FMath::FRandRange(90.f, 210.f) + FVector::UpVector * FMath::FRandRange(110.f, 240.f));
		Chunk->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-420.f, 420.f), FMath::FRandRange(-420.f, 420.f), FMath::FRandRange(-420.f, 420.f)));
		++ChunkCount;
	}
	SetLifeSpan(FMath::Clamp(Lifetime, 0.5f, 4.f));
}
