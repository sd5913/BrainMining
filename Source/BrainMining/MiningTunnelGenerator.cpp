#include "MiningTunnelGenerator.h"
#include "GeneratedMiningRock.h"
#include "MiningLayoutRoom.h"
#include "MiningLayoutSpace.h"
#include "MiningSpaceGeometry.h"
#include "MiningHollowGeometry.h"
#include "MiningHollowChamber.h"
#include "MiningHollowBranches.h"
#include "MiningTunnelRoute.h"
#include "MiningTunnelShapeSection.h"
#include "MiningRoutePlanner.h"
#include "DrawDebugHelpers.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SplineComponent.h"
#include "Components/TextRenderComponent.h"
#include "MiningComponent.h"
#include "BrainMiningCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMeshActor.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"
#include "Operations/MeshBoolean.h"
#include "PhysicsEngine/BodySetup.h"
#include "UDynamicMesh.h"
#if WITH_EDITOR
#include "Editor.h"
#include "ScopedTransaction.h"
#endif

using namespace UE::Geometry;

namespace
{
	FDynamicMesh3 MakeBox(const FVector& Center, const FVector& Size, const FQuat& Rotation = FQuat::Identity)
	{
		FMinimalBoxMeshGenerator Generator;
		Generator.Box = FOrientedBox3d(FVector3d::Zero(), FVector3d(Size * 0.5));
		Generator.Generate();
		FDynamicMesh3 Mesh(&Generator);
		for (int32 VertexId : Mesh.VertexIndicesItr())
		{
			Mesh.SetVertex(VertexId, FVector3d(Rotation.RotateVector(FVector(Mesh.GetVertex(VertexId))) + Center));
		}
		return Mesh;
	}

	bool Combine(FDynamicMesh3& Target, const FDynamicMesh3& Tool, FMeshBoolean::EBooleanOp Mode)
	{
		if (Tool.TriangleCount() == 0)
		{
			return true;
		}
		if (Target.TriangleCount() == 0)
		{
			if (Mode == FMeshBoolean::EBooleanOp::Union)
			{
				Target = Tool;
			}
			return true;
		}
		const double Tolerances[] = {0.000001, 0.00001, 0.0001, 0.001};
		for (int32 Attempt = 0; Attempt < 8; ++Attempt)
		{
			FDynamicMesh3 First = Target;
			FDynamicMesh3 Second = Tool;
			First.DiscardAttributes();
			Second.DiscardAttributes();
			if (!First.HasTriangleGroups()) First.EnableTriangleGroups(0);
			if (!Second.HasTriangleGroups()) Second.EnableTriangleGroups(0);
			FDynamicMesh3 Result;
			FMeshBoolean Operation(&First, &Second, &Result, Mode);
			Operation.bSimplifyAlongNewEdges = Attempt % 2 == 0;
			Operation.bPreserveTriangleGroups = false;
			Operation.SnapTolerance = Tolerances[Attempt / 2];
			if (Operation.Compute() && Operation.CreatedBoundaryEdges.Num() == 0 &&
				(Result.TriangleCount() == 0 || Result.IsClosed()) &&
				Result.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly))
			{
				Target = MoveTemp(Result);
				return true;
			}
		}
		return false;
	}

	FDynamicMesh3 MakeRouteMesh(const MiningRoutes::FPlan& Plan, const MiningRoutes::FSettings& Settings, double Thickness = 0)
	{
		FDynamicMesh3 Mesh;
		TArray<FVector> Centers;
		TArray<FVector> Sides;
		const double Radius = Settings.Width * 0.75;
		const double HalfWidth = Settings.Width * 0.5 + Thickness;
		const double VerticalThickness = Thickness / FMath::Cos(FMath::DegreesToRadians(Settings.MaxSlope));
		for (int32 Index = 0; Index < Plan.Points.Num(); ++Index)
		{
			const FVector Point = Plan.Points[Index];
			const FVector Incoming = (Index > 0 ? Point - Plan.Points[Index - 1] : Plan.Points[1] - Point).GetSafeNormal2D();
			const FVector Outgoing = (Index + 1 < Plan.Points.Num() ? Plan.Points[Index + 1] - Point : Incoming).GetSafeNormal2D();
			const FVector IncomingSide(-Incoming.Y, Incoming.X, 0);
			const FVector OutgoingSide(-Outgoing.Y, Outgoing.X, 0);
			if (Index == 0 || Index == Plan.Points.Num() - 1)
			{
				Centers.Add(Point + Incoming * (Index == 0 ? -2.0 - Thickness : 2.0 + Thickness));
				Sides.Add(IncomingSide * HalfWidth);
			}
			else
			{
				Centers.Add(Point - Incoming * Radius);
				Sides.Add(IncomingSide * HalfWidth);
				const FVector Bisector = (IncomingSide + OutgoingSide).GetSafeNormal();
				Centers.Add(Point);
				Sides.Add(Bisector * (HalfWidth / FMath::Max(0.25, Bisector.Dot(IncomingSide))));
				Centers.Add(Point + Outgoing * Radius);
				Sides.Add(OutgoingSide * HalfWidth);
			}
		}
		for (int32 Ring = 0; Ring < Centers.Num(); ++Ring)
		{
			Mesh.AppendVertex(FVector3d(Centers[Ring] - Sides[Ring] - FVector(0, 0, VerticalThickness)));
			Mesh.AppendVertex(FVector3d(Centers[Ring] + Sides[Ring] - FVector(0, 0, VerticalThickness)));
			Mesh.AppendVertex(FVector3d(Centers[Ring] + Sides[Ring] + FVector(0, 0, Settings.Height + VerticalThickness)));
			Mesh.AppendVertex(FVector3d(Centers[Ring] - Sides[Ring] + FVector(0, 0, Settings.Height + VerticalThickness)));
			if (Ring == 0) continue;
			for (int32 Edge = 0; Edge < 4; ++Edge)
			{
				const int32 Previous = (Ring - 1) * 4;
				const int32 Current = Ring * 4;
				const int32 Next = (Edge + 1) % 4;
				Mesh.AppendTriangle(Previous + Edge, Previous + Next, Current + Next);
				Mesh.AppendTriangle(Previous + Edge, Current + Next, Current + Edge);
			}
		}
		Mesh.AppendTriangle(0, 2, 1);
		Mesh.AppendTriangle(0, 3, 2);
		const int32 Last = (Centers.Num() - 1) * 4;
		Mesh.AppendTriangle(Last, Last + 1, Last + 2);
		Mesh.AppendTriangle(Last, Last + 2, Last + 3);
		Mesh.ReverseOrientation();
		return Mesh;
	}

	bool MakeDirectMesh(const MiningRoutes::FPlan& Plan, double Thickness, FDynamicMesh3& Mesh, double CapOverlap = 2)
	{
		if (Plan.FaceSections.Num() > 1)
		{
			const int32 Axis = Plan.StartWall / 2;
			const int32 SideAxis = Axis == 0 ? 1 : 0;
			const int32 UpAxis = Axis == 2 ? 1 : 2;
			const FVector Direction = MiningRoutes::WallDirection(Plan.StartWall);
			TArray<TArray<FVector>> Rings;
			Rings.Add(TArray<FVector>(Plan.FaceSections[0].GetData(), 4));
			for (const TArray<FVector>& Corners : Plan.FaceSections) Rings.Add(TArray<FVector>(Corners.GetData() + 4, 4));
			for (int32 Ring = 0; Ring < Rings.Num(); ++Ring)
			{
				for (int32 Corner = 0; Corner < 4; ++Corner)
				{
					FVector Point = Rings[Ring][Corner];
					for (const int32 Dimension : {SideAxis, UpAxis})
					{
						double Expansion = Thickness;
						for (const int32 Neighbor : {Ring - 1, Ring + 1})
						{
							if (!Rings.IsValidIndex(Neighbor)) continue;
							const FVector Delta = Rings[Neighbor][Corner] - Rings[Ring][Corner];
							const double Slope = Delta[Dimension] / Delta[Axis];
							Expansion = FMath::Max(Expansion, Thickness * FMath::Sqrt(1 + Slope * Slope));
						}
						const double Sign = Dimension == SideAxis ? (Corner == 0 || Corner == 3 ? -1 : 1) : (Corner < 2 ? -1 : 1);
						Point[Dimension] += Expansion * Sign;
					}
					if (Ring == 0) Point -= Direction * (Thickness + CapOverlap);
					if (Ring == Rings.Num() - 1) Point += Direction * (Thickness + CapOverlap);
					Mesh.AppendVertex(FVector3d(Point));
				}
			}
			const auto Face = [&Mesh](int32 First, int32 Second, int32 Third, int32 Fourth, const FVector3d& Inside)
			{
				const FVector3d Origin = Mesh.GetVertex(First);
				const FVector3d Normal = (Mesh.GetVertex(Second) - Origin).Cross(Mesh.GetVertex(Third) - Origin);
				if (Normal.Dot(Inside - Origin) < 0)
				{
					Mesh.AppendTriangle(First, Third, Second);
					Mesh.AppendTriangle(First, Fourth, Third);
				}
				else
				{
					Mesh.AppendTriangle(First, Second, Third);
					Mesh.AppendTriangle(First, Third, Fourth);
				}
			};
			for (int32 Ring = 1; Ring < Rings.Num(); ++Ring)
			{
				const int32 Previous = (Ring - 1) * 4;
				const int32 Current = Ring * 4;
				const FVector3d Inside = (Mesh.GetVertex(Previous) + Mesh.GetVertex(Previous + 2) + Mesh.GetVertex(Current) + Mesh.GetVertex(Current + 2)) * 0.25;
				for (int32 Edge = 0; Edge < 4; ++Edge) Face(Previous + Edge, Previous + (Edge + 1) % 4, Current + (Edge + 1) % 4, Current + Edge, Inside);
				if (Ring == 1) Face(0, 3, 2, 1, Inside);
				if (Ring == Rings.Num() - 1) Face(Current, Current + 1, Current + 2, Current + 3, Inside);
			}
			return Mesh.IsClosed() && Mesh.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly);
		}
		if (Plan.FaceCorners.Num() != 8) return false;
		static const int32 Faces[6][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}};
		FVector Center = FVector::ZeroVector;
		for (const FVector& Corner : Plan.FaceCorners) Center += Corner / 8.0;
		TArray<FVector> Normals;
		TArray<double> Distances;
		for (int32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
		{
			const FVector Origin = Plan.FaceCorners[Faces[FaceIndex][0]];
			FVector Normal = FVector::CrossProduct(Plan.FaceCorners[Faces[FaceIndex][1]] - Origin, Plan.FaceCorners[Faces[FaceIndex][2]] - Origin).GetSafeNormal();
			if (Normal.IsNearlyZero()) return false;
			if (Normal.Dot(Center - Origin) > 0) Normal *= -1;
			Normals.Add(Normal);
			Distances.Add(Normal.Dot(Origin) + Thickness + (FaceIndex < 2 ? CapOverlap : 0.0));
		}
		for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
		{
			TArray<int32> Adjacent;
			for (int32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
			{
				for (int32 FaceCorner : Faces[FaceIndex])
				{
					if (FaceCorner == CornerIndex) { Adjacent.Add(FaceIndex); break; }
				}
			}
			const FVector FirstNormal = Normals[Adjacent[0]];
			const FVector SecondNormal = Normals[Adjacent[1]];
			const FVector ThirdNormal = Normals[Adjacent[2]];
			const double Determinant = FirstNormal.Dot(FVector::CrossProduct(SecondNormal, ThirdNormal));
			if (FMath::Abs(Determinant) < 1.e-8) return false;
			const FVector Corner = (FVector::CrossProduct(SecondNormal, ThirdNormal) * Distances[Adjacent[0]] +
				FVector::CrossProduct(ThirdNormal, FirstNormal) * Distances[Adjacent[1]] +
				FVector::CrossProduct(FirstNormal, SecondNormal) * Distances[Adjacent[2]]) / Determinant;
			if (Corner.ContainsNaN()) return false;
			Mesh.AppendVertex(FVector3d(Corner));
		}
		for (int32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
		{
			const int32* Face = Faces[FaceIndex];
			const FVector FaceNormal = FVector::CrossProduct(Plan.FaceCorners[Face[1]] - Plan.FaceCorners[Face[0]], Plan.FaceCorners[Face[2]] - Plan.FaceCorners[Face[0]]);
			if (FaceNormal.Dot(Normals[FaceIndex]) > 0)
			{
				Mesh.AppendTriangle(Face[0], Face[2], Face[1]);
				Mesh.AppendTriangle(Face[0], Face[3], Face[2]);
			}
			else
			{
				Mesh.AppendTriangle(Face[0], Face[1], Face[2]);
				Mesh.AppendTriangle(Face[0], Face[2], Face[3]);
			}
		}
		return Mesh.IsClosed() && Mesh.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly);
	}

	void PrepareNormals(FDynamicMesh3& Mesh)
	{
		if (Mesh.TriangleCount() > 0)
		{
			Mesh.EnableAttributes();
			FMeshNormals::InitializeOverlayToPerTriangleNormals(Mesh.Attributes()->PrimaryNormals());
		}
	}
}

