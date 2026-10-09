#include "MiningSpatialGeometry.h"
#include "MiningSpatialBuild.h"
#include "MiningChunkGeometry.h"
#include "MiningLayoutRoom.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Algo/Reverse.h"
#include "MiningSpatialEnvelope.h"
#include "Operations/MeshSelfUnion.h"

using namespace UE::Geometry;

namespace
{
	bool HasSelfIntersections(const FDynamicMesh3& Mesh)
	{
		FDynamicMeshAABBTree3 Tree(&Mesh, true);
		if (!Tree.TestSelfIntersection()) return false;
		const auto Intersections = Tree.FindAllSelfIntersections(true, FDynamicMeshAABBTree3::FQueryOptions(), [](FIntrTriangle3Triangle3d& Intersection) { Intersection.SetReportCoplanarIntersection(true); return Intersection.Find(); });
		return !Intersections.Points.IsEmpty() || !Intersections.Segments.IsEmpty() || !Intersections.Polygons.IsEmpty();
	}
	FDynamicMesh3 BoxMesh(const FBox& Box)
	{
		FMinimalBoxMeshGenerator Generator;
		Generator.Box = FOrientedBox3d(Box.GetCenter(), Box.GetExtent()); Generator.Generate();
		return FDynamicMesh3(&Generator);
	}
	FVector Tangent(const FMiningSpatialKnot& First, const FMiningSpatialKnot& Last, double Parameter)
	{
		const double Inverse = 1 - Parameter;
		return (3 * Inverse * Inverse * First.Leave + 6 * Inverse * Parameter * (Last.Position + Last.Arrive - First.Position - First.Leave) - 3 * Parameter * Parameter * Last.Arrive).GetSafeNormal();
	}
	void ProfileStations(const TArray<FVector2D>& Profile, TArray<double>& Stations)
	{
		Stations = {0};
		for (int32 Index = 0; Index < Profile.Num(); ++Index) Stations.Add(Stations.Last() + FVector2D::Distance(Profile[Index], Profile[(Index + 1) % Profile.Num()]));
		const double Length = Stations.Last();
		for (double& Station : Stations) Station /= Length;
	}
	FVector2D ProfileAt(const TArray<FVector2D>& Profile, const TArray<double>& Stations, double Station)
	{
		for (int32 Index = 0; Index < Profile.Num(); ++Index)
			if (Station <= Stations[Index + 1]) return FMath::Lerp(Profile[Index], Profile[(Index + 1) % Profile.Num()], (Station - Stations[Index]) / (Stations[Index + 1] - Stations[Index]));
		return Profile[0];
	}
}

TArray<FMiningPenPoint> MiningSpatial::Rectangle(double Width, double Height)
{
	TArray<FMiningPenPoint> Result;
	for (const FVector2D& Position : {FVector2D(-Width, -Height), FVector2D(Width, -Height), FVector2D(Width, Height), FVector2D(-Width, Height)})
	{
		FMiningPenPoint Point; Point.Position = Position * 0.5; Result.Add(Point);
	}
	return Result;
}

TArray<FMiningPenPoint> MiningSpatial::Ellipse(double Width, double Height)
{
	TArray<FMiningPenPoint> Result;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const double Angle = Index * UE_DOUBLE_PI * 0.5;
		FMiningPenPoint Point;
		Point.Position = FVector2D(FMath::Cos(Angle) * Width, FMath::Sin(Angle) * Height) * 0.5;
		Point.Leave = FVector2D(-FMath::Sin(Angle) * Width, FMath::Cos(Angle) * Height) * 0.276142375;
		Point.Arrive = -Point.Leave; Point.Mode = EMiningPenMode::Smooth; Result.Add(Point);
	}
	return Result;
}

