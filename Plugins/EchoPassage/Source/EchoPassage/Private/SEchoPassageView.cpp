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
    auto Text=[&](const FString& T,float X,float Y,int Font,FLinearColor C){FSlateDrawElement::MakeText(Out,++L,G.ToPaintGeometry(FVector2f(1500*S,60*S),FSlateLayoutTransform(FVector2f(O+FVector2D(X,Y)*S))),T,FCoreStyle::GetDefaultFontStyle("Regular",FMath::Max(10,FMath::RoundToInt(Font*S))),ESlateDrawEffect::None,C);};
    const FLinearColor White(.75f,.79f,.8f),Dim(.36f,.39f,.4f),Warm(.95f,.65f,.29f);
    Text(TEXT("回声通路"),48,35,27,White);Text(Stage->bWalkThroughConnections?TEXT("连续地图 / 回声迷宫"):Stage->bUseMaze?TEXT("回声迷宫 → 余烬牌室   /   0.3"):TEXT("通路 1–2   /   原型 0.3"),48,76,13,Dim);
    if(Owner->bChangingLevel)
    {
        Box(0,0,1600,900,FLinearColor(0,0,0,.75f));
        Text(TEXT("穿过暖光，进入余烬牌室……"),545,690,24,Warm);
        return L;
    }
    if(Stage->bCompleted)
    {
        Box(425,300,750,216,FLinearColor(.015f,.013f,.01f,.92f));Box(425,300,3,216,Warm);
        Text(TEXT("前面，有光。"),468,328,33,Warm);
        Text(Stage->bWalkThroughConnections?TEXT("继续穿过出口，沿步道回到其他区域。"):TEXT("你走出了黑暗，来到了余烬牌室门前。"),468,390,21,White);
        Text(FString::Printf(TEXT("敲击 %d 次    ·    R 重新体验"),Stage->KnockCount),468,449,16,Dim);
    }
    else if(!Stage->bCollapsed)
    {
        Text(Stage->bWalkThroughConnections?TEXT("从坡道继续向前，留意脚下。"):TEXT("离开电脑房，沿走廊向前。"),545,727,24,White);
    }
    else if(!Stage->bLanded)Text(TEXT("地面塌了！"),700,718,25,White);
    else
    {
        // Quiet center reticle remains legible without revealing the environment.
        Box(797,448,6,2,FLinearColor(.55f,.59f,.6f));Box(799,446,2,6,FLinearColor(.55f,.59f,.6f));
        FString Hint;
        if(Time-Stage->LandingTime<6)Hint=TEXT("什么也看不见……用镐子敲一下试试。");
        else if(Time<Owner->FeedbackUntil)Hint=Owner->Feedback;
        else if(Time-Stage->LastEcho>4.5f)Hint=TEXT("回声消散了。再敲一下，记住前面的路。");
        if(!Hint.IsEmpty())Text(Hint,482,767,20,White);
        const float Ready=FMath::Clamp((Time-Stage->LastEcho)/Stage->KnockCooldown,0.f,1.f);
        Box(710,824,180,2,FLinearColor(.08f,.09f,.1f));Box(710,824,180*Ready,2,Dim);
    }
    Text(TEXT("W A S D  移动     鼠标  环视     左键  敲地 / 敲墙     R  重来"),48,856,15,Dim);
    return L;
}