AMiningTunnelGenerator::AMiningTunnelGenerator()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("GeneratorRoot")));
#if WITH_EDITORONLY_DATA
	bLockLocation = true;
#endif
}

AGeneratedMiningRock* AMiningTunnelGenerator::GetActiveSegment() const
{
	return GeneratedSegments.IsValidIndex(ActiveSegmentIndex) ? GeneratedSegments[ActiveSegmentIndex].Get() : nullptr;
}

void AMiningTunnelGenerator::ResetProgression()
{
	for (ADynamicMeshActor* Seal : RuntimeSeals)
	{
		if (IsValid(Seal)) Seal->Destroy();
	}
	RuntimeSeals.Reset();
	RecycleHiddenSince.Reset();
	RoomEntryElapsed = 0;
	CurrentRoomIndex = 0;
	ActiveSegmentIndex = bGeneratedSegmented && !GeneratedSegments.IsEmpty() ? 0 : INDEX_NONE;
	for (int32 Index = 0; Index < GeneratedSegments.Num(); ++Index)
	{
		if (IsValid(GeneratedSegments[Index]))
		{
			GeneratedSegments[Index]->SetSegmentState(!bGeneratedSegmented || Index == ActiveSegmentIndex
				? EMiningSegmentState::Active : EMiningSegmentState::Waiting);
		}
	}
	ProgressionStatus = bGeneratedSegmented ? TEXT("One-way mining: enter the next room fully") : TEXT("Legacy unsegmented layout");
}

void AMiningTunnelGenerator::UpdateProgression(APawn* Player, float DeltaSeconds)
{
	if (!Player || !bGeneratedSegmented) return;
	const int32 NextRoom = CurrentRoomIndex + 1;
	if (GeneratedRoomBounds.IsValidIndex(NextRoom) && GeneratedArrivalWalls.IsValidIndex(CurrentRoomIndex))
	{
		const UCapsuleComponent* Capsule = Player->FindComponentByClass<UCapsuleComponent>();
		const FVector Center = (Capsule ? Capsule->GetComponentLocation() : Player->GetActorLocation()) - GetActorLocation();
		const FVector Extent = Capsule ? FVector(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()) : FVector(40, 40, 90);
		const FBox& Room = GeneratedRoomBounds[NextRoom];
		const FBox TolerantRoom = Room.ExpandBy(2);
		const FVector Outward = MiningRoutes::WallDirection(GeneratedArrivalWalls[CurrentRoomIndex]);
		const int32 Axis = GeneratedArrivalWalls[CurrentRoomIndex] / 2;
		const FVector Face = Room.GetCenter() + Outward * Room.GetExtent();
		const double Inset = FMath::Clamp<double>(RoomEntryInset, 0, FMath::Max(0.0, Room.GetSize()[Axis] * 0.5 - Extent[Axis] - 2));
		const bool bInside = TolerantRoom.IsInsideOrOn(Center - Extent) && TolerantRoom.IsInsideOrOn(Center + Extent) &&
			FVector::DotProduct(Face - Center, Outward) >= Extent[Axis] + Inset;
		RoomEntryElapsed = bInside ? RoomEntryElapsed + FMath::Max(0.f, DeltaSeconds) : 0;
		if (bInside && RoomEntryElapsed >= FMath::Clamp(RoomEntryHoldTime, 0.f, 2.f))
		{
			if (AGeneratedMiningRock* Completed = GetActiveSegment()) Completed->SetSegmentState(EMiningSegmentState::Frozen);
			CurrentRoomIndex = NextRoom;
			ActiveSegmentIndex = GeneratedSegments.IsValidIndex(CurrentRoomIndex) ? CurrentRoomIndex : INDEX_NONE;
			if (AGeneratedMiningRock* Active = GetActiveSegment()) Active->SetSegmentState(EMiningSegmentState::Active);
			RoomEntryElapsed = 0;
			ProgressionStatus = ActiveSegmentIndex == INDEX_NONE ? TEXT("Final room reached; completed tunnels frozen")
				: FString::Printf(TEXT("Room %d reached; segment %d / %d active"), CurrentRoomIndex, ActiveSegmentIndex + 1, GeneratedSegments.Num());
			if (UMiningComponent* Mining = Player->FindComponentByClass<UMiningComponent>())
			{
				Mining->LastMinedRock = nullptr;
				Mining->TargetRock = nullptr;
				Mining->Status = ProgressionStatus;
			}
		}
	}
	if (CompletedSegmentPolicy == ECompletedMiningSegments::SealAndRecycle)
	{
		const int32 RecycleBefore = CurrentRoomIndex - FMath::Clamp(FrozenSegmentsToKeep, 1, 15);
		for (int32 Index = 0; Index < RecycleBefore; ++Index)
		{
			if (TryRecycleSegment(Index, Player)) break;
		}
	}
	else RecycleHiddenSince.Reset();
}

bool AMiningTunnelGenerator::TryRecycleSegment(int32 Index, APawn* Player)
{
	bool bSafeToRecycle = false;
	ON_SCOPE_EXIT { if (!bSafeToRecycle) RecycleHiddenSince.Remove(Index); };
	if (!GeneratedSegments.IsValidIndex(Index) || !IsValid(GeneratedSegments[Index]) ||
		GeneratedSegments[Index]->SegmentState != EMiningSegmentState::Frozen ||
		!GeneratedRoomBounds.IsValidIndex(Index + 1) || !GeneratedArrivalWalls.IsValidIndex(Index) ||
		!GeneratedSegmentBounds.IsValidIndex(Index) || !IsValid(GeneratedBedrock)) return false;
	if (BedrockMaterial && BedrockMaterial->GetBlendMode() != BLEND_Opaque) return false;
	const FBox& OldBounds = GeneratedSegmentBounds[Index];
	for (int32 Later = Index + 1; Later < GeneratedSegmentBounds.Num(); ++Later)
	{
		if (OldBounds.ExpandBy(2).Intersect(GeneratedSegmentBounds[Later])) return false;
	}
	for (int32 Later = Index + 2; Later < GeneratedRoomBounds.Num(); ++Later)
	{
		if (OldBounds.ExpandBy(2).Intersect(GeneratedRoomBounds[Later])) return false;
	}
	const FBox& Room = GeneratedRoomBounds[Index + 1];
	const int32 Wall = GeneratedArrivalWalls[Index];
	const FVector Outward = MiningRoutes::WallDirection(Wall);
	FVector Size = Room.GetSize() + FVector(2);
	Size[Wall / 2] = 20;
	const FVector Center = Room.GetCenter() + Outward * (Room.GetExtent() + FVector(10));
	const FBox SealBounds(Center - Size * 0.5, Center + Size * 0.5);
	const FBox PlayerBounds = Player->GetComponentsBoundingBox(true).ShiftBy(-GetActorLocation());
	if (SealBounds.ExpandBy(200).Intersect(PlayerBounds) || OldBounds.Intersect(PlayerBounds)) return false;
	if (const APlayerController* Controller = Cast<APlayerController>(Player->GetController()))
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		const FVector ToSeal = Center + GetActorLocation() - ViewLocation;
		if (FVector::DotProduct(ToSeal, ViewRotation.Vector()) + Size.Size() * 0.5 >= -100) return false;
	}
	else return false;
	bSafeToRecycle = true;
	const double Now = GetWorld()->GetTimeSeconds();
	const double* Since = RecycleHiddenSince.Find(Index);
	if (!Since)
	{
		RecycleHiddenSince.Add(Index, Now);
		return false;
	}
	if (Now - *Since < FMath::Clamp(RecycleHiddenTime, 0.5f, 5.f)) return false;
	FActorSpawnParameters Spawn;
	Spawn.Owner = this;
	Spawn.OverrideLevel = GetLevel();
	Spawn.ObjectFlags |= RF_Transient;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADynamicMeshActor* Seal = GetWorld()->SpawnActor<ADynamicMeshActor>(GetActorLocation(), FRotator::ZeroRotator, Spawn);
	if (!Seal) return false;
	UDynamicMeshComponent* Mesh = Seal->GetDynamicMeshComponent();
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetComplexAsSimpleCollisionEnabled(true, false);
	Mesh->SetDeferredCollisionUpdatesEnabled(true, false);
	Mesh->bUseAsyncCooking = false;
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	FDynamicMesh3 Geometry = MakeBox(Center, Size);
	PrepareNormals(Geometry);
	Mesh->SetMesh(MoveTemp(Geometry));
	Mesh->GetBodySetup()->SetFlags(RF_Transient);
	Mesh->UpdateCollision(false);
	Mesh->SetMaterial(0, BedrockMaterial);
	RuntimeSeals.Add(Seal);
	GeneratedSegments[Index]->RecycleGeometry();
	return true;
}

