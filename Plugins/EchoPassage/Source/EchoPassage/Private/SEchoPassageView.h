#pragma once
#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
class UEchoPassageInteractionComponent;
class SEchoPassageView : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SEchoPassageView){} SLATE_ARGUMENT(TWeakObjectPtr<UEchoPassageInteractionComponent>,Owner) SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Owner=Args._Owner; SetVisibility(EVisibility::HitTestInvisible); }
    virtual FVector2D ComputeDesiredSize(float) const override {return FVector2D(1600,900);}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
private: TWeakObjectPtr<UEchoPassageInteractionComponent> Owner;
};
