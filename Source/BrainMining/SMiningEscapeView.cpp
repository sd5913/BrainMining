#include "SMiningEscapeView.h"
#include "BrainMiningFonts.h"
#include "MiningRoomEscape.h"
#include "MiningInteractionHUDWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
int32 SMiningEscapeView::OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const
{
    if(!Owner.IsValid() || Owner->Phase==EMiningEscapePhase::Idle || Owner->Phase==EMiningEscapePhase::Escaped)return Layer;
    const auto* E=Owner.Get();const FVector2D Size=G.GetLocalSize();const float S=FMath::Min(Size.X/1600,Size.Y/900);const FVector2D Origin=(Size-FVector2D(1600,900)*S)*.5;
    int32 L=Layer;auto* Brush=FCoreStyle::Get().GetBrush("WhiteBrush");
    const FLinearColor Gold(1,.74,.28),White(.94,.97,1),Dark(.015,.022,.03,.92);
    auto Box=[&](float X,float Y,float W,float H,FLinearColor C){FSlateDrawElement::MakeBox(Out,++L,G.ToPaintGeometry(FVector2f(W*S,H*S),FSlateLayoutTransform(FVector2f(Origin+FVector2D(X,Y)*S))),Brush,ESlateDrawEffect::None,C);};
    auto Text=[&](FString T,float X,float Y,int32 Font,FLinearColor C,bool bTitle=false){FSlateDrawElement::MakeText(Out,++L,G.ToPaintGeometry(FVector2f(1550*S,65*S),FSlateLayoutTransform(FVector2f(Origin+FVector2D(X,Y)*S))),T,BrainMiningFonts::Style(bTitle ? TEXT("Bold") : TEXT("Medium"),FMath::Max(10,FMath::RoundToInt(Font*S))),ESlateDrawEffect::None,C);};
    if(E->Phase==EMiningEscapePhase::Failed)
    {
        FSlateDrawElement::MakeBox(Out,++L,G.ToPaintGeometry(),Brush,ESlateDrawEffect::None,FLinearColor(.22f,.005f,.02f,.56f));
        const FString Message=TEXT("被水淹没没关系，再来一次吧！");
        const auto Font=BrainMiningFonts::Body(FMath::RoundToInt(30*S));
        const float Width=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Message,Font).X/S;
        Text(Message,(1600-Width)*.5f,420,30,White);return L;
    }
    if (!UMiningInteractionHUDWidget::IsActiveFor(E->GetWorld()->GetFirstPlayerController())) Box(797,447,6,6,White);
    auto* PC=E->GetWorld()->GetFirstPlayerController();if(!PC)return L;
    FVector2D P;const FVector Target=E->GuideTarget();
    if(PC->ProjectWorldLocationToScreen(Target,P,true))
    {
        // Projection returns render pixels; this Slate overlay paints in local units.
        int32 Width=0,Height=0;PC->GetViewportSize(Width,Height);
        if(Width>0 && Height>0){P.X*=Size.X/Width;P.Y*=Size.Y/Height;}
    }
    else
    {FVector Eye;FRotator Aim;PC->GetPlayerViewPoint(Eye,Aim);const FVector D=Target-Eye;P=Size*.5+FVector2D(FVector::DotProduct(D,FRotationMatrix(Aim).GetUnitAxis(EAxis::Y)),-FVector::DotProduct(D,FRotationMatrix(Aim).GetUnitAxis(EAxis::Z))).GetSafeNormal()*Size.Y*.7;}
    P=(P-Origin)/S;P.X=FMath::Clamp(P.X,160.,1340.);P.Y=FMath::Clamp(P.Y,195.,720.);
    Box(P.X-12,P.Y-12,24,24,Gold);Box(P.X-7,P.Y-7,14,14,Dark);
    const FString Label=FString::Printf(TEXT("%.1f 米"),FMath::Max(0.f,E->ExitDistance()));
    const auto DistanceFont=BrainMiningFonts::Style(TEXT("Medium"),FMath::Max(10,FMath::RoundToInt(18*S)));
    const float LabelWidth=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label,DistanceFont).X/S;
    Text(Label,P.X-LabelWidth*.5f+1,P.Y+23,18,FLinearColor(0,0,0,.85f));
    Text(Label,P.X-LabelWidth*.5f,P.Y+22,18,White);
    return L;
}
