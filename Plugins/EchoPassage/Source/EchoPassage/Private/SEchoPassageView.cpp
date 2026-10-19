#include "SEchoPassageView.h"
#include "EchoPassagePlayer.h"
#include "EchoPassageStage.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Engine/World.h"
int32 SEchoPassageView::OnPaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Cull,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const
{
    if(!Owner.IsValid() || !Owner->Stage)return Layer;
    const auto* Stage=Owner->Stage.Get();const float Time=Stage->GetWorld()->GetTimeSeconds();
    const FVector2D Size=G.GetLocalSize();const float S=FMath::Min(Size.X/1600,Size.Y/900);const FVector2D O=(Size-FVector2D(1600,900)*S)*.5;
    int32 L=Layer;const auto* Brush=FCoreStyle::Get().GetBrush("WhiteBrush");
    auto Box=[&](float X,float Y,float W,float H,FLinearColor C){FSlateDrawElement::MakeBox(Out,++L,G.ToPaintGeometry(FVector2f(W*S,H*S),FSlateLayoutTransform(FVector2f(O+FVector2D(X,Y)*S))),Brush,ESlateDrawEffect::None,C);};
    const FLinearColor Dim(.36f,.39f,.4f);
    if(Stage->bCollapsed && Stage->bLanded && !Owner->bChangingLevel)
    {
        if (!Owner->bUseSharedInteractionHUD) { Box(797,448,6,2,FLinearColor(.55f,.59f,.6f));Box(799,446,2,6,FLinearColor(.55f,.59f,.6f)); }
        const float Ready=FMath::Clamp((Time-Stage->LastEcho)/Stage->KnockCooldown,0.f,1.f);
        Box(710,824,180,2,FLinearColor(.08f,.09f,.1f));Box(710,824,180*Ready,2,Dim);
    }
    return L;
}
