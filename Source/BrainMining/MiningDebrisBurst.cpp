#include "MiningDebrisBurst.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AMiningDebrisBurst::AMiningDebrisBurst()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("BurstRoot"));
	Tags.AddUnique(TEXT("BM_PlayerEffect"));
}

void AMiningDebrisBurst::PreparePool(const TArray<TObjectPtr<UStaticMesh>>& Meshes, int32 AdditionalCount)
{
	if (Meshes.IsEmpty() || bPoolActive) return;
	bPooled = true;
	SetActorHiddenInGame(true);
	const int32 TargetCount = FMath::Min(64, PooledChunks.Num() + FMath::Clamp(AdditionalCount, 0, 4));
	while (PooledChunks.Num() < TargetCount)
	{
		const int32 Index = PooledChunks.Num();
		auto* Chunk = NewObject<UStaticMeshComponent>(this);
		AddInstanceComponent(Chunk);
		Chunk->SetupAttachment(RootComponent);
		const int32 MeshIndex = Index < 16 ? Index % FMath::Min(4, Meshes.Num()) : FMath::Min(4 + Index % 4, Meshes.Num() - 1);
		Chunk->SetStaticMesh(Meshes[MeshIndex]);
		Chunk->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Chunk->SetCollisionObjectType(ECC_PhysicsBody);
		Chunk->SetCollisionResponseToAllChannels(ECR_Ignore);
		Chunk->SetGenerateOverlapEvents(false);
		Chunk->SetCanEverAffectNavigation(false);
		Chunk->SetLinearDamping(.45f);
		Chunk->SetAngularDamping(1.4f);
		Chunk->SetVisibility(false);
		Chunk->RegisterComponent();
		Chunk->SetSimulatePhysics(true);
		Chunk->SetSimulatePhysics(false);
		PooledChunks.Add(Chunk);
	}
}

void AMiningDebrisBurst::ReleaseToPool()
{
	if (!bPooled) return;
	SetLifeSpan(0);
	for (UStaticMeshComponent* Chunk : PooledChunks)
	{
		Chunk->SetSimulatePhysics(false);
		Chunk->SetCollisionResponseToAllChannels(ECR_Ignore);
		Chunk->SetVisibility(false);
		Chunk->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
	}
	SetActorHiddenInGame(true);
	bPoolActive = false;
	ChunkCount = 0;
}

void AMiningDebrisBurst::LifeSpanExpired()
{
	if (bPooled) ReleaseToPool();
	else Super::LifeSpanExpired();
}

void AMiningDebrisBurst::Initialize(const TArray<TObjectPtr<UStaticMesh>>& Meshes, UMaterialInterface* Material, const FVector& SurfaceNormal, float CutRadius, int32 Count, float Lifetime)
{
	SpawnChunks(Meshes, Material, SurfaceNormal, CutRadius, Count, Lifetime, false);
}

void AMiningDebrisBurst::InitializeBlast(const TArray<TObjectPtr<UStaticMesh>>& Meshes, UMaterialInterface* Material, const FVector& Direction, float CutRadius, int32 Count, float Lifetime)
{
	Tags.AddUnique(TEXT("BM_PlayerEffect"));
	SpawnChunks(Meshes, Material, Direction, CutRadius, Count, Lifetime, true);
}

