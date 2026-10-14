#pragma once
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateBrush.h"
class AMiningCollectionField;
class SMiningCollectionView : public SCompoundWidget
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
    virtual void OnMouseLeave(const FPointerEvent&) override;
    virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override { bDragging=false; }
private:
    TWeakObjectPtr<AMiningCollectionField> Owner;
    FSlateBrush PreviewBrush;
    bool bDragging = false;
    FVector2D LastMouse;
    FVector2D HoverMouse=FVector2D(-1,-1);
    int32 SlotAt(FVector2D) const;
    void TurnPage(int32 Direction);
    FVector2D ToVirtual(const FGeometry&, const FVector2D&) const;
};
