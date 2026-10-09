#include "MiningDeveloperMapData.h"
#include "MiningManualGeometry.h"

bool FMiningMapRegion::Contains(const FVector& Position) const
{
	if (!Bounds.IsValid || Position.ContainsNaN() || !Bounds.IsInsideOrOn(Position)) return false;
	if (!SurfaceTriangles.IsEmpty())
	{
		const FVector Direction = FVector(1, 0.3713907, 0.529117).GetSafeNormal();
		TArray<double> Hits;
		for (const FIntVector& Triangle : SurfaceTriangles)
		{
			if (!Vertices.IsValidIndex(Triangle.X) || !Vertices.IsValidIndex(Triangle.Y) || !Vertices.IsValidIndex(Triangle.Z)) continue;
			const FVector EdgeFirst = Vertices[Triangle.Y] - Vertices[Triangle.X], EdgeLast = Vertices[Triangle.Z] - Vertices[Triangle.X];
			const FVector Cross = FVector::CrossProduct(Direction, EdgeLast);
			const double Determinant = FVector::DotProduct(EdgeFirst, Cross);
			if (FMath::Abs(Determinant) < 1e-9) continue;
			const FVector Offset = Position - Vertices[Triangle.X];
			const double First = FVector::DotProduct(Offset, Cross) / Determinant;
			if (First < -1e-9 || First > 1 + 1e-9) continue;
			const FVector Other = FVector::CrossProduct(Offset, EdgeFirst);
			const double Last = FVector::DotProduct(Direction, Other) / Determinant;
			if (Last < -1e-9 || First + Last > 1 + 1e-9) continue;
			const double Distance = FVector::DotProduct(EdgeLast, Other) / Determinant;
			if (FMath::Abs(Distance) < 0.01) return true;
			if (Distance > 0 && !Hits.ContainsByPredicate([Distance](double Existing) { return FMath::Abs(Existing - Distance) < 0.0001; })) Hits.Add(Distance);
		}
		return Hits.Num() % 2 == 1;
	}
	if (!PolygonXZ.IsEmpty()) return MiningManual::Contains(PolygonXZ, FVector2D(Position.X, Position.Z));
	if (bEllipsoid)
	{
		const FVector Radius = Bounds.GetExtent().ComponentMax(FVector(1));
		return ((Position - Bounds.GetCenter()) / Radius).SizeSquared() <= 1.00001;
	}
	for (const FPlane& Plane : Planes)
	{
		if (Plane.PlaneDot(Position) > 0.1) return false;
	}
	return true;
}

FMiningMapRegion MiningDeveloperMap::Convex(const FString& Name, EMiningMapRegionKind Kind, int32 Group, const TArray<FVector>& Vertices)
{
	FMiningMapRegion Region;
	Region.Name = Name;
	Region.Kind = Kind;
	Region.GroupIndex = Group;
	Region.Vertices = Vertices;
	for (const FVector& Vertex : Vertices) Region.Bounds += Vertex;
	for (int32 First = 0; First < Vertices.Num(); ++First)
	{
		for (int32 Second = First + 1; Second < Vertices.Num(); ++Second)
		{
			for (int32 Third = Second + 1; Third < Vertices.Num(); ++Third)
			{
				FVector Normal = FVector::CrossProduct(Vertices[Second] - Vertices[First], Vertices[Third] - Vertices[First]).GetSafeNormal();
				if (Normal.IsNearlyZero()) continue;
				if (FVector::DotProduct(Normal, Region.Bounds.GetCenter() - Vertices[First]) > 0) Normal *= -1;
				const FPlane Candidate(Vertices[First], Normal);
				bool bSupporting = true;
				for (const FVector& Vertex : Vertices)
				{
					if (Candidate.PlaneDot(Vertex) > 0.1) { bSupporting = false; break; }
				}
				if (bSupporting && !Region.Planes.ContainsByPredicate([&Candidate](const FPlane& Plane) { return Plane.Equals(Candidate, 0.0001); })) Region.Planes.Add(Candidate);
			}
		}
	}
	return Region;
}

FMiningMapRegion MiningDeveloperMap::Box(const FString& Name, EMiningMapRegionKind Kind, int32 Group, const FBox& Bounds)
{
	TArray<FVector> Vertices;
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		Vertices.Add(FVector(Corner & 1 ? Bounds.Max.X : Bounds.Min.X, Corner & 2 ? Bounds.Max.Y : Bounds.Min.Y, Corner & 4 ? Bounds.Max.Z : Bounds.Min.Z));
	}
	return Convex(Name, Kind, Group, Vertices);
}

FVector2D MiningDeveloperMap::Project(const FVector& Point, EMiningMapView View)
{
	switch (View)
	{
	case EMiningMapView::Orbit3D: return FVector2D(Project3D(Point, FRotator(-30, -45, 0)));
	case EMiningMapView::TopXY: return FVector2D(Point.X, -Point.Y);
	case EMiningMapView::SideYZ: return FVector2D(Point.Y, -Point.Z);
	default: return FVector2D(Point.X, -Point.Z);
	}
}

