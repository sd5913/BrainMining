#include "MiningManualGeometry.h"
#include "MiningSpatialGeometry.h"
#include "MiningManualRegion.h"
#include "MiningLayoutRoom.h"
#include "MiningChunkGeometry.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "Operations/MeshBoolean.h"

using namespace UE::Geometry;

namespace
{
double Cross(const FVector2D& First, const FVector2D& Second, const FVector2D& Third)
{
	return FVector2D::CrossProduct(Second - First, Third - First);
}

bool OnEdge(const FVector2D& Point, const FVector2D& First, const FVector2D& Last)
{
	return FMath::Abs(Cross(First, Last, Point)) < 0.001 && FVector2D::DotProduct(Point - First, Point - Last) <= 0.001;
}

bool Intersects(const FVector2D& First, const FVector2D& Last, const FVector2D& OtherFirst, const FVector2D& OtherLast)
{
	return (Cross(First, Last, OtherFirst) * Cross(First, Last, OtherLast) < 0 && Cross(OtherFirst, OtherLast, First) * Cross(OtherFirst, OtherLast, Last) < 0) ||
		OnEdge(First, OtherFirst, OtherLast) || OnEdge(Last, OtherFirst, OtherLast) || OnEdge(OtherFirst, First, Last) || OnEdge(OtherLast, First, Last);
}

FDynamicMesh3 BoxMesh(const FBox& Bounds)
{
	FMinimalBoxMeshGenerator Generator;
	Generator.Box = FOrientedBox3d(FVector3d(Bounds.GetCenter()), FVector3d(Bounds.GetExtent())); Generator.Generate();
	return FDynamicMesh3(&Generator);
}

bool Subdivide(const FVector2D& Start, const FVector2D& First, const FVector2D& Second, const FVector2D& End, double Tolerance, int32 Level, TArray<FVector2D>& Output)
{
	const double Chord = FVector2D::Distance(Start, End);
	const double Excess = FVector2D::Distance(Start, First) + FVector2D::Distance(First, Second) + FVector2D::Distance(Second, End) - Chord;
	const double Distance = Chord > 0.001 ? FMath::Max(FMath::Abs(Cross(Start, End, First)), FMath::Abs(Cross(Start, End, Second))) / Chord : Excess;
	if (Distance <= Tolerance && Excess <= Tolerance)
	{
		Output.Add(End); return Output.Num() <= 512;
	}
	if (Level >= 16 || Output.Num() >= 512) return false;
	const FVector2D FirstHalf = (Start + First) * 0.5, MiddleHalf = (First + Second) * 0.5, LastHalf = (Second + End) * 0.5;
	const FVector2D Left = (FirstHalf + MiddleHalf) * 0.5, Right = (MiddleHalf + LastHalf) * 0.5, Middle = (Left + Right) * 0.5;
	return Subdivide(Start, FirstHalf, Left, Middle, Tolerance, Level + 1, Output) && Subdivide(Middle, Right, LastHalf, End, Tolerance, Level + 1, Output);
}
}

FVector2D MiningManual::Evaluate(const FMiningPenPoint& First, const FMiningPenPoint& Last, double Parameter)
{
	const double Inverse = 1 - Parameter;
	const FVector2D FirstControl = First.Position + (First.Mode == EMiningPenMode::Corner ? FVector2D::ZeroVector : First.Leave);
	const FVector2D LastControl = Last.Position + (Last.Mode == EMiningPenMode::Corner ? FVector2D::ZeroVector : Last.Arrive);
	return First.Position * Inverse * Inverse * Inverse + FirstControl * 3 * Inverse * Inverse * Parameter + LastControl * 3 * Inverse * Parameter * Parameter + Last.Position * Parameter * Parameter * Parameter;
}

