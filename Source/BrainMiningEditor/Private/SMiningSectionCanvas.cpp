#include "SMiningSectionCanvas.h"
#include "MiningSpatialVisualizer.h"
#include "ScopedTransaction.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Editor.h"

SMiningSectionCanvas::~SMiningSectionCanvas() { End(); }
void SMiningSectionCanvas::Tick(const FGeometry& Geometry, double Time, float Delta)
{
	SLeafWidget::Tick(Geometry, Time, Delta);
	const FVector2D Size = Geometry.GetLocalSize();
	if (Size.X > 100 && Size.Y > 100 && !Size.Equals(ViewSize, 1)) { ViewSize = Size; Fit(); }
}
FMiningSpatialKnot* SMiningSectionCanvas::Current() const
{
	if (!FMiningSpatialSelection::Editable() || FMiningSpatialSelection::Tunnel->Kind == EMiningManualKind::SideRoom) return nullptr;
	return &FMiningSpatialSelection::Tunnel->Knots[FMiningSpatialSelection::Knot];
}
FVector2D SMiningSectionCanvas::Screen(const FVector2D& Value) const { return ViewSize * 0.5 + FVector2D(Value.X - Center.X, Center.Y - Value.Y) * Zoom; }
FVector2D SMiningSectionCanvas::Local(const FVector2D& Value) const { const FVector2D Offset = (Value - ViewSize * 0.5) / Zoom; return Center + FVector2D(Offset.X, -Offset.Y); }
void SMiningSectionCanvas::End() { if (DragActor.IsValid()) DragActor->Changed(); Transaction.Reset(); DragActor.Reset(); DragKnot = INDEX_NONE; bPan = false; }
void SMiningSectionCanvas::Fit()
{
	End(); Point = INDEX_NONE;
	if (FMiningSpatialKnot* Knot = Current())
	{
		FBox2D Bounds(ForceInit); for (const FMiningPenPoint& Item : Knot->Profile) { Bounds += Item.Position; Bounds += Item.Position + Item.Arrive; Bounds += Item.Position + Item.Leave; }
		if (Bounds.bIsValid) { Center = Bounds.GetCenter(); Zoom = FMath::Clamp(FMath::Min((ViewSize.X - 80) / FMath::Max(100.0, Bounds.GetSize().X), (ViewSize.Y - 80) / FMath::Max(100.0, Bounds.GetSize().Y)), 0.01, 3.0); }
	}
}

int32 SMiningSectionCanvas::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Clip, FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0.025f, 0.04f, 0.06f));
	const auto Line = [&](TArray<FVector2D> Points, const FLinearColor& Color) { FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, 1.5); };
	const auto Dot = [&](const FVector2D& Position, const FLinearColor& Color, double Radius) { FSlateDrawElement::MakeBox(Elements, Layer + 2, Geometry.ToPaintGeometry(FVector2D(Radius * 2), FSlateLayoutTransform(Screen(Position) - FVector2D(Radius))), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, Color); };
	Line({FVector2D(0, Screen(FVector2D::ZeroVector).Y), FVector2D(ViewSize.X, Screen(FVector2D::ZeroVector).Y)}, FLinearColor(0.15f, 0.2f, 0.25f));
	Line({FVector2D(Screen(FVector2D::ZeroVector).X, 0), FVector2D(Screen(FVector2D::ZeroVector).X, ViewSize.Y)}, FLinearColor(0.15f, 0.2f, 0.25f));
	const FMiningSpatialKnot* Knot = Current();
	if (Knot)
	{
		TArray<FVector2D> Outline; FString Error;
		if (MiningManual::Sample(Knot->Profile, true, 3, Outline, Error) && Outline.Num())
		{
			for (FVector2D& Item : Outline) Item = Screen(Item);
			Outline.Add(FVector2D(Outline[0])); Line(Outline, FLinearColor(0.15f, 0.8f, 1));
		}
		for (int32 Index = 0; Index < Knot->Profile.Num(); ++Index)
		{
			const FMiningPenPoint& Item = Knot->Profile[Index];
			Dot(Item.Position, Item.bLocked ? FLinearColor::Gray : Index == Point ? FLinearColor::Yellow : FLinearColor::White, 5);
			if (Index == Point && Item.Mode != EMiningPenMode::Corner)
				for (const FVector2D& Tangent : {Item.Arrive, Item.Leave})
				{ Line({Screen(Item.Position), Screen(Item.Position + Tangent)}, FLinearColor::Yellow); Dot(Item.Position + Tangent, FLinearColor::Yellow, 4); }
		}
	}
	const FString Text = Knot ? TEXT("局部截面 UV  | 拖点/手柄 · Ctrl+曲线插点\n右键平移 · 滚轮缩放 · Delete删点 · Ctrl+Z撤销") : TEXT("在三维视口点击隧道节点后编辑此截面\n支线房间在详细信息中改尺寸，可直接移动/旋转");
	FSlateDrawElement::MakeText(Elements, Layer + 3, Geometry.ToPaintGeometry(FVector2D(600, 50), FSlateLayoutTransform(FVector2D(10, 8))), Text, FAppStyle::GetFontStyle("SmallFont"), ESlateDrawEffect::None, FLinearColor::White);
	return Layer + 4;
}

