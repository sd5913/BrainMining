#include "MiningRoutePlanner.h"

namespace MiningRoutes
{
	FVector WallDirection(int32 Wall)
	{
		static const FVector Directions[] = {FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0), FVector(0, 0, 1), FVector(0, 0, -1)};
		return Directions[FMath::Clamp(Wall, 0, 5)];
	}

	FVector Door(const FBox& Room, int32 Wall)
	{
		const FVector Direction = WallDirection(Wall);
		if (Wall >= 4) return Room.GetCenter() + Direction * Room.GetExtent();
		return FVector(Room.GetCenter().X, Room.GetCenter().Y, Room.Min.Z) + Direction * Room.GetExtent();
	}

	bool Direct(const FBox& First, const FBox& Second, int32 StartWall, int32 EndWall, FPlan& Plan, FString& Error)
	{
		Plan = FPlan();
		const FVector Delta = Second.GetCenter() - First.GetCenter();
		if (StartWall < 0 && EndWall >= 0) StartWall = EndWall ^ 1;
		if (StartWall < 0)
		{
			int32 Axis = INDEX_NONE;
			double LargestGap = 2;
			for (int32 Candidate = 0; Candidate < 3; ++Candidate)
			{
				if (Candidate == 2 && Axis != INDEX_NONE) break;
				const double Gap = FMath::Abs(Delta[Candidate]) - First.GetExtent()[Candidate] - Second.GetExtent()[Candidate];
				if (Gap > LargestGap) { LargestGap = Gap; Axis = Candidate; }
			}
			if (Axis == INDEX_NONE) { Error = TEXT("Rooms need a gap between a pair of facing surfaces"); return false; }
			StartWall = Axis * 2 + (Delta[Axis] < 0 ? 1 : 0);
		}
		if (EndWall < 0) EndWall = StartWall ^ 1;
		if (StartWall > 5 || EndWall > 5 || (StartWall ^ 1) != EndWall)
		{
			Error = TEXT("Direct links need opposite parallel faces; choose matching walls or Auto"); return false;
		}
		const int32 Axis = StartWall / 2;
		const int32 SideAxis = Axis == 0 ? 1 : 0;
		const int32 UpAxis = Axis == 2 ? 1 : 2;
		const FVector Direction = WallDirection(StartWall);
		const FVector FirstCenter = First.GetCenter() + Direction * First.GetExtent();
		const FVector SecondCenter = Second.GetCenter() - Direction * Second.GetExtent();
		if ((SecondCenter - FirstCenter).Dot(Direction) <= 2)
		{
			Error = TEXT("Selected faces do not face across a positive gap; move rooms or select another face"); return false;
		}
		for (int32 EndIndex = 0; EndIndex < 2; ++EndIndex)
		{
			const FBox& Room = EndIndex == 0 ? First : Second;
			const FVector Center = EndIndex == 0 ? FirstCenter : SecondCenter;
			for (const FIntPoint& Sign : {FIntPoint(-1, -1), FIntPoint(1, -1), FIntPoint(1, 1), FIntPoint(-1, 1)})
			{
				FVector Corner = Center;
				Corner[SideAxis] += Sign.X * Room.GetExtent()[SideAxis];
				Corner[UpAxis] += Sign.Y * Room.GetExtent()[UpAxis];
				Plan.FaceCorners.Add(Corner);
			}
		}
		Plan.Points = {Door(First, StartWall), Door(Second, EndWall)};
		const FVector FloorDelta = Plan.Points[1] - Plan.Points[0];
		Plan.MaxSlope = FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(FloorDelta.Z), FloorDelta.Size2D()));
		Plan.Length = FVector::Distance(FirstCenter, SecondCenter);
		Plan.StartWall = StartWall;
		Plan.EndWall = EndWall;
		Plan.FaceSections.Add(Plan.FaceCorners);
		return true;
	}

	bool Shape(FPlan& Plan, TArray<FShapeFrame> Frames, FString& Error)
	{
		if (Frames.IsEmpty()) return true;
		if (Plan.FaceCorners.Num() != 8 || Frames.Num() > 24)
		{
			Error = TEXT("Use at most 24 shape sections on a full-face tunnel"); return false;
		}
		const int32 Axis = Plan.StartWall / 2;
		const int32 SideAxis = Axis == 0 ? 1 : 0;
		const int32 UpAxis = Axis == 2 ? 1 : 2;
		const FVector Direction = WallDirection(Plan.StartWall);
		for (const FShapeFrame& Frame : Frames)
		{
			if (Frame.Center.ContainsNaN() || !FMath::IsFinite(Frame.Width) || !FMath::IsFinite(Frame.Height) ||
				FMath::Min(Frame.Width, Frame.Height) < 250 || FMath::Max(Frame.Width, Frame.Height) > 10000)
			{
				Error = TEXT("Section width and height must be finite, 250-10000 cm"); return false;
			}
		}
		Frames.Sort([Direction](const FShapeFrame& First, const FShapeFrame& Second) { return First.Center.Dot(Direction) < Second.Center.Dot(Direction); });
		TArray<TArray<FVector>> Rings;
		Rings.Add(TArray<FVector>(Plan.FaceCorners.GetData(), 4));
		for (const FShapeFrame& Frame : Frames)
		{
			TArray<FVector> Ring;
			for (const FIntPoint& Sign : {FIntPoint(-1, -1), FIntPoint(1, -1), FIntPoint(1, 1), FIntPoint(-1, 1)})
			{
				FVector Corner = Frame.Center;
				Corner[SideAxis] += Sign.X * Frame.Width * 0.5;
				Corner[UpAxis] += Sign.Y * Frame.Height * 0.5;
				Ring.Add(Corner);
			}
			Rings.Add(MoveTemp(Ring));
		}
		Rings.Add(TArray<FVector>(Plan.FaceCorners.GetData() + 4, 4));
		FPlan Candidate = Plan;
		Candidate.Points.Reset();
		Candidate.FaceSections.Reset();
		Candidate.Length = 0;
		Candidate.MaxSlope = 0;
		for (int32 Index = 0; Index < Rings.Num(); ++Index)
		{
			const FVector Center = (Rings[Index][0] + Rings[Index][2]) * 0.5;
			FVector Floor = Center;
			if (Axis != 2) Floor.Z = Rings[Index][0].Z;
			Candidate.Points.Add(Floor);
			if (Index == 0) continue;
			const double Gap = (Rings[Index][0] - Rings[Index - 1][0]).Dot(Direction);
			if (Gap < 100)
			{
				Error = TEXT("Keep sections between room faces, at least 100 cm apart along the connection axis; no backwards folds"); return false;
			}
			TArray<FVector> Corners = Rings[Index - 1];
			Corners.Append(Rings[Index]);
			Candidate.FaceSections.Add(MoveTemp(Corners));
			const FVector PreviousCenter = (Rings[Index - 1][0] + Rings[Index - 1][2]) * 0.5;
			Candidate.Length += FVector::Distance(PreviousCenter, Center);
			const FVector Delta = Floor - Candidate.Points[Index - 1];
			Candidate.MaxSlope = FMath::Max(Candidate.MaxSlope, FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(Delta.Z), Delta.Size2D())));
		}
		Plan = MoveTemp(Candidate);
		return true;
	}

	bool BranchAnchor(const FPlan& Plan, const FVector& Target, bool bFloorAligned, double BranchHeight, FVector& Anchor)
	{
		double Nearest = TNumericLimits<double>::Max();
		for (const TArray<FVector>& Corners : Plan.FaceSections)
		{
			if (Corners.Num() != 8) continue;
			FVector Start = (Corners[0] + Corners[2]) * 0.5;
			FVector Finish = (Corners[4] + Corners[6]) * 0.5;
			const FVector Delta = Finish - Start;
			const double Along = FMath::Clamp((Target - Start).Dot(Delta) / FMath::Max(1.0, Delta.SizeSquared()), 0.05, 0.95);
			FVector Candidate = Start + Delta * Along;
			if (bFloorAligned && Plan.StartWall < 4) Candidate.Z = FMath::Lerp(Corners[0].Z, Corners[4].Z, Along) + BranchHeight * 0.5;
			const double Distance = FVector::DistSquared(Candidate, Target);
			if (Distance < Nearest) { Anchor = Candidate; Nearest = Distance; }
		}
		return Nearest < TNumericLimits<double>::Max();
	}

	FBox FSection::Bounds(double Width, double Height, double Margin) const
	{
		const FVector Direction = (End - Start).GetSafeNormal2D();
		const FVector Side(-Direction.Y, Direction.X, 0);
		FBox Result(ForceInit);
		for (const FVector& Point : {Start, End})
		{
			Result += Point + Side * (Width * 0.5);
			Result += Point - Side * (Width * 0.5) + FVector(0, 0, Height);
		}
		return Result.ExpandBy(Margin);
	}

	bool Build(const TArray<FVector>& Points, const FSettings& Settings, FPlan& Plan, FString& Error)
	{
		Plan = FPlan();
		if (Points.Num() < 4 || Points.Num() > 20)
		{
			Error = TEXT("Route needs 4-20 points including the two doorway landings");
			return false;
		}
		Plan.Points = Points;
		const double Radius = Settings.Width * 0.75;
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			if (Points[Index].ContainsNaN()) { Error = TEXT("Non-finite route point"); return false; }
			if (Index > 0 && Index < Points.Num() - 1)
			{
				const FVector Incoming = (Points[Index] - Points[Index - 1]).GetSafeNormal2D();
				const FVector Outgoing = (Points[Index + 1] - Points[Index]).GetSafeNormal2D();
				if (Incoming.Dot(Outgoing) < -0.1) { Error = TEXT("Turn exceeds 95 degrees; add a wider turning platform"); return false; }
				Plan.Pads.Add(FBox(Points[Index] - FVector(Radius, Radius, 0), Points[Index] + FVector(Radius, Radius, Settings.Height)));
			}
		}
		for (int32 Index = 1; Index < Points.Num(); ++Index)
		{
			const FVector Direction = (Points[Index] - Points[Index - 1]).GetSafeNormal2D();
			const double Distance = FVector::Dist2D(Points[Index], Points[Index - 1]);
			const double TrimStart = Index == 1 ? 0 : Radius;
			const double TrimEnd = Index == Points.Num() - 1 ? 0 : Radius;
			const double Run = Distance - TrimStart - TrimEnd;
			if (Run < 30) { Error = TEXT("Control points too close for a turning platform"); return false; }
			const double Slope = FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(Points[Index].Z - Points[Index - 1].Z), Run));
			if (Slope > Settings.MaxSlope + 0.01) { Error = TEXT("Slope exceeds Max Slope; extend route or move a control point"); return false; }
			if ((Index == 1 || Index == Points.Num() - 1) && Slope > 0.01) { Error = TEXT("Doorway approach must be level"); return false; }
			Plan.MaxSlope = FMath::Max(Plan.MaxSlope, Slope);
			Plan.Length += FVector::Distance(Points[Index], Points[Index - 1]);
			Plan.Sections.Add({Points[Index - 1] + Direction * TrimStart, Points[Index] - Direction * TrimEnd});
		}
		return true;
	}

	bool Validate(const FPlan& Plan, const FSettings& Settings, const TArray<FBox>& Rooms, int32 First, int32 Second, const TArray<FBox>& Reserved, FString& Error)
	{
		TArray<FBox> Volumes;
		for (const FSection& Section : Plan.Sections) Volumes.Add(Section.Bounds(Settings.Width, Settings.Height));
		Volumes.Append(Plan.Pads);
		for (int32 Index = 0; Index < Volumes.Num(); ++Index)
		{
			for (int32 RoomIndex = 0; RoomIndex < Rooms.Num(); ++RoomIndex)
			{
				if ((RoomIndex == First && Index == 0) || (RoomIndex == Second && Index == Plan.Sections.Num() - 1)) continue;
				if (Volumes[Index].ExpandBy(Settings.Thickness + 10).Intersect(Rooms[RoomIndex])) { Error = TEXT("Route intersects a room outside its doorway"); return false; }
			}
			for (const FBox& Obstacle : Reserved)
			{
				if (Volumes[Index].ExpandBy(Settings.Thickness + 10).Intersect(Obstacle)) { Error = TEXT("Route intersects another tunnel"); return false; }
			}
		}
		for (int32 FirstSection = 0; FirstSection < Plan.Sections.Num(); ++FirstSection)
		{
			for (int32 SecondSection = FirstSection + 2; SecondSection < Plan.Sections.Num(); ++SecondSection)
			{
				if (Volumes[FirstSection].ExpandBy(Settings.Thickness).Intersect(Volumes[SecondSection])) { Error = TEXT("Route folds back into itself"); return false; }
			}
		}
		return true;
	}

	bool Find(const TArray<FBox>& Rooms, int32 First, int32 Second, const FSettings& Settings, const TArray<FBox>& Reserved, int32 StartWall, int32 EndWall, FPlan& Plan, FString& Error)
	{
		double BestCost = TNumericLimits<double>::Max();
		const double Radius = Settings.Width * 0.75;
		const double Landing = Radius + Settings.Thickness + 120;
		for (int32 FirstWall = 0; FirstWall < 4; ++FirstWall)
		{
			if (StartWall >= 0 && FirstWall != StartWall) continue;
			for (int32 SecondWall = 0; SecondWall < 4; ++SecondWall)
			{
				if (EndWall >= 0 && SecondWall != EndWall) continue;
				const FVector FirstDoor = Door(Rooms[First], FirstWall);
				const FVector SecondDoor = Door(Rooms[Second], SecondWall);
				const FVector FirstLanding = FirstDoor + WallDirection(FirstWall) * Landing;
				const FVector SecondLanding = SecondDoor + WallDirection(SecondWall) * Landing;
				TArray<TArray<FVector>> Candidates;
				Candidates.Add({FirstLanding, SecondLanding});
				Candidates.Add({FirstLanding, FVector(FirstLanding.X, SecondLanding.Y, 0), SecondLanding});
				Candidates.Add({FirstLanding, FVector(SecondLanding.X, FirstLanding.Y, 0), SecondLanding});
				const double Step = FMath::Max(1000.0, Settings.Width * 2 + Settings.Thickness * 2);
				for (double Offset = Step; Offset <= Settings.SearchDistance; Offset += Step)
				{
					for (double Sign : {-1.0, 1.0})
					{
						const double LaneY = (Sign < 0 ? FMath::Min(FirstLanding.Y, SecondLanding.Y) : FMath::Max(FirstLanding.Y, SecondLanding.Y)) + Sign * Offset;
						const double LaneX = (Sign < 0 ? FMath::Min(FirstLanding.X, SecondLanding.X) : FMath::Max(FirstLanding.X, SecondLanding.X)) + Sign * Offset;
						Candidates.Add({FirstLanding, FVector(FirstLanding.X, LaneY, 0), FVector(SecondLanding.X, LaneY, 0), SecondLanding});
						Candidates.Add({FirstLanding, FVector(LaneX, FirstLanding.Y, 0), FVector(LaneX, SecondLanding.Y, 0), SecondLanding});
					}
				}
				for (TArray<FVector> Middle : Candidates)
				{
					double TotalRun = 0;
					bool bTooShort = false;
					for (int32 Index = 1; Index < Middle.Num(); ++Index)
					{
						const double Run = FVector::Dist2D(Middle[Index], Middle[Index - 1]) - Radius * 2;
						bTooShort |= Run < 30;
						TotalRun += Run;
					}
					if (bTooShort || TotalRun <= 0 || FMath::Abs(SecondDoor.Z - FirstDoor.Z) > TotalRun * FMath::Tan(FMath::DegreesToRadians(Settings.MaxSlope))) continue;
					double Progress = 0;
					Middle[0].Z = FirstDoor.Z;
					for (int32 Index = 1; Index < Middle.Num(); ++Index)
					{
						Progress += FVector::Dist2D(Middle[Index], Middle[Index - 1]) - Radius * 2;
						Middle[Index].Z = FMath::Lerp(FirstDoor.Z, SecondDoor.Z, Progress / TotalRun);
					}
					TArray<FVector> Points = {FirstDoor};
					Points.Append(Middle);
					Points.Add(SecondDoor);
					FPlan Candidate;
					FString Reason;
					if (!Build(Points, Settings, Candidate, Reason) || !Validate(Candidate, Settings, Rooms, First, Second, Reserved, Reason)) continue;
					const double Cost = Candidate.Length + Candidate.Points.Num() * 100;
					if (Cost < BestCost)
					{
						BestCost = Cost;
						Candidate.StartWall = FirstWall;
						Candidate.EndWall = SecondWall;
						Plan = MoveTemp(Candidate);
					}
				}
			}
		}
		if (BestCost == TNumericLimits<double>::Max())
		{
			Error = TEXT("No collision-free walkable candidate within Search Distance; change door walls, extend search or edit a manual route");
			return false;
		}
		return true;
	}
}