FVector MiningSpatial::Evaluate(const FMiningSpatialKnot& First, const FMiningSpatialKnot& Last, double Parameter)
{
	const double Inverse = 1 - Parameter;
	return First.Position * Inverse * Inverse * Inverse + (First.Position + First.Leave) * 3 * Inverse * Inverse * Parameter + (Last.Position + Last.Arrive) * 3 * Inverse * Parameter * Parameter + Last.Position * Parameter * Parameter * Parameter;
}

bool MiningSpatial::Insert(TArray<FMiningSpatialKnot>& Knots, int32 Segment, double Parameter)
{
	if (!Knots.IsValidIndex(Segment + 1) || Segment < 0 || Knots.Num() >= 64 || Parameter <= 0 || Parameter >= 1 || Knots[Segment].bLocked || Knots[Segment + 1].bLocked) return false;
	const FMiningSpatialKnot First = Knots[Segment], Last = Knots[Segment + 1];
	const FVector FirstSplit = FMath::Lerp(First.Position, First.Position + First.Leave, Parameter);
	const FVector MiddleSplit = FMath::Lerp(First.Position + First.Leave, Last.Position + Last.Arrive, Parameter);
	const FVector LastSplit = FMath::Lerp(Last.Position + Last.Arrive, Last.Position, Parameter);
	const FVector Arrive = FMath::Lerp(FirstSplit, MiddleSplit, Parameter), Leave = FMath::Lerp(MiddleSplit, LastSplit, Parameter);
	FMiningSpatialKnot Inserted = First;
	Inserted.Position = FMath::Lerp(Arrive, Leave, Parameter); Inserted.Arrive = Arrive - Inserted.Position; Inserted.Leave = Leave - Inserted.Position;
	Inserted.Roll = FMath::Lerp(First.Roll, Last.Roll, Parameter); Inserted.SectionScale = FMath::Lerp(First.SectionScale, Last.SectionScale, Parameter);
	if (First.Profile.Num() == Last.Profile.Num())
		for (int32 Index = 0; Index < Inserted.Profile.Num(); ++Index)
		{
			Inserted.Profile[Index].Position = FMath::Lerp(First.Profile[Index].Position, Last.Profile[Index].Position, Parameter);
			Inserted.Profile[Index].Arrive = FMath::Lerp(First.Profile[Index].Arrive, Last.Profile[Index].Arrive, Parameter);
			Inserted.Profile[Index].Leave = FMath::Lerp(First.Profile[Index].Leave, Last.Profile[Index].Leave, Parameter);
		}
	Knots[Segment].Leave = FirstSplit - First.Position; Knots[Segment + 1].Arrive = LastSplit - Last.Position;
	Knots.Insert(Inserted, Segment + 1); return true;
}

