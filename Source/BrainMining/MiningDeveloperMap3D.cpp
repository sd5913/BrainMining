#include "MiningDeveloperMapData.h"

FVector MiningDeveloperMap::Project3D(const FVector& Point, const FRotator& Rotation)
{
	const FRotationMatrix Basis(Rotation);
	return FVector(FVector::DotProduct(Point, Basis.GetUnitAxis(EAxis::Y)), -FVector::DotProduct(Point, Basis.GetUnitAxis(EAxis::Z)), FVector::DotProduct(Point, Basis.GetUnitAxis(EAxis::X)));
}

bool MiningDeveloperMap::FSurfaceEdge::Visible(const FVector& ViewDirection) const
{
	return !bShared || FVector::DotProduct(FirstNormal, SecondNormal) < 0.7
		|| FVector::DotProduct(FirstNormal, ViewDirection) * FVector::DotProduct(SecondNormal, ViewDirection) < 0;
}

MiningDeveloperMap::FSurface MiningDeveloperMap::BuildSurface(const FMiningMapRegion& Region)
{
	FSurface Surface;
	if (!Region.Bounds.IsValid || Region.Bounds.Min.ContainsNaN() || Region.Bounds.Max.ContainsNaN()) return Surface;
	if (!Region.SurfaceTriangles.IsEmpty())
	{
		Surface.Vertices = Region.Vertices;
		Surface.Triangles = Region.SurfaceTriangles;
	}
	else if (!Region.PolygonXZ.IsEmpty() && !Region.PolygonTriangles.IsEmpty() && Region.Vertices.Num() == Region.PolygonXZ.Num() * 2)
	{
		Surface.Vertices = Region.Vertices;
		const int32 Count = Region.PolygonXZ.Num();
		for (const FIntVector& Triangle : Region.PolygonTriangles)
		{
			if (Triangle.GetMin() < 0 || Triangle.GetMax() >= Count) continue;
			Surface.Triangles.Add(Triangle);
			Surface.Triangles.Add(FIntVector(Triangle.Z + Count, Triangle.Y + Count, Triangle.X + Count));
		}
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 Next = (Index + 1) % Count;
			Surface.Triangles.Add(FIntVector(Index, Next + Count, Next));
			Surface.Triangles.Add(FIntVector(Index, Index + Count, Next + Count));
		}
	}
	else if (Region.bEllipsoid)
	{
		const FVector Center = Region.Bounds.GetCenter(), Radius = Region.Bounds.GetExtent();
		constexpr int32 Slices = 16, Stacks = 8;
		Surface.Vertices.Add(Center + FVector(0, 0, Radius.Z));
		for (int32 Stack = 1; Stack < Stacks; ++Stack)
		{
			const double Latitude = PI * Stack / Stacks;
			for (int32 Slice = 0; Slice < Slices; ++Slice)
			{
				const double Longitude = 2 * PI * Slice / Slices;
				Surface.Vertices.Add(Center + Radius * FVector(FMath::Sin(Latitude) * FMath::Cos(Longitude), FMath::Sin(Latitude) * FMath::Sin(Longitude), FMath::Cos(Latitude)));
			}
		}
		const int32 Bottom = Surface.Vertices.Add(Center - FVector(0, 0, Radius.Z));
		for (int32 Slice = 0; Slice < Slices; ++Slice)
		{
			const int32 Next = (Slice + 1) % Slices;
			Surface.Triangles.Add(FIntVector(0, 1 + Slice, 1 + Next));
			Surface.Triangles.Add(FIntVector(Bottom, 1 + (Stacks - 2) * Slices + Next, 1 + (Stacks - 2) * Slices + Slice));
			for (int32 Stack = 0; Stack < Stacks - 2; ++Stack)
			{
				const int32 First = 1 + Stack * Slices + Slice, Second = 1 + Stack * Slices + Next;
				Surface.Triangles.Add(FIntVector(First, First + Slices, Second));
				Surface.Triangles.Add(FIntVector(Second, First + Slices, Second + Slices));
			}
		}
	}
	else if (!Region.Planes.IsEmpty())
	{
		Surface.Vertices = Region.Vertices;
		for (const FPlane& Plane : Region.Planes)
		{
			FVector Right, Up;
			const FVector Normal(Plane.X, Plane.Y, Plane.Z);
			Normal.FindBestAxisVectors(Right, Up);
			TArray<FVector2D> FacePoints;
			TArray<int32> FaceIndices;
			const FVector Origin = Region.Bounds.GetCenter();
			for (int32 Index = 0; Index < Surface.Vertices.Num(); ++Index)
			{
				const FVector& Vertex = Surface.Vertices[Index];
				if (Vertex.ContainsNaN() || FMath::Abs(Plane.PlaneDot(Vertex)) > 0.2) continue;
				FacePoints.Add(FVector2D(FVector::DotProduct(Vertex - Origin, Right), FVector::DotProduct(Vertex - Origin, Up)));
				FaceIndices.Add(Index);
			}
			const TArray<FVector2D> Outline = Hull(FacePoints);
			TArray<int32> Ordered;
			for (const FVector2D& Point : Outline)
			{
				const int32 Found = FacePoints.IndexOfByPredicate([&Point](const FVector2D& Candidate) { return Candidate.Equals(Point, 0.001); });
				if (Found != INDEX_NONE) Ordered.Add(FaceIndices[Found]);
			}
			for (int32 Index = 1; Index + 1 < Ordered.Num(); ++Index)
			{
				FIntVector Triangle(Ordered[0], Ordered[Index], Ordered[Index + 1]);
				if (FVector::DotProduct(FVector::CrossProduct(Surface.Vertices[Triangle.Y] - Surface.Vertices[Triangle.X], Surface.Vertices[Triangle.Z] - Surface.Vertices[Triangle.X]), Normal) < 0) Swap(Triangle.Y, Triangle.Z);
				Surface.Triangles.Add(Triangle);
			}
		}
	}
	else
	{
		const FMiningMapRegion Fallback = Box(Region.Name, Region.Kind, Region.GroupIndex, Region.Bounds);
		if (Fallback.Planes.IsEmpty()) return Surface;
		Surface = BuildSurface(Fallback);
		Surface.bBoundsFallback = true;
		return Surface;
	}
	Surface.Triangles.RemoveAll([&Surface](const FIntVector& Triangle)
	{
		if (!Surface.Vertices.IsValidIndex(Triangle.X) || !Surface.Vertices.IsValidIndex(Triangle.Y) || !Surface.Vertices.IsValidIndex(Triangle.Z)) return true;
		const FVector& First = Surface.Vertices[Triangle.X];
		const FVector& Second = Surface.Vertices[Triangle.Y];
		const FVector& Third = Surface.Vertices[Triangle.Z];
		return First.ContainsNaN() || Second.ContainsNaN() || Third.ContainsNaN() || FVector::CrossProduct(Second - First, Third - First).IsNearlyZero();
	});
	TMap<FIntPoint, int32> EdgeIndices;
	for (const FIntVector& Triangle : Surface.Triangles)
	{
		const FVector Normal = FVector::CrossProduct(Surface.Vertices[Triangle.Y] - Surface.Vertices[Triangle.X], Surface.Vertices[Triangle.Z] - Surface.Vertices[Triangle.X]).GetSafeNormal();
		for (int32 Side = 0; Side < 3; ++Side)
		{
			const int32 First = Triangle[Side], Second = Triangle[(Side + 1) % 3];
			const FIntPoint Key(FMath::Min(First, Second), FMath::Max(First, Second));
			if (const int32* Found = EdgeIndices.Find(Key))
			{
				Surface.Edges[*Found].SecondNormal = Normal;
				Surface.Edges[*Found].bShared = true;
			}
			else
			{
				FSurfaceEdge Edge;
				Edge.Vertices = Key; Edge.FirstNormal = Normal;
				EdgeIndices.Add(Key, Surface.Edges.Add(Edge));
			}
		}
	}
	return Surface;
}