TArray<AMiningLayoutRoom*> AMiningTunnelGenerator::GetOrderedRooms() const
{
	TArray<AMiningLayoutRoom*> Rooms;
	if (GetWorld())
	{
		for (TActorIterator<AMiningLayoutRoom> Room(GetWorld()); Room; ++Room)
		{
			if (Room->GetLevel() == GetLevel() && Room->LayoutId == LayoutId)
			{
				Rooms.Add(*Room);
			}
		}
	}
	Rooms.Sort([](const AMiningLayoutRoom& First, const AMiningLayoutRoom& Second)
	{
		if (First.bStartRoom != Second.bStartRoom) return First.bStartRoom;
		return First.ConnectionOrder == Second.ConnectionOrder ? First.GetPathName() < Second.GetPathName() : First.ConnectionOrder < Second.ConnectionOrder;
	});
	return Rooms;
}

uint32 AMiningTunnelGenerator::GetInputHash() const
{
	uint32 Hash = HashCombineFast(GetTypeHash(LayoutId), GetTypeHash(TunnelWidth));
	Hash = HashCombineFast(Hash, GetTypeHash(TunnelHeight));
	for (const AMiningLayoutSpace* Space : GetLayoutSpaces()) Hash = HashCombineFast(Hash, Space->GetGeometryHash());
	Hash = HashCombineFast(Hash, GetTypeHash(BedrockThickness));
	Hash = HashCombineFast(Hash, GetTypeHash(bDirectRoomFaces));
	Hash = HashCombineFast(Hash, GetTypeHash(bHollowRegionPrototype));
	Hash = HashCombineFast(Hash, GetTypeHash(HollowRegion.Get()));
	if (bHollowRegionPrototype && IsValid(HollowRegion)) Hash = HashCombineFast(Hash, HollowRegion->GetGeometryHash());
	Hash = HashCombineFast(Hash, GetTypeHash(SegmentTriangleBudget));
	Hash = HashCombineFast(Hash, GetTypeHash(bAutomaticChunks));
	Hash = HashCombineFast(Hash, GetTypeHash(ChunkLength));
	Hash = HashCombineFast(Hash, GetTypeHash(bSimplifyChunks));
	Hash = HashCombineFast(Hash, GetTypeHash(SimplificationTolerance));
	Hash = HashCombineFast(Hash, GetTypeHash(MaxSlopeDegrees));
	Hash = HashCombineFast(Hash, GetTypeHash(SearchDistance));
	Hash = HashCombineFast(Hash, GetTypeHash(DoorMargin));
	Hash = HashCombineFast(Hash, GetTypeHash(bRequireDescending));
	Hash = HashCombineFast(Hash, GetTypeHash(GetActorTransform().ToString()));
	Hash = HashCombineFast(Hash, GetTypeHash(BedrockMaterial.Get()));
	Hash = HashCombineFast(Hash, GetTypeHash(TunnelMaterial.Get()));
	for (const AMiningLayoutRoom* Room : GetOrderedRooms())
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Room));
		Hash = HashCombineFast(Hash, GetTypeHash(Room->ConnectionOrder));
		Hash = HashCombineFast(Hash, GetTypeHash(Room->bStartRoom));
		Hash = HashCombineFast(Hash, GetTypeHash(Room->bFloorAnchored));
		Hash = HashCombineFast(Hash, GetTypeHash(Room->RoomSize));
		Hash = HashCombineFast(Hash, GetTypeHash(Room->GetActorTransform().ToString()));
	}
	for (const AMiningTunnelRoute* Route : Routes)
	{
		if (!IsValid(Route)) continue;
		for (const AMiningTunnelShapeSection* Section : Route->GetShapeSections()) Hash = HashCombineFast(Hash, Section->GetGeometryHash());
		Hash = HashCombineFast(Hash, GetTypeHash(Route->bAutomatic));
		Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Route->StartWall)));
		Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Route->EndWall)));
		if (!Route->bAutomatic)
		{
			for (const FVector& Point : Route->GetPoints()) Hash = HashCombineFast(Hash, GetTypeHash(Point));
		}
	}
	return Hash;
}

void AMiningTunnelGenerator::RequestRebuild()
{
	bPendingRebuild = true;
	bLayoutApplied = false;
	LastInputChangeTime = FPlatformTime::Seconds();
}

TArray<AMiningLayoutSpace*> AMiningTunnelGenerator::GetLayoutSpaces() const
{
	TArray<AMiningLayoutSpace*> Spaces;
	if (GetWorld()) for (TActorIterator<AMiningLayoutSpace> Space(GetWorld()); Space; ++Space)
	{
		if (Space->GetLevel() == GetLevel() && Space->LayoutId == LayoutId) Spaces.Add(*Space);
	}
	Spaces.Sort([](const AMiningLayoutSpace& First, const AMiningLayoutSpace& Second) { return First.GetPathName() < Second.GetPathName(); });
	return Spaces;
}

AMiningLayoutSpace* AMiningTunnelGenerator::AddMiningSpace()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld()) return nullptr;
	const TArray<AMiningLayoutRoom*> Rooms = GetOrderedRooms();
	const int32 Index = NewSpaceFromRoom ? Rooms.IndexOfByKey(NewSpaceFromRoom.Get()) : 0;
	if (Index < 0 || !Rooms.IsValidIndex(Index + 1)) { LastBuildStatus = TEXT("Choose a start room with a following room before adding a space"); return nullptr; }
	const FScopedTransaction Transaction(NSLOCTEXT("Mining", "AddSpace", "Add Mining Space"));
	FActorSpawnParameters Spawn;
	Spawn.OverrideLevel = GetLevel();
	Spawn.ObjectFlags |= RF_Transactional;
	const FVector Center = (Rooms[Index]->GetRoomBounds().GetCenter() + Rooms[Index + 1]->GetRoomBounds().GetCenter()) * 0.5;
	AMiningLayoutSpace* Space = GetWorld()->SpawnActor<AMiningLayoutSpace>(Center - FVector(0, 0, 350), FRotator::ZeroRotator, Spawn);
	if (!Space) return nullptr;
	Space->Modify();
	Space->FromRoom = Rooms[Index];
	Space->LayoutId = LayoutId;
	Space->SetActorLabel(TEXT("SPACE_Cave_EDIT_ME"));
	Space->SetFolderPath(FName(*(TEXT("AutoMine/") + LayoutId.ToString() + TEXT("/Spaces_EDIT"))));
	Space->OnConstruction(Space->GetActorTransform());
	GEditor->SelectNone(false, true);
	GEditor->SelectActor(Space, true, true);
	RequestRebuild();
	return Space;
#else
	return nullptr;
#endif
}

void AMiningTunnelGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!bLayoutApplied || GetInputHash() != ObservedInputHash)
	{
		RequestRebuild();
	}
	RefreshEditorVisibility();
}

void AMiningTunnelGenerator::RefreshEditorVisibility()
{
#if WITH_EDITOR
	if (IsValid(GeneratedBedrock))
	{
		GeneratedBedrock->SetIsTemporarilyHiddenInEditor(PreviewMode != EMiningPreviewMode::Game && !bShowBedrockInEditor);
	}
	if (IsValid(GeneratedFill)) GeneratedFill->SetIsTemporarilyHiddenInEditor(PreviewMode != EMiningPreviewMode::Game);
	for (AGeneratedMiningRock* Segment : GeneratedSegments)
	{
		if (IsValid(Segment)) Segment->SetIsTemporarilyHiddenInEditor(PreviewMode != EMiningPreviewMode::Game);
	}
	if (IsValid(PreviewBedrock)) PreviewBedrock->SetIsTemporarilyHiddenInEditor(PreviewMode != EMiningPreviewMode::Section);
	if (IsValid(PreviewFill)) PreviewFill->SetIsTemporarilyHiddenInEditor(PreviewMode == EMiningPreviewMode::Game);
	for (AMiningLayoutSpace* Space : GetLayoutSpaces()) Space->SetGuideVisibility(PreviewMode != EMiningPreviewMode::Game);
	for (const AMiningLayoutRoom* Room : GetOrderedRooms())
	{
		Room->Bounds->SetVisibility(PreviewMode != EMiningPreviewMode::Game);
		Room->Label->SetVisibility(PreviewMode != EMiningPreviewMode::Game);
	}
	for (const AMiningTunnelRoute* Route : Routes)
	{
		if (!IsValid(Route)) continue;
		Route->StatusLabel->SetVisibility(PreviewMode != EMiningPreviewMode::Game);
		Route->Path->SetDrawDebug(PreviewMode != EMiningPreviewMode::Game);
		for (AMiningTunnelShapeSection* Section : Route->GetShapeSections()) Section->SetGuideVisibility(PreviewMode != EMiningPreviewMode::Game);
	}
#endif
}

void AMiningTunnelGenerator::BeginPlay()
{
	Super::BeginPlay();
	RemoveStaleSegments();
	if (IsValid(GeneratedBedrock))
	{
		GeneratedBedrock->SetActorHiddenInGame(false);
	}
	ResetProgression();
}