bool MiningSpatial::Sample(const TArray<FMiningSpatialKnot>& Knots, const FVector& InitialUp, double Tolerance, TArray<FRing>& Rings, FString& Error, FDiagnostic* Diagnostic, bool bRelaxed)
{
	Rings.Reset();
	if (Diagnostic) *Diagnostic = FDiagnostic();
	if (Knots.Num() < 2 || Knots.Num() > 64 || InitialUp.ContainsNaN() || InitialUp.IsNearlyZero() || !FMath::IsFinite(Tolerance) || Tolerance < 2 || Tolerance > 50) { Error = TEXT("三维节点数量、初始方向或采样误差无效"); return false; }
	TArray<TArray<FVector2D>> Profiles;
	TArray<TArray<double>> Stations;
	TArray<double> Common{0};
	for (const FMiningSpatialKnot& Knot : Knots)
	{
		if (Diagnostic) { Diagnostic->Knot = Profiles.Num(); Diagnostic->Location = Knot.Position; Diagnostic->Bounds = FBox(Knot.Position - FVector(150), Knot.Position + FVector(150)); }
		if (Knot.Position.ContainsNaN() || Knot.Arrive.ContainsNaN() || Knot.Leave.ContainsNaN() || !FMath::IsFinite(Knot.Roll) || FMath::Abs(Knot.Roll) > 7200 || Knot.SectionScale.ContainsNaN() || Knot.SectionScale.GetMin() < 0.05 || Knot.SectionScale.GetMax() > 20)
		{ Error = TEXT("节点位置/手柄/倍率无效；截面倍率0.05–20，累计扭转不超过7200度"); return false; }
		TArray<FVector2D>& Profile = Profiles.AddDefaulted_GetRef(); TArray<FIntVector> Triangles;
		if (!MiningManual::Sample(Knot.Profile, true, Tolerance, Profile, Error) || !MiningManual::Triangulate(Profile, Triangles, Error)) return false;
		double Area = 0;
		for (int32 Index = 0; Index < Profile.Num(); ++Index) Area += FVector2D::CrossProduct(Profile[Index], Profile[(Index + 1) % Profile.Num()]);
		if (Area < 0) Algo::Reverse(MakeArrayView(Profile.GetData() + 1, Profile.Num() - 1));
		TArray<double>& LocalStations = Stations.AddDefaulted_GetRef(); ProfileStations(Profile, LocalStations);
		for (double Station : LocalStations)
			if (Station < 0.99999 && !Common.ContainsByPredicate([Station](double Existing) { return FMath::Abs(Existing - Station) < 0.00001; })) Common.Add(Station);
		for (FVector2D& Point : Profile) Point *= Knot.SectionScale;
	}
	Common.Sort();
	if (Common.Num() > 64) { Error = TEXT("各截面合并后超过64个轮廓采样点；减少轮廓细节或增加曲线采样误差"); return false; }
	FVector PreviousTangent = FVector::ZeroVector, TransportedUp = InitialUp.GetSafeNormal();
	for (int32 Segment = 0; Segment < Knots.Num() - 1; ++Segment)
	{
		const FMiningSpatialKnot& First = Knots[Segment]; const FMiningSpatialKnot& Last = Knots[Segment + 1];
		if (Diagnostic) { Diagnostic->Knot = Segment; Diagnostic->Segment = Segment; Diagnostic->Bounds = FBox(ForceInit); Diagnostic->Bounds += First.Position; Diagnostic->Bounds += Last.Position; Diagnostic->Location = (First.Position + Last.Position) * 0.5; }
		const double ControlLength = First.Leave.Length() + FVector::Distance(First.Position + First.Leave, Last.Position + Last.Arrive) + Last.Arrive.Length();
		if (ControlLength < 10 || FVector::Distance(First.Position, Last.Position) < 1) { Error = TEXT("相邻节点太近，请分开或删除重复节点"); return false; }
		const int32 Steps = FMath::Max3<int32>(2, FMath::CeilToInt(ControlLength / FMath::Min(100.0, Tolerance * 12)), FMath::CeilToInt(FMath::Abs(Last.Roll - First.Roll) / 8));
		if (Rings.Num() + Steps + 1 > 256) { Error = TEXT("单条隧道超过256个扫掠截面；请分成多条相接的隧道，预算不提升"); return false; }
		for (int32 Step = Segment ? 1 : 0; Step <= Steps; ++Step)
		{
			const double Parameter = Step / static_cast<double>(Steps);
			FRing Ring; Ring.Center = Evaluate(First, Last, Parameter); Ring.Tangent = Tangent(First, Last, Parameter); Ring.Segment = Segment;
			if (Diagnostic) { Diagnostic->Location = Ring.Center; Diagnostic->Ring = Rings.Num(); }
			if (Ring.Tangent.IsNearlyZero() && bRelaxed) Ring.Tangent = (Evaluate(First, Last, FMath::Min(1.0, Parameter + 0.001)) - Evaluate(First, Last, FMath::Max(0.0, Parameter - 0.001))).GetSafeNormal();
			if (Ring.Tangent.IsNearlyZero()) { Error = TEXT("曲线有零切线/尖点，请拉开节点的曲线手柄"); return false; }
			if (!PreviousTangent.IsNearlyZero())
			{
				if (!bRelaxed && FVector::DotProduct(PreviousTangent, Ring.Tangent) < 0.75) { Error = FString::Printf(TEXT("第%d段采样%d/%d转向过急（%.1f度）；请拉长手柄或增加平滑节点"), Segment + 1, Step, Steps, FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(PreviousTangent, Ring.Tangent)))); return false; }
				TransportedUp = FQuat::FindBetweenNormals(PreviousTangent, Ring.Tangent).RotateVector(TransportedUp);
			}
			TransportedUp = (TransportedUp - Ring.Tangent * FVector::DotProduct(TransportedUp, Ring.Tangent)).GetSafeNormal();
			if (TransportedUp.IsNearlyZero())
			{
				const FVector Fallback = FMath::Abs(Ring.Tangent.Z) < 0.9 ? FVector::UpVector : FVector::RightVector;
				TransportedUp = (Fallback - Ring.Tangent * FVector::DotProduct(Fallback, Ring.Tangent)).GetSafeNormal();
			}
			const FQuat Twist(Ring.Tangent, FMath::DegreesToRadians(FMath::Lerp(static_cast<double>(First.Roll), static_cast<double>(Last.Roll), Parameter)));
			Ring.Up = Twist.RotateVector(TransportedUp); Ring.Right = FVector::CrossProduct(Ring.Up, Ring.Tangent).GetSafeNormal();
			Ring.Knot = Step == 0 ? Segment : Step == Steps ? Segment + 1 : INDEX_NONE;
			for (double Station : Common) Ring.Profile.Add(FMath::Lerp(ProfileAt(Profiles[Segment], Stations[Segment], Station), ProfileAt(Profiles[Segment + 1], Stations[Segment + 1], Station), Parameter));
			TArray<FIntVector> Triangles;
			if (!MiningManual::Triangulate(Ring.Profile, Triangles, Error)) { Error = TEXT("截面过渡折叠：") + Error; return false; }
			PreviousTangent = Ring.Tangent; Rings.Add(MoveTemp(Ring));
		}
	}
	return true;
}

