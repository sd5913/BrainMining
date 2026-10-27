#pragma once
#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "GameFramework/PlayerController.h"
class APlayerController;
namespace MiningWaypoint
{
    int32 Paint(const FGeometry& G,FSlateWindowElementList& Out,int32 Layer,APlayerController* PC,const FVector& Target,const FString& Label);
}
class SMiningWaypoint : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SMiningWaypoint){} SLATE_ATTRIBUTE(TOptional<FVector>,Target) SLATE_ARGUMENT(APlayerController*,Player) SLATE_ATTRIBUTE(FString,Label) SLATE_END_ARGS()
    void Construct(const FArguments& Args){Target=Args._Target;Player=Args._Player;Label=Args._Label;SetVisibility(EVisibility::HitTestInvisible);}
    virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(1600,900);}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 L,const FWidgetStyle&,bool) const override
    {const auto P=Target.Get();return P.IsSet()&&Player.IsValid()?MiningWaypoint::Paint(G,Out,L,Player.Get(),P.GetValue(),Label.Get()):L;}
private:
    TAttribute<TOptional<FVector>> Target;
    TWeakObjectPtr<APlayerController> Player;
    TAttribute<FString> Label;
};
