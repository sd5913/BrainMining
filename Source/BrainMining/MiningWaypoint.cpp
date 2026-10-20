#include "MiningWaypoint.h"
#include "BrainMiningFonts.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"

int32 MiningWaypoint::Paint(const FGeometry& G,FSlateWindowElementList& Out,int32 L,APlayerController* PC,const FVector& Target,const FString& Label)
{
    if(!PC || PC->IsPaused() || PC->bShowMouseCursor)return L;
    const FVector2D Size=G.GetLocalSize();const float S=FMath::Min(Size.X/1600,Size.Y/900);
    if(S<=0)return L;
    const FVector2D Origin=(Size-FVector2D(1600,900)*S)*.5;
    FVector2D P;
    if(PC->ProjectWorldLocationToScreen(Target,P,true))
    {int32 W=0,H=0;PC->GetViewportSize(W,H);if(W>0&&H>0){P.X*=Size.X/W;P.Y*=Size.Y/H;}}
    else
    {FVector Eye;FRotator Aim;PC->GetPlayerViewPoint(Eye,Aim);const FVector D=Target-Eye;P=Size*.5+FVector2D(FVector::DotProduct(D,FRotationMatrix(Aim).GetUnitAxis(EAxis::Y)),-FVector::DotProduct(D,FRotationMatrix(Aim).GetUnitAxis(EAxis::Z))).GetSafeNormal()*Size.Y*.7;}
    P=(P-Origin)/S;P.X=FMath::Clamp(P.X,160.,1340.);P.Y=FMath::Clamp(P.Y,195.,720.);
    const auto* Brush=FCoreStyle::Get().GetBrush("WhiteBrush");
    auto Box=[&](float X,float Y,float W,float H,FLinearColor C){FSlateDrawElement::MakeBox(Out,++L,G.ToPaintGeometry(FVector2f(W*S,H*S),FSlateLayoutTransform(FVector2f(Origin+FVector2D(X,Y)*S))),Brush,ESlateDrawEffect::None,C);};
    Box(P.X-12,P.Y-12,24,24,FLinearColor(1,.74,.28));Box(P.X-7,P.Y-7,14,14,FLinearColor(.015,.022,.03,.92));
    const auto Font=BrainMiningFonts::Style(TEXT("Medium"),FMath::Max(10,FMath::RoundToInt(18*S)));
    const float Width=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label,Font).X/S;
    for(int I=0;I<2;++I)FSlateDrawElement::MakeText(Out,++L,G.ToPaintGeometry(FVector2f(1550*S,65*S),FSlateLayoutTransform(FVector2f(Origin+FVector2D(P.X-Width*.5f+(I?0:1),P.Y+22+(I?0:1))*S))),Label,Font,ESlateDrawEffect::None,I?FLinearColor(.94,.97,1):FLinearColor(0,0,0,.85));
    return L;
}