void AMiningTunnelGenerator::RemoveStaleSegments()
{
	for (TActorIterator<AGeneratedMiningRock> Segment(GetWorld()); Segment; ++Segment)
	{
		if (Segment->GetOwner() == this && *Segment != GeneratedFill && !GeneratedSegments.Contains(*Segment))
		{
			Segment->Destroy();
		}
	}
}

void AMiningTunnelGenerator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetWorld()->IsGameWorld())
	{
		if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			if (!bRuntimeConfigured && bImproveMiningVisibility)
			{
				if (ABrainMiningCharacter* Character = Cast<ABrainMiningCharacter>(Player)) Character->ConfigureMiningLighting(HeadlampIntensity, FillLightIntensity);
			}
			if (UMiningComponent* Mining = Player->FindComponentByClass<UMiningComponent>())
			{
				if (Mining->FallResetDistance > 0)
				{
					Mining->FallResetDistance = FMath::Max(Mining->FallResetDistance, RecommendedFallResetDistance);
				}
			}
			bRuntimeConfigured = true;
			UpdateProgression(Player, DeltaSeconds);
		}
		return;
	}
	RefreshEditorVisibility();
	DrawLayoutGuides();
	uint32 PreviewHash = HashCombineFast(GetTypeHash(static_cast<uint8>(PreviewMode)), GetTypeHash(SectionPosition));
	PreviewHash = HashCombineFast(PreviewHash, GetTypeHash(SectionAxis));
	PreviewHash = HashCombineFast(PreviewHash, GetTypeHash(bSectionKeepPositive));
	PreviewHash = HashCombineFast(PreviewHash, GetTypeHash(PreviewBedrockMaterial.Get()));
	PreviewHash = HashCombineFast(PreviewHash, GetTypeHash(PreviewFillMaterial.Get()));
	if (PreviewHash != ObservedPreviewHash)
	{
		ObservedPreviewHash = PreviewHash;
		UpdatePreview();
	}
	const uint32 InputHash = GetInputHash();
	if (InputHash != ObservedInputHash)
	{
		ObservedInputHash = InputHash;
		RequestRebuild();
	}
	const bool bEditingSpace = GetLayoutSpaces().ContainsByPredicate([](const AMiningLayoutSpace* Space) { return Space->IsInteractiveEdit(); });
	const bool bEditingShape = Routes.ContainsByPredicate([](const AMiningTunnelRoute* Route)
	{
		return IsValid(Route) && Route->GetShapeSections().ContainsByPredicate([](const AMiningTunnelShapeSection* Section) { return Section->IsInteractiveEdit(); });
	});
	bool bEditingRegion = bHollowRegionPrototype && IsValid(HollowRegion) && HollowRegion->IsInteractiveEdit();
	if (bHollowRegionPrototype && IsValid(HollowRegion))
		bEditingRegion |= HollowRegion->GetChambers().ContainsByPredicate([](const AMiningHollowChamber* Chamber) { return Chamber->IsInteractiveEdit(); });
#if WITH_EDITOR
	bEditingRegion |= bHollowRegionPrototype && GEditor && GEditor->IsTransactionActive();
#endif
	if (bAutoRebuild && bPendingRebuild && !bEditingSpace && !bEditingShape && !bEditingRegion && FPlatformTime::Seconds() - LastInputChangeTime > 0.3)
	{
		RebuildTunnels();
	}
}

