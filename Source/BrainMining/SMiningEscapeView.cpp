#include "SMiningEscapeView.h"
#include "MiningRoomEscape.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
int32 SMiningEscapeView::OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const
{
    if(!Owner.IsValid() || Owner->Phase==EMiningEscapePhase::Idle)return Layer;
    const auto* E=Owner.Get();const FVector2D Size=G.GetLocalSize();const float S=FMath::Min(Size.X/1600,Size.Y/900);const FVector2D Origin=(Size-FVector2D(1600,900)*S)*.5;
    int32 L=Layer;auto* Brush=FCoreStyle::Get().GetBrush("WhiteBrush");
    const FLinearColor Gold(1,.74,.28),White(.94,.97,1),Cyan(.12,.85,1),Dark(.015,.022,.03,.92);
    auto Box=[&](float X,float Y,float W,float H,FLinearColor C){FSlateDrawElement::MakeBox(Out,++L,G.ToPaintGeometry(FVector2f(W*S,H*S),FSlateLayoutTransform(FVector2f(Origin+FVector2D(X,Y)*S))),Brush,ESlateDrawEffect::None,C);};
    auto Text=[&](FString T,float X,float Y,int32 Font,FLinearColor C){FSlateDrawElement::MakeText(Out,++L,G.ToPaintGeometry(FVector2f(1550*S,65*S),FSlateLayoutTransform(FVector2f(Origin+FVector2D(X,Y)*S))),T,FCoreStyle::GetDefaultFontStyle("Regular",FMath::Max(10,FMath::RoundToInt(Font*S))),ESlateDrawEffect::None,C);};
    if(E->Phase==EMiningEscapePhase::Failed)
    {
        Box(0,0,1600,900,FLinearColor(.22,.005,.02,.56));Box(410,340,800,150,Dark);
        Text(E->FailureReason,450,365,30,White);Text(TEXT("正在回到涨水检查点……无需重新打牌"),450,422,21,Gold);return L;
    }
    if(E->Phase==EMiningEscapePhase::Escaped)
    {
        Box(390,300,820,185,Dark);Text(TEXT("逃生成功 · 已离开带电水域"),440,335,30,Gold);
        Text(TEXT("卡牌奖励已保留。按 R 可重新体验逃生段。"),440,402,20,White);return L;
    }
    Box(32,24,600,116,Dark);
    Text(E->Phase==EMiningEscapePhase::Warning?TEXT("墙后传来水声……快离开桌面！"):TEXT("水正在上涨！飞向高处的电脑"),54,40,25,Gold);
    if(E->Phase==EMiningEscapePhase::Warning)Text(FString::Printf(TEXT("%.0f 秒后涌水  ·  已建立逃生检查点"),FMath::CeilToFloat(E->WarningLeft())),54,84,20,White);
    else Text(FString::Printf(TEXT("房间灌满 %.0fs   |   电脑进水 %.0fs   |   第 %d 次尝试"),E->SecondsLeft(),E->ElectricalSecondsLeft(),E->Attempts),54,84,19,White);
    Box(1420,180,18,430,Dark);const float Rise=FMath::Clamp((E->WaterZ-E->WaterStartZ)/FMath::Max(1.f,E->WaterEndZ-E->WaterStartZ),0.f,1.f);
    Box(1422,608-426*Rise,14,426*Rise,Cyan);Text(TEXT("水位"),1404,145,17,Cyan);
    Box(350,798,900,65,Dark);Text(TEXT("按住空格飞行 · 左键挖掘 · R 重试本段"),475,808,21,White);
    Box(797,447,6,6,White);
    auto* PC=E->GetWorld()->GetFirstPlayerController();if(!PC)return L;
    FVector2D P;const FVector Target=E->GuideTarget();
    if(!PC->ProjectWorldLocationToScreen(Target,P,true))
    {FVector Eye;FRotator Aim;PC->GetPlayerViewPoint(Eye,Aim);const FVector D=Target-Eye;P=Size*.5+FVector2D(FVector::DotProduct(D,FRotationMatrix(Aim).GetUnitAxis(EAxis::Y)),-FVector::DotProduct(D,FRotationMatrix(Aim).GetUnitAxis(EAxis::Z))).GetSafeNormal()*Size.Y*.7;}
    P=(P-Origin)/S;P.X=FMath::Clamp(P.X,160.,1340.);P.Y=FMath::Clamp(P.Y,195.,720.);
    Box(P.X-12,P.Y-12,24,24,Gold);Box(P.X-7,P.Y-7,14,14,Dark);
    const FString Label=E->bEnteredComputer?FString::Printf(TEXT("斜向上挖 · 通路 %d / %d"),E->ClearedNodes,E->ComputerRocks.Num()):TEXT("安全出口 → 飞到电脑屏幕，挖进去");
    Box(P.X-210,P.Y+22,480,40,Dark);Text(Label,P.X-198,P.Y+29,17,E->bEnteredComputer?Cyan:Gold);
    return L;
}
