#include "MiningHollowGeometry.h"
#include "MiningChunkGeometry.h"
#include "Generators/MinimalBoxMeshGenerator.h"

using namespace UE::Geometry;

bool MiningHollow::Plan(const FSettings& Settings, FPlan& Result, FString& Error)
{
	const auto Fail = [&Error](const TCHAR* Text) { Error = Text; return false; };
	for (double Value : {Settings.Radius, Settings.Width, Settings.Above, Settings.Below, Settings.MaxSlope})
		if (!FMath::IsFinite(Value)) return Fail(TEXT("Region values must be finite"));
	if (Settings.Radius < 150 || Settings.Radius > 1000 || Settings.Width < 1000 || Settings.Width > 8000 ||
		Settings.Above < 100 || Settings.Above > 3000 || Settings.Below < 100 || Settings.Below > 3000 ||
		Settings.MaxSlope < 5 || Settings.MaxSlope > 35 || Settings.Points.Num() < 2 || Settings.Points.Num() > 12 || Settings.Plugs.Num() > 12)
		return Fail(TEXT("Use 2-12 path points, at most 12 plugs and supported region dimensions"));
	const FBox& First = Settings.FirstRoom;
	const FBox& Last = Settings.LastRoom;
	if (!First.IsValid || !Last.IsValid || First.Min.ContainsNaN() || First.Max.ContainsNaN() || Last.Min.ContainsNaN() || Last.Max.ContainsNaN())
		return Fail(TEXT("Invalid room bounds"));
	const double Gap = Last.Min.X - First.Max.X;
	if (Gap < 1000 || Gap > 24000 || Last.Min.Z > First.Min.Z + 0.01)
		return Fail(TEXT("Prototype needs the second room 1000-24000 cm along +X, at the same or a lower floor"));
	for (const FBox& Room : {First, Last})
		if (Room.GetSize().Y < Settings.Radius * 2 + 20 || Room.GetSize().Z < Settings.Radius * 2 + 20)
			return Fail(TEXT("Circular portal does not fit the room; enlarge room width/height or reduce Passage Radius"));
	FPlan Candidate;
	Candidate.Radius = Settings.Radius;
	Candidate.Points = Settings.Points;
	Candidate.Points[0] = FVector(First.Max.X, First.GetCenter().Y, First.Min.Z + Settings.Radius);
	Candidate.Points.Last() = FVector(Last.Min.X, Last.GetCenter().Y, Last.Min.Z + Settings.Radius);
	const double CenterY = (First.GetCenter().Y + Last.GetCenter().Y) * 0.5;
	Candidate.Bounds = FBox(FVector(First.Max.X, CenterY - Settings.Width * 0.5, Last.Min.Z - Settings.Below),
		FVector(Last.Min.X, CenterY + Settings.Width * 0.5, First.Min.Z + Settings.Radius * 2 + Settings.Above));
	Candidate.Distances.Add(0);
	for (int32 Index = 0; Index < Candidate.Points.Num(); ++Index)
	{
		const FVector& Point = Candidate.Points[Index];
		if (Point.ContainsNaN()) return Fail(TEXT("Path points must be finite"));
		if (Point.Y - Settings.Radius < Candidate.Bounds.Min.Y + 100 || Point.Y + Settings.Radius > Candidate.Bounds.Max.Y - 100 ||
			Point.Z - Settings.Radius < Candidate.Bounds.Min.Z + 100 || Point.Z + Settings.Radius > Candidate.Bounds.Max.Z - 100)
			return Fail(TEXT("Keep at least 100 cm of ordinary rock between the passage and the outer region boundary"));
		if (Index == 0) continue;
		const FVector Delta = Point - Candidate.Points[Index - 1];
		if (Delta.X < 100 || Delta.Z > 0.01 || FMath::Abs(Delta.Y) > Delta.X * 0.7)
			return Fail(TEXT("First-stage path must progress along +X, never climb, and avoid sharp sideways turns; move spline points"));
		if (FMath::RadiansToDegrees(FMath::Atan2(-Delta.Z, Delta.Size2D())) > Settings.MaxSlope)
			return Fail(TEXT("Passage exceeds Maximum Slope; increase room separation or adjust path points"));
		Candidate.Length += Delta.Size();
		Candidate.Distances.Add(Candidate.Length);
	}
	for (const FMiningHollowPlug& Plug : Settings.Plugs)
	{
		if (!Plug.bEnabled) continue;
		if (!FMath::IsFinite(Plug.Position) || !FMath::IsFinite(Plug.Thickness) || Plug.Position <= 0 || Plug.Position >= 1 || Plug.Thickness < 50 || Plug.Thickness > 1000)
			return Fail(TEXT("Plug position must be between 0 and 1; thickness must be 50-1000 cm"));
		const double Center = Plug.Position * Candidate.Length;
		const FVector2D Interval(Center - Plug.Thickness * 0.5, Center + Plug.Thickness * 0.5);
		if (Interval.X < 300 || Interval.Y > Candidate.Length - 300) return Fail(TEXT("Keep plugs at least 300 cm away from room portals"));
		Candidate.PlugIntervals.Add(Interval);
	}
	Candidate.PlugIntervals.Sort([](const FVector2D& FirstInterval, const FVector2D& SecondInterval) { return FirstInterval.X < SecondInterval.X; });
	for (int32 Index = 0; Index < Candidate.PlugIntervals.Num(); ++Index)
	{
		const FVector2D Interval = Candidate.PlugIntervals[Index];
		if (Index && Interval.X - Candidate.PlugIntervals[Index - 1].Y < 250) return Fail(TEXT("Plugs overlap or leave less than 250 cm of open passage"));
		const TArray<FVector> Points = Slice(Candidate, Interval.X, Interval.Y);
		for (int32 Point = 1; Point < Points.Num(); ++Point)
		{
			FMiningGuideVolume Guide;
			Guide.Start = Points[Point - 1]; Guide.End = Points[Point]; Guide.Radius = Settings.Radius;
			Candidate.Guides.Add(Guide);
		}
	}
	if (Candidate.Guides.Num() > 16) return Fail(TEXT("Too many plug/path intersections; use fewer plugs or path points (maximum 16 material volumes)"));
	Result = MoveTemp(Candidate);
	return true;
}

