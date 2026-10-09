#include "SMiningPenCanvas.h"
#include "MiningManualRegion.h"
#include "MiningManualGeometry.h"
#include "MiningLayoutRoom.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

namespace
{
FBox ActorBox(AActor* Actor)
{
	if (const AMiningLayoutRoom* Room = Cast<AMiningLayoutRoom>(Actor)) return Room->GetRoomBounds();
	if (const AMiningManualShape* Shape = Cast<AMiningManualShape>(Actor))
	{
		if (Shape->Kind == EMiningManualKind::SideRoom) return Shape->GetRoomBounds();
		return FBox(Shape->Bounds->GetComponentLocation() - Shape->Bounds->GetUnscaledBoxExtent(), Shape->Bounds->GetComponentLocation() + Shape->Bounds->GetUnscaledBoxExtent());
	}
	if (const AMiningManualRegion* Region = Cast<AMiningManualRegion>(Actor)) return Region->GetRockBounds();
	return FBox(ForceInit);
}
}

void SMiningPenCanvas::Construct(const FArguments& Arguments) { OnSelected = Arguments._OnSelected; SetClipping(EWidgetClipping::ClipToBounds); }
SMiningPenCanvas::~SMiningPenCanvas() { End(); }
bool SMiningPenCanvas::Editable() const { return GEditor && !GEditor->PlayWorld && Region.IsValid(); }
FVector2D SMiningPenCanvas::Screen(const FVector2D& Point) const { return FVector2D((Point.X - Center.X) * Scale, (Center.Y - Point.Y) * Scale) + ViewSize * 0.5; }
FVector2D SMiningPenCanvas::World(const FVector2D& Point) const { const FVector2D Local = (Point - ViewSize * 0.5) / Scale; return Center + FVector2D(Local.X, -Local.Y); }
FVector2D SMiningPenCanvas::Snap(const FVector2D& Point) const { return bSnap ? FVector2D(FMath::GridSnap(Point.X, 10.0), FMath::GridSnap(Point.Y, 10.0)) : Point; }
void SMiningPenCanvas::SetSelected(AActor* Value) { End(); Selected = Value; PointSelection.Reset(); PendingPortal = 0; }
void SMiningPenCanvas::SetRegion(AMiningManualRegion* Value) { End(); Region = Value; Selected = nullptr; PointSelection.Reset(); ValidationCache.Reset(); Fit(); }

void SMiningPenCanvas::Fit()
{
	if (!Region.IsValid()) return;
	FBox Bounds = Region->GetRockBounds();
	if (Region->FromRoom) Bounds += Region->FromRoom->GetRoomBounds();
	if (Region->ToRoom) Bounds += Region->ToRoom->GetRoomBounds();
	Center = FVector2D(Bounds.GetCenter().X, Bounds.GetCenter().Z);
	Scale = FMath::Clamp(FMath::Min((ViewSize.X - 100) / FMath::Max(100.0, Bounds.GetSize().X), (ViewSize.Y - 100) / FMath::Max(100.0, Bounds.GetSize().Z)), 0.005, 2.0);
}

void SMiningPenCanvas::Begin(const FString& Name)
{
	if (Transaction || !Editable() || !Selected.IsValid()) return;
	Transaction = MakeUnique<FScopedTransaction>(FText::FromString(Name));
	Selected->Modify(); if (Selected->GetRootComponent()) Selected->GetRootComponent()->Modify();
	OriginalLocation = Selected->GetActorLocation();
	if (AMiningManualShape* Shape = Cast<AMiningManualShape>(Selected))
	{
		OriginalA = Shape->BoundaryA; OriginalB = Shape->BoundaryB; OriginalSize = FVector(Shape->Size.X, Shape->Depth, Shape->Size.Y);
		if (Shape->Kind != EMiningManualKind::SideRoom)
		{
			const FBox Box = ActorBox(Shape); OriginalSize = Box.GetSize();
			OriginalPivot = FVector2D(Box.GetCenter().X - OriginalLocation.X, Box.Min.Z - OriginalLocation.Z);
		}
	}
	else if (AMiningLayoutRoom* Room = Cast<AMiningLayoutRoom>(Selected)) OriginalSize = Room->RoomSize;
	else if (AMiningManualRegion* Manual = Cast<AMiningManualRegion>(Selected)) OriginalSize = Manual->RockSize;
}

