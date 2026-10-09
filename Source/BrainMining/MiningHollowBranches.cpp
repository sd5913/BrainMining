#include "MiningHollowBranches.h"
#include "MiningChunkGeometry.h"
#include "Generators/MinimalBoxMeshGenerator.h"

using namespace UE::Geometry;

namespace
{
	TArray<FBox> SpanBounds(const TArray<TArray<FVector>>& Rings)
	{
		TArray<FBox> Boxes;
		for (int32 Index = 1; Index < Rings.Num(); ++Index)
		{
			FBox Bounds(ForceInit);
			for (const FVector& Point : Rings[Index - 1]) Bounds += Point;
			for (const FVector& Point : Rings[Index]) Bounds += Point;
			Boxes.Add(Bounds);
		}
		return Boxes;
	}

	bool ClipSpan(const FVector& Start, const FVector& End, const FBox& Bounds, double& Exit)
	{
		double Low = 0, High = 1;
		const FVector Delta = End - Start;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (FMath::Abs(Delta[Axis]) < 0.0001)
			{
				if (Start[Axis] < Bounds.Min[Axis] || Start[Axis] > Bounds.Max[Axis]) return false;
				continue;
			}
			double First = (Bounds.Min[Axis] - Start[Axis]) / Delta[Axis];
			double Last = (Bounds.Max[Axis] - Start[Axis]) / Delta[Axis];
			if (First > Last) Swap(First, Last);
			Low = FMath::Max(Low, First); High = FMath::Min(High, Last);
			if (Low > High) return false;
		}
		Exit = High;
		return true;
	}
}

TArray<TArray<FVector>> MiningHollowBranches::Rings(const TArray<FVector>& Points, double Radius)
{
	TArray<TArray<FVector>> Result;
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		FVector Direction = Index == 0 ? Points[1] - Points[0] : Index + 1 == Points.Num() ? Points.Last() - Points[Index - 1]
			: (Points[Index] - Points[Index - 1]).GetSafeNormal2D() + (Points[Index + 1] - Points[Index]).GetSafeNormal2D();
		Direction = Direction.GetSafeNormal2D();
		const FVector Side(-Direction.Y, Direction.X, 0);
		TArray<FVector>& Ring = Result.AddDefaulted_GetRef();
		for (int32 SideIndex = 0; SideIndex < 16; ++SideIndex)
		{
			const double Angle = SideIndex * UE_DOUBLE_TWO_PI / 16;
			Ring.Add(Points[Index] + Radius * (FMath::Cos(Angle) * Side + FMath::Sin(Angle) * FVector::UpVector));
		}
	}
	return Result;
}