bool MiningSpatial::Sweep(const TArray<FRing>& Rings, FDynamicMesh3& Mesh, FString& Error, bool bCheckIntersections, FDiagnostic* Diagnostic, bool bResolveOverlaps)
{
	Mesh.Clear();
	if (Diagnostic) *Diagnostic = FDiagnostic();
	if (Rings.Num() < 2) { Error = TEXT("扫掠至少需要两个截面"); return false; }
	const int32 Count = Rings[0].Profile.Num();
	if (Count < 3 || Count > 64 || Rings.Num() > 256) { Error = TEXT("扫掠采样预算无效"); return false; }
	for (const FRing& Ring : Rings)
	{
		if (Ring.Profile.Num() != Count) { Error = TEXT("截面对应顶点数不一致"); return false; }
		for (const FVector2D& Point : Ring.Profile) Mesh.AppendVertex(Ring.Center + Ring.Right * Point.X + Ring.Up * Point.Y);
	}
	for (int32 Section = 0; Section < Rings.Num() - 1; ++Section)
		for (int32 Vertex = 0; Vertex < Count; ++Vertex)
		{
			const int32 First = Section * Count + Vertex, Next = Section * Count + (Vertex + 1) % Count;
			const FVector Forward = (Rings[Section].Tangent + Rings[Section + 1].Tangent).GetSafeNormal();
			if (!bResolveOverlaps && FVector::DotProduct(Mesh.GetVertex(First + Count) - Mesh.GetVertex(First), Forward) <= 0.001)
			{
				if (Diagnostic)
				{
					Diagnostic->Ring = Section; Diagnostic->Segment = Rings[Section + 1].Segment; Diagnostic->Knot = Diagnostic->Segment;
					Diagnostic->Location = (Rings[Section].Center + Rings[Section + 1].Center) * 0.5;
					Diagnostic->Bounds = FBox(ForceInit);
					for (int32 Offset = Section * Count; Offset < (Section + 2) * Count; ++Offset) Diagnostic->Bounds += Mesh.GetVertex(Offset);
				}
				Error = FString::Printf(TEXT("第%d个扫掠截面内弯折叠：请拉大转弯半径或减小截面，不能让相邻截面反向穿插"), Section + 1); return false;
			}
			Mesh.AppendTriangle(First, Next + Count, Next); Mesh.AppendTriangle(First, First + Count, Next + Count);
		}
	for (int32 End = 0; End < 2; ++End)
	{
		TArray<FIntVector> Triangles;
		if (!MiningManual::Triangulate(End ? Rings.Last().Profile : Rings[0].Profile, Triangles, Error)) return false;
		const int32 Offset = End ? (Rings.Num() - 1) * Count : 0;
		for (const FIntVector& Triangle : Triangles)
			if (End) Mesh.AppendTriangle(Offset + Triangle.Z, Offset + Triangle.Y, Offset + Triangle.X);
			else Mesh.AppendTriangle(Triangle.X, Triangle.Y, Triangle.Z);
	}
	if (!Mesh.IsClosed() || !Mesh.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly)) { Error = TEXT("三维扫掠网格不封闭，保留旧结果"); return false; }
	for (int32 Triangle : Mesh.TriangleIndicesItr())
		if (Mesh.GetTriArea(Triangle) < 0.00001) { Error = TEXT("截面产生退化三角形，请放大尖角或调整轮廓"); return false; }
	if (bCheckIntersections)
	{
		if (HasSelfIntersections(Mesh))
		{
			FString UnionStatus;
			if (bResolveOverlaps)
			{
				const FDynamicMesh3 Original = Mesh;
				for (int32 Attempt = 0; Attempt < 8; ++Attempt)
				{
					const double Tolerance = FMath::Pow(10.0, -6 + Attempt / 2);
					Mesh = Original;
					FMeshSelfUnion Operation(&Mesh); Operation.SnapTolerance = Tolerance; Operation.NormalOffset = Tolerance * 2;
					Operation.bCollapseDegenerateEdgesOnCut = Attempt % 2 == 0;
					const bool bComputed = Operation.Compute();
					const bool bClosed = Mesh.TriangleCount() > 0 && Mesh.IsClosed();
					const bool bValid = Mesh.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly);
					const bool bIntersections = HasSelfIntersections(Mesh);
					if (bComputed && Operation.CreatedBoundaryEdges.IsEmpty() && bClosed && bValid && !bIntersections) return true;
					UnionStatus = FString::Printf(TEXT("（边界%d，封闭%d，自交%d）"), Operation.CreatedBoundaryEdges.Num(), bClosed, bIntersections);
				}
				if (Rings.Num() > 3)
				{
					const int32 Middle = Rings.Num() / 2;
					TArray<FRing> FirstRings, LastRings;
					FirstRings.Append(Rings.GetData(), Middle + 1);
					LastRings.Append(Rings.GetData() + Middle - 1, Rings.Num() - Middle + 1);
					FDynamicMesh3 First, Last, Combined; FString PieceError;
					if (Sweep(FirstRings, First, PieceError, true, nullptr, true) && Sweep(LastRings, Last, PieceError, true, nullptr, true) &&
						MiningChunks::Union(First, Last, Combined, true) && Combined.TriangleCount() > 0 && !HasSelfIntersections(Combined))
					{
						Mesh = MoveTemp(Combined); return true;
					}
				}
				Mesh = Original;
			}
			if (Diagnostic) { for (int32 Vertex : Mesh.VertexIndicesItr()) Diagnostic->Bounds += Mesh.GetVertex(Vertex); Diagnostic->Location = Diagnostic->Bounds.GetCenter(); }
			Error = bResolveOverlaps ? TEXT("交汇表面尚未能合并成封闭实体；设计保留，可以继续移动节点或拆成独立相交通道。") + UnionStatus : TEXT("隧道自交或内弯挤穿：减小截面/扭转，拉大转弯半径；分支用独立隧道相交"); return false;
		}
	}
	return true;
}