void SMiningPenCanvas::Notify()
{
	if (!Selected.IsValid()) return;
	Selected->OnConstruction(Selected->GetActorTransform()); Selected->MarkPackageDirty();
	if (GEditor) GEditor->RedrawLevelEditingViewports();
}

void SMiningPenCanvas::End()
{
	if (Transaction) { Notify(); Transaction.Reset(); }
	DragMode = 0;
}

bool SMiningPenCanvas::BoundEndpoint(const AMiningManualShape& Shape, const FIntPoint& Point) const
{
	const TArray<FMiningPenPoint>& Points = Point.X == 0 ? Shape.BoundaryA : Shape.BoundaryB;
	return Shape.Kind == EMiningManualKind::Passage && ((Point.Y == 0 && Shape.StartPortal.Room) || (Point.Y == Points.Num() - 1 && Shape.EndPortal.Room));
}

FString SMiningPenCanvas::Help() const
{
	if (PendingPortal) return TEXT("绑定入口：在画布点击目标房间的墙面（左右/上下）；ESC 取消。只绑定通道端点，不重置中段。");
	if (bBend) return TEXT("整体弯曲：拖绿色导向柄，两侧一起变形，锁定点和端点不动。绑定按路径顺序保存，不会在折返处跳到邻段。加删点后可重新绑定。");
	return FString::Printf(TEXT("%s  |  当前边界 %s  |  %s  |  左键拖点/空间 · Shift 多选 · Ctrl+曲线插点 · 右键平移 · 滚轮缩放 · Delete 删点 · Ctrl+Z 撤销"), bDraw ? TEXT("钢笔追加：点击加点，按住拖出手柄") : TEXT("选择编辑"), ActiveSide == 0 ? TEXT("A / 闭合轮廓") : TEXT("B"), bSnap ? TEXT("10cm 吸附") : TEXT("不吸附"));
}

