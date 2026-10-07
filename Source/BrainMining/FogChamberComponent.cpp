#include "FogChamberComponent.h"
#include "BrainMining.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMeshEditor.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "GameFramework/PlayerController.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

using namespace UE::Geometry;

UFogChamberComponent::UFogChamberComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	PuffMeshAsset = Plane.Object;
}

void UFogChamberComponent::BeginPlay()
{
	Super::BeginPlay();
	FogMesh = NewObject<UInstancedStaticMeshComponent>(GetOwner(), TEXT("ChamberFogPuffs"));
	FogMesh->SetupAttachment(GetOwner()->GetRootComponent());
	FogMesh->SetMobility(EComponentMobility::Movable);
	FogMesh->SetStaticMesh(PuffMeshAsset);
	FogMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FogMesh->SetGenerateOverlapEvents(false);
	FogMesh->SetCastShadow(false);
	FogMesh->SetCanEverAffectNavigation(false);
	FogMesh->SetNumCustomDataFloats(1);
	FogMesh->RegisterComponent();
	Puffs.SetNum(192);
	PuffTransforms.SetNum(192);
	for (int32 PuffIndex = 0; PuffIndex < Puffs.Num(); ++PuffIndex)
	{
		FogMesh->AddInstance(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector));
	}
	ResetEffects();
}

void UFogChamberComponent::ResetEffects()
{
	State = EFogChamberState::Sealed;
	BreachCount = 0;
	ActivePuffs = 0;
	RemainingWall = -1;
	Pressure = 1;
	VentAge = 0;
	SpawnRemainder = 0;
	AmbientRemainder = 0;
	OutletLocation = FVector::ZeroVector;
	OutletDirection = FVector::ForwardVector;
	AirNodes.Reset();
	Seeps.Reset();
	Random.Initialize(1042026);
	for (int32 PuffIndex = 0; PuffIndex < Puffs.Num(); ++PuffIndex)
	{
		Puffs[PuffIndex] = FPuff();
		PuffTransforms[PuffIndex] = FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
		FogMesh->SetCustomDataValue(PuffIndex, 0, 0);
	}
	if (FogMesh)
	{
		FogMesh->SetMaterial(0, FogMaterial);
		FogMesh->SetVisibility(bRoomReady && FogMaterial != nullptr);
		FogMesh->BatchUpdateInstancesTransforms(0, PuffTransforms, false, true, true);
	}
}

bool UFogChamberComponent::InitializeRoom(FDynamicMesh3& Mesh, FVector RockSize)
{
	bRoomReady = false;
	RockExtent = RockSize * 0.5;
	RoomBounds = FBox(RoomOffset - RoomSize * 0.5, RoomOffset + RoomSize * 0.5);
	const FVector OuterMargin = RockExtent - RoomOffset.GetAbs() - RoomSize * 0.5;
	if (bEnabled && !RoomSize.ContainsNaN() && !RoomOffset.ContainsNaN() &&
		RoomSize.GetMin() >= 100 && OuterMargin.GetMin() >= 10)
	{
		FMinimalBoxMeshGenerator Generator;
		Generator.Box = FOrientedBox3d(FVector3d(RoomOffset), FVector3d(RoomSize * 0.5));
		Generator.Generate();
		FDynamicMesh3 Interior(&Generator);
		Interior.ReverseOrientation();
		FDynamicMeshEditor Editor(&Mesh);
		FMeshIndexMappings Mappings;
		Editor.AppendMesh(&Interior, Mappings);
		bRoomReady = true;
	}
	ResetEffects();
	return !bEnabled || bRoomReady;
}