bool MiningSpatial::Shape(const AMiningSpatialTunnel& Source, double Tolerance, FDynamicMesh3& Mesh, TArray<FRing>& Rings, FString& Error, FDiagnostic* Diagnostic)
{
	if (Source.Kind == EMiningManualKind::SideRoom)
	{
		Rings.Reset();
		if (Source.RoomSize.ContainsNaN() || Source.RoomSize.GetMin() < 250 || Source.GetActorTransform().ContainsNaN() || !Source.GetActorScale3D().Equals(FVector::OneVector)) { Error = TEXT("支线房间尺寸至少250cm，缩放为1；支持任意旋转"); return false; }
		Mesh = BoxMesh(FBox(-Source.RoomSize * 0.5, Source.RoomSize * 0.5));
		for (int32 Vertex : Mesh.VertexIndicesItr()) Mesh.SetVertex(Vertex, Source.GetActorTransform().TransformPosition(Mesh.GetVertex(Vertex)));
		return true;
	}
	TArray<FMiningSpatialKnot> Resolved;
	const bool bRelaxed = Source.Region && Source.Region->bAutomaticBedrock;
	return Source.ResolveKnots(Resolved, Error) && Sample(Resolved, Source.GetActorTransform().TransformVectorNoScale(Source.InitialUp), Tolerance, Rings, Error, Diagnostic, bRelaxed) && Sweep(Rings, Mesh, Error, true, Diagnostic, bRelaxed);
}