int32 SMiningPenCanvas::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	ViewSize = Geometry.GetLocalSize();
	FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0.025f, 0.035f, 0.05f));
	const auto Line = [&](TArray<FVector2D> Points, const FLinearColor& Color, float Width = 1.5f)
	{
		FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Width);
	};
	const auto Dot = [&](const FVector2D& Point, const FLinearColor& Color, float Radius = 4)
	{
		FSlateDrawElement::MakeBox(Elements, Layer + 3, Geometry.ToPaintGeometry(FVector2D(Radius * 2), FSlateLayoutTransform(Point - FVector2D(Radius))), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, Color);
	};
	const auto Label = [&](const FVector2D& Point, const FString& Text, const FLinearColor& Color)
	{
		FSlateDrawElement::MakeText(Elements, Layer + 4, Geometry.ToPaintGeometry(FVector2D(500, 20), FSlateLayoutTransform(Point)), Text, FAppStyle::GetFontStyle("SmallFont"), ESlateDrawEffect::None, Color);
	};
	const FVector2D Min = World(FVector2D(0, ViewSize.Y)), Max = World(FVector2D(ViewSize.X, 0));
	const double Grid = Scale > 0.3 ? 100 : Scale > 0.05 ? 500 : 2000;
	for (double Position = FMath::GridSnap(Min.X, Grid); Position <= Max.X; Position += Grid) Line({Screen(FVector2D(Position, Min.Y)), Screen(FVector2D(Position, Max.Y))}, FLinearColor(0.08f, 0.11f, 0.14f), 1);
	for (double Position = FMath::GridSnap(Min.Y, Grid); Position <= Max.Y; Position += Grid) Line({Screen(FVector2D(Min.X, Position)), Screen(FVector2D(Max.X, Position))}, FLinearColor(0.08f, 0.11f, 0.14f), 1);
	if (!Region.IsValid()) { Label(FVector2D(24), TEXT("选择一个手绘矿区，或新建矿区后设置两个主房间。"), FLinearColor::White); return Layer + 5; }
	const auto DrawBox = [&](AActor* Actor, const FBox& Box, const FLinearColor& Color)
	{
		const FVector2D Low(Box.Min.X, Box.Min.Z), High(Box.Max.X, Box.Max.Z);
		Line({Screen(Low), Screen(FVector2D(High.X, Low.Y)), Screen(High), Screen(FVector2D(Low.X, High.Y)), Screen(Low)}, Color, Selected == Actor ? 3 : 1.5f);
		Label(Screen(FVector2D(Low.X, High.Y)) + FVector2D(4, 4), Actor->GetActorNameOrLabel(), Color);
		if (Selected == Actor)
		{
			Dot(Screen(FVector2D(High.X, (High.Y + Low.Y) * 0.5)) + FVector2D(14, 0), FLinearColor::Yellow, 6);
			Dot(Screen(FVector2D((High.X + Low.X) * 0.5, High.Y)) + FVector2D(0, -14), FLinearColor::Yellow, 6);
		}
	};
	DrawBox(Region.Get(), Region->GetRockBounds(), FLinearColor(0.55f, 0.38f, 0.17f));
	for (AMiningLayoutRoom* Room : {Region->FromRoom.Get(), Region->ToRoom.Get()}) if (IsValid(Room)) DrawBox(Room, Room->GetRoomBounds(), FLinearColor(0.2f, 0.95f, 0.65f));
	for (AMiningManualShape* Shape : Region->GetShapes())
	{
		if (!Shape->bEnabled) continue;
		if (Shape->Kind == EMiningManualKind::SideRoom) { DrawBox(Shape, Shape->GetRoomBounds(), FLinearColor(0.3f, 0.85f, 0.65f)); continue; }
		if (Selected == Shape && Shape->Kind != EMiningManualKind::Passage) DrawBox(Shape, ActorBox(Shape), FLinearColor(0.55f, 0.55f, 0.28f));
		FPreviewValidation& Validation = ValidationCache.FindOrAdd(Shape);
		const uint32 Hash = HashCombineFast(Shape->GetGeometryHash(), GetTypeHash(Region->CurveTolerance));
		if (!Transaction && (!Validation.bChecked || Validation.Hash != Hash))
		{
			MiningManual::FShapePlan Plan; Validation.Error.Reset();
			Validation.bValid = MiningManual::Shape(*Shape, Region->CurveTolerance, Plan, Validation.Error);
			Validation.Hash = Hash; Validation.bChecked = true;
		}
		FString Error = Validation.Error;
		const bool bValid = Validation.bValid;
		for (int32 Side = 0; Side < (Shape->Kind == EMiningManualKind::Passage ? 2 : 1); ++Side)
		{
			TArray<FMiningPenPoint> Points; Shape->ResolveBoundary(Side == 1, Points, Error);
			TArray<FVector2D> Polyline; MiningManual::Sample(Points, Shape->Kind != EMiningManualKind::Passage, Region->CurveTolerance, Polyline, Error);
			TArray<FVector2D> ScreenPoints; for (const FVector2D& Point : Polyline) ScreenPoints.Add(Screen(Point));
			if (Shape->Kind != EMiningManualKind::Passage && !ScreenPoints.IsEmpty()) ScreenPoints.Add(FVector2D(ScreenPoints[0]));
			const FLinearColor Color = !bValid ? FLinearColor(1, 0.25f, 0.2f) : Shape->bHiddenCavity ? FLinearColor(1, 0.4f, 0.8f) : Side == 0 ? FLinearColor(0.2f, 0.75f, 1) : FLinearColor(1, 0.65f, 0.25f);
			if (ScreenPoints.Num() >= 2) Line(ScreenPoints, Color, Selected == Shape ? 3 : 1.5f);
			if (Side == 0 && !ScreenPoints.IsEmpty()) Label(ScreenPoints[ScreenPoints.Num() / 2] + FVector2D(5, -20), Shape->GetActorNameOrLabel(), Color);
			if (Selected != Shape) continue;
			for (int32 Index = 0; Index < Points.Num(); ++Index)
			{
				const FMiningPenPoint& Point = Points[Index]; const FIntPoint Key(Side, Index);
				if (PointSelection.Contains(Key) && Point.Mode != EMiningPenMode::Corner)
				{
					for (const FVector2D& Tangent : {Point.Arrive, Point.Leave})
					{
						Line({Screen(Point.Position), Screen(Point.Position + Tangent)}, FLinearColor(0.7f, 0.65f, 1), 1);
						Dot(Screen(Point.Position + Tangent), FLinearColor(0.8f, 0.6f, 1), 3);
					}
				}
				Dot(Screen(Point.Position), Point.bLocked || BoundEndpoint(*Shape, Key) ? FLinearColor(0.4f, 0.5f, 0.6f) : PointSelection.Contains(Key) ? FLinearColor::Yellow : Color, PointSelection.Contains(Key) ? 6 : 4);
			}
		}
		if (!bValid && Selected == Shape) Label(FVector2D(15, ViewSize.Y - 35), Error, FLinearColor(1, 0.45f, 0.25f));
		if (bBend && Selected == Shape && Shape->Kind == EMiningManualKind::Passage)
		{
			TArray<FMiningPenPoint> First, Second; Shape->ResolveBoundary(false, First, Error); Shape->ResolveBoundary(true, Second, Error);
			if (MiningManual::GuideBound(First) && MiningManual::GuideBound(Second))
				for (double Station : {0.25, 0.5, 0.75}) Dot(Screen((MiningManual::GuidePoint(First, Station) + MiningManual::GuidePoint(Second, Station)) * 0.5), FLinearColor::Green, 8);
		}
	}
	if (GEditor && GEditor->PlayWorld) Label(FVector2D(20), TEXT("运行中：编辑已锁定，停止 PIE 后继续。"), FLinearColor::Yellow);
	else if (Transaction) Label(FVector2D(20), TEXT("轻量轮廓预览 · 松开鼠标后检查并构建地形"), FLinearColor::Yellow);
	return Layer + 5;
}