bool MiningHollow::ValidateChain(const TArray<FPlan>& Plans, const TArray<FBox>& Rooms, double BedrockThickness, FString& Error)
{
	if (Plans.IsEmpty() || Rooms.Num() != Plans.Num() + 1 || !FMath::IsFinite(BedrockThickness) || BedrockThickness <= 0)
	{
		Error = TEXT("Invalid hollow region chain or bedrock thickness"); return false;
	}
	for (int32 Index = 0; Index < Plans.Num(); ++Index)
	{
		const FBox Outer = Plans[Index].Bounds.ExpandBy(BedrockThickness);
		for (int32 Other = 0; Other < Index; ++Other)
			if (Outer.Intersect(Plans[Other].Bounds.ExpandBy(BedrockThickness)))
			{
				Error = FString::Printf(TEXT("Regions %d and %d have overlapping outer bedrock; enlarge their separating room or reduce bedrock thickness"), Other + 1, Index + 1); return false;
			}
		for (int32 RoomIndex = 0; RoomIndex < Rooms.Num(); ++RoomIndex)
			if (RoomIndex != Index && RoomIndex != Index + 1 && Outer.Intersect(Rooms[RoomIndex].ExpandBy(BedrockThickness)))
			{
				Error = FString::Printf(TEXT("Region %d intersects unrelated room %d; move the room to prevent a progression bypass"), Index + 1, RoomIndex + 1); return false;
			}
	}
	return true;
}

TArray<FVector> MiningHollow::Slice(const FPlan& Plan, double Start, double End)
{
	TArray<FVector> Points;
	for (int32 Index = 1; Index < Plan.Points.Num(); ++Index)
	{
		const double Low = FMath::Max(Start, Plan.Distances[Index - 1]);
		const double High = FMath::Min(End, Plan.Distances[Index]);
		if (High - Low < 0.000001) continue;
		const double Span = Plan.Distances[Index] - Plan.Distances[Index - 1];
		const FVector First = FMath::Lerp(Plan.Points[Index - 1], Plan.Points[Index], (Low - Plan.Distances[Index - 1]) / Span);
		const FVector Last = FMath::Lerp(Plan.Points[Index - 1], Plan.Points[Index], (High - Plan.Distances[Index - 1]) / Span);
		if (Points.IsEmpty()) Points.Add(First);
		Points.Add(Last);
	}
	return Points;
}

TArray<FVector> MiningHollow::Ring(const FVector& Center, double Radius)
{
	TArray<FVector> Vertices;
	for (int32 Side = 0; Side < 16; ++Side)
	{
		const double Angle = Side * UE_DOUBLE_TWO_PI / 16;
		Vertices.Add(Center + FVector(0, FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius));
	}
	return Vertices;
}