bool MiningManual::Insert(TArray<FMiningPenPoint>& Points, int32 Segment, double Parameter, bool bClosed)
{
	if (!Points.IsValidIndex(Segment) || Points.Num() < 2 || (!bClosed && Segment >= Points.Num() - 1) || Parameter <= 0.001 || Parameter >= 0.999) return false;
	const int32 Next = (Segment + 1) % Points.Num();
	if (Points[Segment].bLocked || Points[Next].bLocked) return false;
	FMiningPenPoint& First = Points[Segment]; FMiningPenPoint& Last = Points[Next];
	const FVector2D FirstControl = First.Position + (First.Mode == EMiningPenMode::Corner ? FVector2D::ZeroVector : First.Leave);
	const FVector2D LastControl = Last.Position + (Last.Mode == EMiningPenMode::Corner ? FVector2D::ZeroVector : Last.Arrive);
	const FVector2D Left = FMath::Lerp(First.Position, FirstControl, Parameter), Middle = FMath::Lerp(FirstControl, LastControl, Parameter), Right = FMath::Lerp(LastControl, Last.Position, Parameter);
	const FVector2D LeftControl = FMath::Lerp(Left, Middle, Parameter), RightControl = FMath::Lerp(Middle, Right, Parameter);
	FMiningPenPoint Added; Added.Position = FMath::Lerp(LeftControl, RightControl, Parameter);
	Added.GuideStation = First.GuideStation >= 0 && Last.GuideStation > First.GuideStation ? FMath::Lerp(First.GuideStation, Last.GuideStation, Parameter) : -1;
	Added.Arrive = LeftControl - Added.Position; Added.Leave = RightControl - Added.Position; Added.Mode = EMiningPenMode::Independent;
	if (First.Mode == EMiningPenMode::Corner) First.Arrive = FVector2D::ZeroVector;
	if (Last.Mode == EMiningPenMode::Corner) Last.Leave = FVector2D::ZeroVector;
	First.Leave = Left - First.Position; Last.Arrive = Right - Last.Position;
	First.Mode = EMiningPenMode::Independent; Last.Mode = EMiningPenMode::Independent;
	Points.Insert(Added, Segment + 1); return true;
}

bool MiningManual::GuideBound(const TArray<FMiningPenPoint>& Points)
{
	if (Points.Num() < 2 || !FMath::IsNearlyEqual(Points[0].GuideStation, 0.0f) || !FMath::IsNearlyEqual(Points.Last().GuideStation, 1.0f)) return false;
	for (int32 Index = 1; Index < Points.Num(); ++Index)
		if (!FMath::IsFinite(Points[Index].GuideStation) || Points[Index].GuideStation <= Points[Index - 1].GuideStation) return false;
	return true;
}

bool MiningManual::BindGuide(TArray<FMiningPenPoint>& Points)
{
	if (Points.Num() < 2) return false;
	double Length = 0;
	Points[0].GuideStation = 0;
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		double SegmentLength = 0; FVector2D Previous = Points[Index - 1].Position;
		for (int32 Step = 1; Step <= 32; ++Step)
		{
			const FVector2D Current = Evaluate(Points[Index - 1], Points[Index], Step / 32.0);
			SegmentLength += FVector2D::Distance(Current, Previous); Previous = Current;
		}
		Length += FMath::Max(1.0, SegmentLength); Points[Index].GuideStation = Length;
	}
	if (!FMath::IsFinite(Length) || Length <= 0) return false;
	for (FMiningPenPoint& Point : Points) Point.GuideStation /= Length;
	return true;
}

FVector2D MiningManual::GuidePoint(const TArray<FMiningPenPoint>& Points, double Station)
{
	if (!GuideBound(Points)) return FVector2D::ZeroVector;
	for (int32 Index = 1; Index < Points.Num(); ++Index)
		if (Station <= Points[Index].GuideStation)
			return Evaluate(Points[Index - 1], Points[Index], FMath::Clamp((Station - Points[Index - 1].GuideStation) / (Points[Index].GuideStation - Points[Index - 1].GuideStation), 0.0, 1.0));
	return Points.Last().Position;
}