FReply SMiningPenCanvas::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	DragStart = World(Local); OriginalCenter = Center;
	if (Event.GetEffectingButton() == EKeys::RightMouseButton) { DragMode = 1; return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this)); }
	if (!Editable() || Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
	AMiningManualShape* Shape = Cast<AMiningManualShape>(Selected);
	if (PendingPortal)
	{
		if (!Shape || Shape->Kind != EMiningManualKind::Passage) { PendingPortal = 0; return FReply::Handled(); }
		TArray<AActor*> Candidates{Region->FromRoom.Get(), Region->ToRoom.Get()};
		for (AMiningManualShape* Candidate : Region->GetShapes()) if (Candidate->Kind == EMiningManualKind::SideRoom && Candidate->bEnabled) Candidates.Add(Candidate);
		for (AActor* Candidate : Candidates)
		{
			if (!IsValid(Candidate)) continue;
			const FBox Box = ActorBox(Candidate);
			if (DragStart.X < Box.Min.X || DragStart.X > Box.Max.X || DragStart.Y < Box.Min.Z || DragStart.Y > Box.Max.Z) continue;
			Begin(TEXT("手绘：绑定房间入口"));
			FMiningManualPortal& Portal = PendingPortal == 2 ? Shape->EndPortal : Shape->StartPortal;
			Portal.Room = Candidate;
			const double Horizontal = (DragStart.X - Box.GetCenter().X) / Box.GetExtent().X, Vertical = (DragStart.Y - Box.GetCenter().Z) / Box.GetExtent().Z;
			const bool bVerticalWall = FMath::Abs(Horizontal) > FMath::Abs(Vertical);
			Portal.Wall = bVerticalWall ? (Horizontal >= 0 ? EMiningPortalWall::PositiveX : EMiningPortalWall::NegativeX) : (Vertical >= 0 ? EMiningPortalWall::PositiveZ : EMiningPortalWall::NegativeZ);
			Portal.Offset = 0; Portal.DepthOffset = Shape->GetActorLocation().Y - Box.GetCenter().Y;
			PendingPortal = 0; End(); return FReply::Handled().SetUserFocus(SharedThis(this));
		}
		return FReply::Handled().SetUserFocus(SharedThis(this));
	}
	if (Shape && Shape->Kind != EMiningManualKind::SideRoom)
	{
		if (bBend && Shape->Kind == EMiningManualKind::Passage)
		{
			TArray<FMiningPenPoint> First, Second; FString Error; Shape->ResolveBoundary(false, First, Error); Shape->ResolveBoundary(true, Second, Error);
			if (MiningManual::GuideBound(First) && MiningManual::GuideBound(Second))
				for (int32 Index = 1; Index <= 3; ++Index)
					if (FVector2D::Distance(Local, Screen((MiningManual::GuidePoint(First, Index * 0.25) + MiningManual::GuidePoint(Second, Index * 0.25)) * 0.5)) < 12)
					{
						Begin(TEXT("手绘：整体弯曲两侧")); Handle = FIntPoint(0, Index); DragMode = 9;
						return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
					}
		}
		for (const FIntPoint& Key : PointSelection)
		{
			TArray<FMiningPenPoint> Points; FString Error; Shape->ResolveBoundary(Key.X == 1, Points, Error);
			if (!Points.IsValidIndex(Key.Y) || Points[Key.Y].bLocked || Points[Key.Y].Mode == EMiningPenMode::Corner) continue;
			for (int32 Tangent = 0; Tangent < 2; ++Tangent)
				if (FVector2D::Distance(Local, Screen(Points[Key.Y].Position + (Tangent == 0 ? Points[Key.Y].Arrive : Points[Key.Y].Leave))) < 8)
				{
					Begin(TEXT("手绘：拖动曲线手柄")); Handle = Key; DragMode = Tangent == 0 ? 3 : 4;
					return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
				}
		}
		for (int32 Side = 0; Side < (Shape->Kind == EMiningManualKind::Passage ? 2 : 1); ++Side)
		{
			TArray<FMiningPenPoint> Points; FString Error; Shape->ResolveBoundary(Side == 1, Points, Error);
			for (int32 Index = 0; Index < Points.Num(); ++Index)
				if (FVector2D::Distance(Local, Screen(Points[Index].Position)) < 9)
				{
					const FIntPoint Key(Side, Index); ActiveSide = Side;
					if (!Event.IsShiftDown() && !PointSelection.Contains(Key)) PointSelection.Reset();
					if (Event.IsShiftDown() && PointSelection.Contains(Key)) PointSelection.Remove(Key); else PointSelection.Add(Key);
					Begin(TEXT("手绘：移动控制点")); DragMode = 2;
					return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
				}
		}
		if (Event.IsControlDown())
		{
			double Best = 12; int32 BestSide = -1, BestSegment = -1; double BestParameter = 0;
			for (int32 Side = 0; Side < (Shape->Kind == EMiningManualKind::Passage ? 2 : 1); ++Side)
			{
				TArray<FMiningPenPoint> Points; FString Error; Shape->ResolveBoundary(Side == 1, Points, Error);
				for (int32 Index = 0; Index < Points.Num() - (Shape->Kind == EMiningManualKind::Passage ? 1 : 0); ++Index)
					for (int32 Step = 1; Step < 100; ++Step)
					{
						const double Distance = FVector2D::Distance(Screen(MiningManual::Evaluate(Points[Index], Points[(Index + 1) % Points.Num()], Step / 100.0)), Local);
						if (Distance < Best) { Best = Distance; BestSide = Side; BestSegment = Index; BestParameter = Step / 100.0; }
					}
			}
			if (BestSegment >= 0)
			{
				Begin(TEXT("手绘：保形插入节点")); TArray<FMiningPenPoint> Points;
				FString Error; Shape->ResolveBoundary(BestSide == 1, Points, Error);
				const FVector2D Origin(Shape->GetActorLocation().X, Shape->GetActorLocation().Z);
				for (FMiningPenPoint& Point : Points) Point.Position -= Origin;
				if (MiningManual::Insert(Points, BestSegment, BestParameter, Shape->Kind != EMiningManualKind::Passage))
				{
					(BestSide == 0 ? Shape->BoundaryA : Shape->BoundaryB) = MoveTemp(Points);
					PointSelection = {FIntPoint(BestSide, BestSegment + 1)}; ActiveSide = BestSide;
				}
				End(); return FReply::Handled().SetUserFocus(SharedThis(this));
			}
		}
		if (bDraw)
		{
			ActiveSide = Shape->Kind == EMiningManualKind::Passage ? ActiveSide : 0;
			TArray<FMiningPenPoint>& Points = ActiveSide == 0 ? Shape->BoundaryA : Shape->BoundaryB;
			if (Points.Num() >= 128 || (Shape->Kind == EMiningManualKind::Passage && Shape->EndPortal.Room)) return FReply::Handled();
			Begin(TEXT("手绘：钢笔追加节点")); FMiningPenPoint Point;
			Point.Position = Snap(DragStart) - FVector2D(Shape->GetActorLocation().X, Shape->GetActorLocation().Z); Points.Add(Point);
			Handle = FIntPoint(ActiveSide, Points.Num() - 1); PointSelection = {Handle}; DragMode = 5;
			return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
		}
	}
	if (Selected.IsValid() && (!Shape || Shape->Kind != EMiningManualKind::Passage))
	{
		const FBox Box = ActorBox(Selected.Get());
		if (Box.IsValid)
		{
			const FVector2D Width = Screen(FVector2D(Box.Max.X, Box.GetCenter().Z)) + FVector2D(14, 0), Height = Screen(FVector2D(Box.GetCenter().X, Box.Max.Z)) + FVector2D(0, -14);
			if (FVector2D::Distance(Local, Width) < 10 || FVector2D::Distance(Local, Height) < 10)
			{
				Begin(TEXT("手绘：调整空间尺寸")); DragMode = FVector2D::Distance(Local, Width) < 10 ? 7 : 8;
				return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
			}
		}
	}
	AActor* Hit = nullptr;
	for (AMiningManualShape* Candidate : Region->GetShapes())
	{
		if (!Candidate->bEnabled) continue;
		MiningManual::FShapePlan Plan; FString Error;
		if (MiningManual::Shape(*Candidate, Region->CurveTolerance, Plan, Error) && MiningManual::Contains(Plan.Polygon, DragStart)) Hit = Candidate;
	}
	for (AMiningLayoutRoom* Room : {Region->FromRoom.Get(), Region->ToRoom.Get()})
		if (IsValid(Room)) { const FBox Box = Room->GetRoomBounds(); if (DragStart.X >= Box.Min.X && DragStart.X <= Box.Max.X && DragStart.Y >= Box.Min.Z && DragStart.Y <= Box.Max.Z) Hit = Room; }
	if (!Hit) Hit = Region.Get();
	Selected = Hit; PointSelection.Reset(); if (OnSelected) OnSelected(Hit);
	Begin(TEXT("手绘：移动空间")); DragMode = 6;
	return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
}