bool AMiningTunnelGenerator::RebuildTunnels()
{
	if (bBuilding || !GetWorld() || GetWorld()->IsGameWorld())
	{
		return false;
	}
	TGuardValue<bool> Guard(bBuilding, true);
	const double Started = FPlatformTime::Seconds();
	ON_SCOPE_EXIT
	{
		LastBuildMilliseconds = (FPlatformTime::Seconds() - Started) * 1000.0;
		ObservedInputHash = GetInputHash();
		bPendingRebuild = false;
		if (!bLayoutApplied && !LastBuildStatus.StartsWith(TEXT("NOT APPLIED"))) LastBuildStatus = TEXT("NOT APPLIED: ") + LastBuildStatus;
		if (bHollowRegionPrototype && IsValid(HollowRegion)) HollowRegion->RegionStatus = LastBuildStatus;
		if (bHollowRegionPrototype && IsValid(HollowRegion))
			for (AMiningHollowChamber* Chamber : HollowRegion->GetChambers())
				Chamber->ChamberStatus = !Chamber->bEnabled ? TEXT("Disabled; edits retained") : bLayoutApplied ? TEXT("Ready; same region / no progression trigger; move floor center or edit size") : LastBuildStatus;
	};
	bPendingRebuild = false;
	bLayoutApplied = false;
	LayoutWarnings.Empty();
	ObservedInputHash = GetInputHash();
	const TArray<AMiningLayoutRoom*> Rooms = GetOrderedRooms();
	for (const AMiningTunnelRoute* Route : Routes)
	{
		if (!IsValid(Route)) continue;
		const int32 First = Rooms.IndexOfByKey(Route->FromRoom.Get());
		const bool bHasShapes = Route->GetShapeSections().ContainsByPredicate([](const AMiningTunnelShapeSection* Section) { return Section->bEnabled; });
		if (bHasShapes && (bHollowRegionPrototype || !bDirectRoomFaces || !Rooms.IsValidIndex(First + 1) || First == INDEX_NONE || Rooms[First + 1] != Route->ToRoom))
		{
			LastBuildStatus = TEXT("NOT APPLIED: an edited tunnel no longer connects consecutive rooms. Disable or reassign its shape sections first; previous geometry retained");
			return false;
		}
	}
	if (Rooms.IsEmpty() || Rooms.Num() > 16)
	{
		LastBuildStatus = TEXT("Need 1-16 rooms in this layout; previous geometry preserved");
		return false;
	}
	if (!GetActorRotation().IsNearlyZero() || !GetActorScale3D().Equals(FVector::OneVector))
	{
		LastBuildStatus = TEXT("Keep generator rotation 0 and scale 1; edit the room actors instead");
		return false;
	}
	TArray<FBox> Boxes;
	TSet<int32> Orders;
	int32 StartCount = 0;
	for (AMiningLayoutRoom* Room : Rooms)
	{
		if (Orders.Contains(Room->ConnectionOrder) || !Room->GetActorRotation().IsNearlyZero() ||
			Room->RoomSize.ContainsNaN() || Room->GetActorLocation().ContainsNaN() || Room->GetActorScale3D().ContainsNaN())
		{
			LastBuildStatus = TEXT("Each room needs a unique Connection Order, finite dimensions and zero rotation");
			return false;
		}
		Room->UpgradeFloorAnchor();
		StartCount += Room->bStartRoom ? 1 : 0;
		const FBox Box = Room->GetRoomBounds().ShiftBy(-GetActorLocation());
		if (Box.GetSize().GetMin() < 250)
		{
			LastBuildStatus = TEXT("Room interiors must be at least 250 cm on every axis");
			return false;
		}
		Orders.Add(Room->ConnectionOrder);
		Boxes.Add(Box);
	}
	if (StartCount > 1) { LastBuildStatus = TEXT("NOT APPLIED: multiple start rooms"); return false; }
	if (StartCount == 0) { Rooms[0]->bStartRoom = true; Rooms[0]->OnConstruction(Rooms[0]->GetActorTransform()); }
	for (int32 First = 0; First < Boxes.Num(); ++First)
	{
		for (int32 Second = First + 1; Second < Boxes.Num(); ++Second)
		{
			if (Boxes[First].ExpandBy(10).Intersect(Boxes[Second])) { LastBuildStatus = TEXT("NOT APPLIED: rooms overlap; separate their interiors"); return false; }
		}
	}
	if (!FMath::IsFinite(BedrockThickness) || (!bDirectRoomFaces && (!FMath::IsFinite(TunnelWidth) || !FMath::IsFinite(TunnelHeight))))
	{
		LastBuildStatus = TEXT("Tunnel dimensions must be finite");
		return false;
	}
	if (!bDirectRoomFaces && (!FMath::IsFinite(MaxSlopeDegrees) || MaxSlopeDegrees < 5 || MaxSlopeDegrees > 35 ||
		!FMath::IsFinite(SearchDistance) || SearchDistance < 1000 || SearchDistance > 50000 ||
		!FMath::IsFinite(DoorMargin) || DoorMargin < 30 || DoorMargin > 200))
	{
		LastBuildStatus = TEXT("NOT APPLIED: routing settings outside supported ranges"); return false;
	}
	const double Thickness = FMath::Clamp<double>(BedrockThickness, 30, 500);
	if (bAutomaticChunks && (!FMath::IsFinite(ChunkLength) || ChunkLength < 400 || ChunkLength > 3000 ||
		!FMath::IsFinite(SimplificationTolerance) || SimplificationTolerance < 0.05f || SimplificationTolerance > 1))
	{
		LastBuildStatus = TEXT("NOT APPLIED: invalid chunk settings; previous geometry preserved");
		return false;
	}
	FDynamicMesh3 Shell;
	FDynamicMesh3 Fill;
	TArray<FDynamicMesh3> SegmentMeshes;
	TArray<FDynamicMesh3> Cavities;
	for (const FBox& Box : Boxes)
	{
		if (!Combine(Shell, MakeBox(Box.GetCenter(), Box.GetSize() + FVector(Thickness * 2)), FMeshBoolean::EBooleanOp::Union))
		{
			LastBuildStatus = TEXT("Room shell union failed; previous geometry preserved");
			return false;
		}
		Cavities.Add(MakeBox(Box.GetCenter(), Box.GetSize()));
	}
	int32 NewTunnelCount = 0;
	TArray<FBox> Reserved;
	TArray<TObjectPtr<AMiningTunnelRoute>> ActiveRoutes;
	TArray<MiningRoutes::FPlan> Plans;
	MiningHollow::FPlan HollowPlan;
	TArray<MiningHollowBranches::FPlan> HollowBranches;
	TArray<AMiningHollowChamber*> ActiveChambers;
	if (bHollowRegionPrototype)
	{
		if (!bDirectRoomFaces || !bAutomaticChunks || Rooms.Num() != 2 || !IsValid(HollowRegion) || HollowRegion->GetLevel() != GetLevel() ||
			HollowRegion->FromRoom != Rooms[0] || HollowRegion->ToRoom != Rooms[1] ||
			!HollowRegion->GetActorRotation().IsNearlyZero() || !HollowRegion->GetActorScale3D().Equals(FVector::OneVector))
		{
			LastBuildStatus = TEXT("Hollow prototype needs exactly two ordered rooms, matching region references, direct mode, automatic chunks, and region rotation 0 / scale 1");
			return false;
		}
		MiningHollow::FSettings Settings;
		Settings.FirstRoom = Boxes[0]; Settings.LastRoom = Boxes[1];
		Settings.Points = HollowRegion->GetPathPoints();
		for (FVector& Point : Settings.Points) Point -= GetActorLocation();
		Settings.Plugs = HollowRegion->Plugs;
		Settings.Radius = HollowRegion->PassageRadius;
		Settings.Width = HollowRegion->RegionWidth;
		Settings.Above = HollowRegion->RockAbove;
		Settings.Below = HollowRegion->RockBelow;
		Settings.MaxSlope = HollowRegion->MaximumSlope;
		FDynamicMesh3 RegionRock;
		if (!MiningHollow::Plan(Settings, HollowPlan, LastBuildStatus))
		{
			HollowRegion->RegionStatus = TEXT("NOT APPLIED: ") + LastBuildStatus;
			return false;
		}
		TArray<MiningHollowBranches::FSettings> BranchSettings;
		for (AMiningHollowChamber* Chamber : HollowRegion->GetChambers())
		{
			if (!Chamber->bEnabled) continue;
			if (!Chamber->GetActorRotation().IsNearlyZero() || Chamber->GetActorScale3D().GetMin() <= 0 || Chamber->ChamberSize.GetMin() <= 0)
			{
				LastBuildStatus = Chamber->GetActorNameOrLabel() + TEXT(": use rotation 0 and positive scale/size; previous terrain retained"); return false;
			}
			MiningHollowBranches::FSettings& Branch = BranchSettings.AddDefaulted_GetRef();
			Branch.Name = Chamber->GetActorNameOrLabel();
			Branch.Chamber = Chamber->GetChamberBounds().ShiftBy(-GetActorLocation());
			Branch.InteriorPoints = Chamber->GetInteriorPoints();
			for (FVector& Point : Branch.InteriorPoints) Point -= GetActorLocation();
			Branch.Connection = Chamber->ConnectionPosition;
			Branch.Radius = Chamber->BranchRadius;
			Branch.Thickness = Chamber->EntranceThickness;
			Branch.bHidden = Chamber->bHiddenEntrance;
			ActiveChambers.Add(Chamber);
		}
		if (!MiningHollowBranches::Plan(HollowPlan, BranchSettings, HollowBranches, LastBuildStatus) ||
			!MiningHollow::Fill(HollowPlan, RegionRock, LastBuildStatus) || !MiningHollowBranches::Carve(HollowBranches, RegionRock, LastBuildStatus)) return false;
		if (!Combine(Shell, MakeBox(HollowPlan.Bounds.GetCenter(), HollowPlan.Bounds.GetSize() + FVector(Thickness * 2)), FMeshBoolean::EBooleanOp::Union))
		{
			LastBuildStatus = TEXT("Region outer bedrock union failed; previous terrain retained"); return false;
		}
		Cavities.Add(MakeBox(HollowPlan.Bounds.GetCenter(), HollowPlan.Bounds.GetSize()));
		TArray<FVector> Portals = HollowPlan.Points;
		Portals[0].X -= Thickness + 2; Portals.Last().X += Thickness + 2;
		FDynamicMesh3 PortalMesh;
		if (!MiningHollow::Tube(Portals, HollowPlan.Radius, PortalMesh)) { LastBuildStatus = TEXT("Invalid region portals"); return false; }
		Cavities.Add(MoveTemp(PortalMesh));
		SegmentMeshes.Add(MoveTemp(RegionRock));
		MiningRoutes::FPlan ProgressPlan;
		ProgressPlan.StartWall = 0; ProgressPlan.EndWall = 1;
		ProgressPlan.Points = HollowPlan.Points;
		ProgressPlan.Length = HollowPlan.Length;
		Plans.Add(MoveTemp(ProgressPlan));
		NewTunnelCount = 1;
	}
	for (int32 RoomIndex = 1; !bHollowRegionPrototype && RoomIndex < Boxes.Num(); ++RoomIndex)
	{
		const FBox& First = Boxes[RoomIndex - 1];
		const FBox& Second = Boxes[RoomIndex];
		AMiningTunnelRoute* Route = nullptr;
		for (AMiningTunnelRoute* Existing : Routes)
		{
			if (IsValid(Existing) && Existing->FromRoom == Rooms[RoomIndex - 1] && Existing->ToRoom == Rooms[RoomIndex]) { Route = Existing; break; }
		}
		if (!Route)
		{
			FActorSpawnParameters RouteSpawn;
			RouteSpawn.Owner = this;
			RouteSpawn.OverrideLevel = GetLevel();
			RouteSpawn.ObjectFlags |= RF_Transactional;
			Route = GetWorld()->SpawnActor<AMiningTunnelRoute>(FVector::ZeroVector, FRotator::ZeroRotator, RouteSpawn);
			if (!Route) { LastBuildStatus = TEXT("NOT APPLIED: route actor creation failed"); return false; }
			Route->FromRoom = Rooms[RoomIndex - 1];
			Route->ToRoom = Rooms[RoomIndex];
			Routes.Add(Route);
#if WITH_EDITOR
			Route->SetActorLabel(FString::Printf(TEXT("ROUTE_%02d_TO_%02d_EDIT_SPLINE"), Route->FromRoom->ConnectionOrder, Route->ToRoom->ConnectionOrder));
			Route->SetFolderPath(FName(*(TEXT("AutoMine/") + LayoutId.ToString() + TEXT("/Routes_EDIT_OPTIONAL"))));
#endif
		}
		ActiveRoutes.Add(Route);
		MiningRoutes::FSettings Settings;
		Settings.Width = FMath::Min<double>(FMath::Clamp(TunnelWidth, 200.f, 1200.f), FMath::Min(FMath::Min(First.GetSize().X, First.GetSize().Y), FMath::Min(Second.GetSize().X, Second.GetSize().Y)) - DoorMargin * 2);
		Settings.Height = FMath::Min<double>(FMath::Clamp(TunnelHeight, 220.f, 1000.f), FMath::Min(First.GetSize().Z, Second.GetSize().Z) - DoorMargin);
		Settings.Thickness = Thickness;
		Settings.MaxSlope = MaxSlopeDegrees;
		Settings.SearchDistance = SearchDistance;
		if (!bDirectRoomFaces && (Settings.Width < 200 || Settings.Height < 220)) { LastBuildStatus = TEXT("NOT APPLIED: room too small for doorway margin and player clearance"); Route->RouteStatus = LastBuildStatus; return false; }
		MiningRoutes::FPlan Plan;
		FString Reason;
		const bool bAscending = Second.Min.Z > First.Min.Z + 1;
		if (bAscending)
		{
			LayoutWarnings += FString::Printf(TEXT("%d -> %d rises %.1fm. "), Route->FromRoom->ConnectionOrder, Route->ToRoom->ConnectionOrder, (Second.Min.Z - First.Min.Z) / 100);
			if (bRequireDescending) { LastBuildStatus = TEXT("NOT APPLIED: descending-only route would rise; change start/order, rooms were not moved"); Route->RouteStatus = LastBuildStatus; return false; }
		}
		bool bFound = false;
		if (bDirectRoomFaces)
		{
			bFound = MiningRoutes::Direct(First, Second, static_cast<int32>(Route->StartWall) - 1, static_cast<int32>(Route->EndWall) - 1, Plan, Reason);
			TArray<MiningRoutes::FShapeFrame> Frames;
			for (AMiningTunnelShapeSection* Section : Route->GetShapeSections())
			{
				Section->SectionStatus = Section->bEnabled ? TEXT("Pending; check generator build status") : TEXT("Disabled; original edit retained");
				if (!Section->bEnabled || !bFound) continue;
				if (Section->GetActorTransform().ContainsNaN() || !Section->GetActorRotation().IsNearlyZero(0.01) || Section->GetActorScale3D().GetMin() <= 0)
				{
					Reason = TEXT("Shape sections require finite position, positive scale and zero rotation"); bFound = false; break;
				}
				const FVector2D Size = Section->GetWorldSize(Plan.StartWall);
				Frames.Add({Section->GetActorLocation() - GetActorLocation(), Size.X, Size.Y});
			}
			if (bFound) bFound = MiningRoutes::Shape(Plan, MoveTemp(Frames), Reason);
			if (bFound && Plan.MaxSlope > 35) LayoutWarnings += FString::Printf(TEXT("%d -> %d is steep (%.1f deg); direct geometry is not a walkability guarantee. "), Route->FromRoom->ConnectionOrder, Route->ToRoom->ConnectionOrder, Plan.MaxSlope);
		}
		else if (static_cast<int32>(Route->StartWall) > 4 || static_cast<int32>(Route->EndWall) > 4)
		{
			Reason = TEXT("Top/bottom faces are only supported by direct full-face connections");
		}
		else if (Route->bAutomatic)
		{
			bFound = MiningRoutes::Find(Boxes, RoomIndex - 1, RoomIndex, Settings, Reserved, static_cast<int32>(Route->StartWall) - 1, static_cast<int32>(Route->EndWall) - 1, Plan, Reason);
		}
		else
		{
			TArray<FVector> Points = Route->GetPoints();
			for (FVector& Point : Points) Point -= GetActorLocation();
			if (Points.Num() >= 4)
			{
				const int32 FirstWall = Route->StartWall == EMiningDoorWall::Auto ? Route->ResolvedStartWall : static_cast<int32>(Route->StartWall) - 1;
				const int32 SecondWall = Route->EndWall == EMiningDoorWall::Auto ? Route->ResolvedEndWall : static_cast<int32>(Route->EndWall) - 1;
				const double Landing = Settings.Width * 0.75 + Thickness + 120;
				Points[0] = MiningRoutes::Door(First, FirstWall);
				Points[1] = Points[0] + MiningRoutes::WallDirection(FirstWall) * Landing;
				Points.Last() = MiningRoutes::Door(Second, SecondWall);
				Points[Points.Num() - 2] = Points.Last() + MiningRoutes::WallDirection(SecondWall) * Landing;
				bFound = MiningRoutes::Build(Points, Settings, Plan, Reason) && MiningRoutes::Validate(Plan, Settings, Boxes, RoomIndex - 1, RoomIndex, Reserved, Reason);
				Plan.StartWall = FirstWall;
				Plan.EndWall = SecondWall;
			}
			else Reason = TEXT("Manual route has fewer than four points");
		}
		if (!bFound)
		{
			Route->RouteStatus = TEXT("NOT APPLIED: ") + Reason;
			for (AMiningTunnelShapeSection* Section : Route->GetShapeSections()) if (Section->bEnabled) Section->SectionStatus = Route->RouteStatus;
			LastBuildStatus = FString::Printf(TEXT("NOT APPLIED %d -> %d: %s. Previous geometry retained"), Route->FromRoom->ConnectionOrder, Route->ToRoom->ConnectionOrder, *Reason);
			return false;
		}
		FDynamicMesh3 Inner;
		FDynamicMesh3 Outer;
		if (bDirectRoomFaces)
		{
			if (!MakeDirectMesh(Plan, 0, Inner) || !MakeDirectMesh(Plan, Thickness, Outer))
			{
				LastBuildStatus = TEXT("NOT APPLIED: direct face connection is degenerate; previous geometry retained"); return false;
			}
		}
		else
		{
			Inner = MakeRouteMesh(Plan, Settings);
			Outer = MakeRouteMesh(Plan, Settings, Thickness);
		}
		if (!Combine(Shell, Outer, FMeshBoolean::EBooleanOp::Union))
		{
			LastBuildStatus = FString::Printf(TEXT("NOT APPLIED: route %d outer sweep union failed; previous geometry retained"), RoomIndex); return false;
		}
		FDynamicMesh3 FillPiece = Inner;
		if (bDirectRoomFaces)
		{
			FillPiece.Clear();
			if (!MakeDirectMesh(Plan, 0, FillPiece, 0)) { LastBuildStatus = TEXT("NOT APPLIED: exact face fill failed; previous geometry retained"); return false; }
			FBox FillBounds(ForceInit);
			for (const TArray<FVector>& Corners : Plan.FaceSections) for (const FVector& Corner : Corners) FillBounds += Corner;
			for (int32 OtherRoom = 0; OtherRoom < Boxes.Num(); ++OtherRoom)
			{
				if (OtherRoom == RoomIndex - 1 || OtherRoom == RoomIndex || !FillBounds.Intersect(Boxes[OtherRoom])) continue;
				if (Plan.FaceSections.Num() > 1)
				{
					FDynamicMesh3 Intersection = FillPiece;
					if (!Combine(Intersection, MakeBox(Boxes[OtherRoom].GetCenter(), Boxes[OtherRoom].GetSize()), FMeshBoolean::EBooleanOp::Intersect) || Intersection.TriangleCount() > 0)
					{
						LastBuildStatus = TEXT("NOT APPLIED: shaped tunnel crosses another main room; move its sections. Previous geometry retained"); return false;
					}
					continue;
				}
				if (!Combine(FillPiece, MakeBox(Boxes[OtherRoom].GetCenter(), Boxes[OtherRoom].GetSize() + FVector(0.5)), FMeshBoolean::EBooleanOp::Difference))
				{
					LastBuildStatus = TEXT("NOT APPLIED: intersecting room clearance failed; previous geometry retained"); return false;
				}
			}
		}
		if (bDirectRoomFaces)
		{
			for (int32 EarlierIndex = 0; EarlierIndex < SegmentMeshes.Num(); ++EarlierIndex)
			{
				const FDynamicMesh3& Earlier = SegmentMeshes[EarlierIndex];
				if ((Plan.FaceSections.Num() > 1 || Plans[EarlierIndex].FaceSections.Num() > 1) && FillPiece.GetBounds().Intersects(Earlier.GetBounds()))
				{
					FDynamicMesh3 Intersection = FillPiece;
					if (!Combine(Intersection, Earlier, FMeshBoolean::EBooleanOp::Intersect) || Intersection.TriangleCount() > 0)
					{
						LastBuildStatus = TEXT("NOT APPLIED: shaped tunnels cross another progression group; move the sections. Previous geometry retained"); return false;
					}
					continue;
				}
				if (FillPiece.GetBounds().Intersects(Earlier.GetBounds()) &&
					!Combine(FillPiece, Earlier, FMeshBoolean::EBooleanOp::Difference))
				{
					LastBuildStatus = TEXT("NOT APPLIED: segment overlap separation failed; previous geometry retained"); return false;
				}
			}
			SegmentMeshes.Add(MoveTemp(FillPiece));
		}
		else if (!Combine(Fill, FillPiece, FMeshBoolean::EBooleanOp::Union)) { LastBuildStatus = TEXT("NOT APPLIED: route fill union failed; previous geometry retained"); return false; }
		Cavities.Add(MoveTemp(Inner));
		for (const MiningRoutes::FSection& Section : Plan.Sections) Reserved.Add(Section.Bounds(Settings.Width, Settings.Height, Thickness));
		for (const FBox& Pad : Plan.Pads) Reserved.Add(Pad.ExpandBy(Thickness));
		Plans.Add(MoveTemp(Plan));
		++NewTunnelCount;
	}
	const TArray<AMiningLayoutSpace*> Spaces = GetLayoutSpaces();
	if ((!bDirectRoomFaces || bHollowRegionPrototype) && Spaces.ContainsByPredicate([](const AMiningLayoutSpace* Space) { return Space->bEnabled; }))
	{
		LastBuildStatus = TEXT("NOT APPLIED: existing editable spaces require original direct room-face mode; previous geometry retained");
		return false;
	}
	TArray<FMiningMapRegion> NewMapRegions;
	for (int32 Index = 0; Index < Boxes.Num(); ++Index)
	{
		NewMapRegions.Add(MiningDeveloperMap::Box(FString::Printf(TEXT("R%d%s"), Rooms[Index]->ConnectionOrder, Index == 0 ? TEXT(" START") : TEXT("")),
			EMiningMapRegionKind::Room, Index, Boxes[Index].ShiftBy(GetActorLocation())));
	}
	for (int32 Index = 0; Index < Plans.Num(); ++Index)
	{
		const FString Name = FString::Printf(TEXT("Tunnel R%d > R%d"), Rooms[Index]->ConnectionOrder, Rooms[Index + 1]->ConnectionOrder);
		if (!Plans[Index].FaceSections.IsEmpty())
		{
			for (TArray<FVector> Vertices : Plans[Index].FaceSections)
			{
				for (FVector& Vertex : Vertices) Vertex += GetActorLocation();
				NewMapRegions.Add(MiningDeveloperMap::Convex(Name, EMiningMapRegionKind::Tunnel, Index, Vertices));
			}
		}
		else
		{
			for (const MiningRoutes::FSection& Section : Plans[Index].Sections)
				NewMapRegions.Add(MiningDeveloperMap::Box(Name + TEXT(" (bounds)"), EMiningMapRegionKind::Tunnel, Index, Section.Bounds(TunnelWidth, TunnelHeight).ShiftBy(GetActorLocation())));
		}
	}
	if (bHollowRegionPrototype)
	{
		NewMapRegions.Add(MiningDeveloperMap::Box(TEXT("Ordinary diggable rock"), EMiningMapRegionKind::RockRegion, 0, HollowPlan.Bounds.ShiftBy(GetActorLocation())));
		const auto AddTubeRegion = [&](const FVector& Start, const FVector& End, const FString& Name, EMiningMapRegionKind Kind)
		{
			TArray<FVector> Vertices = MiningHollow::Ring(Start + GetActorLocation(), HollowPlan.Radius);
			Vertices.Append(MiningHollow::Ring(End + GetActorLocation(), HollowPlan.Radius));
			NewMapRegions.Add(MiningDeveloperMap::Convex(Name, Kind, 0, Vertices));
		};
		for (int32 Index = 1; Index < HollowPlan.Points.Num(); ++Index)
			AddTubeRegion(HollowPlan.Points[Index - 1], HollowPlan.Points[Index], TEXT("Hollow main passage"), EMiningMapRegionKind::Tunnel);
		for (const FMiningGuideVolume& Guide : HollowPlan.Guides)
			AddTubeRegion(Guide.Start, Guide.End, TEXT("Diggable guide plug"), EMiningMapRegionKind::GuidePlug);
		MiningHollowBranches::AddMapRegions(HollowBranches, GetActorLocation(), NewMapRegions);
	}
	if (bDirectRoomFaces && !bHollowRegionPrototype && !MiningSpaces::Apply(Spaces, Rooms, Boxes, Plans, GetActorLocation(), Thickness, SegmentMeshes, Cavities, Shell, LastBuildStatus, &NewMapRegions)) return false;
	for (const FDynamicMesh3& Cavity : Cavities)
	{
		if (!Combine(Shell, Cavity, FMeshBoolean::EBooleanOp::Difference))
		{
			LastBuildStatus = TEXT("Bedrock doorway cut failed; previous geometry preserved");
			return false;
		}
	}
	for (const FBox& Box : Boxes)
	{
		if (bDirectRoomFaces) break;
		if (!Combine(Fill, MakeBox(Box.GetCenter(), Box.GetSize() + FVector(0.5)), FMeshBoolean::EBooleanOp::Difference))
		{
			LastBuildStatus = TEXT("Room clearance cut failed; previous geometry preserved");
			return false;
		}
	}
	PrepareNormals(Shell);
	if (!bDirectRoomFaces) SegmentMeshes.Add(MoveTemp(Fill));
	for (FDynamicMesh3& SegmentMesh : SegmentMeshes) PrepareNormals(SegmentMesh);
	FActorSpawnParameters Spawn;
	Spawn.Owner = this;
	Spawn.OverrideLevel = GetLevel();
	Spawn.ObjectFlags |= RF_Transactional;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (!IsValid(GeneratedBedrock))
	{
		GeneratedBedrock = GetWorld()->SpawnActor<ADynamicMeshActor>(GetActorLocation(), FRotator::ZeroRotator, Spawn);
	}
	TArray<TObjectPtr<AGeneratedMiningRock>> NewSegments;
	for (int32 Index = 0; Index < SegmentMeshes.Num(); ++Index)
	{
		AGeneratedMiningRock* Segment = GetWorld()->SpawnActor<AGeneratedMiningRock>(GetActorLocation(), FRotator::ZeroRotator, Spawn);
		if (!Segment)
		{
			for (AGeneratedMiningRock* Created : NewSegments) Created->Destroy();
			LastBuildStatus = TEXT("Could not create segments; previous geometry preserved");
			return false;
		}
		NewSegments.Add(Segment);
		Segment->SegmentIndex = bDirectRoomFaces ? Index : INDEX_NONE;
		Segment->MaxTriangles = FMath::Clamp(SegmentTriangleBudget, 1000, 30000);
		Segment->RockMaterial = TunnelMaterial;
		if (bHollowRegionPrototype)
		{
			Segment->GuideMaterialVolumes = HollowPlan.Guides;
			for (FMiningGuideVolume& Guide : Segment->GuideMaterialVolumes) { Guide.Start += GetActorLocation(); Guide.End += GetActorLocation(); }
		}
		Segment->bSimplifyChunks = bSimplifyChunks;
		Segment->SimplificationTolerance = SimplificationTolerance;
		if (bAutomaticChunks && !Segment->SetChunkedMesh(SegmentMeshes[Index], ChunkLength, bHollowRegionPrototype))
		{
			for (AGeneratedMiningRock* Created : NewSegments) Created->Destroy();
			LastBuildStatus = TEXT("NOT APPLIED: chunk generation failed or exceeds 128 chunks per tunnel; previous geometry preserved");
			return false;
		}
		if (!bAutomaticChunks) Segment->SetGeneratedMesh(FDynamicMesh3(SegmentMeshes[Index]));
		if ((bAutomaticChunks ? Segment->LargestChunkTriangles : Segment->TriangleCount) > SegmentTriangleBudget)
		{
			for (AGeneratedMiningRock* Created : NewSegments) Created->Destroy();
			LastBuildStatus = TEXT("NOT APPLIED: initial space geometry exceeds the existing triangle budget; simplify spaces or shorten chunks");
			return false;
		}
	}
	if (!IsValid(GeneratedPlayerStart))
	{
		GeneratedPlayerStart = GetWorld()->SpawnActor<APlayerStart>(GetActorLocation(), FRotator::ZeroRotator, Spawn);
	}
	if (!GeneratedBedrock || !GeneratedPlayerStart)
	{
		for (AGeneratedMiningRock* Created : NewSegments) Created->Destroy();
		LastBuildStatus = TEXT("Could not create generated actors");
		return false;
	}
	Modify();
	TArray<AActor*> GeneratedActors{GeneratedBedrock, GeneratedPlayerStart};
	for (AGeneratedMiningRock* Segment : NewSegments) GeneratedActors.Add(Segment);
	for (AActor* Generated : GeneratedActors)
	{
		Generated->Modify();
		Generated->GetRootComponent()->SetMobility(EComponentMobility::Movable);
		Generated->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		Generated->SetActorTransform(GetActorTransform());
#if WITH_EDITOR
		Generated->SetFolderPath(FName(*(TEXT("AutoMine/") + LayoutId.ToString() + TEXT("/Generated"))));
#endif
	}
	UDynamicMeshComponent* BedrockMesh = GeneratedBedrock->GetDynamicMeshComponent();
	BedrockMesh->SetCollisionProfileName(TEXT("BlockAll"));
	BedrockMesh->SetComplexAsSimpleCollisionEnabled(true, false);
	BedrockMesh->SetDeferredCollisionUpdatesEnabled(true, false);
	BedrockMesh->bUseAsyncCooking = false;
	BedrockMesh->SetGenerateOverlapEvents(false);
	BedrockMesh->GetDynamicMesh()->Modify();
	BedrockMesh->SetMesh(MoveTemp(Shell));
	BedrockMesh->GetBodySetup()->SetFlags(RF_Transient);
	BedrockMesh->UpdateCollision(false);
	BedrockMesh->SetMaterial(0, BedrockMaterial);
	TArray<FBox> NewSegmentBounds;
	for (int32 Index = 0; Index < NewSegments.Num(); ++Index)
	{
		AGeneratedMiningRock* Segment = NewSegments[Index];
		const FAxisAlignedBox3d Bounds = SegmentMeshes[Index].TriangleCount() ? SegmentMeshes[Index].GetBounds() : Cavities[Rooms.Num() + Index].GetBounds();
		NewSegmentBounds.Add(FBox(FVector(Bounds.Min), FVector(Bounds.Max)));
#if WITH_EDITOR
		Segment->SetActorLabel(FString::Printf(TEXT("GENERATED_Segment_%02d_EDIT_ROOMS_INSTEAD"), Index + 1));
#endif
	}
	if (IsValid(GeneratedFill) && !GeneratedSegments.Contains(GeneratedFill)) GeneratedFill->Destroy();
	for (AGeneratedMiningRock* Previous : GeneratedSegments)
	{
		if (IsValid(Previous)) Previous->Destroy();
	}
	GeneratedSegments = MoveTemp(NewSegments);
	GeneratedFill = GeneratedSegments.IsEmpty() ? nullptr : GeneratedSegments[0];
	RemoveStaleSegments();
	GeneratedRoomBounds = Boxes;
	GeneratedMapRegions = MoveTemp(NewMapRegions);
	GeneratedSegmentBounds = MoveTemp(NewSegmentBounds);
	GeneratedArrivalWalls.Reset();
	for (const MiningRoutes::FPlan& Plan : Plans) GeneratedArrivalWalls.Add(Plan.EndWall);
	bGeneratedSegmented = bDirectRoomFaces;
	const FVector StartPosition = FVector(Boxes[0].GetCenter().X, Boxes[0].GetCenter().Y, Boxes[0].Min.Z + 105) + GetActorLocation();
	GeneratedPlayerStart->SetActorLocation(StartPosition);
	if (Rooms.Num() > 1)
	{
		const FVector Facing = (Rooms[1]->GetActorLocation() - Rooms[0]->GetActorLocation()).GetSafeNormal2D();
		GeneratedPlayerStart->SetActorRotation(Facing.Rotation());
	}
	double LowestFloor = Boxes[0].Min.Z;
	for (const FBox& Box : Boxes)
	{
		LowestFloor = FMath::Min(LowestFloor, Box.Min.Z);
	}
	for (const FBox& Bounds : GeneratedSegmentBounds) if (Bounds.IsValid) LowestFloor = FMath::Min(LowestFloor, Bounds.Min.Z);
	RecommendedFallResetDistance = FMath::Max(2000.0, Boxes[0].Min.Z - LowestFloor + 1500.0);
#if WITH_EDITOR
	GeneratedBedrock->SetActorLabel(TEXT("GENERATED_Bedrock_EDIT_ROOMS_INSTEAD"));
	GeneratedPlayerStart->SetActorLabel(TEXT("GENERATED_Start_FOLLOWS_FIRST_ROOM"));
#endif
	RoomCount = Rooms.Num();
	TunnelCount = NewTunnelCount;
	SpaceCount = 0;
	for (AMiningLayoutSpace* Space : Spaces)
	{
		if (Space->bEnabled) { ++SpaceCount; Space->SpaceStatus.ReplaceInline(TEXT("Prepared for"), TEXT("Ready:")); Space->SpaceStatus.ReplaceInline(TEXT("; confirm generator Ready status"), TEXT("; not a progression room")); }
	}
	++BuildRevision;
	for (int32 Index = 0; Index < ActiveRoutes.Num(); ++Index)
	{
		AMiningTunnelRoute* Route = ActiveRoutes[Index];
		TArray<FVector> Points = Plans[Index].Points;
		for (FVector& Point : Points) Point += GetActorLocation();
		Route->SetPoints(Points);
		Route->bDirectConnection = bDirectRoomFaces;
#if WITH_EDITOR
		Route->SetActorLabel(FString::Printf(TEXT("%s_%02d_TO_%02d_%s"), bDirectRoomFaces ? TEXT("LINK") : TEXT("ROUTE"), Route->FromRoom->ConnectionOrder, Route->ToRoom->ConnectionOrder, bDirectRoomFaces ? (Plans[Index].FaceSections.Num() > 1 ? TEXT("SHAPED_FULL_FACE") : TEXT("FULL_FACE_AUTO")) : TEXT("EDIT_SPLINE")));
		Route->SetFolderPath(FName(*(TEXT("AutoMine/") + LayoutId.ToString() + (bDirectRoomFaces ? TEXT("/Links_AUTO") : TEXT("/Routes_EDIT_OPTIONAL")))));
#endif
		Route->MaximumSlope = Plans[Index].MaxSlope;
		Route->RouteLength = Plans[Index].Length;
		Route->ResolvedStartWall = Plans[Index].StartWall;
		Route->ResolvedEndWall = Plans[Index].EndWall;
		Route->RouteStatus = FString::Printf(TEXT("%s: %.1f deg, %.1fm%s"), bDirectRoomFaces ? TEXT("DIRECT / FULL FACE") : TEXT("Ready"), Route->MaximumSlope, Route->RouteLength / 100, Points.Last().Z > Points[0].Z + 1 ? TEXT("; ASCENDING") : TEXT(""));
		if (Plans[Index].FaceSections.Num() > 1) Route->RouteStatus += FString::Printf(TEXT("; %d editable shape sections"), Plans[Index].FaceSections.Num() - 1);
		for (AMiningTunnelShapeSection* Section : Route->GetShapeSections())
		{
			Section->SectionStatus = Section->bEnabled ? TEXT("Ready; move to bend, change Width/Height to resize; no rotation") : TEXT("Disabled; original edit retained");
			Section->RefreshGuides();
		}
	}
	for (AMiningTunnelRoute* Route : Routes)
	{
		if (IsValid(Route) && !ActiveRoutes.Contains(Route)) Route->Destroy();
	}
	Routes = MoveTemp(ActiveRoutes);
	if (bHollowRegionPrototype)
	{
		HollowRegion->Path->Modify();
		HollowRegion->Path->SetLocationAtSplinePoint(0, HollowPlan.Points[0] + GetActorLocation(), ESplineCoordinateSpace::World, false);
		HollowRegion->Path->SetLocationAtSplinePoint(HollowRegion->Path->GetNumberOfSplinePoints() - 1, HollowPlan.Points.Last() + GetActorLocation(), ESplineCoordinateSpace::World, true);
		for (int32 Index = 0; Index < ActiveChambers.Num(); ++Index)
		{
			TArray<FVector> Points = HollowBranches[Index].Points;
			for (FVector& Point : Points) Point += GetActorLocation();
			ActiveChambers[Index]->ApplyResolvedPath(Points);
		}
		SpaceCount = ActiveChambers.Num();
	}
	bLayoutApplied = true;
	bPendingRebuild = false;
	ObservedInputHash = GetInputHash();
	LastBuildStatus = FString::Printf(TEXT("Ready: %d rooms, %d %s links. %s"), RoomCount, TunnelCount, bDirectRoomFaces ? TEXT("direct full-face") : TEXT("walkable"), *LayoutWarnings);
	if (bHollowRegionPrototype)
	{
		LastBuildStatus = FString::Printf(TEXT("Ready: hollow region, %d plugs, %d chambers, %d spatial chunks; 30000 triangle maximum per chunk"), HollowPlan.PlugIntervals.Num(), HollowBranches.Num(), GeneratedFill->Chunks.Num());
		HollowRegion->RegionStatus = LastBuildStatus;
		HollowRegion->RefreshBounds(HollowPlan.Bounds.ShiftBy(GetActorLocation()));
	}
	UpdatePreview();
	RefreshEditorVisibility();
	MarkPackageDirty();
	return true;
}