void MiningManual::Bend(TArray<FMiningPenPoint>& Points, double Station, const FVector2D& Delta)
{
	if (!GuideBound(Points) || Delta.ContainsNaN()) return;
	for (int32 Index = 1; Index < Points.Num() - 1; ++Index)
	{
		if (Points[Index].bLocked) continue;
		const double Weight = FMath::Clamp(1 - FMath::Abs(Points[Index].GuideStation - Station) / 0.4, 0.0, 1.0);
		Points[Index].Position += Delta * Weight * Weight * (3 - 2 * Weight);
	}
}

bool MiningManual::Sample(const TArray<FMiningPenPoint>& Points, bool bClosed, double Tolerance, TArray<FVector2D>& Output, FString& Error)
{
	Output.Reset();
	if (Points.Num() < (bClosed ? 3 : 2) || Points.Num() > 128 || !FMath::IsFinite(Tolerance) || Tolerance < 2 || Tolerance > 50)
	{
		Error = TEXT("开放边界至少 2 点，闭合轮廓至少 3 点；每侧最多 128 控制点，采样误差 2–50 cm"); return false;
	}
	for (const FMiningPenPoint& Point : Points)
		if (Point.Position.ContainsNaN() || Point.Arrive.ContainsNaN() || Point.Leave.ContainsNaN()) { Error = TEXT("控制点或手柄含无效数值"); return false; }
	Output.Add(Points[0].Position);
	for (int32 Index = 0; Index < Points.Num() - (bClosed ? 0 : 1); ++Index)
	{
		const FMiningPenPoint& First = Points[Index]; const FMiningPenPoint& Last = Points[(Index + 1) % Points.Num()];
		if (!Subdivide(First.Position, First.Position + (First.Mode == EMiningPenMode::Corner ? FVector2D::ZeroVector : First.Leave), Last.Position + (Last.Mode == EMiningPenMode::Corner ? FVector2D::ZeroVector : Last.Arrive), Last.Position, Tolerance, 0, Output))
		{
			Error = TEXT("曲线采样过密；增大采样误差或拆分空间，不会提高挖掘预算"); return false;
		}
	}
	if (bClosed && Output.Last().Equals(Output[0], 0.001)) Output.Pop();
	return true;
}

bool MiningManual::Contains(const TArray<FVector2D>& Polygon, const FVector2D& Point)
{
	bool bInside = false;
	for (int32 Index = 0; Index < Polygon.Num(); ++Index)
	{
		const FVector2D& First = Polygon[Index]; const FVector2D& Last = Polygon[(Index + 1) % Polygon.Num()];
		if (OnEdge(Point, First, Last)) return true;
		if ((First.Y > Point.Y) != (Last.Y > Point.Y) && Point.X < (Last.X - First.X) * (Point.Y - First.Y) / (Last.Y - First.Y) + First.X) bInside = !bInside;
	}
	return bInside;
}

