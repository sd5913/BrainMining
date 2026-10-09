#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "MiningSpatialTunnel.h"

class FScopedTransaction;

class SMiningSectionCanvas : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMiningSectionCanvas) {} SLATE_END_ARGS()
	void Construct(const FArguments& Arguments) {}
	virtual ~SMiningSectionCanvas() override;
	virtual FVector2D ComputeDesiredSize(float Scale) const override { return FVector2D(420, 330); }
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual void Tick(const FGeometry& Geometry, double Time, float Delta) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Clip, FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& Event) override;
	void Fit();
	void Mode(EMiningPenMode Mode);
	void Lock();
	void Delete();
private:
	FMiningSpatialKnot* Current() const;
	FVector2D Screen(const FVector2D& Point) const;
	FVector2D Local(const FVector2D& Point) const;
	void End();
	TUniquePtr<FScopedTransaction> Transaction;
	TWeakObjectPtr<AMiningSpatialTunnel> DragActor;
	int32 DragKnot = INDEX_NONE, Point = INDEX_NONE, Handle = 0;
	bool bPan = false;
	FVector2D Center = FVector2D::ZeroVector, LastMouse = FVector2D::ZeroVector;
	FVector2D ViewSize = FVector2D(420, 330);
	double Zoom = 0.4;
};
