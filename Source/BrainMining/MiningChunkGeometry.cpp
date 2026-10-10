#include "MiningChunkGeometry.h"
#include "BrainMining.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "MeshConstraints.h"
#include "MeshSimplification.h"
#include "Operations/MeshBoolean.h"
#include "ProjectionTargets.h"

using namespace UE::Geometry;

namespace
{
	bool Boolean(const FDynamicMesh3& Source, const FDynamicMesh3& Tool, FMeshBoolean::EBooleanOp Mode, FDynamicMesh3& Result)
	{
		FDynamicMesh3 First = Source;
		FDynamicMesh3 Second = Tool;
		First.DiscardAttributes();
		Second.DiscardAttributes();
		if (!First.HasTriangleGroups()) First.EnableTriangleGroups(0);
		if (!Second.HasTriangleGroups()) Second.EnableTriangleGroups(0);
		// SnapTolerance is measured after FMeshBoolean normalizes both meshes.
		// Fine first passes preserve short edges in large external mining regions.
		const double Tolerances[] = {0.00000001, 0.0000001, 0.000001, 0.00001, 0.0001, 0.001};
		for (int32 Attempt = 0; Attempt < 12; ++Attempt)
		{
			Result.Clear();
			FMeshBoolean Operation(&First, &Second, &Result, Mode);
			Operation.bSimplifyAlongNewEdges = Attempt % 2 == 0;
			Operation.bPreserveTriangleGroups = false;
			Operation.SnapTolerance = Tolerances[Attempt / 2];
			const bool bComputed = Operation.Compute();
			if (bComputed && Operation.CreatedBoundaryEdges.IsEmpty() &&
				(Result.TriangleCount() == 0 || Result.IsClosed()) &&
				Result.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly)) return true;
		}
		return false;
	}

	bool WithinEnvelope(const FDynamicMesh3& Source, const FDynamicMesh3& Target, double Tolerance)
	{
		FDynamicMeshAABBTree3 Tree(&Target, true);
		const double Limit = FMath::Square(Tolerance + 0.00001);
		const auto Near = [&Tree, Limit](const FVector3d& Point)
		{
			double DistanceSquared;
			return Tree.FindNearestTriangle(Point, DistanceSquared) != INDEX_NONE && DistanceSquared <= Limit;
		};
		for (int32 Vertex : Source.VertexIndicesItr())
		{
			if (!Near(Source.GetVertex(Vertex))) return false;
		}
		for (int32 Triangle : Source.TriangleIndicesItr())
		{
			FVector3d First, Second, Third;
			Source.GetTriVertices(Triangle, First, Second, Third);
			if (!Near((First + Second + Third) / 3) || !Near((First + Second) / 2) ||
				!Near((Second + Third) / 2) || !Near((Third + First) / 2)) return false;
		}
		return true;
	}
}

bool MiningChunks::Split(const FDynamicMesh3& Source, double Length, TArray<FDynamicMesh3>& Meshes, TArray<FBox>& Bounds)
{
	Meshes.Reset();
	Bounds.Reset();
	if (Source.TriangleCount() == 0) return true;
	const FAxisAlignedBox3d SourceBounds = Source.GetBounds();
	const FVector Size(SourceBounds.Max - SourceBounds.Min);
	const int32 Axis = Size.X >= Size.Y && Size.X >= Size.Z ? 0 : Size.Y >= Size.Z ? 1 : 2;
	const int32 Count = FMath::CeilToInt(Size[Axis] / Length);
	if (Count < 1 || Count > 128) return false;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FBox Cell(FVector(SourceBounds.Min) - FVector(1), FVector(SourceBounds.Max) + FVector(1));
		Cell.Min[Axis] = Index == 0 ? SourceBounds.Min[Axis] - 1 : SourceBounds.Min[Axis] + Size[Axis] * Index / Count;
		Cell.Max[Axis] = Index == Count - 1 ? SourceBounds.Max[Axis] + 1 : SourceBounds.Min[Axis] + Size[Axis] * (Index + 1) / Count;
		FDynamicMesh3 Piece;
		if (Count == 1) Piece = Source;
		else
		{
			FMinimalBoxMeshGenerator Generator;
			Generator.Box = FOrientedBox3d(FVector3d(Cell.GetCenter()), FVector3d(Cell.GetExtent()));
			Generator.Generate();
			const FDynamicMesh3 Tool(&Generator);
			if (!Boolean(Source, Tool, FMeshBoolean::EBooleanOp::Intersect, Piece)) return false;
		}
		if (Piece.TriangleCount() == 0) continue;
		Piece.CompactInPlace();
		const FAxisAlignedBox3d PieceBounds = Piece.GetBounds();
		Bounds.Add(FBox(FVector(PieceBounds.Min), FVector(PieceBounds.Max)));
		Normals(Piece);
		Meshes.Add(MoveTemp(Piece));
	}
	return !Meshes.IsEmpty();
}

