#include "MiningSpaceGeometry.h"
#include "MiningLayoutRoom.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "Generators/SphereGenerator.h"
#include "Operations/MeshBoolean.h"

using namespace UE::Geometry;

namespace
{
	struct FSpaceGeometry
	{
		AMiningLayoutSpace* Marker = nullptr;
		int32 Segment = INDEX_NONE;
		FDynamicMesh3 Interior;
		FDynamicMesh3 Exterior;
		FDynamicMesh3 Empty;
		TArray<FDynamicMesh3> SeparateEmptyParts;
		FDynamicMesh3 Wall;
		FVector Anchor = FVector::ZeroVector;
		FVector WallCenter = FVector::ZeroVector;
		FVector WallSize = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
	};

	FDynamicMesh3 BoxMesh(const FVector& Center, const FVector& Size, const FQuat& Rotation = FQuat::Identity)
	{
		FMinimalBoxMeshGenerator Generator;
		Generator.Box = FOrientedBox3d(FVector3d::Zero(), FVector3d(Size * 0.5));
		Generator.Generate();
		FDynamicMesh3 Result(&Generator);
		for (int32 Vertex : Result.VertexIndicesItr()) Result.SetVertex(Vertex, FVector3d(Center + Rotation.RotateVector(FVector(Result.GetVertex(Vertex)))));
		return Result;
	}

	bool CombineSpaceGeometry(FDynamicMesh3& Target, const FDynamicMesh3& Tool, FMeshBoolean::EBooleanOp Mode)
	{
		if (Tool.TriangleCount() == 0) return true;
		if (Target.TriangleCount() == 0)
		{
			if (Mode == FMeshBoolean::EBooleanOp::Union) Target = Tool;
			return true;
		}
		FDynamicMesh3 First = Target;
		FDynamicMesh3 Second = Tool;
		First.DiscardAttributes();
		Second.DiscardAttributes();
		if (!First.HasTriangleGroups()) First.EnableTriangleGroups(0);
		if (!Second.HasTriangleGroups()) Second.EnableTriangleGroups(0);
		const double Tolerances[] = {0.000001, 0.00001, 0.0001, 0.001};
		for (int32 Attempt = 0; Attempt < 8; ++Attempt)
		{
			FDynamicMesh3 Result;
			FMeshBoolean Operation(&First, &Second, &Result, Mode);
			Operation.bSimplifyAlongNewEdges = Attempt % 2 == 0;
			Operation.bPreserveTriangleGroups = false;
			Operation.SnapTolerance = Tolerances[Attempt / 2];
			if (Operation.Compute() && Operation.CreatedBoundaryEdges.IsEmpty() &&
				(Result.TriangleCount() == 0 || Result.IsClosed()) &&
				Result.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly))
			{
				Target = MoveTemp(Result);
				return true;
			}
		}
		return false;
	}

	int32 Intersects(const FDynamicMesh3& First, const FDynamicMesh3& Second)
	{
		if (!First.TriangleCount() || !Second.TriangleCount() || !First.GetBounds().Intersects(Second.GetBounds())) return 0;
		FDynamicMesh3 Intersection = First;
		if (!CombineSpaceGeometry(Intersection, Second, FMeshBoolean::EBooleanOp::Intersect)) return -1;
		return Intersection.TriangleCount() > 0 ? 1 : 0;
	}

	FDynamicMesh3 BranchMesh(const FVector& Start, const FVector& Finish, double Width, double Height, double StartPadding, double EndPadding, bool bUpright)
	{
		const FVector Delta = Finish - Start;
		if (!bUpright) return BoxMesh((Start + Finish + Delta.GetSafeNormal() * (EndPadding - StartPadding)) * 0.5,
			FVector(Delta.Size() + StartPadding + EndPadding, Width, Height), FRotationMatrix::MakeFromX(Delta).ToQuat());
		const double Length = Delta.Size2D();
		const FQuat Yaw = FRotationMatrix::MakeFromXZ(Delta.GetSafeNormal2D(), FVector::UpVector).ToQuat();
		FDynamicMesh3 Result = BoxMesh(FVector((Length + EndPadding - StartPadding) * 0.5, 0, 0), FVector(Length + StartPadding + EndPadding, Width, Height));
		for (int32 Vertex : Result.VertexIndicesItr())
		{
			const FVector Point(Result.GetVertex(Vertex));
			FVector World = Start + Yaw.RotateVector(Point);
			World.Z += Delta.Z * Point.X / Length;
			Result.SetVertex(Vertex, FVector3d(World));
		}
		return Result;
	}

	double EntryDistance(const FDynamicMesh3& Mesh, const FVector& Start, const FVector& Direction)
	{
		double Nearest = TNumericLimits<double>::Max();
		for (int32 Triangle : Mesh.TriangleIndicesItr())
		{
			FVector3d First, Second, Third;
			Mesh.GetTriVertices(Triangle, First, Second, Third);
			const FVector3d Edge = Second - First;
			const FVector3d Other = Third - First;
			const FVector3d Cross = FVector3d(Direction).Cross(Other);
			const double Determinant = Edge.Dot(Cross);
			if (FMath::Abs(Determinant) < 0.00000001) continue;
			const FVector3d Offset = FVector3d(Start) - First;
			const double AlongEdge = Offset.Dot(Cross) / Determinant;
			const FVector3d Side = Offset.Cross(Edge);
			const double AlongOther = FVector3d(Direction).Dot(Side) / Determinant;
			const double Distance = Other.Dot(Side) / Determinant;
			if (AlongEdge >= -0.000001 && AlongOther >= -0.000001 && AlongEdge + AlongOther <= 1.000001 && Distance > 0)
				Nearest = FMath::Min(Nearest, Distance);
		}
		return Nearest;
	}
}