FReply SMiningPenCanvas::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!HasMouseCapture() || DragMode == 0) return FReply::Unhandled();
	const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	if (DragMode == 1)
	{
		const FVector2D Movement = Event.GetCursorDelta() / Geometry.Scale / Scale; Center += FVector2D(-Movement.X, Movement.Y); return FReply::Handled();
	}
	if (!Editable() || !Selected.IsValid()) { End(); return FReply::Handled().ReleaseMouseCapture(); }
	const FVector2D Position = World(Local), Delta = Snap(Position) - Snap(DragStart);
	AMiningManualShape* Shape = Cast<AMiningManualShape>(Selected);
	if (Shape && DragMode == 2)
	{
		for (const FIntPoint& Key : PointSelection)
		{
			TArray<FMiningPenPoint>& Points = Key.X == 0 ? Shape->BoundaryA : Shape->BoundaryB;
			const TArray<FMiningPenPoint>& Original = Key.X == 0 ? OriginalA : OriginalB;
			if (Points.IsValidIndex(Key.Y) && Original.IsValidIndex(Key.Y) && !Points[Key.Y].bLocked && !BoundEndpoint(*Shape, Key)) Points[Key.Y].Position = Original[Key.Y].Position + Delta;
		}
	}
	else if (Shape && DragMode >= 3 && DragMode <= 5)
	{
		TArray<FMiningPenPoint>& Points = Handle.X == 0 ? Shape->BoundaryA : Shape->BoundaryB;
		if (Points.IsValidIndex(Handle.Y))
		{
			FMiningPenPoint& Point = Points[Handle.Y]; TArray<FMiningPenPoint> Resolved; FString Error; Shape->ResolveBoundary(Handle.X == 1, Resolved, Error);
			const FVector2D Tangent = Snap(Position) - Resolved[Handle.Y].Position;
			if (DragMode == 5 && Tangent.Size() > 10) Point.Mode = EMiningPenMode::Smooth;
			if (DragMode == 3) Point.Arrive = Tangent; else Point.Leave = Tangent;
			if (Point.Mode == EMiningPenMode::Smooth) { if (DragMode == 3) Point.Leave = -Tangent; else Point.Arrive = -Tangent; }
		}
	}
	else if (Shape && DragMode == 9)
	{
		Shape->BoundaryA = OriginalA; Shape->BoundaryB = OriginalB;
		MiningManual::Bend(Shape->BoundaryA, Handle.Y * 0.25, Delta); MiningManual::Bend(Shape->BoundaryB, Handle.Y * 0.25, Delta);
	}
	else if (DragMode == 6) Selected->SetActorLocation(OriginalLocation + FVector(Delta.X, 0, Delta.Y));
	else if (DragMode == 7 || DragMode == 8)
	{
		FVector Size = OriginalSize;
		if (DragMode == 7) Size.X = FMath::Max(250.0, OriginalSize.X + Delta.X * 2);
		else Size.Z = FMath::Max(250.0, OriginalSize.Z + Delta.Y * (Cast<AMiningManualRegion>(Selected) ? 2 : 1));
		if (Shape)
		{
			Shape->Size = FVector2D(Size.X, Size.Z);
			if (Shape->Kind != EMiningManualKind::SideRoom)
			{
				const FVector2D Factors(Size.X / FMath::Max(1.0, OriginalSize.X), Size.Z / FMath::Max(1.0, OriginalSize.Z));
				Shape->BoundaryA = OriginalA; Shape->BoundaryB = OriginalB;
				for (TArray<FMiningPenPoint>* Points : {&Shape->BoundaryA, &Shape->BoundaryB})
					for (FMiningPenPoint& Point : *Points)
					{
						if (Point.bLocked) continue;
						Point.Position = OriginalPivot + (Point.Position - OriginalPivot) * Factors;
						Point.Arrive *= Factors; Point.Leave *= Factors;
					}
			}
		}
		else if (AMiningLayoutRoom* Room = Cast<AMiningLayoutRoom>(Selected)) Room->RoomSize = Size;
		else if (AMiningManualRegion* Manual = Cast<AMiningManualRegion>(Selected)) Manual->RockSize = Size;
	}
	if (Selected.IsValid()) Selected->OnConstruction(Selected->GetActorTransform());
	return FReply::Handled();
}