FReply SMiningSectionCanvas::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	End(); LastMouse = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	if (Event.GetEffectingButton() == EKeys::RightMouseButton) { bPan = true; return FReply::Handled().CaptureMouse(SharedThis(this)); }
	FMiningSpatialKnot* Knot = Current();
	if (!Knot || Knot->bLocked || Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
	if (Event.IsControlDown())
	{
		int32 BestSegment = INDEX_NONE; double BestParameter = 0, Distance = 15;
		for (int32 Segment = 0; Segment < Knot->Profile.Num(); ++Segment)
			for (int32 Step = 1; Step < 50; ++Step)
			{
				const double Parameter = Step / 50.0;
				const double Candidate = FVector2D::Distance(LastMouse, Screen(MiningManual::Evaluate(Knot->Profile[Segment], Knot->Profile[(Segment + 1) % Knot->Profile.Num()], Parameter)));
				if (Candidate < Distance) { Distance = Candidate; BestSegment = Segment; BestParameter = Parameter; }
			}
		if (BestSegment != INDEX_NONE)
		{
			FScopedTransaction InsertTransaction(FText::FromString(TEXT("截面钢笔插点"))); FMiningSpatialSelection::Tunnel->Modify();
			const int32 OriginalCount = Knot->Profile.Num(), OriginalPoint = Point;
			if (MiningManual::Insert(Knot->Profile, BestSegment, BestParameter, true)) Point = BestSegment + 1;
			FMiningSpatialSelection::Tunnel->Changed();
			if (Current() && Current()->Profile.Num() == OriginalCount) { Point = OriginalPoint; InsertTransaction.Cancel(); }
		}
		return FReply::Handled().SetUserFocus(SharedThis(this));
	}
	int32 Hit = INDEX_NONE; Handle = 0;
	if (Knot->Profile.IsValidIndex(Point) && Knot->Profile[Point].Mode != EMiningPenMode::Corner)
		for (int32 Part = 1; Part <= 2; ++Part)
			if (FVector2D::Distance(LastMouse, Screen(Knot->Profile[Point].Position + (Part == 1 ? Knot->Profile[Point].Arrive : Knot->Profile[Point].Leave))) < 10) { Hit = Point; Handle = Part; }
	if (Hit == INDEX_NONE)
		for (int32 Index = 0; Index < Knot->Profile.Num(); ++Index)
			if (FVector2D::Distance(LastMouse, Screen(Knot->Profile[Index].Position)) < 10) Hit = Index;
	Point = Hit;
	if (Hit == INDEX_NONE || Knot->Profile[Hit].bLocked) return FReply::Handled().SetUserFocus(SharedThis(this));
	Transaction = MakeUnique<FScopedTransaction>(FText::FromString(TEXT("编辑隧道截面轮廓")));
	DragActor = FMiningSpatialSelection::Tunnel; DragKnot = FMiningSpatialSelection::Knot; DragActor->Modify();
	return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
}