bool MiningChunks::SpatialChunkCounts(const FVector& Size, double Length, FIntVector& Counts)
{
	if (!FMath::IsFinite(Length) || Length < 400 || Length > 3000 || Size.ContainsNaN() ||
		Size.GetMin() <= 0 || Size.GetMax() > Length * MaxSpatialChunks) return false;
	const FIntVector Candidate(FMath::CeilToInt(Size.X / Length), FMath::CeilToInt(Size.Y / Length), FMath::CeilToInt(Size.Z / Length));
	if (static_cast<int64>(Candidate.X) * Candidate.Y * Candidate.Z > MaxSpatialChunks) return false;
	Counts = Candidate;
	return true;
}

bool MiningChunks::SplitSpatial(const FDynamicMesh3& Source, double Length, TArray<FDynamicMesh3>& Meshes, TArray<FBox>& Bounds)
{
	if (!FMath::IsFinite(Length) || Length < 400 || Length > 3000) return false;
	TArray<FDynamicMesh3> Pieces;
	TArray<FBox> PieceBounds;
	if (Source.TriangleCount() == 0) { Meshes.Reset(); Bounds.Reset(); return true; }
	const FAxisAlignedBox3d SourceBounds = Source.GetBounds();
	const FVector Minimum(SourceBounds.Min);
	const FVector Size(SourceBounds.Max - SourceBounds.Min);
	FIntVector Counts;
	if (Minimum.ContainsNaN() || !SpatialChunkCounts(Size, Length, Counts)) return false;
	Pieces.Reserve(Counts.X * Counts.Y * Counts.Z);
	PieceBounds.Reserve(Counts.X * Counts.Y * Counts.Z);
	for (int32 XIndex = 0; XIndex < Counts.X; ++XIndex)
	for (int32 YIndex = 0; YIndex < Counts.Y; ++YIndex)
	for (int32 ZIndex = 0; ZIndex < Counts.Z; ++ZIndex)
	{
		const FIntVector Indices(XIndex, YIndex, ZIndex);
		FBox Cell(ForceInit);
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Cell.Min[Axis] = Minimum[Axis] + Size[Axis] * Indices[Axis] / Counts[Axis] - (Indices[Axis] == 0 ? 1 : 0);
			Cell.Max[Axis] = Minimum[Axis] + Size[Axis] * (Indices[Axis] + 1) / Counts[Axis] + (Indices[Axis] == Counts[Axis] - 1 ? 1 : 0);
		}
		FMinimalBoxMeshGenerator Generator;
		Generator.Box = FOrientedBox3d(FVector3d(Cell.GetCenter()), FVector3d((Cell.Max - Cell.Min) * 0.5));
		Generator.Generate();
		FDynamicMesh3 Piece;
		const FDynamicMesh3 Tool(&Generator);
		if (!Boolean(Source, Tool, FMeshBoolean::EBooleanOp::Intersect, Piece))
		{
			UE_LOG(LogBrainMining, Warning, TEXT("Spatial boolean failed at %d,%d,%d / %d,%d,%d: source %d closed %d, result %d closed %d valid %d"), XIndex,YIndex,ZIndex,Counts.X,Counts.Y,Counts.Z,Source.TriangleCount(),Source.IsClosed(),Piece.TriangleCount(),Piece.IsClosed(),Piece.CheckValidity(FDynamicMesh3::FValidityOptions(),EValidityCheckFailMode::ReturnOnly));
			return false;
		}
		if (Piece.TriangleCount() == 0) continue;
		Piece.CompactInPlace();
		const FAxisAlignedBox3d ActualBounds = Piece.GetBounds();
		PieceBounds.Add(FBox(FVector(ActualBounds.Min), FVector(ActualBounds.Max)));
		Normals(Piece);
		Pieces.Add(MoveTemp(Piece));
	}
	if (Pieces.IsEmpty()) return false;
	Meshes = MoveTemp(Pieces);
	Bounds = MoveTemp(PieceBounds);
	return true;
}

bool MiningChunks::Subtract(const FDynamicMesh3& Source, const FDynamicMesh3& Cutter, FDynamicMesh3& Result)
{
	return Boolean(Source, Cutter, FMeshBoolean::EBooleanOp::Difference, Result);
}