void AMiningDebrisBurst::SpawnChunks(const TArray<TObjectPtr<UStaticMesh>>& Meshes, UMaterialInterface* Material, const FVector& SurfaceNormal, float CutRadius, int32 Count, float Lifetime, bool bBlast)
{
	if (Meshes.IsEmpty() || !Material) { if (bPooled) ReleaseToPool(); else Destroy(); return; }
	if (bPooled) ReleaseToPool();
	ChunkCount = 0;
	SourceMaterial = Material;
	const FVector Normal = SurfaceNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	FVector Tangent, Bitangent;
	Normal.FindBestAxisVectors(Tangent, Bitangent);
	const float SizeFactor = FMath::Clamp(CutRadius / 160.f, 0.55f, 1.8f);
	const float SpreadRadius = FMath::Clamp(CutRadius * 0.22f, 12.f, 45.f);
	Count = bBlast ? FMath::Clamp(Count, 1, 96) : FMath::Clamp(Count, 12, 64);
	if (bPooled) Count = FMath::Min(Count, PooledChunks.Num());
	bPoolActive = bPooled && Count > 0;
	SetActorHiddenInGame(false);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// Large irregular chunks, medium clods and small crumbs, rather than identical chips.
		const float Fraction = float(Index) / Count;
		const bool bLarge = Fraction < 0.25f;
		const bool bMedium = Fraction < 0.7f;
		const int32 FirstMesh = bLarge ? 0 : FMath::Min(4, Meshes.Num() - 1);
		const int32 PoolIndex = PooledChunks.Num() == 64 && !bLarge ? 16 + Index - FMath::CeilToInt(Count * .25f) : Index;
		UStaticMeshComponent* Chunk = bPooled ? PooledChunks[PoolIndex].Get() : nullptr;
		UStaticMesh* Mesh = Chunk ? Chunk->GetStaticMesh() : Meshes[FMath::RandRange(FirstMesh, FMath::Min(FirstMesh + 3, Meshes.Num() - 1))];
		if (!Mesh) continue;
		const float Diameter = SizeFactor * (bLarge ? FMath::FRandRange(14.f, 24.f) : bMedium ? FMath::FRandRange(7.f, 13.f) : FMath::FRandRange(3.f, 6.f));
		const float Scale = Diameter / FMath::Max(1.f, static_cast<float>(Mesh->GetBounds().BoxExtent.GetMax() * 2));
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Distance = SpreadRadius * FMath::Sqrt(FMath::FRand());
		const FVector Radial = Tangent * FMath::Cos(Angle) + Bitangent * FMath::Sin(Angle);
		if (!Chunk)
		{
			Chunk = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("SoilChunk%d"), Index));
			AddInstanceComponent(Chunk);
			Chunk->SetupAttachment(RootComponent);
			Chunk->SetStaticMesh(Mesh);
		}
		Chunk->SetVisibility(true);
		const FTransform SpawnTransform(
			FRotator(FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-180.f, 180.f)),
			GetActorLocation() + Normal * FMath::FRandRange(8.f, 20.f) + Radial * Distance,
			FVector(Scale, Scale * FMath::FRandRange(0.8f, 1.2f), Scale));
		Chunk->SetWorldTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
		for (int32 Slot = 0; Slot < FMath::Max(1, Mesh->GetStaticMaterials().Num()); ++Slot) Chunk->SetMaterial(Slot, SourceMaterial);
		Chunk->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Chunk->SetCollisionObjectType(ECC_PhysicsBody);
		Chunk->SetCollisionResponseToAllChannels(ECR_Ignore);
		Chunk->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Chunk->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		Chunk->SetGenerateOverlapEvents(false);
		Chunk->SetLinearDamping(0.45f);
		Chunk->SetAngularDamping(1.4f);
		if (!Chunk->IsRegistered()) Chunk->RegisterComponent();
		Chunk->SetSimulatePhysics(true);
		// Reused Chaos bodies retain their previous simulated pose. Teleport after
		// enabling simulation as well, so the next physics sync cannot put this
		// strike's fragments back at the preceding strike's world position.
		Chunk->SetWorldTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
		Chunk->WakeAllRigidBodies();
		Chunk->SetPhysicsLinearVelocity(Normal * FMath::FRandRange(170.f, 300.f) + Radial * FMath::FRandRange(90.f, 210.f) + FVector::UpVector * FMath::FRandRange(110.f, 240.f));
		if (bBlast) Chunk->SetPhysicsLinearVelocity(Normal * FMath::FRandRange(450.f, 850.f) + Radial * FMath::FRandRange(100.f, 250.f) + FVector::UpVector * FMath::FRandRange(130.f, 350.f));
		Chunk->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-420.f, 420.f), FMath::FRandRange(-420.f, 420.f), FMath::FRandRange(-420.f, 420.f)));
		++ChunkCount;
	}
	SetLifeSpan(FMath::Clamp(Lifetime, 0.5f, 4.f));
}
