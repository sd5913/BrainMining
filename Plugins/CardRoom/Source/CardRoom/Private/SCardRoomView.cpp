#include "SCardRoomView.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "Engine/Font.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    UObject* CardResource(const FString& Name)
    {
        static TMap<FString,TStrongObjectPtr<UObject>> Cache;
        auto& Resource=Cache.FindOrAdd(Name);
        if(!Resource.IsValid()) Resource.Reset(LoadObject<UMaterialInterface>(nullptr,*(TEXT("/Game/Rooms/CardDuel/UI/Refined/")+Name+TEXT(".")+Name)));
        return Resource.Get();
    }
}
FVector2D SCardRoomView::ToVirtual(const FGeometry& G,const FVector2D& P) const
{
    const FVector2D Size=G.GetLocalSize(); const float S=FMath::Min(Size.X/1600,Size.Y/900);
    return (G.AbsoluteToLocal(P)-(Size-FVector2D(1600,900)*S)*.5)/S;
}
FSlateRect SCardRoomView::CardRect(int32 Index,const FCardRoomRun& Run) const
{
    const bool Many=Run.Hand.Num()>5;
    const float W=Many?120.f:138.f,H=Many?180.f:208.f,Step=Many?43.f:160.f;
    const float X=800.f-(W+(Run.Hand.Num()-1)*Step)*.5f+Index*Step;
    const float Y=(Many?698.f:672.f)-(Run.Staged.Contains(Index)?30.f:0.f);
    return FSlateRect(X,Y,X+W,Y+H);
}
int32 SCardRoomView::OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const
{
    if(!Owner.IsValid() || !Owner->Stage) return Layer;
    const auto& R=Owner->Stage->Run;
    if(R.Phase==ECardRoomPhase::Exploring || R.Phase==ECardRoomPhase::Collected) return Layer;
    const FVector2D Size=G.GetLocalSize(); const float S=FMath::Min(Size.X/1600,Size.Y/900);
    const FVector2D Origin=(Size-FVector2D(1600,900)*S)*.5;
    int32 L=Layer;
    auto Geo=[&](float X,float Y,float W,float H){return G.ToPaintGeometry(FVector2f(W*S,H*S),FSlateLayoutTransform(FVector2f(Origin+FVector2D(X,Y)*S)));};
    auto Art=[&](const FString& Name,float X,float Y,float W,float H){
        FSlateBrush B;B.DrawAs=ESlateBrushDrawType::Image;B.SetResourceObject(CardResource(Name));
        FSlateDrawElement::MakeBox(Out,++L,Geo(X,Y,W,H),&B,ESlateDrawEffect::None,FLinearColor::White);
    };
    auto Box=[&](float X,float Y,float W,float H,FLinearColor Color,float Radius){
        FSlateRoundedBoxBrush B(FLinearColor::White,Radius*S);
        FSlateDrawElement::MakeBox(Out,++L,Geo(X,Y,W,H),&B,ESlateDrawEffect::None,Color);
    };
    if(R.Round>0)
    {
        auto Health=[&](bool Player,float HP,float Y){
            const float X=46,W=330,H=52;
            Art(Player?TEXT("M_Health_Player"):TEXT("M_Health_Opponent"),X,Y,W,H);
            const float Fill=FMath::Clamp(HP/100.f,0.f,1.f);
            if(Fill<1.f) Box(X+W*.17f+W*.814f*Fill,Y+6,W*.814f*(1-Fill),H-12,FLinearColor(.015f,.018f,.02f,.78f),4.f);
        };
        Health(false,R.RivalHP,110); Health(true,R.PlayerHP,755);
    }
    if(R.Phase==ECardRoomPhase::Choosing)
    {
        static TStrongObjectPtr<UFont> Font(LoadObject<UFont>(nullptr,TEXT("/Game/Fonts/F_SourceHanSansCN_Medium.F_SourceHanSansCN_Medium")));
        const TCHAR* Names[]={TEXT("激光"),TEXT("毒"),TEXT("火")};
        const TCHAR* Materials[]={TEXT("M_Card_Laser"),TEXT("M_Card_Poison"),TEXT("M_Card_Fire")};
        for(int32 I=0;I<R.Hand.Num();++I)
        {
            const auto B=CardRect(I,R);const float W=B.Right-B.Left,H=B.Bottom-B.Top;
            const bool Selected=R.Staged.Contains(I);
            Box(B.Left+4,B.Top+7,W,H,FLinearColor(0,0,0,.4f),9.f);
            Box(B.Left-2,B.Top-2,W+4,H+4,Selected?FLinearColor(.95f,.78f,.4f):FLinearColor(.55f,.5f,.4f),10.f);
            Art(Materials[static_cast<int32>(R.Hand[I])],B.Left,B.Top,W,H);
            // The name sits at the exposed upper-left edge even in the 17-card fan.
            const int32 N=static_cast<int32>(R.Hand[I]);
            FSlateFontInfo F(Font.Get(),FMath::Max(9,FMath::RoundToInt((R.Hand.Num()>5?18:22)*S)),TEXT("Medium"));
            F.OutlineSettings.OutlineSize=1;F.OutlineSettings.OutlineColor=FLinearColor(.12f,.1f,.08f,.8f);
            const FString Name=R.Hand.Num()>5 && N==0 ? TEXT("激\n光") : Names[N];
            FSlateDrawElement::MakeText(Out,++L,Geo(B.Left+9,B.Top+12,W-18,50),Name,F,ESlateDrawEffect::None,FLinearColor(.98f,.94f,.83f));
            if(Owner->bGamepadNavigation && Owner->FocusedCard==I) Box(B.Left+4,B.Bottom+5,W-8,4,FLinearColor(1,.8f,.3f),2.f);
        }
    }
    return L;
}
FReply SCardRoomView::OnTouchStarted(const FGeometry& G,const FPointerEvent& E)
{
    return OnMouseButtonDown(G,E);
}
FReply SCardRoomView::OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
    if(!Owner.IsValid() || !Owner->Stage || (!E.IsTouchEvent() && E.GetEffectingButton()!=EKeys::LeftMouseButton))return FReply::Unhandled();
    Owner->bGamepadNavigation=false;Owner->bConfirmLeave=false;
    Mouse=ToVirtual(G,E.GetScreenSpacePosition());auto& R=Owner->Stage->Run;
    if(R.Phase==ECardRoomPhase::Exploring)return FReply::Unhandled();
    if(R.Phase==ECardRoomPhase::Choosing)
        for(int32 I=R.Hand.Num()-1;I>=0;--I)if(In(Mouse,CardRect(I,R)))
        {
            if(R.Staged.Contains(I))R.Unstage(I);else R.Stage(I);
            if(Owner->Stage->AudioOverride.IsBound())Owner->Stage->Sound(TEXT("S_CardSelect"),.3f);
            break;
        }
    return FReply::Handled().SetUserFocus(SharedThis(this));
}
FReply SCardRoomView::OnTouchStarted(const FGeometry& G,const FPointerEvent& E)
{
    return OnMouseButtonDown(G,E);
}
FReply SCardRoomView::OnMouseButtonUp(const FGeometry& G,const FPointerEvent& E) { return FReply::Unhandled(); }
FReply SCardRoomView::OnMouseMove(const FGeometry& G,const FPointerEvent& E) { Mouse=ToVirtual(G,E.GetScreenSpacePosition()); return FReply::Unhandled(); }

FReply SCardRoomView::OnKeyDown(const FGeometry& G,const FKeyEvent& E)
{
    if(!Owner.IsValid()) return FReply::Unhandled();
    if(E.GetKey()==EKeys::SpaceBar) { Owner->ConfirmCards(); return FReply::Handled(); }
    if(E.GetKey()==EKeys::E) { Owner->Interact(); return FReply::Handled(); }
    return FReply::Unhandled();
}