FReply SMiningPenCanvas::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) { if (!HasMouseCapture()) return FReply::Unhandled(); End(); return FReply::Handled().ReleaseMouseCapture(); }
void SMiningPenCanvas::OnMouseCaptureLost(const FCaptureLostEvent& Event) { End(); }
FReply SMiningPenCanvas::OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()), Before = World(Local);
	Scale = FMath::Clamp(Scale * FMath::Pow(1.15, Event.GetWheelDelta()), 0.005, 3.0); Center += Before - World(Local);
	return FReply::Handled();
}

void SMiningPenCanvas::Mode(EMiningPenMode Value)
{
	AMiningManualShape* Shape = Cast<AMiningManualShape>(Selected); if (!Editable() || !Shape || PointSelection.IsEmpty()) return;
	Begin(TEXT("手绘：切换节点类型"));
	for (const FIntPoint& Key : PointSelection)
	{
		TArray<FMiningPenPoint>& Points = Key.X == 0 ? Shape->BoundaryA : Shape->BoundaryB; if (!Points.IsValidIndex(Key.Y) || Points[Key.Y].bLocked) continue;
		FMiningPenPoint& Point = Points[Key.Y];
		if (Value != EMiningPenMode::Corner && Point.Mode == EMiningPenMode::Corner)
		{
			const int32 Last = Points.Num() - 1;
			Point.Leave = (Points[FMath::Min(Key.Y + 1, Last)].Position - Points[FMath::Max(0, Key.Y - 1)].Position) / 6; Point.Arrive = -Point.Leave;
		}
		Point.Mode = Value; if (Value == EMiningPenMode::Smooth) Point.Arrive = -Point.Leave;
	}
	End();
}

