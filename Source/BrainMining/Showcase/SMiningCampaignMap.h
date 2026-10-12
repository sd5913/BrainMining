#pragma once
#include "Widgets/SLeafWidget.h"

class AMiningExhibitHall;

// A schematic of the authored route, not a rendering of solid / excavated rock.
class SMiningCampaignMap : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SMiningCampaignMap) {} SLATE_ARGUMENT(AMiningExhibitHall*, Hall) SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Hall=Args._Hall; }
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent& Event) override;
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1400,900); }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
private:
    TWeakObjectPtr<AMiningExhibitHall> Hall;
};
