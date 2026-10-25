#include "SMiningEscapeView.h"
#include "MiningWaypoint.h"
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
    return MiningWaypoint::Paint(G,Out,L,PC,E->GuideTarget(),(E->bEnteredComputer ? FString::Printf(TEXT("%.1f 米"),FMath::Max(0.f,E->ExitDistance())) : FString(TEXT("想办法进入电视"))));
}