bool MiningManual::Triangulate(const TArray<FVector2D>& Polygon, TArray<FIntVector>& Triangles, FString& Error)
{
	Triangles.Reset();
	if (Polygon.Num() < 3 || Polygon.Num() > 1024) { Error = TEXT("轮廓顶点数不合法"); return false; }
	double Area = 0;
	for (int32 Index = 0; Index < Polygon.Num(); ++Index)
	{
		const int32 Next = (Index + 1) % Polygon.Num();
		if (Polygon[Index].ContainsNaN() || Polygon[Index].Equals(Polygon[Next], 0.01)) { Error = TEXT("存在重复/过近控制点或无效坐标"); return false; }
		Area += FVector2D::CrossProduct(Polygon[Index], Polygon[Next]);
		for (int32 Other = Index + 1; Other < Polygon.Num(); ++Other)
		{
			const int32 OtherNext = (Other + 1) % Polygon.Num();
			if (Other == Next || OtherNext == Index) continue;
			if (Intersects(Polygon[Index], Polygon[Next], Polygon[Other], Polygon[OtherNext])) { Error = TEXT("轮廓自交、两侧交叉或非相邻边相碰，请移动标记节点"); return false; }
		}
	}
	if (FMath::Abs(Area) < 1) { Error = TEXT("轮廓没有有效面积"); return false; }
	TArray<int32> Remaining;
	for (int32 Index = 0; Index < Polygon.Num(); ++Index) Remaining.Add(Area > 0 ? Index : Polygon.Num() - Index - 1);
	while (Remaining.Num() > 3)
	{
		bool bFound = false;
		for (int32 Index = 0; Index < Remaining.Num(); ++Index)
		{
			const int32 Previous = Remaining[(Index + Remaining.Num() - 1) % Remaining.Num()], Current = Remaining[Index], Next = Remaining[(Index + 1) % Remaining.Num()];
			if (Cross(Polygon[Previous], Polygon[Current], Polygon[Next]) <= 0.0001) continue;
			bool bOccupied = false;
			for (int32 Other : Remaining)
			{
				if (Other == Previous || Other == Current || Other == Next) continue;
				if (Cross(Polygon[Previous], Polygon[Current], Polygon[Other]) >= -0.0001 && Cross(Polygon[Current], Polygon[Next], Polygon[Other]) >= -0.0001 && Cross(Polygon[Next], Polygon[Previous], Polygon[Other]) >= -0.0001) { bOccupied = true; break; }
			}
			if (!bOccupied) { Triangles.Add(FIntVector(Previous, Current, Next)); Remaining.RemoveAt(Index); bFound = true; break; }
		}
		if (!bFound) { Error = TEXT("轮廓三角化失败，检查尖角/重叠边；保留上次可玩结果"); return false; }
	}
	Triangles.Add(FIntVector(Remaining[0], Remaining[1], Remaining[2])); return true;
}

bool MiningManual::Extrude(const TArray<FVector2D>& Polygon, double MinY, double MaxY, FDynamicMesh3& Mesh, FString& Error)
{
	TArray<FIntVector> Triangles;
	if (!FMath::IsFinite(MinY) || !FMath::IsFinite(MaxY) || MaxY - MinY < 1 || !Triangulate(Polygon, Triangles, Error)) return false;
	Mesh.Clear();
	for (double Depth : {MinY, MaxY}) for (const FVector2D& Point : Polygon) Mesh.AppendVertex(FVector3d(Point.X, Depth, Point.Y));
	const int32 Count = Polygon.Num();
	for (const FIntVector& Triangle : Triangles)
	{
		Mesh.AppendTriangle(Triangle.X, Triangle.Y, Triangle.Z);
		Mesh.AppendTriangle(Triangle.Z + Count, Triangle.Y + Count, Triangle.X + Count);
	}
	double Area = 0;
	for (int32 Index = 0; Index < Count; ++Index) Area += FVector2D::CrossProduct(Polygon[Index], Polygon[(Index + 1) % Count]);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 First = Area > 0 ? Index : (Index + 1) % Count, Next = Area > 0 ? (Index + 1) % Count : Index;
		Mesh.AppendTriangle(First, First + Count, Next + Count); Mesh.AppendTriangle(First, Next + Count, Next);
	}
	Mesh.ReverseOrientation();
	if (!Mesh.IsClosed() || !Mesh.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly)) { Error = TEXT("轮廓挤出网格不封闭"); return false; }
	return true;
}

