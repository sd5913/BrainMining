#include "SCardRoomView.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "Engine/Font.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "UObject/StrongObjectPtr.h"

UObject* SCardRoomView::CardResource(const FString& Name) const
{
    auto& Resource=Resources.FindOrAdd(Name);
    if(!Resource.IsValid()) Resource.Reset(LoadObject<UMaterialInterface>(nullptr,*(TEXT("/Game/Rooms/CardDuel/UI/Refined/")+Name+TEXT(".")+Name)));
    return Resource.Get();
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
    const bool Focused = Owner.IsValid() && Owner->bGamepadInput && Owner->GetFocusedCardIndex() == Index;
    const float Y=(Many?698.f:672.f)-(Run.Staged.Contains(Index)?30.f:0.f)-(Focused?20.f:0.f);
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
    auto Art=[&](const FString& Name,float X,float Y,float W,float H,FLinearColor Tint=FLinearColor::White){
        FSlateBrush B;B.DrawAs=ESlateBrushDrawType::Image;B.SetResourceObject(CardResource(Name));
        FSlateDrawElement::MakeBox(Out,++L,Geo(X,Y,W,H),&B,ESlateDrawEffect::None,Tint);
    };
    auto Box=[&](float X,float Y,float W,float H,FLinearColor Color,float Radius){
        FSlateRoundedBoxBrush B(FLinearColor::White,Radius*S);
        FSlateDrawElement::MakeBox(Out,++L,Geo(X,Y,W,H),&B,ESlateDrawEffect::None,Color);
    };
    if(R.Round>0)
    {
        auto Clip=[&](float X,float Y,float W,float H){
            const FVector2D Min=G.LocalToAbsolute(Origin+FVector2D(X,Y)*S);
            const FVector2D Max=G.LocalToAbsolute(Origin+FVector2D(X+W,Y+H)*S);
            Out.PushClip(FSlateClippingZone(FSlateRect(Min.X,Min.Y,Max.X,Max.Y)));
        };
        auto Health=[&](bool Player,float HP,float Y){
            const float X=46, BarX=X+39, BarY=Y+13, BarW=289, BarH=26;
            const FLinearColor Color=Player?FLinearColor(.64f,.43f,.13f):FLinearColor(.58f,.16f,.12f);
            Box(BarX,BarY+3,BarW,BarH,FLinearColor(.025f,.018f,.012f,.35f),13.f);
            Box(BarX,BarY,BarW,BarH,FLinearColor(.39f,.30f,.21f,.88f),13.f);
            Box(BarX+2,BarY+2,BarW-4,BarH-4,FLinearColor(.055f,.038f,.026f,.95f),11.f);
            const float Fill=FMath::Clamp(HP/100.f,0.f,1.f);
            if(Fill>0.f)
            {
                // Clip the continuous fill, so zero HP is empty and damage cannot cover the frame or portrait.
                // Start the measured fill after the overlapping badge: even
                // one HP remains visible instead of hiding beneath the icon.
                Clip(BarX+17,BarY+3,(BarW-20)*Fill,BarH-6);
                Box(BarX+17,BarY+3,BarW-20,BarH-6,Color,10.f);
                Out.PopClip();
            }
            // The badge overlaps the continuous track and shares its frame, rather
            // than clipping a portrait out of the old segmented health texture.
            const FLinearColor Frame(.39f,.30f,.21f,.98f), Ink(.055f,.038f,.026f,1);
            const FLinearColor Ivory(.91f,.82f,.65f,1);
            Box(X,Y+3,56,52,FLinearColor(.025f,.018f,.012f,.35f),18);
            Box(X,Y,56,52,Frame,18);Box(X+2,Y+2,52,48,Ink,16);
            Box(X+5,Y+5,46,42,Color*.70f,13);
            if(Player)
            {
                // Miner: warm face, rounded hard hat and a pale headlamp.
                Box(X+17,Y+24,23,17,Ivory,8);
                Box(X+12,Y+12,32,21,Color*1.32f,12);
                Box(X+9,Y+27,38,5,Ivory,2.5f);
                Box(X+24,Y+12,8,18,Frame,3);
                Box(X+22,Y+17,12,10,Ivory,4);
                Box(X+25,Y+19,6,6,FLinearColor(1,.93f,.70f),3);
            }
            else
            {
                // Opponent: the square cardboard robot from the room.
                Box(X+9,Y+22,5,12,Ivory,2);Box(X+42,Y+22,5,12,Ivory,2);
                Box(X+13,Y+13,30,27,Ivory,6);
                Box(X+18,Y+23,6,6,Ink,3);Box(X+32,Y+23,6,6,Ink,3);
                Box(X+21,Y+33,14,3,Ink,1.5f);
                Box(X+18,Y+18,8,2,Ink,1);Box(X+30,Y+18,8,2,Ink,1);
            }
        };
        Health(false,R.RivalHP,110); Health(true,R.PlayerHP,755);
    }
    if(R.Phase==ECardRoomPhase::Choosing)
    {
        auto& CachedFont=Resources.FindOrAdd(TEXT("CardFont"));
        if(!CachedFont.IsValid()) CachedFont.Reset(LoadObject<UFont>(nullptr,TEXT("/Game/Fonts/F_SourceHanSansCN_Medium.F_SourceHanSansCN_Medium")));
        UObject* Font=CachedFont.Get();
        const TCHAR* Names[]={TEXT("激光"),TEXT("毒"),TEXT("火")};
        const TCHAR* Materials[]={TEXT("M_Card_Laser"),TEXT("M_Card_Poison"),TEXT("M_Card_Fire")};
        int32 Focused = Owner->bGamepadInput ? Owner->GetFocusedCardIndex() : INDEX_NONE;
        if(!Owner->bGamepadInput)
            for(int32 I=R.Hand.Num()-1;I>=0;--I)if(In(Mouse,CardRect(I,R))){Focused=I;break;}
        TArray<int32> DrawOrder;
        for (int32 Index = 0; Index < R.Hand.Num(); ++Index) if (Index != Focused) DrawOrder.Add(Index);
        if (Focused != INDEX_NONE) DrawOrder.Add(Focused);
        for (const int32 I : DrawOrder)
        {
            const auto B=CardRect(I,R);const float W=B.Right-B.Left,H=B.Bottom-B.Top;
            const bool Selected=R.Staged.Contains(I);
            Box(B.Left+4,B.Top+7,W,H,FLinearColor(0,0,0,.4f),9.f);
            const float Border = I == Focused ? 4.f : 2.f;
            Box(B.Left-Border,B.Top-Border,W+Border*2,H+Border*2,I == Focused ? FLinearColor::White : (Selected?FLinearColor(.95f,.78f,.4f):FLinearColor(.55f,.5f,.4f)),10.f);
            Art(Materials[static_cast<int32>(R.Hand[I])],B.Left,B.Top,W,H);
            // The name sits at the exposed upper-left edge even in the 17-card fan.
            const int32 N=static_cast<int32>(R.Hand[I]);
            FSlateFontInfo F(Font,FMath::Max(9,FMath::RoundToInt((R.Hand.Num()>5?18:22)*S)),TEXT("Medium"));
            F.OutlineSettings.OutlineSize=1;F.OutlineSettings.OutlineColor=FLinearColor(.12f,.1f,.08f,.8f);
            const FString Name=R.Hand.Num()>5 && N==0 ? TEXT("激\n光") : Names[N];
            FSlateDrawElement::MakeText(Out,++L,Geo(B.Left+9,B.Top+12,W-18,50),Name,F,ESlateDrawEffect::None,FLinearColor(.98f,.94f,.83f));
        }
    }
    return L;
}
FReply SCardRoomView::OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
    if(!Owner.IsValid() || !Owner->Stage || (!E.IsTouchEvent() && E.GetEffectingButton()!=EKeys::LeftMouseButton))return FReply::Unhandled();
    Owner->bGamepadInput = false;
    Mouse=ToVirtual(G,E.GetScreenSpacePosition());auto& R=Owner->Stage->Run;
    if(R.Phase==ECardRoomPhase::Exploring)return FReply::Unhandled();
    if(R.Phase==ECardRoomPhase::Choosing)
        for(int32 I=R.Hand.Num()-1;I>=0;--I)if(In(Mouse,CardRect(I,R)))
        {
            Owner->ToggleCardSelection(I);
            break;
        }
    return FReply::Handled().SetUserFocus(SharedThis(this));
}
FReply SCardRoomView::OnTouchStarted(const FGeometry& G,const FPointerEvent& E)
{
    return OnMouseButtonDown(G,E);
}
FReply SCardRoomView::OnMouseButtonUp(const FGeometry& G,const FPointerEvent& E) { return FReply::Unhandled(); }
FReply SCardRoomView::OnMouseMove(const FGeometry& G,const FPointerEvent& E) { if(Owner.IsValid())Owner->bGamepadInput=false; Mouse=ToVirtual(G,E.GetScreenSpacePosition()); Invalidate(EInvalidateWidgetReason::Paint); return FReply::Unhandled(); }

FReply SCardRoomView::OnKeyDown(const FGeometry& G,const FKeyEvent& E)
{
    if(!Owner.IsValid()) return FReply::Unhandled();
    if(E.GetKey()==EKeys::SpaceBar) { Owner->bGamepadInput = false; Owner->ConfirmCards(); return FReply::Handled(); }
    if(E.GetKey()==EKeys::E) { Owner->Interact(); return FReply::Handled(); }
    return FReply::Unhandled();
}