bool MiningHollowBranches::Tube(const TArray<FVector>& Points, double Radius, FDynamicMesh3& Mesh)
{
	if (Points.Num() < 2 || Radius <= 0) return false;
	Mesh.Clear();
	for (const TArray<FVector>& Ring : Rings(Points, Radius)) for (const FVector& Vertex : Ring) Mesh.AppendVertex(Vertex);
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

bool MiningHollowBranches::Plan(const MiningHollow::FPlan& Region, const TArray<FSettings>& Settings, TArray<FPlan>& Result, FString& Error)
{
	if (Settings.Num() > 8) { Error = TEXT("At most 8 enabled chambers per hollow region; previous terrain retained"); return false; }
	TArray<TArray<FVector>> MainRings;
	for (const FVector& Point : Region.Points) MainRings.Add(MiningHollow::Ring(Point, Region.Radius));
	const TArray<FBox> MainBounds = SpanBounds(MainRings);
	TArray<FBox> PlugBounds;
	for (const FMiningGuideVolume& Guide : Region.Guides)
	{
		FBox Box(ForceInit);
		for (const FVector& Vertex : MiningHollow::Ring(Guide.Start, Guide.Radius)) Box += Vertex;
		for (const FVector& Vertex : MiningHollow::Ring(Guide.End, Guide.Radius)) Box += Vertex;
		PlugBounds.Add(Box);
	}
	TArray<FPlan> Candidates;
	TArray<FBox> Reserved;
	for (const FSettings& Input : Settings)
	{
		const auto Fail = [&](const TCHAR* Message) { Error = Input.Name + TEXT(": ") + Message + TEXT("; previous terrain retained"); return false; };
		if (!FMath::IsFinite(Input.Connection) || Input.Connection < 0.05 || Input.Connection > 0.95 ||
			!FMath::IsFinite(Input.Radius) || Input.Radius < 150 || Input.Radius > 500 ||
			!FMath::IsFinite(Input.Thickness) || Input.Thickness < 50 || Input.Thickness > 600 || Input.InteriorPoints.Num() > 4 ||
			!Input.Chamber.IsValid || Input.Chamber.Min.ContainsNaN() || Input.Chamber.Max.ContainsNaN())
			return Fail(TEXT("Invalid dimensions, connection fraction or path; use 0-4 interior points and radius 150-500 cm"));
		const FVector Size = Input.Chamber.GetSize();
		if (Size.GetMin() < Input.Radius * 2 + 40 || Size.GetMax() > 2500)
			return Fail(TEXT("Chamber dimensions must fit branch diameter + 40 cm and not exceed 2500 cm"));
		if (!Region.Bounds.ExpandBy(-100).IsInsideOrOn(Input.Chamber.Min) || !Region.Bounds.ExpandBy(-100).IsInsideOrOn(Input.Chamber.Max))
			return Fail(TEXT("Move or resize chamber to leave 100 cm of rock inside the region boundary"));
		for (const FBox& Main : MainBounds)
			if (Main.ExpandBy(50).Intersect(Input.Chamber)) return Fail(TEXT("Chamber overlaps the main passage; move it farther sideways"));
		FPlan Candidate;
		Candidate.Name = Input.Name; Candidate.Chamber = Input.Chamber; Candidate.Radius = Input.Radius; Candidate.bHidden = Input.bHidden;
		const FVector Anchor = MiningHollow::Slice(Region, Region.Length * Input.Connection, Region.Length * Input.Connection + 1)[0];
		Candidate.Points.Add(Anchor + FVector(0, 0, Input.Radius - Region.Radius));
		Candidate.Points.Append(Input.InteriorPoints);
		Candidate.Points.Add(FVector(Input.Chamber.GetCenter().X, Input.Chamber.GetCenter().Y, Input.Chamber.Min.Z + Input.Radius));
		const double SideSign = FMath::Sign(Candidate.Points.Last().Y - Candidate.Points[0].Y);
		for (int32 Index = 1; Index < Candidate.Points.Num(); ++Index)
		{
			const FVector Delta = Candidate.Points[Index] - Candidate.Points[Index - 1];
			if (Candidate.Points[Index].ContainsNaN() || Delta.Y * SideSign < 100 || Delta.Size2D() < 100 || FMath::Abs(Delta.Y) < Delta.Size2D() * 0.65)
				return Fail(TEXT("Branch must extend sideways without loops; each span needs 100 cm outward Y movement"));
			if (FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(Delta.Z), Delta.Size2D())) > 25)
				return Fail(TEXT("Branch slope exceeds 25 degrees; adjust chamber floor or interior points"));
			if (Index > 1 && FVector::DotProduct(Delta.GetSafeNormal2D(), (Candidate.Points[Index - 1] - Candidate.Points[Index - 2]).GetSafeNormal2D()) < 0.5)
				return Fail(TEXT("Branch turn exceeds 60 degrees; soften the bend"));
		}
		const TArray<TArray<FVector>> BranchRings = Rings(Candidate.Points, Input.Radius);
		for (int32 Index = 1; Index < BranchRings.Num(); ++Index)
		{
			const FVector Direction = (Candidate.Points[Index] - Candidate.Points[Index - 1]).GetSafeNormal2D();
			double FirstMax = -TNumericLimits<double>::Max(), LastMin = TNumericLimits<double>::Max();
			for (const FVector& Vertex : BranchRings[Index - 1]) FirstMax = FMath::Max(FirstMax, FVector::DotProduct(Vertex, Direction));
			for (const FVector& Vertex : BranchRings[Index]) LastMin = FMath::Min(LastMin, FVector::DotProduct(Vertex, Direction));
			if (LastMin - FirstMax < 10) return Fail(TEXT("Branch bend is too tight for its radius; space control points farther apart or reduce radius"));
		}
		const TArray<FBox> BranchBounds = SpanBounds(BranchRings);
		TArray<FBox> Occupied = BranchBounds;
		Occupied.Add(Input.Chamber);
		for (const FBox& Box : Occupied)
		{
			if (!Region.Bounds.ExpandBy(-100).IsInsideOrOn(Box.Min) || !Region.Bounds.ExpandBy(-100).IsInsideOrOn(Box.Max))
				return Fail(TEXT("Branch must remain 100 cm inside the outer region"));
			for (const FBox& Plug : PlugBounds) if (Box.ExpandBy(50).Intersect(Plug)) return Fail(TEXT("Branch is too close to a main-path plug; move Connection Position"));
			for (const FBox& Existing : Reserved) if (Box.ExpandBy(50).Intersect(Existing)) return Fail(TEXT("Chamber or branch overlaps another chamber/branch; move them apart"));
		}
		double ExitFraction = 0;
		for (const FBox& Main : MainBounds)
		{
			double Exit = 0;
			if (ClipSpan(Candidate.Points[0], Candidate.Points[1], Main.ExpandBy(Input.Radius + 25), Exit)) ExitFraction = FMath::Max(ExitFraction, Exit);
		}
		const double FirstLength = FVector::Distance(Candidate.Points[0], Candidate.Points[1]);
		if (ExitFraction >= 1) return Fail(TEXT("First branch span is too short to leave the main passage; move its first bend farther sideways"));
		for (int32 Index = 1; Index < BranchBounds.Num(); ++Index)
			for (const FBox& Main : MainBounds)
				if (BranchBounds[Index].ExpandBy(25).Intersect(Main)) return Fail(TEXT("Branch reconnects to the main passage; loops are not supported in this stage"));
		Candidate.OpenPoints = Candidate.Points;
		if (Input.bHidden)
		{
			const double EndFraction = ExitFraction + Input.Thickness / FirstLength;
			if ((1 - EndFraction) * FirstLength < 150) return Fail(TEXT("Hidden entrance needs more space before its first bend/chamber; move it farther sideways"));
			Candidate.SealPoints = {FMath::Lerp(Candidate.Points[0], Candidate.Points[1], ExitFraction), FMath::Lerp(Candidate.Points[0], Candidate.Points[1], EndFraction)};
			const FBox SealBounds = SpanBounds(Rings(Candidate.SealPoints, Input.Radius))[0];
			if (SealBounds.ExpandBy(50).Intersect(Input.Chamber)) return Fail(TEXT("Chamber would cut through its hidden entrance; move chamber farther away or reduce seal thickness"));
			Candidate.OpenPoints[0] = Candidate.SealPoints.Last();
			for (const FBox& Box : SpanBounds(Rings(Candidate.OpenPoints, Input.Radius)))
				for (const FBox& Main : MainBounds)
					if (Box.Intersect(Main)) return Fail(TEXT("Hidden cavity would open into main passage; move the first bend farther outward"));
		}
		Reserved.Append(Occupied);
		Candidates.Add(MoveTemp(Candidate));
	}
	Result = MoveTemp(Candidates);
	return true;
}