bool MiningManual::Shape(const AMiningManualShape& Source, double Tolerance, FShapePlan& Result, FString& Error)
{
	Result = FShapePlan(); Result.Source = const_cast<AMiningManualShape*>(&Source);
	if (Source.GetActorTransform().ContainsNaN() || !Source.GetActorRotation().IsNearlyZero() || !Source.GetActorScale3D().Equals(FVector::OneVector) || !FMath::IsFinite(Source.Depth) || Source.Depth < 100 || Source.Depth > 8000)
	{
		Error = TEXT("手绘空间需旋转 0、缩放 1、深度 100–8000 cm；使用尺寸和控制点编辑"); return false;
	}
	Result.MinY = Source.GetActorLocation().Y - Source.Depth * 0.5; Result.MaxY = Result.MinY + Source.Depth;
	if (Source.Kind == EMiningManualKind::SideRoom)
	{
		if (Source.Size.ContainsNaN() || Source.Size.GetMin() < 250) { Error = TEXT("支线房间宽高不能小于 250 cm"); return false; }
		const FBox Box = Source.GetRoomBounds();
		Result.Polygon = {FVector2D(Box.Min.X, Box.Min.Z), FVector2D(Box.Max.X, Box.Min.Z), FVector2D(Box.Max.X, Box.Max.Z), FVector2D(Box.Min.X, Box.Max.Z)};
	}
	else
	{
		TArray<FMiningPenPoint> Points;
		if (!Source.ResolveBoundary(false, Points, Error) || !Sample(Points, Source.Kind != EMiningManualKind::Passage, Tolerance, Result.Polygon, Error)) return false;
		if (Source.Kind == EMiningManualKind::Passage)
		{
			TArray<FVector2D> Second;
			if (!Source.ResolveBoundary(true, Points, Error) || !Sample(Points, false, Tolerance, Second, Error)) return false;
			for (int32 Index = Second.Num() - 1; Index >= 0; --Index) Result.Polygon.Add(Second[Index]);
		}
	}
	return Triangulate(Result.Polygon, Result.Triangles, Error);
}