void SMiningPenCanvas::Lock()
{
	AMiningManualShape* Shape = Cast<AMiningManualShape>(Selected); if (!Editable() || !Shape) return;
	Begin(TEXT("手绘：锁定/解锁节点"));
	for (const FIntPoint& Key : PointSelection) { TArray<FMiningPenPoint>& Points = Key.X == 0 ? Shape->BoundaryA : Shape->BoundaryB; if (Points.IsValidIndex(Key.Y)) Points[Key.Y].bLocked = !Points[Key.Y].bLocked; }
	End();
}

void SMiningPenCanvas::DeletePoints()
{
	AMiningManualShape* Shape = Cast<AMiningManualShape>(Selected); if (!Editable() || !Shape || PointSelection.IsEmpty()) return;
	Begin(TEXT("手绘：删除控制点"));
	for (int32 Side = 0; Side < 2; ++Side)
	{
		TArray<FMiningPenPoint>& Points = Side == 0 ? Shape->BoundaryA : Shape->BoundaryB;
		for (int32 Index = Points.Num() - 1; Index >= 0; --Index)
			if (PointSelection.Contains(FIntPoint(Side, Index)) && !Points[Index].bLocked && !BoundEndpoint(*Shape, FIntPoint(Side, Index))) Points.RemoveAt(Index);
	}
	PointSelection.Reset(); End();
}