FReply SMiningSectionCanvas::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2D Mouse = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()), Delta = Local(Mouse) - Local(LastMouse); LastMouse = Mouse;
	if (bPan) { Center -= Delta; return FReply::Handled(); }
	FMiningSpatialKnot* Knot = Current();
	if (!Transaction || !Knot || DragActor != FMiningSpatialSelection::Tunnel || DragKnot != FMiningSpatialSelection::Knot || !Knot->Profile.IsValidIndex(Point)) { if (Transaction) End(); return FReply::Unhandled(); }
	FMiningPenPoint& Item = Knot->Profile[Point];
	if (Handle == 0) Item.Position += Delta;
	else
	{
		FVector2D& Target = Handle == 1 ? Item.Arrive : Item.Leave; FVector2D& Opposite = Handle == 1 ? Item.Leave : Item.Arrive;
		Target += Delta; if (Item.Mode == EMiningPenMode::Smooth) Opposite = -Target;
	}
	DragActor->Changed(); return FReply::Handled();
}

FReply SMiningSectionCanvas::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) { End(); return FReply::Handled().ReleaseMouseCapture(); }
void SMiningSectionCanvas::OnMouseCaptureLost(const FCaptureLostEvent& Event) { End(); }
FReply SMiningSectionCanvas::OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) { Zoom = FMath::Clamp(Zoom * FMath::Pow(1.15, Event.GetWheelDelta()), 0.01, 5.0); return FReply::Handled(); }
FReply SMiningSectionCanvas::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.IsControlDown() && Event.GetKey() == EKeys::Z && GEditor && !GEditor->PlayWorld) { End(); GEditor->UndoTransaction(); return FReply::Handled(); }
	if (Event.IsControlDown() && Event.GetKey() == EKeys::Y && GEditor && !GEditor->PlayWorld) { End(); GEditor->RedoTransaction(); return FReply::Handled(); }
	if (Event.GetKey() == EKeys::Delete) { Delete(); return FReply::Handled(); }
	return FReply::Unhandled();
}
void SMiningSectionCanvas::Mode(EMiningPenMode Value)
{
	FMiningSpatialKnot* Knot = Current(); if (!Knot || Knot->bLocked || !Knot->Profile.IsValidIndex(Point) || Knot->Profile[Point].bLocked) return;
	FScopedTransaction Change(FText::FromString(TEXT("截面控制点类型"))); FMiningSpatialSelection::Tunnel->Modify();
	FMiningPenPoint& Item = Knot->Profile[Point]; Item.Mode = Value;
	if (Value == EMiningPenMode::Corner) Item.Arrive = Item.Leave = FVector2D::ZeroVector;
	else if (Item.Leave.IsNearlyZero()) { Item.Leave = (Knot->Profile[(Point + 1) % Knot->Profile.Num()].Position - Knot->Profile[(Point + Knot->Profile.Num() - 1) % Knot->Profile.Num()].Position) * 0.15; Item.Arrive = -Item.Leave; }
	else if (Value == EMiningPenMode::Smooth) Item.Arrive = -Item.Leave;
	FMiningSpatialSelection::Tunnel->Changed();
}
void SMiningSectionCanvas::Lock()
{
	FMiningSpatialKnot* Knot = Current(); if (!Knot || Knot->bLocked || !Knot->Profile.IsValidIndex(Point)) return;
	FScopedTransaction Change(FText::FromString(TEXT("锁定截面控制点"))); FMiningSpatialSelection::Tunnel->Modify(); Knot->Profile[Point].bLocked = !Knot->Profile[Point].bLocked;
	FMiningSpatialSelection::Tunnel->Changed();
}
void SMiningSectionCanvas::Delete()
{
	End(); FMiningSpatialKnot* Knot = Current(); if (!Knot || Knot->bLocked || Knot->Profile.Num() <= 3 || !Knot->Profile.IsValidIndex(Point) || Knot->Profile[Point].bLocked) return;
	FScopedTransaction Change(FText::FromString(TEXT("删除截面控制点"))); FMiningSpatialSelection::Tunnel->Modify();
	const int32 OriginalCount = Knot->Profile.Num(), OriginalPoint = Point;
	Knot->Profile.RemoveAt(Point); Point = INDEX_NONE; FMiningSpatialSelection::Tunnel->Changed();
	if (Current() && Current()->Profile.Num() == OriginalCount) { Point = OriginalPoint; Change.Cancel(); }
}