void AMiningTunnelGenerator::ShowLayout()
{
	Modify();
	PreviewMode = EMiningPreviewMode::Layout;
	bShowBedrockInEditor = false;
	UpdatePreview();
}

void AMiningTunnelGenerator::ShowSection()
{
	Modify();
	PreviewMode = EMiningPreviewMode::Section;
	bShowBedrockInEditor = false;
	UpdatePreview();
}

void AMiningTunnelGenerator::ShowGameGeometry()
{
	Modify();
	PreviewMode = EMiningPreviewMode::Game;
	RefreshEditorVisibility();
}

void AMiningTunnelGenerator::UpdatePreview()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld() || !IsValid(GeneratedBedrock)) return;
	const auto CreatePreview = [this](TObjectPtr<ADynamicMeshActor>& Actor, const TCHAR* Label)
	{
		if (!IsValid(Actor))
		{
			FActorSpawnParameters Spawn;
			Spawn.Owner = this;
			Spawn.OverrideLevel = GetLevel();
			Actor = GetWorld()->SpawnActor<ADynamicMeshActor>(GetActorLocation(), FRotator::ZeroRotator, Spawn);
			if (!Actor) return;
			Actor->bIsEditorOnlyActor = true;
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorLabel(Label);
			Actor->SetFolderPath(FName(*(TEXT("AutoMine/") + LayoutId.ToString() + TEXT("/Preview_NO_COLLISION"))));
		}
		Actor->SetActorTransform(GetActorTransform());
		Actor->GetDynamicMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Actor->GetDynamicMeshComponent()->SetCastShadow(false);
	};
	CreatePreview(PreviewBedrock, TEXT("PREVIEW_Bedrock_SECTION_ONLY"));
	CreatePreview(PreviewFill, TEXT("PREVIEW_Diggable_EDITOR_ONLY"));
	if (!PreviewBedrock || !PreviewFill) return;
	FDynamicMesh3 Rock;
	FDynamicMesh3 Dirt;
	GeneratedBedrock->GetDynamicMeshComponent()->GetDynamicMesh()->ProcessMesh([&Rock](const FDynamicMesh3& Mesh) { Rock = Mesh; });
	if (GeneratedSegments.IsEmpty() && IsValid(GeneratedFill))
	{
		GeneratedFill->RockMesh->GetDynamicMesh()->ProcessMesh([&Dirt](const FDynamicMesh3& Mesh) { Dirt = Mesh; });
	}
	for (const AGeneratedMiningRock* Segment : GeneratedSegments)
	{
		if (!IsValid(Segment)) continue;
		FDynamicMesh3 SegmentGeometry;
		Segment->CopyEditorPreviewMesh(SegmentGeometry);
		const auto Append = [&Dirt](const FDynamicMesh3& Mesh)
		{
			TMap<int32, int32> Vertices;
			for (int32 Vertex : Mesh.VertexIndicesItr()) Vertices.Add(Vertex, Dirt.AppendVertex(Mesh.GetVertex(Vertex)));
			for (int32 Triangle : Mesh.TriangleIndicesItr())
			{
				const FIndex3i Indices = Mesh.GetTriangle(Triangle);
				Dirt.AppendTriangle(Vertices[Indices.A], Vertices[Indices.B], Vertices[Indices.C]);
			}
		};
		Append(SegmentGeometry);
	}
	if (PreviewMode == EMiningPreviewMode::Section)
	{
		const FAxisAlignedBox3d Bounds = Rock.GetBounds();
		FVector Minimum(Bounds.Min - FVector3d(100));
		FVector Maximum(Bounds.Max + FVector3d(100));
		const int32 Axis = FMath::Clamp(SectionAxis, 0, 2);
		const double Plane = SectionPosition - GetActorLocation()[Axis];
		if (bSectionKeepPositive) Minimum[Axis] = FMath::Max(Minimum[Axis], Plane);
		else Maximum[Axis] = FMath::Min(Maximum[Axis], Plane);
		if (Minimum[Axis] >= Maximum[Axis]) { Rock.Clear(); Dirt.Clear(); }
		else
		{
			const FDynamicMesh3 Clip = MakeBox((Minimum + Maximum) * 0.5, Maximum - Minimum);
			if (!Combine(Rock, Clip, FMeshBoolean::EBooleanOp::Intersect) || !Combine(Dirt, Clip, FMeshBoolean::EBooleanOp::Intersect))
			{
				Rock.Clear(); Dirt.Clear();
				LayoutWarnings += TEXT(" Section preview failed; adjust Section Position slightly. Game mesh unchanged.");
			}
		}
	}
	PrepareNormals(Rock);
	PrepareNormals(Dirt);
	PreviewBedrock->GetDynamicMeshComponent()->SetMesh(MoveTemp(Rock));
	PreviewFill->GetDynamicMeshComponent()->SetMesh(MoveTemp(Dirt));
	PreviewBedrock->GetDynamicMeshComponent()->SetMaterial(0, PreviewBedrockMaterial ? PreviewBedrockMaterial.Get() : BedrockMaterial.Get());
	PreviewFill->GetDynamicMeshComponent()->SetMaterial(0, PreviewFillMaterial ? PreviewFillMaterial.Get() : TunnelMaterial.Get());
	if (bHollowRegionPrototype && IsValid(GeneratedFill)) PreviewFill->GetDynamicMeshComponent()->SetMaterial(0, GeneratedFill->GetMiningMaterial());
	RefreshEditorVisibility();