void SMiningPenCanvas::RebindGuide()
{
	AMiningManualShape* Shape = Cast<AMiningManualShape>(Selected);
	if (!Editable() || !Shape || Shape->Kind != EMiningManualKind::Passage) return;
	Begin(TEXT("手绘：绑定整体弯曲导向"));
	for (bool bSecond : {false, true})
	{
		TArray<FMiningPenPoint> Resolved; FString Error;
		if (!Shape->ResolveBoundary(bSecond, Resolved, Error) || !MiningManual::BindGuide(Resolved)) continue;
		TArray<FMiningPenPoint>& Authored = bSecond ? Shape->BoundaryB : Shape->BoundaryA;
		for (int32 Index = 0; Index < Authored.Num(); ++Index) Authored[Index].GuideStation = Resolved[Index].GuideStation;
	}
	End();
}

void SMiningPenCanvas::ToggleBend()
{
	bBend = !bBend; bDraw = false;
	AMiningManualShape* Shape = Cast<AMiningManualShape>(Selected);
	if (bBend && Shape && (!MiningManual::GuideBound(Shape->BoundaryA) || !MiningManual::GuideBound(Shape->BoundaryB))) RebindGuide();
}

void SMiningPenCanvas::ReverseBoundary()
{
	AMiningManualShape* Shape = Cast<AMiningManualShape>(Selected); if (!Editable() || !Shape || Shape->Kind != EMiningManualKind::Passage || Shape->StartPortal.Room || Shape->EndPortal.Room) return;
	Begin(TEXT("手绘：反转当前边界")); TArray<FMiningPenPoint>& Points = ActiveSide == 0 ? Shape->BoundaryA : Shape->BoundaryB;
	Algo::Reverse(Points); for (FMiningPenPoint& Point : Points) { Swap(Point.Arrive, Point.Leave); if (Point.GuideStation >= 0) Point.GuideStation = 1 - Point.GuideStation; }
	PointSelection.Reset(); End();
}

FReply SMiningPenCanvas::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (!Editable()) return FReply::Unhandled();
	if (Event.GetKey() == EKeys::Escape && PendingPortal) { PendingPortal = 0; return FReply::Handled(); }
	if (Event.GetKey() == EKeys::Escape && Transaction) { End(); GEditor->UndoTransaction(); return FReply::Handled().ReleaseMouseCapture(); }
	if (Event.IsControlDown() && Event.GetKey() == EKeys::Z) { End(); if (Event.IsShiftDown()) GEditor->RedoTransaction(); else GEditor->UndoTransaction(); return FReply::Handled(); }
	if (Event.IsControlDown() && Event.GetKey() == EKeys::Y) { End(); GEditor->RedoTransaction(); return FReply::Handled(); }
	if (Event.GetKey() == EKeys::Delete) { DeletePoints(); return FReply::Handled(); }
	if (Event.GetKey() == EKeys::F) { Fit(); return FReply::Handled(); }
	return FReply::Unhandled();
}
