#include "MiningSpatialEnvelope.h"
#include "MiningChunkGeometry.h"
#include "CompGeom/ConvexHull3.h"

using namespace UE::Geometry;

namespace
{
	bool ExpandedHull(const TArray<FVector>& Points, double Margin, FDynamicMesh3& Mesh)
	{
		TArray<FVector> Expanded;
		Expanded.Reserve(Points.Num() * 8);
		for (const FVector& Point : Points)
			for (int32 Corner = 0; Corner < 8; ++Corner)
				Expanded.Add(Point + FVector(Corner & 1 ? Margin : -Margin, Corner & 2 ? Margin : -Margin, Corner & 4 ? Margin : -Margin));
		FConvexHull3d Hull;
		Hull.SimplificationSettings.DegenerateEdgeTolerance = 0.000001;
		Hull.SimplificationSettings.SkipAtHullDistanceAbsolute = 0.000001;
		if (!Hull.Solve(Expanded.Num(), [&](int32 Index) { return Expanded[Index]; })) return false;
		Mesh.Clear(); TMap<int32, int32> Vertices;
		for (const FIndex3i& Face : Hull.GetTriangles())
		{
			FIndex3i Mapped;
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 Index = Face[Corner];
				if (!Vertices.Contains(Index)) Vertices.Add(Index, Mesh.AppendVertex(Expanded[Index]));
				Mapped[Corner] = Vertices[Index];
			}
			if (Mesh.AppendTriangle(Mapped) < 0) return false;
		}
		double Volume = 0;
		for (int32 Triangle : Mesh.TriangleIndicesItr())
		{
			FVector3d First, Second, Third; Mesh.GetTriVertices(Triangle, First, Second, Third);
			Volume += First.Dot(Second.Cross(Third));
		}
		if (Volume > 0) Mesh.ReverseOrientation();
		return Mesh.IsClosed() && Mesh.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly);
	}
	bool AddHull(const TArray<FVector>& Points, double Margin, FDynamicMesh3& Mesh, FString& Error)
	{
		FDynamicMesh3 Piece;
		if (!ExpandedHull(Points, Margin, Piece)) { Error = TEXT("外围凸包构建失败"); return false; }
		if (Mesh.TriangleCount() == 0) { Mesh = MoveTemp(Piece); return true; }
		FDynamicMesh3 Combined;
		if (!MiningChunks::Union(Mesh, Piece, Combined, true)) { Error = TEXT("外围凸包合并失败"); return false; }
		Mesh = MoveTemp(Combined); return true;
	}
}

bool MiningSpatial::AddEnvelope(const FDynamicMesh3& Shape, const TArray<FRing>& Rings, const FVector& Origin, double Margin, double Thickness, FDynamicMesh3& Domain, FDynamicMesh3& Outer, FString& Error)
{
	const auto Add = [&](const TArray<FVector>& Points)
	{
		if (AddHull(Points, Margin, Domain, Error) && AddHull(Points, Margin + Thickness, Outer, Error)) return true;
		Error += TEXT("；设计已保留，请检查极小细缝或重合尖角。"); return false;
	};
	if (Rings.IsEmpty())
	{
		TArray<FVector> Points;
		for (int32 Vertex : Shape.VertexIndicesItr()) Points.Add(FVector(Shape.GetVertex(Vertex)) - Origin);
		return Add(Points);
	}
	for (int32 Start = 0; Start < Rings.Num() - 1;)
	{
		int32 Finish = FMath::Min(Start + 4, Rings.Num() - 1);
		const auto Straight = [&](int32 Index)
		{
			const FRing& First = Rings[Start]; const FRing& Other = Rings[Index];
			if (!First.Right.Equals(Other.Right, 0.000001) || !First.Up.Equals(Other.Up, 0.000001) || First.Profile.Num() != Other.Profile.Num() || FVector::CrossProduct(Other.Center - First.Center, First.Tangent).Length() > 0.000001) return false;
			for (int32 Vertex = 0; Vertex < First.Profile.Num(); ++Vertex) if (!First.Profile[Vertex].Equals(Other.Profile[Vertex], 0.000001)) return false;
			return true;
		};
		bool bStraight = true;
		for (int32 Index = Start + 1; Index <= Finish; ++Index) bStraight &= Straight(Index);
		if (bStraight) while (Finish + 1 < Rings.Num() && Straight(Finish + 1)) ++Finish;
		TArray<FVector> Points;
		for (int32 Index = Start; Index <= Finish; Index += bStraight ? Finish - Start : 1)
			for (const FVector2D& Point : Rings[Index].Profile)
				Points.Add(Rings[Index].Center + Rings[Index].Right * Point.X + Rings[Index].Up * Point.Y - Origin);
		if (!Add(Points)) return false;
		Start = Finish;
	}
	return true;
}