bool MiningHollowBranches::Carve(const TArray<FPlan>& Plans, FDynamicMesh3& Rock, FString& Error)
{
	if (Plans.IsEmpty()) return true;
	FDynamicMesh3 Candidate(Rock);
	for (const FPlan& Plan : Plans)
	{
		FDynamicMesh3 Cutter, Result;
		if (!Tube(Plan.OpenPoints, Plan.Radius, Cutter) || !MiningChunks::Subtract(Candidate, Cutter, Result))
		{
			Error = Plan.Name + TEXT(": branch boolean failed; previous terrain retained"); return false;
		}
		Candidate = MoveTemp(Result);
		FMinimalBoxMeshGenerator Box;
		Box.Box = FOrientedBox3d(FVector3d(Plan.Chamber.GetCenter()), FVector3d(Plan.Chamber.GetExtent()));
		Box.Generate();
		Cutter = FDynamicMesh3(&Box);
		if (!MiningChunks::Subtract(Candidate, Cutter, Result))
		{
			Error = Plan.Name + TEXT(": chamber boolean failed; previous terrain retained"); return false;
		}
		Candidate = MoveTemp(Result);
	}
	Rock = MoveTemp(Candidate);
	return true;
}

void MiningHollowBranches::AddMapRegions(const TArray<FPlan>& Plans, const FVector& Origin, TArray<FMiningMapRegion>& Regions, int32 GroupIndex)
{
	for (const FPlan& Plan : Plans)
	{
		const auto AddTube = [&](const TArray<FVector>& Points, EMiningMapRegionKind Kind, const FString& Label)
		{
			const TArray<TArray<FVector>> Sections = Rings(Points, Plan.Radius);
			for (int32 Index = 1; Index < Sections.Num(); ++Index)
			{
				TArray<FVector> Vertices = Sections[Index - 1]; Vertices.Append(Sections[Index]);
				for (FVector& Vertex : Vertices) Vertex += Origin;
				FMiningMapRegion Region = MiningDeveloperMap::Convex(Plan.Name + Label, Kind, GroupIndex, Vertices);
				Region.bRockFilled = Kind == EMiningMapRegionKind::HiddenEntrance;
				Regions.Add(MoveTemp(Region));
			}
		};
		AddTube(Plan.OpenPoints, EMiningMapRegionKind::Branch, TEXT(" / branch"));
		Regions.Add(MiningDeveloperMap::Box(Plan.Name + (Plan.bHidden ? TEXT(" / hidden cave") : TEXT(" / open cave")), EMiningMapRegionKind::Space, GroupIndex, Plan.Chamber.ShiftBy(Origin)));
		if (Plan.bHidden) AddTube({Plan.Points[0], Plan.SealPoints.Last()}, EMiningMapRegionKind::HiddenEntrance, TEXT(" / diggable entrance (design)"));
	}
}