TArray<FVector2D> MiningDeveloperMap::Hull(TArray<FVector2D> Points)
{
	Points.Sort([](const FVector2D& First, const FVector2D& Second) { return First.X == Second.X ? First.Y < Second.Y : First.X < Second.X; });
	TArray<FVector2D> Unique;
	for (const FVector2D& Point : Points)
	{
		if (Unique.IsEmpty() || !Point.Equals(Unique.Last(), 0.001)) Unique.Add(Point);
	}
	if (Unique.Num() < 3) return Unique;
	TArray<FVector2D> Result;
	for (const FVector2D& Point : Unique)
	{
		while (Result.Num() >= 2 && FVector2D::CrossProduct(Result.Last() - Result[Result.Num() - 2], Point - Result.Last()) <= 0) Result.Pop();
		Result.Add(Point);
	}
	const int32 LowerCount = Result.Num();
	for (int32 Index = Unique.Num() - 2; Index >= 0; --Index)
	{
		const FVector2D& Point = Unique[Index];
		while (Result.Num() > LowerCount && FVector2D::CrossProduct(Result.Last() - Result[Result.Num() - 2], Point - Result.Last()) <= 0) Result.Pop();
		Result.Add(Point);
	}
	Result.Pop();
	return Result;
}

TArray<FVector2D> MiningDeveloperMap::Outline(const FMiningMapRegion& Region, EMiningMapView View)
{
	TArray<FVector2D> Points;
	if (View == EMiningMapView::SideXZ && !Region.PolygonXZ.IsEmpty())
	{
		for (const FVector2D& Point : Region.PolygonXZ) Points.Add(FVector2D(Point.X, -Point.Y));
		return Points;
	}
	if (Region.bEllipsoid)
	{
		const FVector2D Center = Project(Region.Bounds.GetCenter(), View);
		const FVector2D Radius = Project(Region.Bounds.GetExtent(), View).GetAbs();
		for (int32 Step = 0; Step < 32; ++Step)
		{
			const double Angle = Step * 2 * PI / 32;
			Points.Add(Center + FVector2D(FMath::Cos(Angle) * Radius.X, FMath::Sin(Angle) * Radius.Y));
		}
		return Points;
	}
	for (const FVector& Vertex : Region.Vertices) Points.Add(Project(Vertex, View));
	return Hull(MoveTemp(Points));
}

TArray<FVector2D> MiningDeveloperMap::ClipPolygon(TArray<FVector2D> Points, const FBox2D& Clip)
{
	for (int32 Edge = 0; Edge < 4 && !Points.IsEmpty(); ++Edge)
	{
		const int32 Axis = Edge / 2;
		const double Sign = Edge % 2 == 0 ? 1 : -1;
		const double Limit = Edge % 2 == 0 ? Clip.Min[Axis] : Clip.Max[Axis];
		TArray<FVector2D> Output;
		FVector2D Previous = Points.Last();
		double PreviousDistance = Sign * (Previous[Axis] - Limit);
		for (const FVector2D& Current : Points)
		{
			const double Distance = Sign * (Current[Axis] - Limit);
			if ((Distance >= 0) != (PreviousDistance >= 0)) Output.Add(Previous + (Current - Previous) * (PreviousDistance / (PreviousDistance - Distance)));
			if (Distance >= 0) Output.Add(Current);
			Previous = Current;
			PreviousDistance = Distance;
		}
		Points = MoveTemp(Output);
	}
	return Points;
}

bool MiningDeveloperMap::ClipLine(FVector2D& Start, FVector2D& Finish, const FBox2D& Clip)
{
	const FVector2D Delta = Finish - Start;
	double Entry = 0;
	double Exit = 1;
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		if (FMath::Abs(Delta[Axis]) < 0.00001)
		{
			if (Start[Axis] < Clip.Min[Axis] || Start[Axis] > Clip.Max[Axis]) return false;
			continue;
		}
		double First = (Clip.Min[Axis] - Start[Axis]) / Delta[Axis];
		double Second = (Clip.Max[Axis] - Start[Axis]) / Delta[Axis];
		if (First > Second) Swap(First, Second);
		Entry = FMath::Max(Entry, First);
		Exit = FMath::Min(Exit, Second);
		if (Entry > Exit) return false;
	}
	Finish = Start + Delta * Exit;
	Start += Delta * Entry;
	return true;
}

int32 MiningDeveloperMap::FindRegion(const TArray<FMiningMapRegion>& Regions, const FVector& Position)
{
	const EMiningMapRegionKind Priority[] = {EMiningMapRegionKind::Room, EMiningMapRegionKind::Space, EMiningMapRegionKind::HiddenEntrance, EMiningMapRegionKind::GuidePlug, EMiningMapRegionKind::Branch, EMiningMapRegionKind::Tunnel, EMiningMapRegionKind::RockRegion};
	for (EMiningMapRegionKind Kind : Priority)
	{
		for (int32 Index = 0; Index < Regions.Num(); ++Index)
		{
			if (Regions[Index].Kind == Kind && Regions[Index].Contains(Position)) return Index;
		}
	}
	return INDEX_NONE;
}

double MiningDeveloperMap::FitScale(const FBox2D& WorldBounds, const FVector2D& ScreenSize)
{
	if (!WorldBounds.bIsValid || ScreenSize.X <= 0 || ScreenSize.Y <= 0) return 1;
	const FVector2D Size = WorldBounds.GetSize();
	return FMath::Min(ScreenSize.X / FMath::Max(100.0, Size.X), ScreenSize.Y / FMath::Max(100.0, Size.Y));
}