FDynamicMesh3 MiningSpaces::ShapeMesh(EMiningSpaceShape Shape, const FBox& Bounds)
{
	if (Shape == EMiningSpaceShape::Box) return BoxMesh(Bounds.GetCenter(), Bounds.GetSize());
	FSphereGenerator Generator;
	Generator.Radius = 1;
	Generator.NumPhi = 13;
	Generator.NumTheta = 24;
	Generator.Generate();
	FDynamicMesh3 Result(&Generator);
	for (int32 Vertex : Result.VertexIndicesItr()) Result.SetVertex(Vertex, FVector3d(Bounds.GetCenter() + FVector(Result.GetVertex(Vertex)) * Bounds.GetExtent()));
	return Result;
}

bool MiningSpaces::Apply(const TArray<AMiningLayoutSpace*>& Spaces, const TArray<AMiningLayoutRoom*>& Rooms,
	const TArray<FBox>& RoomBounds, const TArray<MiningRoutes::FPlan>& Plans, const FVector& Origin,
	double Thickness, TArray<FDynamicMesh3>& Segments, TArray<FDynamicMesh3>& Cavities, FDynamicMesh3& Shell, FString& Error,
	TArray<FMiningMapRegion>* MapRegions)
{
	TArray<FSpaceGeometry> Geometry;
	const auto Fail = [&Error](AMiningLayoutSpace* Space, const TCHAR* Reason)
	{
		Error = FString::Printf(TEXT("NOT APPLIED [%s]: %s. Previous geometry retained."), *Space->SpaceName, Reason);
		Space->SpaceStatus = Error;
		return false;
	};
	for (AMiningLayoutSpace* Space : Spaces)
	{
		Space->SpaceStatus = Space->bEnabled ? TEXT("Pending layout validation") : TEXT("Disabled; not part of terrain");
		Space->ResolvedWallSize = FVector::ZeroVector;
	}
	for (AMiningLayoutSpace* Space : Spaces)
	{
		if (!Space->bEnabled) continue;
		if (Geometry.Num() >= 64) return Fail(Space, TEXT("At most 64 enabled spaces per layout"));
		const int32 Segment = Rooms.IndexOfByKey(Space->FromRoom.Get());
		if (!Segments.IsValidIndex(Segment) || !Plans.IsValidIndex(Segment)) return Fail(Space, TEXT("Choose the START room of an existing tunnel"));
		const FVector Scale = Space->GetActorScale3D();
		const FVector Size = Space->SpaceSize * Scale;
		if (Space->GetActorTransform().ContainsNaN() || Space->SpaceSize.ContainsNaN() || Scale.GetMin() <= 0 ||
			Size.GetMin() < 250 || Size.GetMax() > 10000 || !Space->GetActorRotation().IsNearlyZero(0.01))
			return Fail(Space, TEXT("Use positive scale, no rotation, and world dimensions 250-10000 cm"));
		const FBox Bounds = Space->GetSpaceBounds().ShiftBy(-Origin);
		if (MapRegions)
		{
			FMiningMapRegion Region = MiningDeveloperMap::Box(Space->SpaceName, EMiningMapRegionKind::Space, Segment, Bounds.ShiftBy(Origin));
			Region.bEllipsoid = Space->Shape == EMiningSpaceShape::Ellipsoid;
			Region.bRockFilled = Space->Contents == EMiningSpaceContents::DiggableRock;
			MapRegions->Add(MoveTemp(Region));
		}
		FSpaceGeometry Item;
		Item.Marker = Space;
		Item.Segment = Segment;
		Item.Interior = ShapeMesh(Space->Shape, Bounds);
		Item.Exterior = ShapeMesh(Space->Shape, Bounds.ExpandBy(Thickness));
		if (Space->Contents == EMiningSpaceContents::Empty) Item.Empty = Item.Interior;
		if (Space->Connection == EMiningSpaceConnection::StraightBranch)
		{
			if (!FMath::IsFinite(Space->BranchWidth) || !FMath::IsFinite(Space->BranchHeight) || Space->BranchWidth < 200 || Space->BranchWidth > 1200 ||
				Space->BranchHeight < 250 || Space->BranchHeight > 1200 || FMath::Max(Space->BranchWidth, Space->BranchHeight) > Size.GetMin() * 0.8)
				return Fail(Space, TEXT("Branch must be 200-1200 cm wide, 250-1200 cm high, and fit inside the space"));
			const MiningRoutes::FPlan& Plan = Plans[Segment];
			if (!MiningRoutes::BranchAnchor(Plan, Bounds.GetCenter(), Space->bAlignBranchFloors, Space->BranchHeight, Item.Anchor))
				return Fail(Space, TEXT("Tunnel has no valid cross-sections for the branch"));
			FVector Destination = Bounds.GetCenter();
			if (Space->bAlignBranchFloors)
			{
				Destination.Z = Bounds.Min.Z + Space->BranchHeight * 0.5;
			}
			const FVector Delta = Destination - Item.Anchor;
			const double Length = Delta.Size();
			if (Length < 100 || Length > 10000) return Fail(Space, TEXT("Straight branch length must be 100-10000 cm; use Overlap for an inline space"));
			if (Space->bAlignBranchFloors && Delta.Size2D() < 100) return Fail(Space, TEXT("Floor-aligned branches need at least 100 cm of horizontal travel"));
			const FVector Direction = Delta / Length;
			Item.Rotation = FRotationMatrix::MakeFromX(Direction).ToQuat();
			const FDynamicMesh3 Cave = Item.Interior;
			FDynamicMesh3 Branch = BranchMesh(Item.Anchor, Destination, Space->BranchWidth, Space->BranchHeight, 5, 5, Space->bAlignBranchFloors);
			if (MapRegions)
			{
				TArray<FVector> Vertices;
				for (int32 Vertex : Branch.VertexIndicesItr()) Vertices.Add(FVector(Branch.GetVertex(Vertex)) + Origin);
				FMiningMapRegion Region = MiningDeveloperMap::Convex(TEXT("Branch: ") + Space->SpaceName, EMiningMapRegionKind::Branch, Segment, Vertices);
				Region.bRockFilled = Space->Entrance == EMiningSpaceEntrance::RockFilled;
				MapRegions->Add(MoveTemp(Region));
			}
			const double VerticalThickness = Space->bAlignBranchFloors ? Thickness * Length / Delta.Size2D() : Thickness;
			if (!CombineSpaceGeometry(Item.Interior, Branch, FMeshBoolean::EBooleanOp::Union) ||
				!CombineSpaceGeometry(Item.Exterior, BranchMesh(Item.Anchor, Destination, Space->BranchWidth + Thickness * 2, Space->BranchHeight + VerticalThickness * 2, 5 + Thickness, 5 + Thickness, Space->bAlignBranchFloors), FMeshBoolean::EBooleanOp::Union))
				return Fail(Space, TEXT("Could not merge branch and space; adjust position or dimensions"));
			if (Space->Entrance == EMiningSpaceEntrance::Open && !CombineSpaceGeometry(Item.Empty, Branch, FMeshBoolean::EBooleanOp::Union))
				return Fail(Space, TEXT("Could not open branch"));
			if (Space->Entrance == EMiningSpaceEntrance::HiddenWall)
			{
				if (Space->Contents != EMiningSpaceContents::Empty || !FMath::IsFinite(Space->EntranceRockThickness) || Space->EntranceRockThickness < 30 || Space->EntranceRockThickness > 500)
					return Fail(Space, TEXT("Hidden Wall requires an empty cave and rock thickness 30-500 cm"));
				if (Intersects(Item.Empty, Cavities[Rooms.Num() + Segment]) != 0)
					return Fail(Space, TEXT("Hidden cave overlaps the main tunnel; move it farther to the side"));
				const double Entry = EntryDistance(Cave, Item.Anchor, Direction);
				const double OpenLength = Entry - Space->EntranceRockThickness;
				if (OpenLength < 100 || Entry > Length) return Fail(Space, TEXT("Not enough distance for the hidden entrance wall"));
				Item.WallCenter = Item.Anchor + Direction * (OpenLength + Space->EntranceRockThickness * 0.5);
				Item.WallSize = FVector(Space->EntranceRockThickness, Space->BranchWidth, Space->BranchHeight);
				Item.Wall = BranchMesh(Item.Anchor + Direction * OpenLength, Item.Anchor + Direction * Entry, Space->BranchWidth, Space->BranchHeight, 0, 0, Space->bAlignBranchFloors);
				Item.SeparateEmptyParts.Add(Cave);
				Item.SeparateEmptyParts.Add(BranchMesh(Item.Anchor, Item.Anchor + Direction * OpenLength, Space->BranchWidth, Space->BranchHeight, 5, 0, Space->bAlignBranchFloors));
				if (!CombineSpaceGeometry(Item.Empty, Item.SeparateEmptyParts.Last(), FMeshBoolean::EBooleanOp::Union))
					return Fail(Space, TEXT("Could not create hidden entrance"));
			}
		}
		for (const FBox& Room : RoomBounds)
		{
			if (Intersects(Item.Exterior, BoxMesh(Room.GetCenter(), Room.GetSize())) != 0)
				return Fail(Space, TEXT("Space or branch reaches a main room; leave clearance to preserve room progression"));
		}
		for (int32 Other = 0; Other < Segments.Num(); ++Other)
		{
			if (Other != Segment && Intersects(Item.Exterior, Cavities[Rooms.Num() + Other]) != 0)
				return Fail(Space, TEXT("Space or branch crosses a different tunnel; move or resize it"));
		}
		Geometry.Add(MoveTemp(Item));
		if (MapRegions && Geometry.Last().Wall.TriangleCount())
		{
			const FDynamicMesh3& Wall = Geometry.Last().Wall;
			TArray<FVector> Vertices;
			for (int32 Vertex : Wall.VertexIndicesItr()) Vertices.Add(FVector(Wall.GetVertex(Vertex)) + Origin);
			MapRegions->Add(MiningDeveloperMap::Convex(TEXT("Hidden entrance: ") + Space->SpaceName, EMiningMapRegionKind::HiddenEntrance, Segment, Vertices));
		}
	}
	TSet<int32> Connected;
	for (int32 Pass = 0; Pass < Geometry.Num(); ++Pass)
	{
		for (int32 Index = 0; Index < Geometry.Num(); ++Index)
		{
			if (Connected.Contains(Index)) continue;
			const FSpaceGeometry& Item = Geometry[Index];
			if (Intersects(Item.Interior, Cavities[Rooms.Num() + Item.Segment]) == 1) Connected.Add(Index);
			else
			{
				bool bConnected = false;
				for (int32 Other : Connected)
				{
					if (Geometry[Other].Segment == Item.Segment && Intersects(Item.Interior, Geometry[Other].Interior) == 1) { bConnected = true; break; }
				}
				if (bConnected) Connected.Add(Index);
			}
		}
	}
	for (int32 Index = 0; Index < Geometry.Num(); ++Index)
	{
		const FSpaceGeometry& Item = Geometry[Index];
		if (!Connected.Contains(Index)) return Fail(Item.Marker, TEXT("Disconnected space: overlap the tunnel/another connected space, or choose Straight Branch"));
		for (int32 Other = 0; Other < Geometry.Num(); ++Other)
		{
			if (Other == Index) continue;
			if (Item.Segment != Geometry[Other].Segment && Intersects(Item.Exterior, Geometry[Other].Exterior) != 0)
				return Fail(Item.Marker, TEXT("Spaces belonging to different tunnels overlap"));
			if (Item.Wall.TriangleCount() && Intersects(Item.Wall, Geometry[Other].Empty) != 0)
				return Fail(Item.Marker, TEXT("Another empty space would cut through the hidden entrance wall"));
		}
		const bool bCarveOnly = Item.Marker->Contents == EMiningSpaceContents::Empty &&
			(Item.Marker->Connection == EMiningSpaceConnection::Overlap || Item.Marker->Entrance != EMiningSpaceEntrance::RockFilled);
		if (!CombineSpaceGeometry(Shell, Item.Exterior, FMeshBoolean::EBooleanOp::Union) ||
			(!bCarveOnly && !CombineSpaceGeometry(Segments[Item.Segment], Item.Interior, FMeshBoolean::EBooleanOp::Union)))
			return Fail(Item.Marker, TEXT("Space terrain union failed; avoid exactly touching surfaces"));
		Cavities.Add(Item.Interior);
	}
	for (const FSpaceGeometry& Item : Geometry)
	{
		if (!CombineSpaceGeometry(Segments[Item.Segment], Item.Empty, FMeshBoolean::EBooleanOp::Difference))
		{
			if (Item.SeparateEmptyParts.IsEmpty()) return Fail(Item.Marker, TEXT("Space carving failed; adjust dimensions slightly"));
			FDynamicMesh3 Candidate = Segments[Item.Segment];
			for (const FDynamicMesh3& Part : Item.SeparateEmptyParts)
			{
				if (!CombineSpaceGeometry(Candidate, Part, FMeshBoolean::EBooleanOp::Difference)) return Fail(Item.Marker, TEXT("Hidden entrance carving failed; adjust dimensions slightly"));
			}
			Segments[Item.Segment] = MoveTemp(Candidate);
		}
	}
	for (const FSpaceGeometry& Item : Geometry)
	{
		if (Item.Wall.TriangleCount() && !CombineSpaceGeometry(Segments[Item.Segment], Item.Wall, FMeshBoolean::EBooleanOp::Union))
			return Fail(Item.Marker, TEXT("Could not retain hidden entrance rock"));
		Item.Marker->ResolvedBranchAnchor = Item.Anchor + Origin;
		Item.Marker->ResolvedWallCenter = Item.WallCenter + Origin;
		Item.Marker->ResolvedWallSize = Item.WallSize;
		Item.Marker->ResolvedWallRotation = Item.Rotation;
		Item.Marker->SpaceStatus = FString::Printf(TEXT("Prepared for tunnel %d -> %d; confirm generator Ready status"), Rooms[Item.Segment]->ConnectionOrder, Rooms[Item.Segment + 1]->ConnectionOrder);
	}
	return true;
}