int32 MiningChunks::Simplify(FDynamicMesh3& Mesh, const FDynamicMesh3& Initial, const FBox& Bounds, double Tolerance)
{
	if (Mesh.TriangleCount() < 128 || Tolerance <= 0) return 0;
	FDynamicMesh3 Simplified = Mesh;
	Simplified.DiscardAttributes();
	FDynamicMeshAABBTree3 SourceTree(&Mesh, true);
	FDynamicMeshAABBTree3 InitialTree(&Initial, true);
	FMeshProjectionTarget Projection(&Mesh, &SourceTree);
	FMeshConstraints Constraints;
	TSet<int32> Protected;
	const double Guard = FMath::Max(1.0, Tolerance * 4);
	for (int32 Vertex : Mesh.VertexIndicesItr())
	{
		const FVector3d Position = Mesh.GetVertex(Vertex);
		bool bProtect = false;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			bProtect |= FMath::Abs(Position[Axis] - Bounds.Min[Axis]) < Guard || FMath::Abs(Position[Axis] - Bounds.Max[Axis]) < Guard;
		}
		double DistanceSquared;
		InitialTree.FindNearestTriangle(Position, DistanceSquared);
		if (bProtect || DistanceSquared < Guard * Guard) Protected.Add(Vertex);
	}
	for (int32 Edge : Mesh.EdgeIndicesItr())
	{
		const FIndex2i Triangles = Mesh.GetEdgeT(Edge);
		if (Triangles.B == INDEX_NONE || Mesh.GetTriNormal(Triangles.A).Dot(Mesh.GetTriNormal(Triangles.B)) < 0.8)
		{
			const FIndex2i Vertices = Mesh.GetEdgeV(Edge);
			Protected.Add(Vertices.A);
			Protected.Add(Vertices.B);
		}
	}
	for (int32 Triangle : Mesh.TriangleIndicesItr())
	{
		const FVector3d Center = Mesh.GetTriCentroid(Triangle);
		const FVector3d Normal = Mesh.GetTriNormal(Triangle);
		for (double Direction : {-1.0, 1.0})
		{
			double Distance;
			int32 Hit;
			if (SourceTree.FindNearestHitTriangle(FRay3d(Center + Normal * (Direction * 0.01), Normal * Direction), Distance, Hit) && Distance < Guard * 4)
			{
				const FIndex3i Vertices = Mesh.GetTriangle(Triangle);
				Protected.Add(Vertices.A);
				Protected.Add(Vertices.B);
				Protected.Add(Vertices.C);
			}
		}
	}
	for (int32 Vertex : Protected) Constraints.SetOrUpdateVertexConstraint(Vertex, FVertexConstraint(true, false));
	FQEMSimplification Simplifier(&Simplified);
	Simplifier.SetExternalConstraints(MoveTemp(Constraints));
	Simplifier.AllowCollapseFixedVertsWithSameSetID = false;
	Simplifier.SetProjectionTarget(&Projection);
	Simplifier.GeometricErrorConstraint = FQEMSimplification::EGeometricErrorCriteria::PredictedPointToProjectionTarget;
	Simplifier.GeometricErrorTolerance = Tolerance * 0.2;
	Simplifier.MaxResultTriangleCount = 0;
	Simplifier.bPreventTinyTriangles = true;
	Simplifier.SimplifyToTriangleCount(FMath::Max(12, Mesh.TriangleCount() * 3 / 4));
	const int32 Removed = Mesh.TriangleCount() - Simplified.TriangleCount();
	UE_LOG(LogBrainMining, Verbose, TEXT("Chunk simplify: input=%d protected=%d/%d candidate_reduction=%d"), Mesh.TriangleCount(), Protected.Num(), Mesh.VertexCount(), Removed);
	if (Removed <= 0 || !Simplified.IsClosed() || !Simplified.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly)) return 0;
	for (int32 Vertex : Protected)
	{
		if (!Simplified.IsVertex(Vertex) || Simplified.GetVertex(Vertex) != Mesh.GetVertex(Vertex)) return 0;
	}
	if (!WithinEnvelope(Mesh, Simplified, Tolerance) || !WithinEnvelope(Simplified, Mesh, Tolerance))
	{
		UE_LOG(LogBrainMining, Verbose, TEXT("Chunk simplification discarded: sampled envelope exceeded"));
		return 0;
	}
	Simplified.CompactInPlace();
	Mesh = MoveTemp(Simplified);
	return Removed;
}

void MiningChunks::Normals(FDynamicMesh3& Mesh)
{
	if (Mesh.TriangleCount() == 0) return;
	Mesh.EnableAttributes();
	FMeshNormals::InitializeOverlayToPerTriangleNormals(Mesh.Attributes()->PrimaryNormals());
}
