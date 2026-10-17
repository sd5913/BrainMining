#include "SMiningEscapeView.h"
#include "MiningRoomEscape.h"
#include "MiningInteractionHUDWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
int32 SMiningEscapeView::OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const
{
    if(!Owner.IsValid() || Owner->Phase==EMiningEscapePhase::Idle || Owner->Phase==EMiningEscapePhase::Escaped)return Layer;
    const auto* E=Owner.Get();const FVector2D Size=G.GetLocalSize();const float S=FMath::Min(Size.X/1600,Size.Y/900);const FVector2D Origin=(Size-FVector2D(1600,900)*S)*.5;
    int32 L=Layer;auto* Brush=FCoreStyle::Get().GetBrush("WhiteBrush");
    const FLinearColor Gold(1,.74,.28),White(.94,.97,1),Cyan(.12,.85,1),Dark(.015,.022,.03,.92);
    auto Box=[&](float X,float Y,float W,float H,FLinearColor C){FSlateDrawElement::MakeBox(Out,++L,G.ToPaintGeometry(FVector2f(W*S,H*S),FSlateLayoutTransform(FVector2f(Origin+FVector2D(X,Y)*S))),Brush,ESlateDrawEffect::None,C);};
    auto Text=[&](FString T,float X,float Y,int32 Font,FLinearColor C){FSlateDrawElement::MakeText(Out,++L,G.ToPaintGeometry(FVector2f(1550*S,65*S),FSlateLayoutTransform(FVector2f(Origin+FVector2D(X,Y)*S))),T,FCoreStyle::GetDefaultFontStyle("Regular",FMath::Max(10,FMath::RoundToInt(Font*S))),ESlateDrawEffect::None,C);};
    if(E->Phase==EMiningEscapePhase::Failed)
    {
        Box(0,0,1600,900,FLinearColor(.22,.005,.02,.56));Box(410,340,800,150,Dark);
        Text(E->FailureReason,450,365,30,White);Text(E->bComputerCheckpoint?TEXT("正在回到电视屏幕前……无需重跑攀升路线"):TEXT("正在回到涨水检查点……无需重新打牌"),450,422,21,Gold);return L;
    }
    Box(32,24,600,64,Dark);
    if(E->Phase==EMiningEscapePhase::Warning)Text(FString::Printf(TEXT("%.0f 秒后水位快速上涨  ·  已建立逃生检查点"),FMath::CeilToFloat(E->WarningLeft())),54,40,20,White);
    else Text(E->bComputerCheckpoint?FString::Printf(TEXT("触电倒计时 %.0fs   |   检查点 2 / 2   |   第 %d 次尝试"),E->ElectricalSecondsLeft(),E->Attempts):FString::Printf(TEXT("房间灌满 %.0fs   |   检查点 1 / 2   |   第 %d 次尝试"),E->SecondsLeft(),E->Attempts),54,40,19,White);
    Box(1420,180,18,430,Dark);const float Rise=FMath::Clamp((E->WaterZ-E->WaterStartZ)/FMath::Max(1.f,E->WaterEndZ-E->WaterStartZ),0.f,1.f);
    Box(1422,608-426*Rise,14,426*Rise,Cyan);Text(TEXT("水位"),1404,145,17,Cyan);
    Text(FString::Printf(TEXT("%.1f m"),(E->WaterZ-E->WaterStartZ)/100.f),1380,625,19,Cyan);
    if(APawn* Pawn=E->GetWorld()->GetFirstPlayerController()->GetPawn())
        Text(FString::Printf(TEXT("距出口 %.1f m"),FVector::Distance(Pawn->GetActorLocation(),E->Exit)/100.f),54,98,21,Gold);
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
    const FString Label=E->bExitGuidance && E->ClearedNodes==E->ComputerRocks.Num()?FString::Printf(TEXT("沿缓坡前往歇脚平台 · %.1f 米"),E->ExitDistance()):E->bEnteredComputer?FString::Printf(TEXT("斜向上挖 · 距亮灯出口 %.1f 米 · %d / %d"),E->ExitDistance(),E->ClearedNodes,E->ComputerRocks.Num()):E->bExitGuidance?FString::Printf(TEXT("挖开屏幕后斜向上 · 距出口 %.1f 米"),E->ExitDistance()):FString::Printf(TEXT("挖进闪光电视屏幕 · 直线距离 %.1f 米"),E->ExitDistance());
    Box(P.X-210,P.Y+22,480,40,Dark);Text(Label,P.X-198,P.Y+29,17,E->bExitGuidance?Cyan:Gold);
    return L;
}
