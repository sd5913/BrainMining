#pragma once
#include "Widgets/SLeafWidget.h"
#include "Styling/SlateBrush.h"
class AMiningCollectionField;
class SMiningCollectionView : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SMiningCollectionView) {} SLATE_ARGUMENT(TWeakObjectPtr<AMiningCollectionField>, Owner) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1600,900); }
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
    virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
    virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override;
    virtual FReply OnMouseButtonDoubleClick(const FGeometry&, const FPointerEvent&) override;
    virtual FReply OnMouseMove(const FGeometry&, const FPointerEvent&) override;
    virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent&) override;
    virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;
    virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override { bDragging=bListDragging=false; }
private:
    TWeakObjectPtr<AMiningCollectionField> Owner;
    FSlateBrush PreviewBrush;
    bool bDragging = false;
    bool bListDragging=false;
    float ListScroll=0,ListDragStart=0;
    FVector2D ListPress;
    FVector2D LastMouse;
    FVector2D ToVirtual(const FGeometry&, const FVector2D&) const;
};