void UFogChamberComponent::ObserveCut(const FDynamicMesh3& Mesh, FVector LocalCenter, double LocalRadius)
{
	if (!bRoomReady || BreachCount > 0)
	{
		return;
	}
	if (AirNodes.ContainsByPredicate([LocalCenter, LocalRadius](const FAirNode& Node)
	{
		return FVector::DistSquared(Node.Center, LocalCenter) < 0.01 && FMath::Abs(Node.Radius - LocalRadius) < 0.01;
	}))
	{
		return;
	}
	FDynamicMeshAABBTree3 Spatial(&Mesh);
	const auto ClearSegment = [&Spatial](FVector Start, FVector End)
	{
		const FVector Delta = End - Start;
		const double Distance = Delta.Length();
		if (Distance < 0.1)
		{
			return true;
		}
		double HitDistance = TNumericLimits<double>::Max();
		int32 HitTriangle = INDEX_NONE;
		return !Spatial.FindNearestHitTriangle(FRay3d(FVector3d(Start), FVector3d(Delta / Distance)), HitDistance, HitTriangle) || HitDistance > Distance;
	};
	FAirNode NewNode;
	NewNode.Center = LocalCenter;
	NewNode.Radius = LocalRadius;
	const FVector ClosestRoom = RoomBounds.GetClosestPointTo(LocalCenter);
	FVector RoomPoint = ClosestRoom + (RoomOffset - ClosestRoom).GetSafeNormal() * 3.0;
	if (RoomBounds.IsInside(LocalCenter))
	{
		RoomPoint = RoomOffset;
	}
	NewNode.bRoom = FVector::Dist(ClosestRoom, LocalCenter) < LocalRadius && ClearSegment(LocalCenter, RoomPoint);
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		for (int32 Side : {-1, 1})
		{
			FVector Outside = LocalCenter;
			Outside[Axis] = Side * (RockExtent[Axis] + LocalRadius + 10);
			NewNode.bExterior |= ClearSegment(LocalCenter, Outside);
		}
	}
	const int32 NewIndex = AirNodes.Num();
	for (int32 NodeIndex = 0; NodeIndex < AirNodes.Num(); ++NodeIndex)
	{
		FAirNode& Previous = AirNodes[NodeIndex];
		if (((Previous.bRoom && NewNode.bRoom) || FVector::Dist(Previous.Center, LocalCenter) < Previous.Radius + LocalRadius) &&
			((Previous.bRoom && NewNode.bRoom) || ClearSegment(LocalCenter, Previous.Center)))
		{
			Previous.Neighbors.Add(NewIndex);
			NewNode.Neighbors.Add(NodeIndex);
		}
	}
	AirNodes.Add(MoveTemp(NewNode));
	TArray<int32> Queue;
	for (int32 NodeIndex = 0; NodeIndex < AirNodes.Num(); ++NodeIndex)
	{
		if (AirNodes[NodeIndex].bExterior)
		{
			Queue.Add(NodeIndex);
		}
	}
	for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
	{
		for (int32 Neighbor : AirNodes[Queue[QueueIndex]].Neighbors)
		{
			if (!AirNodes[Neighbor].bExterior)
			{
				AirNodes[Neighbor].bExterior = true;
				Queue.Add(Neighbor);
			}
		}
	}
	for (int32 NodeIndex = AirNodes.Num() - 1; NodeIndex >= 0; --NodeIndex)
	{
		const FAirNode& Node = AirNodes[NodeIndex];
		if (Node.bRoom && Node.bExterior)
		{
			OutletLocalDirection = (Node.Center - RoomOffset).GetSafeNormal(UE_SMALL_NUMBER, FVector::BackwardVector);
			OutletLocal = RoomBounds.GetClosestPointTo(Node.Center);
			if (RoomBounds.IsInside(Node.Center))
			{
				OutletLocal = Node.Center;
			}
			OutletLocal += (Node.Center - OutletLocal).GetSafeNormal() * 3.0;
			OutletLocation = GetOwner()->GetActorTransform().TransformPosition(OutletLocal);
			OutletDirection = GetOwner()->GetActorTransform().TransformVectorNoScale(OutletLocalDirection).GetSafeNormal();
			State = EFogChamberState::Venting;
			BreachCount = 1;
			RemainingWall = 0;
			Seeps.Reset();
			SpawnRemainder = 24;
			UE_LOG(LogBrainMining, Display, TEXT("Fog chamber breached at %s"), *OutletLocation.ToString());
			return;
		}
	}
	if (AirNodes.Last().bExterior)
	{
		const FVector Direction = (RoomPoint - LocalCenter).GetSafeNormal();
		double HitDistance = TNumericLimits<double>::Max();
		int32 HitTriangle = INDEX_NONE;
		if (!Direction.IsNearlyZero() && Spatial.FindNearestHitTriangle(FRay3d(FVector3d(LocalCenter), FVector3d(Direction)), HitDistance, HitTriangle))
		{
			const FVector Surface = LocalCenter + Direction * HitDistance;
			const float Thickness = FVector::Dist(Surface, RoomBounds.GetClosestPointTo(Surface));
			if (Thickness > 0.1f && Thickness < SeepDistance && HitDistance < FVector::Dist(LocalCenter, RoomPoint))
			{
				RemainingWall = Thickness;
				State = EFogChamberState::Seeping;
				Seeps.RemoveAll([&Surface](const FSeep& Seep) { return FVector::DistSquared(Seep.Position, Surface) < 10000; });
				if (Seeps.Num() >= 4)
				{
					Seeps.RemoveAt(0);
				}
				Seeps.Add({Surface - Direction * 5.0, -Direction, 1.0f - Thickness / FMath::Max(1.0f, SeepDistance), 12.0f});
			}
		}
	}
}

void UFogChamberComponent::SpawnPuff(FVector Position, FVector Velocity, float Size, float Opacity, float Life, bool bAmbient)
{
	for (FPuff& Puff : Puffs)
	{
		if (Puff.Life <= Puff.Age)
		{
			Puff = {Position, Velocity, 0, Life, Size, Opacity, bAmbient};
			return;
		}
	}
}

void UFogChamberComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bRoomReady)
	{
		return;
	}
	if (BreachCount > 0)
	{
		VentAge += DeltaTime;
		Pressure = FMath::Clamp(1.0f - VentAge / FMath::Max(0.5f, BurstDuration), 0.0f, 1.0f);
		State = Pressure > 0 ? EFogChamberState::Venting : EFogChamberState::Open;
	}
	if (!FogMesh || !FogMaterial)
	{
		return;
	}
	const float Step = FMath::Min(DeltaTime, 0.05f);
	const FTransform OwnerTransform = GetOwner()->GetActorTransform();
	AmbientRemainder += Step * 12;
	while (AmbientRemainder >= 1)
	{
		AmbientRemainder -= 1;
		const float Size = FMath::Min(Random.FRandRange(90, 145), static_cast<float>(RoomSize.GetMin() * 0.6));
		const FVector Extent = RoomSize * 0.5 - FVector(Size * 0.6 + 5);
		const FVector Position = RoomOffset + FVector(Random.FRandRange(-Extent.X, Extent.X), Random.FRandRange(-Extent.Y, Extent.Y), Random.FRandRange(-Extent.Z, Extent.Z));
		SpawnPuff(Position, FVector::ZeroVector, Size, 0.22f, 5, true);
	}
	if (BreachCount > 0)
	{
		SpawnRemainder += Step * (6 + Pressure * 65);
		while (SpawnRemainder >= 1)
		{
			SpawnRemainder -= 1;
			SpawnPuff(OutletLocal, OutletLocalDirection * (65 + Pressure * 280) + Random.VRand() * 20 + FVector(0, 0, 20), Random.FRandRange(22, 38), 0.5f, 2.8f, false);
		}
	}
	for (FSeep& Seep : Seeps)
	{
		Seep.Life -= DeltaTime;
		if (Random.FRand() < Step * (5 + 10 * Seep.Strength))
		{
			SpawnPuff(Seep.Position, Seep.Direction * 22 + FVector(0, 0, 24) + Random.VRand() * 8, Random.FRandRange(28, 42), 0.28f + 0.2f * Seep.Strength, 2.5f, false);
		}
	}
	Seeps.RemoveAll([](const FSeep& Seep) { return Seep.Life <= 0; });
	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
	FVector CameraPosition;
	FRotator CameraRotation;
	if (!Controller)
	{
		return;
	}
	Controller->GetPlayerViewPoint(CameraPosition, CameraRotation);
	ActivePuffs = 0;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(FogPuffTrace), true);
	for (int32 PuffIndex = 0; PuffIndex < Puffs.Num(); ++PuffIndex)
	{
		FPuff& Puff = Puffs[PuffIndex];
		Puff.Age += DeltaTime;
		float Opacity = 0;
		if (Puff.Age < Puff.Life)
		{
			FVector Next = Puff.Position + Puff.Velocity * Step;
			if (Puff.bAmbient)
			{
				Next = RoomBounds.GetClosestPointTo(Next);
			}
			else
			{
				FHitResult Hit;
				if (GetWorld()->LineTraceSingleByChannel(Hit, OwnerTransform.TransformPosition(Puff.Position), OwnerTransform.TransformPosition(Next), ECC_Visibility, Query))
				{
					Next = Puff.Position;
					Puff.Velocity = FVector::ZeroVector;
					Puff.Life = FMath::Min(Puff.Life, Puff.Age + 0.18f);
				}
				Puff.Velocity *= FMath::Exp(-Step * 0.55f);
			}
			Puff.Position = Next;
			const float Fraction = Puff.Age / Puff.Life;
			Opacity = Puff.Opacity * FMath::Min(1.0f, Puff.Age / 0.25f) * (1 - Fraction);
			const FVector WorldPosition = OwnerTransform.TransformPosition(Puff.Position);
			const FQuat Facing = FRotationMatrix::MakeFromZ(CameraPosition - WorldPosition).ToQuat();
			const float Size = Puff.Size * (1 + Fraction * (Puff.bAmbient ? 0.2f : 2.0f)) * OwnerTransform.GetScale3D().X / 100.0f;
			PuffTransforms[PuffIndex] = FTransform(Facing, WorldPosition, FVector(Size));
			++ActivePuffs;
		}
		else
		{
			PuffTransforms[PuffIndex] = FTransform(FQuat::Identity, OwnerTransform.GetLocation(), FVector::ZeroVector);
		}
		FogMesh->SetCustomDataValue(PuffIndex, 0, Opacity);
	}
	FogMesh->BatchUpdateInstancesTransforms(0, PuffTransforms, true, true, true);
}

FString UFogChamberComponent::GetStateLabel() const
{
	if (!bRoomReady)
	{
		return TEXT("Chamber disabled");
	}
	switch (State)
	{
	case EFogChamberState::Seeping: return FString::Printf(TEXT("Seeping | wall %.0fcm"), RemainingWall);
	case EFogChamberState::Venting: return FString::Printf(TEXT("BREACH | pressure %.0f%%"), Pressure * 100);
	case EFogChamberState::Open: return TEXT("Chamber open | residual mist");
	default: return TEXT("Chamber sealed");
	}
}
