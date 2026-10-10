#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "MiningManualShape.h"

class AMiningManualRegion;
class FScopedTransaction;

class SMiningPenCanvas : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMiningPenCanvas) {}
		SLATE_ARGUMENT(TFunction<void(AActor*)>, OnSelected)
	SLATE_END_ARGS()
	void Construct(const FArguments& Arguments);
	virtual ~SMiningPenCanvas() override;
	void SetRegion(AMiningManualRegion* Value);
	void SetSelected(AActor* Value);
	void Fit();
	void Mode(EMiningPenMode Value);
	void Lock();
	void DeletePoints();
	void ReverseBoundary();
	void BindPortal(bool bEnd) { PendingPortal = bEnd ? 2 : 1; }
	void ToggleBend();
	void RebindGuide();
	void ToggleDraw() { bDraw = !bDraw; bBend = false; }
	void ToggleSide() { ActiveSide = 1 - ActiveSide; PointSelection.Reset(); }
	void ToggleSnap() { bSnap = !bSnap; }
	FString Help() const;
	FVector2D ViewCenter() const { return Center; }
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D(700, 560); }
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& DrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& Event) override;
private:
	struct FPreviewValidation
	{
		uint32 Hash = 0;
		bool bChecked = false;
		bool bValid = true;
		FString Error;
	};
	mutable TMap<TWeakObjectPtr<AMiningManualShape>, FPreviewValidation> ValidationCache;
	TWeakObjectPtr<AMiningManualRegion> Region;
	TWeakObjectPtr<AActor> Selected;
	TFunction<void(AActor*)> OnSelected;
	TSet<FIntPoint> PointSelection;
	TUniquePtr<FScopedTransaction> Transaction;
	TArray<FMiningPenPoint> OriginalA, OriginalB;
	FVector OriginalLocation = FVector::ZeroVector;
	FVector OriginalSize = FVector::ZeroVector;
	FVector2D OriginalPivot = FVector2D::ZeroVector;
	FVector2D Center = FVector2D::ZeroVector;
	mutable FVector2D ViewSize = FVector2D(700, 560);
	FVector2D DragStart = FVector2D::ZeroVector;
	FVector2D OriginalCenter = FVector2D::ZeroVector;
	FIntPoint Handle = FIntPoint(-1, -1);
	double Scale = 0.1;
	int32 ActiveSide = 0;
	int32 DragMode = 0;
	bool bSnap = true;
	bool bDraw = false;
	bool bBend = false;
	int32 PendingPortal = 0;
	bool Editable() const;
	FVector2D Screen(const FVector2D& Point) const;
	FVector2D World(const FVector2D& Point) const;
	FVector2D Snap(const FVector2D& Point) const;
	void Begin(const FString& Name);
	void End();
	void Notify();
	bool BoundEndpoint(const AMiningManualShape& Shape, const FIntPoint& Point) const;
};