bool MiningManual::Build(AMiningManualRegion& Region, const FVector& Origin, double Bedrock, MiningHollow::FPlan& Hollow, FPlan& Result, FDynamicMesh3& Rock, FString& Error)
{
	if (AMiningSpatialRegion* Spatial = Cast<AMiningSpatialRegion>(&Region)) return MiningSpatial::Build(*Spatial, Origin, Bedrock, Hollow, Result, Rock, Error);
	Result = FPlan(); Hollow = MiningHollow::FPlan();
	if (Region.RockSize.ContainsNaN() || Region.RockSize.GetMin() < 500 || !FMath::IsFinite(Region.CurveTolerance) || Region.CurveTolerance < 2 || Region.CurveTolerance > 50)
	{
		Error = TEXT("手绘矿区尺寸至少 500 cm，采样误差 2–50 cm"); return false;
	}
	if (!Region.GetChambers().IsEmpty() || !Region.Plugs.IsEmpty()) { Error = TEXT("手绘矿区不使用旧洞室/圆管堵块；请用手绘空间对象，旧矿区保持原样"); return false; }
	AMiningManualShape* Main = Region.MainPassage;
	if (!IsValid(Main) || !Main->bEnabled || Main->Region != &Region || Main->Kind != EMiningManualKind::Passage || Main->StartPortal.Room != Region.FromRoom || Main->EndPortal.Room != Region.ToRoom)
	{
		Error = TEXT("指定启用的主线通道，起点/终点入口分别连接本矿区 FromRoom/ToRoom"); return false;
	}
	FVector Start, Finish, Normal;
	if (!Main->ResolvePortal(Main->StartPortal, Start, Normal, Error) || !Main->ResolvePortal(Main->EndPortal, Finish, Normal, Error)) return false;
	Hollow.Bounds = Region.GetRockBounds().ShiftBy(-Origin); Hollow.Points = {Start - Origin, Finish - Origin};
	Hollow.Length = FVector::Distance(Start, Finish); Hollow.Distances = {0, Hollow.Length};
	Result.StartWall = static_cast<int32>(Main->StartPortal.Wall); if (Result.StartWall >= 2) Result.StartWall += 2;
	Result.EndWall = static_cast<int32>(Main->EndPortal.Wall); if (Result.EndWall >= 2) Result.EndWall += 2;
	FDynamicMesh3 Candidate = BoxMesh(Hollow.Bounds);
	for (AMiningManualShape* Source : Region.GetShapes())
	{
		if (!Source->bEnabled) { Source->ShapeStatus = TEXT("已禁用，保留编辑数据"); continue; }
		FShapePlan ShapePlan;
		if (!Shape(*Source, Region.CurveTolerance, ShapePlan, Error)) { Source->ShapeStatus = Error; Error = Source->GetActorNameOrLabel() + TEXT(": ") + Error; return false; }
		for (const FVector2D& Point : ShapePlan.Polygon)
			if (!Region.GetRockBounds().ExpandBy(0.1).IsInsideOrOn(FVector(Point.X, ShapePlan.MinY, Point.Y)) || !Region.GetRockBounds().ExpandBy(0.1).IsInsideOrOn(FVector(Point.X, ShapePlan.MaxY, Point.Y)))
			{
				Error = Source->GetActorNameOrLabel() + TEXT(": 轮廓超出岩石范围；请手动扩展矿区或移动空间"); Source->ShapeStatus = Error; return false;
			}
		if (Source->Kind == EMiningManualKind::RockBlock) { Result.Shapes.Add(MoveTemp(ShapePlan)); continue; }
		TArray<FVector2D> Local = ShapePlan.Polygon;
		if (Source->Kind == EMiningManualKind::Passage)
			for (const FMiningManualPortal* Portal : {&Source->StartPortal, &Source->EndPortal})
			{
				if (!Portal->Room) continue;
				FVector Center, Outward;
				if (!Source->ResolvePortal(*Portal, Center, Outward, Error)) return false;
				const FVector2D Across(-Outward.Z, Outward.X), PortalCenter(Center.X, Center.Z);
				for (FVector2D& Point : Local)
					if (Point.Equals(PortalCenter + Across * Portal->Aperture * 0.5, 0.001) || Point.Equals(PortalCenter - Across * Portal->Aperture * 0.5, 0.001))
						Point -= FVector2D(Outward.X, Outward.Z) * 2;
			}
		for (FVector2D& Point : Local) Point -= FVector2D(Origin.X, Origin.Z);
		FDynamicMesh3 Cutter, Carved;
		if (!Extrude(Local, ShapePlan.MinY - Origin.Y, ShapePlan.MaxY - Origin.Y, Cutter, Error))
		{
			Error = Source->GetActorNameOrLabel() + TEXT(": ") + Error; return false;
		}
		if (!MiningChunks::Subtract(Candidate, Cutter, Carved))
		{
			FMeshBoolean Diagnostic(&Candidate, &Cutter, &Carved, FMeshBoolean::EBooleanOp::Difference);
			const bool bComputed = Diagnostic.Compute();
			Error = FString::Printf(TEXT("%s: 空洞布尔运算失败，旧结果保留 (source=%d cutter=%d result=%d compute=%d boundary=%d closed=%d)"), *Source->GetActorNameOrLabel(), Candidate.TriangleCount(), Cutter.TriangleCount(), Carved.TriangleCount(), bComputed, Diagnostic.CreatedBoundaryEdges.Num(), Carved.IsClosed());
			return false;
		}
		Candidate = MoveTemp(Carved);
		if (Source->Kind == EMiningManualKind::Passage)
			for (const FMiningManualPortal* Portal : {&Source->StartPortal, &Source->EndPortal})
			{
				if (!Portal->Room) continue;
				if (AMiningLayoutRoom* Room = Cast<AMiningLayoutRoom>(Portal->Room); Room && Room != Region.FromRoom && Room != Region.ToRoom)
				{
					Error = TEXT("支线入口不能跨接其他主线房间，避免绕过回收顺序"); return false;
				}
				if ((Portal->Room == Region.FromRoom && Portal->Wall != Main->StartPortal.Wall) || (Portal->Room == Region.ToRoom && Portal->Wall != Main->EndPortal.Wall))
				{
					Error = TEXT("主房间的额外入口必须使用同一连接墙面，避免回收后仍能进入旧矿区"); return false;
				}
				FVector Center, Outward;
				if (!Source->ResolvePortal(*Portal, Center, Outward, Error)) return false;
				const FVector Extent = FMath::Abs(Outward.X) > 0.5 ? FVector(Bedrock + 4, Source->Depth * 0.5, Portal->Aperture * 0.5) : FVector(Portal->Aperture * 0.5, Source->Depth * 0.5, Bedrock + 4);
				FDynamicMesh3 PortalMesh = BoxMesh(FBox(Center - Origin - Extent, Center - Origin + Extent));
				if (!MiningChunks::Subtract(Candidate, PortalMesh, Carved)) { Error = TEXT("入口岩石切除失败"); return false; }
				Candidate = MoveTemp(Carved); Result.PortalCuts.Add(MoveTemp(PortalMesh));
			}
		Result.Shapes.Add(MoveTemp(ShapePlan));
	}
	for (const FShapePlan& Block : Result.Shapes)
		if (Block.Source->Kind == EMiningManualKind::RockBlock)
		{
			TArray<FVector2D> Local = Block.Polygon;
			for (FVector2D& Point : Local) Point -= FVector2D(Origin.X, Origin.Z);
			FDynamicMesh3 Added, Combined;
			if (!Extrude(Local, Block.MinY - Origin.Y, Block.MaxY - Origin.Y, Added, Error) || !MiningChunks::Union(Candidate, Added, Combined))
			{
				Error = Block.Source->GetActorNameOrLabel() + TEXT(": 岩石保留区合并失败，旧结果保留"); return false;
			}
			Candidate = MoveTemp(Combined);
		}
	for (AMiningLayoutRoom* Room : {Region.FromRoom.Get(), Region.ToRoom.Get()})
	{
		FDynamicMesh3 Carved;
		if (!MiningChunks::Subtract(Candidate, BoxMesh(Room->GetRoomBounds().ShiftBy(-Origin)), Carved)) { Error = TEXT("主房间清空失败"); return false; }
		Candidate = MoveTemp(Carved);
	}
	Rock = MoveTemp(Candidate); return true;
}

