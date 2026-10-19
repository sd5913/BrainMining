#pragma once
#include "Widgets/SLeafWidget.h"
#include "CardRoomRun.h"
#include "UObject/StrongObjectPtr.h"
class UCardRoomInteractionComponent;
class SCardRoomView : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCardRoomView) {} SLATE_ARGUMENT(TWeakObjectPtr<UCardRoomInteractionComponent>,Owner) SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Owner=Args._Owner; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1600,900); }
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
    virtual FReply OnTouchStarted(const FGeometry&,const FPointerEvent&) override;
    virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
    virtual FReply OnMouseButtonUp(const FGeometry&,const FPointerEvent&) override;
    virtual FReply OnMouseMove(const FGeometry&,const FPointerEvent&) override;
    virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent&) override;
private:
    TWeakObjectPtr<UCardRoomInteractionComponent> Owner;
    mutable TMap<FString,TStrongObjectPtr<UObject>> Resources;
    UObject* CardResource(const FString& Name) const;
    FVector2D Mouse;
    FVector2D ToVirtual(const FGeometry&,const FVector2D&) const;
    FSlateRect CardRect(int32 Index,const FCardRoomRun& Run) const;
    static bool In(const FVector2D& P,const FSlateRect& R) { return P.X>=R.Left&&P.X<=R.Right&&P.Y>=R.Top&&P.Y<=R.Bottom; }
};