#endif
}

void AMiningTunnelGenerator::DrawLayoutGuides()
{
#if WITH_EDITOR
	if (PreviewMode == EMiningPreviewMode::Game || !GetWorld() || GetWorld()->IsGameWorld()) return;
	for (const AMiningLayoutRoom* Room : GetOrderedRooms())
	{
		const FBox Box = Room->GetRoomBounds();
		DrawDebugBox(GetWorld(), Box.GetCenter(), Box.GetExtent(), bLayoutApplied ? FColor::Cyan : FColor::Orange, false, 0.15f, 1, 4);
		if (!GetWorld()->ViewLocationsRenderedLastFrame.IsEmpty()) Room->Label->SetWorldRotation((GetWorld()->ViewLocationsRenderedLastFrame[0] - Room->Label->GetComponentLocation()).Rotation());
	}
	for (const AMiningTunnelRoute* Route : Routes)
	{
		if (!IsValid(Route) || !IsValid(Route->FromRoom) || !IsValid(Route->ToRoom)) continue;
		TArray<FVector> Points = Route->GetPoints();
		const FColor Color = !bLayoutApplied ? FColor::Red : (Points.Num() > 1 && Points.Last().Z > Points[0].Z + 1 ? FColor::Orange : FColor::Green);
		if (!bLayoutApplied) Points = {Route->FromRoom->GetRoomBounds().GetCenter(), Route->ToRoom->GetRoomBounds().GetCenter()};
		if (!Points.IsEmpty())
		{
			Route->StatusLabel->SetWorldLocation(Points[Points.Num() / 2] + FVector(0, 0, TunnelHeight + 120));
			Route->StatusLabel->SetTextRenderColor(Color);
			Route->StatusLabel->SetText(FText::FromString(FString::Printf(TEXT("%02d -> %02d | %s"), Route->FromRoom->ConnectionOrder, Route->ToRoom->ConnectionOrder, bLayoutApplied ? *Route->RouteStatus : TEXT("NOT APPLIED - check settings"))));
			if (!GetWorld()->ViewLocationsRenderedLastFrame.IsEmpty()) Route->StatusLabel->SetWorldRotation((GetWorld()->ViewLocationsRenderedLastFrame[0] - Route->StatusLabel->GetComponentLocation()).Rotation());
		}
		for (int32 Index = 1; Index < Points.Num(); ++Index)
		{
			DrawDebugDirectionalArrow(GetWorld(), Points[Index - 1] + FVector(0, 0, 100), Points[Index] + FVector(0, 0, 100), 80, Color, false, 0.15f, 1, 5);
		}
	}
	for (AMiningLayoutSpace* Space : GetLayoutSpaces())
	{
		const FBox Bounds = Space->GetSpaceBounds();
		const FColor Color = !Space->bEnabled ? FColor::Silver : !bLayoutApplied ? FColor::Red : Space->Contents == EMiningSpaceContents::Empty ? FColor::Cyan : FColor::Orange;
		DrawDebugBox(GetWorld(), Bounds.GetCenter(), Bounds.GetExtent(), Color, false, 0.15f, 1, 3);
		if (Space->bEnabled && Space->Connection == EMiningSpaceConnection::StraightBranch && bLayoutApplied)
		{
			DrawDebugLine(GetWorld(), Space->ResolvedBranchAnchor, Bounds.GetCenter(), Color, false, 0.15f, 1, 3);
			if (!Space->ResolvedWallSize.IsNearlyZero()) DrawDebugBox(GetWorld(), Space->ResolvedWallCenter, Space->ResolvedWallSize * 0.5, Space->ResolvedWallRotation, FColor::Magenta, false, 0.15f, 1, 5);
		}
		if (!GetWorld()->ViewLocationsRenderedLastFrame.IsEmpty()) Space->Label->SetWorldRotation((GetWorld()->ViewLocationsRenderedLastFrame[0] - Space->Label->GetComponentLocation()).Rotation());
	}
#endif
}