void MiningManual::AddMapRegions(const FPlan& Plan, int32 Group, TArray<FMiningMapRegion>& Regions)
{
	for (FMiningMapRegion Spatial : Plan.SpatialMaps) { Spatial.GroupIndex = Group; Regions.Add(MoveTemp(Spatial)); }
	for (const FShapePlan& Shape : Plan.Shapes)
	{
		FMiningMapRegion Region;
		Region.Name = Shape.Source->GetActorNameOrLabel(); Region.GroupIndex = Group;
		Region.Kind = Shape.Source->bHiddenCavity ? EMiningMapRegionKind::HiddenEntrance : Shape.Source->Kind == EMiningManualKind::Passage ? EMiningMapRegionKind::Tunnel : Shape.Source->Kind == EMiningManualKind::SideRoom ? EMiningMapRegionKind::Branch : EMiningMapRegionKind::Space;
		if (Shape.Source->Kind == EMiningManualKind::RockBlock) { Region.Kind = EMiningMapRegionKind::GuidePlug; Region.bRockFilled = true; }
		Region.PolygonXZ = Shape.Polygon; Region.PolygonTriangles = Shape.Triangles;
		for (double Depth : {Shape.MinY, Shape.MaxY})
			for (const FVector2D& Point : Shape.Polygon)
			{
				const FVector Vertex(Point.X, Depth, Point.Y); Region.Vertices.Add(Vertex); Region.Bounds += Vertex;
			}
		Regions.Add(MoveTemp(Region));
	}
}