void MiningSpatial::StoreSurface(const FDynamicMesh3& Mesh, FMiningMapRegion& Region)
{
	Region.Vertices.Reset(); Region.SurfaceTriangles.Reset(); Region.Bounds = FBox(ForceInit);
	TMap<int32, int32> Indices;
	for (int32 Vertex : Mesh.VertexIndicesItr()) { const FVector Position = Mesh.GetVertex(Vertex); Indices.Add(Vertex, Region.Vertices.Add(Position)); Region.Bounds += Position; }
	for (int32 Triangle : Mesh.TriangleIndicesItr())
	{
		const FIndex3i Face = Mesh.GetTriangle(Triangle); Region.SurfaceTriangles.Add(FIntVector(Indices[Face.A], Indices[Face.B], Indices[Face.C]));
	}
}

bool MiningSpatial::BuildGeometry(const FRegionInput& Input, FRegionBuild& Output)
{
	MiningManual::FPlan& Result = Output.Plan;
	MiningHollow::FPlan& Hollow = Output.Hollow;
	FDynamicMesh3& Rock = Output.Rock;
	FString& Error = Output.Error;
	const FVector Origin = Input.Origin;
	const double Bedrock = Input.Bedrock;
	const bool bAutomatic = Input.bAutomaticBedrock;
	Hollow.Bounds = Input.Bounds.ShiftBy(-Origin); Result.bAutomaticBedrock = bAutomatic;
	Result.StartWall = Input.StartWall; Result.EndWall = Input.EndWall;
	FDynamicMesh3 Candidate;
	if (!bAutomatic) Candidate = BoxMesh(Hollow.Bounds);
	FDynamicMesh3 Air;
	TArray<FDynamicMesh3> Blocks;
	if (bAutomatic)
		for (const FBox& Room : Input.Rooms)
			if (!AddEnvelope(BoxMesh(Room), {}, Origin, Input.ExcavationMargin, Bedrock, Result.RockDomain, Result.OuterDomain, Error)) return false;
	for (int32 SourceIndex = 0; SourceIndex < Input.Sources.Num(); ++SourceIndex)
	{
		const FSourceInput& Source = Input.Sources[SourceIndex];
		Output.ErrorSource = SourceIndex;
		FDynamicMesh3 Cutter; TArray<FRing> Rings;
		if (Source.Kind == EMiningManualKind::SideRoom)
		{
			Cutter = BoxMesh(FBox(-Source.Size * 0.5, Source.Size * 0.5));
			for (int32 Vertex : Cutter.VertexIndicesItr()) Cutter.SetVertex(Vertex, Source.Transform.TransformPosition(Cutter.GetVertex(Vertex)));
		}
		else if (!Sample(Source.Knots, Source.Up, Input.Tolerance, Rings, Error, &Output.Diagnostic, bAutomatic) || !Sweep(Rings, Cutter, Error, true, &Output.Diagnostic, bAutomatic)) { Error = Source.Name + TEXT(": ") + Error; return false; }
		if (bAutomatic && !AddEnvelope(Cutter, Rings, Origin, Input.ExcavationMargin, Bedrock, Result.RockDomain, Result.OuterDomain, Error)) { Error = Source.Name + TEXT(": ") + Error; return false; }
		if (!bAutomatic) for (int32 Vertex : Cutter.VertexIndicesItr())
			if (!Input.Bounds.ExpandBy(0.1).IsInsideOrOn(Cutter.GetVertex(Vertex))) { Error = Source.Name + TEXT(": 超出矿区范围，请扩展岩石尺寸"); return false; }
		FMiningMapRegion Map;
		Map.Name = Source.Name; Map.Kind = Source.bHidden ? EMiningMapRegionKind::HiddenEntrance : Source.Kind == EMiningManualKind::Passage ? EMiningMapRegionKind::Tunnel : Source.Kind == EMiningManualKind::SideRoom ? EMiningMapRegionKind::Branch : EMiningMapRegionKind::Space;
		if (Source.Kind == EMiningManualKind::RockBlock) { Map.Kind = EMiningMapRegionKind::GuidePlug; Map.bRockFilled = true; }
		StoreSurface(Cutter, Map); Result.SpatialMaps.Add(MoveTemp(Map));
		if (Source.bMain)
			for (const FRing& Ring : Rings)
			{
				if (!Hollow.Points.IsEmpty()) Hollow.Length += FVector::Distance(Hollow.Points.Last(), Ring.Center - Origin);
				Hollow.Points.Add(Ring.Center - Origin); Hollow.Distances.Add(Hollow.Length);
			}
		if (Source.Kind == EMiningManualKind::Passage)
			for (int32 End = 0; End < 2; ++End)
			{
				const FPortalInput& Portal = Source.Portals[End];
				if (!Portal.bBound) continue;
				if (bAutomatic)
				{
					if (End) Rings.Last().Center -= Portal.Normal * 2;
					else Rings[0].Center -= Portal.Normal * 2;
					continue;
				}
				const FTransform& Transform = Portal.Transform;
				const FVector HalfSize = Portal.HalfSize, Center = Portal.Center, Normal = Portal.Normal;
				const FRing Endpoint = End ? Rings.Last() : Rings[0];
				const int32 Axis = Portal.Wall / 2;
				for (const FVector2D& Point : Endpoint.Profile)
				{
					const FVector Local = Transform.InverseTransformPosition(Endpoint.Center + Endpoint.Right * Point.X + Endpoint.Up * Point.Y);
					if (FMath::Abs(Local[(Axis + 1) % 3]) > HalfSize[(Axis + 1) % 3] - 1 || FMath::Abs(Local[(Axis + 2) % 3]) > HalfSize[(Axis + 2) % 3] - 1) { Error = TEXT("旋转后的入口截面超出房间墙面；扩大房间或缩小入口截面"); return false; }
				}
				FRing Inner = Endpoint, Outer = Endpoint;
				Inner.Center -= Normal * (Bedrock + 4); Outer.Center += Normal * (Bedrock + 4);
				TArray<FRing> PortalRings = End ? TArray<FRing>{Outer, Inner} : TArray<FRing>{Inner, Outer};
				FDynamicMesh3 PortalMesh;
				if (!Sweep(PortalRings, PortalMesh, Error)) return false;
				for (int32 Vertex : PortalMesh.VertexIndicesItr()) PortalMesh.SetVertex(Vertex, PortalMesh.GetVertex(Vertex) - Origin);
				Result.PortalCuts.Add(MoveTemp(PortalMesh));
				if (End) Rings.Last().Center -= Normal * 2;
				else Rings[0].Center -= Normal * 2;
			}
		if (Source.Kind == EMiningManualKind::Passage && !Sweep(Rings, Cutter, Error, true, nullptr, bAutomatic)) return false;
		for (int32 Vertex : Cutter.VertexIndicesItr()) Cutter.SetVertex(Vertex, Cutter.GetVertex(Vertex) - Origin);
		Output.Cutters.Add(Cutter);
		if (Source.Kind == EMiningManualKind::RockBlock) { Blocks.Add(MoveTemp(Cutter)); continue; }
		if (Air.TriangleCount() == 0) Air = MoveTemp(Cutter);
		else
		{
			FDynamicMesh3 Combined;
			if (!MiningChunks::Union(Air, Cutter, Combined, true)) { Error = Source.Name + TEXT(": 三维空洞连接失败，旧地形保留"); return false; }
			Air = MoveTemp(Combined);
		}
	}
	for (const FDynamicMesh3& Block : Blocks)
	{
		FDynamicMesh3 OpenAir;
		if (!MiningChunks::Subtract(Air, Block, OpenAir, true)) { Error = TEXT("岩石保留区切除失败；旧地形保留"); return false; }
		Air = MoveTemp(OpenAir);
	}
	if (bAutomatic)
	{
		Candidate = Result.RockDomain; Result.Air = Air;
		const FAxisAlignedBox3d Bounds = Candidate.GetBounds(); Hollow.Bounds = FBox(FVector(Bounds.Min), FVector(Bounds.Max));
		Output.Cutters.Add(Candidate);
	}
	FDynamicMesh3 CarvedAir;
	if (!MiningChunks::Subtract(Candidate, Air, CarvedAir, true)) { Error = TEXT("三维空间切除失败，旧地形保留"); return false; }
	Candidate = MoveTemp(CarvedAir);
	for (const FBox& Room : Input.Rooms)
	{
		FDynamicMesh3 Carved;
		if (!MiningChunks::Subtract(Candidate, BoxMesh(Room.ShiftBy(-Origin)), Carved, true)) { Error = TEXT("主房间清理失败"); return false; }
		Candidate = MoveTemp(Carved);
	}
	if (HasSelfIntersections(Candidate))
	{
		Error = TEXT("连接后存在自交/重合面，请调整交汇处截面或接入位置；旧地形保留"); return false;
	}
	Output.ErrorSource = INDEX_NONE; Rock = MoveTemp(Candidate); return true;
}

bool MiningSpatial::Build(AMiningSpatialRegion& Region, const FVector& Origin, double Bedrock, MiningHollow::FPlan& Hollow, MiningManual::FPlan& Result, FDynamicMesh3& Rock, FString& Error)
{
	FRegionInput Input;
	if (!Capture(Region, Origin, Bedrock, Input, Error)) return false;
	const FRegionBuild* Cached = Prepared(&Region);
	if (Cached && (!Cached->bSuccess || Cached->Input.Key != Input.Key)) { Error = TEXT("Prepared spatial geometry no longer matches source; previous terrain preserved"); return false; }
	FRegionBuild Local;
	if (!Cached || Cached->Input.Key != Input.Key)
	{
		Local.Input = MoveTemp(Input);
		if (!BuildGeometry(Local.Input, Local)) { Error = Local.Error; return false; }
		Cached = &Local;
	}
	Hollow = Cached->Hollow; Result = Cached->Plan; Rock = Cached->Rock;
	for (AMiningSpatialTunnel* Source : Region.GetTunnels())
	{
		if (Source->bEnabled) Result.SpatialSources.Add(Source);
		else Source->ShapeStatus = TEXT("已禁用，保留编辑数据");
	}
	return true;
}