bool MiningHollow::Tube(const TArray<FVector>& Points, double Radius, FDynamicMesh3& Mesh)
{
	if (Points.Num() < 2 || Radius <= 0) return false;
	Mesh.Clear();
	for (const FVector& Point : Points) for (const FVector& Vertex : Ring(Point, Radius)) Mesh.AppendVertex(Vertex);
	for (int32 Index = 1; Index < Points.Num(); ++Index)
		for (int32 Side = 0; Side < 16; ++Side)
		{
			const int32 Current = (Index - 1) * 16 + Side;
			const int32 Next = (Index - 1) * 16 + (Side + 1) % 16;
			Mesh.AppendTriangle(Current, Next + 16, Current + 16);
			Mesh.AppendTriangle(Current, Next, Next + 16);
		}
	const int32 FirstCenter = Mesh.AppendVertex(Points[0]);
	const int32 LastCenter = Mesh.AppendVertex(Points.Last());
	const int32 LastRing = (Points.Num() - 1) * 16;
	for (int32 Side = 0; Side < 16; ++Side)
	{
		const int32 Next = (Side + 1) % 16;
		Mesh.AppendTriangle(FirstCenter, Next, Side);
		Mesh.AppendTriangle(LastCenter, LastRing + Side, LastRing + Next);
	}
	Mesh.ReverseOrientation();
	return Mesh.IsClosed() && Mesh.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly);
}

bool MiningHollow::Fill(const FPlan& Plan, FDynamicMesh3& Mesh, FString& Error)
{
	FMinimalBoxMeshGenerator Box;
	Box.Box = FOrientedBox3d(FVector3d(Plan.Bounds.GetCenter()), FVector3d(Plan.Bounds.GetExtent()));
	Box.Generate();
	FDynamicMesh3 Candidate(&Box);
	double Start = 0;
	TArray<FVector2D> EmptyIntervals;
	for (const FVector2D& Interval : Plan.PlugIntervals) { EmptyIntervals.Add(FVector2D(Start, Interval.X)); Start = Interval.Y; }
	EmptyIntervals.Add(FVector2D(Start, Plan.Length));
	for (const FVector2D& Interval : EmptyIntervals)
	{
		TArray<FVector> Points = Slice(Plan, Interval.X, Interval.Y);
		if (Interval.X == 0) Points[0] -= (Points[1] - Points[0]).GetSafeNormal() * Plan.Radius;
		if (Interval.Y == Plan.Length) Points.Last() += (Points.Last() - Points[Points.Num() - 2]).GetSafeNormal() * Plan.Radius;
		FDynamicMesh3 Cavity, Result;
		if (!Tube(Points, Plan.Radius, Cavity))
		{
			Error = FString::Printf(TEXT("Hollow tube invalid at %.1f-%.1f cm (%d points); previous terrain retained"), Interval.X, Interval.Y, Points.Num()); return false;
		}
		if (!MiningChunks::Subtract(Candidate, Cavity, Result))
		{
			Error = FString::Printf(TEXT("Hollow passage boolean failed at %.1f-%.1f cm (%d triangles); previous terrain retained"), Interval.X, Interval.Y, Candidate.TriangleCount()); return false;
		}
		Candidate = MoveTemp(Result);
	}
	Mesh = MoveTemp(Candidate);
	return true;
}

bool MiningHollow::IsGuide(const FVector& Point, const TArray<FMiningGuideVolume>& Guides)
{
	for (const FMiningGuideVolume& Guide : Guides)
	{
		if (Point.X < Guide.Start.X - 0.1 || Point.X > Guide.End.X + 0.1 || Guide.End.X <= Guide.Start.X) continue;
		const FVector Center = FMath::Lerp(Guide.Start, Guide.End, FMath::Clamp((Point.X - Guide.Start.X) / (Guide.End.X - Guide.Start.X), 0.0, 1.0));
		const FVector2D Delta(Point.Y - Center.Y, Point.Z - Center.Z);
		bool bInside = true;
		for (int32 Side = 0; Side < 16; ++Side)
		{
			const double Angle = (Side + 0.5) * UE_DOUBLE_TWO_PI / 16;
			bInside &= FVector2D::DotProduct(Delta, FVector2D(FMath::Cos(Angle), FMath::Sin(Angle))) <= Guide.Radius * FMath::Cos(UE_DOUBLE_PI / 16) + 0.1;
		}
		if (bInside) return true;
	}
	return false;
}
