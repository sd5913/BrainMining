#include "SCardRoomView.h"
#include "CardRoomSymbols.h"
#include "Rendering/SlateRenderer.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Texture2D.h"

namespace CardRoomUI
{
    const FLinearColor Cream(.94f,.86f,.69f,1),Gold(.9f,.61f,.25f,1),Ink(.10f,.07f,.055f,1),Dark(.035f,.044f,.043f,.95f);
    const FSlateRect Match(640,740,960,800),Confirm(1100,590,1530,640),Collect(48,790,548,850),All(1100,538,1530,580);
    FLinearColor Color(ERoomCard C) { return C==ERoomCard::Laser?FLinearColor(.18f,.65f,.77f):C==ERoomCard::Poison?FLinearColor(.43f,.66f,.23f):FLinearColor(.91f,.34f,.15f); }
    UTexture2D* ArtTexture(const TCHAR* Path)
    {
        static TMap<FString,UTexture2D*> Cache;
        UTexture2D*& Texture=Cache.FindOrAdd(Path);
        if(!Texture)
        {
            Texture=LoadObject<UTexture2D>(nullptr,Path);
            if(Texture) Texture->AddToRoot();
        }
        return Texture;
    }
    FSlateBrush ArtBrush(const TCHAR* Path,const FVector2D& Size)
    {
        FSlateBrush Brush;
        Brush.DrawAs=ESlateBrushDrawType::Image;
        Brush.SetResourceObject(ArtTexture(Path));
        Brush.ImageSize=Size;
        return Brush;
    }
}
FVector2D SCardRoomView::ToVirtual(const FGeometry& G,const FVector2D& P) const
{
    const FVector2D Size=G.GetLocalSize(); const float S=FMath::Min(Size.X/1600,Size.Y/900);
    return (G.AbsoluteToLocal(P)-(Size-FVector2D(1600,900)*S)*.5)/S;
}
FSlateRect SCardRoomView::CardRect(int32 Index,const FCardRoomRun& Run) const
{
    const bool Many=Run.Hand.Num()>5; const float Width=Many?64:138,Step=Many?75:160;
    const float Left=800-(Run.Hand.Num()*Step- (Step-Width))*.5+Index*Step;
    const float Top=Run.Staged.Contains(Index)?642:672;
    return FSlateRect(Left,Top,Left+Width,Top+(Many?96:200));
}
int32 SCardRoomView::OnPaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Cull,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const
{
    using namespace CardRoomUI;
    if(!Owner.IsValid() || !Owner->Stage) return Layer;
    const FCardRoomRun& R=Owner->Stage->Run;
    if(R.Phase==ECardRoomPhase::Collected)return Layer;
    const FVector2D Size=G.GetLocalSize(); const float S=FMath::Min(Size.X/1600,Size.Y/900); const FVector2D Origin=(Size-FVector2D(1600,900)*S)*.5;
    const FSlateBrush* Brush=FCoreStyle::Get().GetBrush("WhiteBrush"); int32 L=Layer;
    auto Geo=[&](float X,float Y,float W,float H) { return G.ToPaintGeometry(FVector2f(W*S,H*S),FSlateLayoutTransform(FVector2f(Origin+FVector2D(X,Y)*S))); };
    auto Box=[&](float X,float Y,float W,float H,FLinearColor C) { FSlateDrawElement::MakeBox(Out,++L,G.ToPaintGeometry(FVector2f(W*S,H*S),FSlateLayoutTransform(FVector2f(Origin+FVector2D(X,Y)*S))),Brush,ESlateDrawEffect::None,C); };
    auto Text=[&](FString T,float X,float Y,int32 Font,FLinearColor C=FLinearColor::White) { FSlateDrawElement::MakeText(Out,++L,G.ToPaintGeometry(FVector2f(1500*S,70*S),FSlateLayoutTransform(FVector2f(Origin+FVector2D(X,Y)*S))),T,FCoreStyle::GetDefaultFontStyle("Regular",FMath::Max(9,FMath::RoundToInt(Font*S))),ESlateDrawEffect::None,C); };
    auto Button=[&](FSlateRect Rect,FString Label,bool Active=true) { Box(Rect.Left,Rect.Top,Rect.Right-Rect.Left,Rect.Bottom-Rect.Top,Active?(In(Mouse,Rect)?FLinearColor(1,.76f,.4f):Gold):FLinearColor(.23f,.25f,.24f)); Text(Label,Rect.Left+22,Rect.Top+10,20,Active?Ink:Cream); };
    auto Icon=[&](ERoomCard Type,float X,float Y,float Extent)
    {
        const auto Art=CardRoomSymbols::Make(Type);
        TArray<FSlateVertex> Vertices; TArray<SlateIndex> Indices;
        for(const auto& Tri:Art)
        {
            for(const FVector2f P:{Tri.A,Tri.B,Tri.C})
            {
                const FVector2f Local=FVector2f(Origin)+FVector2f((X+P.X*Extent)*S,(Y+P.Y*Extent)*S);
                Indices.Add(Vertices.Num());
                Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),Local,FVector2f(.5f,.5f),Tri.Color.ToFColor(true)));
            }
        }
        FSlateDrawElement::MakeCustomVerts(Out,++L,FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Brush),Vertices,Indices,nullptr,0,0);
    };
    auto DrawTexture=[&](const TCHAR* Path,float X,float Y,float W,float H)
    {
        FSlateBrush Art=CardRoomUI::ArtBrush(Path,FVector2D(W,H));
        FSlateDrawElement::MakeBox(Out,++L,Geo(X,Y,W,H),&Art,ESlateDrawEffect::None,FLinearColor::White);
    };
    auto Card=[&](int32 Index,FSlateRect B,bool Floating=false)
    {
        const ERoomCard C=R.Hand[Index]; const bool Many=R.Hand.Num()>5&&!Floating; const bool Staged=R.Staged.Contains(Index)&&!Floating;
        const float W=B.Right-B.Left,H=B.Bottom-B.Top;
        const TCHAR* Path=C==ERoomCard::Laser?TEXT("/Game/Rooms/CardDuel/UI/T_Card_Laser.T_Card_Laser"):C==ERoomCard::Poison?TEXT("/Game/Rooms/CardDuel/UI/T_Card_Poison.T_Card_Poison"):TEXT("/Game/Rooms/CardDuel/UI/T_Card_Fire.T_Card_Fire");
        Box(B.Left+5,B.Top+7,W,H,FLinearColor(0,0,0,.48f));
        DrawTexture(Path,B.Left,B.Top,W,H);
        if(Staged)
        {
            Box(B.Left-4,B.Top-4,W+8,4,FLinearColor(1.f,.85f,.28f,.95f));
            Box(B.Left-4,B.Bottom,W+8,4,FLinearColor(1.f,.85f,.28f,.95f));
        }
        const float LabelY=B.Bottom-(Many?28.f:34.f);
        Text(Staged?TEXT("已选中 · 点击取消"):TEXT("点击选择"),B.Left+8,LabelY,Many?10:13,Staged?FLinearColor(1.f,.86f,.35f):FLinearColor(.92f,.92f,.92f));
    };
    if(R.Phase==ECardRoomPhase::Exploring)
    {
        if (!Owner->bUseSharedInteractionHUD) Box(797,447,6,6,Cream);
        return L;
    }
    if(R.Round>0)
    {
        FSlateBrush Round=CardRoomUI::ArtBrush(TEXT("/Game/Rooms/CardDuel/UI/T_RoundDots.T_RoundDots"),FVector2D(100,180));
        FSlateDrawElement::MakeBox(Out,++L,Geo(48,103,100,180),&Round,ESlateDrawEffect::None,FLinearColor::White);
        Text(FString::Printf(TEXT("第 %02d / 04 回合"),R.Round),126,132,25,Cream);
        Box(126,173,245,3,Gold);
        for(int32 i=0;i<4;++i) Box(126+i*65,189,54,4,i<R.Round?Gold:FLinearColor(.2f,.2f,.2f));
        auto Health=[&](bool Player,float HP,float X,float Y)
        {
            const TCHAR* Path=Player?TEXT("/Game/Rooms/CardDuel/UI/T_Health_Player.T_Health_Player"):TEXT("/Game/Rooms/CardDuel/UI/T_Health_Boss.T_Health_Boss");
            const float W=430.f,H=67.f;
            DrawTexture(Path,X,Y,W,H);
            const float CellsX=X+73.f,CellsW=W-80.f,Fill=FMath::Clamp(HP/100.f,0.f,1.f);
            if(Fill<1.f) Box(CellsX+CellsW*Fill,Y+8,CellsW*(1.f-Fill),H-16,FLinearColor(0.015f,.018f,.02f,.72f));
            Text(FString::Printf(TEXT("%d / 100"),FMath::RoundToInt(HP)),X+W-104,Y+24,16,FLinearColor::White);
        };
        Health(true,R.PlayerHP,46,205); Health(false,R.RivalHP,1120,205);
        if(R.bPoisoned) { Box(1355,220,217,36,FLinearColor(.1f,.23f,.08f,.9f)); Text(TEXT("中毒 · 可被点燃"),1370,228,14,FLinearColor(.7f,1,.4f)); }
    }
    if(R.Phase==ECardRoomPhase::Seated || R.Phase==ECardRoomPhase::Matching || R.Phase==ECardRoomPhase::Arrival)
    {
        Box(465,625,670,85,Dark); Text(R.Message,500,638,22,Cream);
        Text(TEXT("今晚的规则很简单：把你的好牌，全都打出去。"),500,678,14,Gold);
        if(R.Phase==ECardRoomPhase::Seated) Button(Match,TEXT("开始匹配     →"));
        else Text(R.Phase==ECardRoomPhase::Matching?TEXT("·   ·   ·"):TEXT("请注意头顶！"),715,745,24,Gold);
    }
    if(R.Phase==ECardRoomPhase::Choosing)
    {
        // Prompts live above the hand. The table stays unobstructed.
        Box(48,596,910,55,Dark);
        FString Prompt=R.CanConfirm()?FString::Printf(TEXT("已选 %d 张 · 按空格，甩向桌心！"),R.Staged.Num()):R.Round==3?TEXT("点击选中「火」和「激光弹」，空格打出组合技"):R.Round==4?TEXT("最后一搏！选中 17 张牌，空格全部甩出"):TEXT("点击手牌选中，再按空格打出");
        Text(Prompt,68,609,19,Cream);
        Button(Confirm,R.CanConfirm()?TEXT("空格   打出选中的牌"):FString::Printf(TEXT("已选 %d / %d 张"),R.Staged.Num(),R.Hand.Num()),R.CanConfirm());
        if(R.Round==4) Button(All,R.CanConfirm()?TEXT("取消全选"):TEXT("全选 17 张"));
        for(int32 i=0;i<R.Hand.Num();++i) Card(i,CardRect(i,R));
    }
    // Opponent speech stays beside the face, never over the table.
    if(R.Phase==ECardRoomPhase::Arrival || R.Phase==ECardRoomPhase::Choosing || R.Phase==ECardRoomPhase::Resolving)
    {
        FString Speech;
        if(R.Phase==ECardRoomPhase::Arrival) Speech=TEXT("哎哟！怎么又是空降？！");
        else if(R.Phase==ECardRoomPhase::Choosing)
        {
            const bool Alt=FMath::Fmod(R.PhaseTime,9.f)>4.5f;
            Speech=R.Round==1?(Alt?TEXT("别紧张，我只是个玩偶。"):TEXT("来呀，让我看看你的牌！")):R.Round==2?(Alt?TEXT("等等，你手里那张绿的……"):TEXT("刚才那下？完全不疼！")):R.Round==3?(Alt?TEXT("你不会还想点火吧？！"):TEXT("咳咳！谁把窗户打开！")):(Alt?TEXT("十七张？！你认真的？！"):TEXT("就剩一滴血，还想翻盘？"));
        }
        else if(R.Round==1) Speech=R.PhaseTime<1?TEXT("等等，那束光是什么？！"):R.PhaseTime<1.75f?TEXT("嗷！我的棉花都焦了！"):TEXT("轮到我啦！接火球！");
        else if(R.Round==2) Speech=R.PhaseTime<1?TEXT("这张绿牌……闻着不对。"):TEXT("呕——！这也算魔法？！");
        else if(R.Round==3) Speech=R.PhaseTime<1?TEXT("别点火！别点火！"):R.PhaseTime<1.8f?TEXT("啊啊啊！我的眉毛！"):TEXT("气死我了！吃我一拳！");
        else Speech=R.PhaseTime<1?TEXT("停！怎么能一次出这么多！"):TEXT("我还会回来的——！");
        if(!(R.Phase==ECardRoomPhase::Resolving && R.Round==4 && R.PhaseTime>2.5f))
        {
            Box(1018,283,500,88,Ink); Box(1022,287,492,80,Cream);
            Box(1005,328,20,14,Cream); Box(996,334,13,7,Cream);
            Text(TEXT("超级人机"),1040,296,12,Ink); Text(Speech,1040,323,18,Ink);
        }
    }

    if(R.Phase==ECardRoomPhase::Resolving)
    {
        Box(335,711,930,114,Dark); Text(R.Message,372,735,23,Cream);
        Text(TEXT("牌已打出，看看会发生什么……"),585,782,15,Gold);
        if(R.Round==3 && R.PhaseTime>2.15f && R.PhaseTime<2.7f) Box(0,0,1600,900,FLinearColor(.7f,.025f,.008f,.16f));
    }
    if(R.Phase==ECardRoomPhase::Victory || R.Phase==ECardRoomPhase::Collected)
    {
        Box(48,610,870,160,Dark); Box(48,610,870,3,Gold);
        Text(R.Phase==ECardRoomPhase::Victory?TEXT("恭喜你获得胜利！"):TEXT("彗尾法球 · 已收藏"),82,634,32,Cream);
        Text(R.Phase==ECardRoomPhase::Victory?TEXT("超级人机已退场。属于你的星光，留在桌上。"):TEXT("本版奖励为房间内占位，不改变挖掘数值。"),82,703,18,Gold);
        if(R.Phase==ECardRoomPhase::Victory) Button(Collect,TEXT("E   收集桌上的彗尾法球"));
    }
    Text(TEXT("点击选牌 / 再点取消     空格出牌"),48,866,12,Cream);
    return L;
}
FReply SCardRoomView::OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
    using namespace CardRoomUI;
    if(!Owner.IsValid()||!Owner->Stage||E.GetEffectingButton()!=EKeys::LeftMouseButton) return FReply::Unhandled();
    Mouse=ToVirtual(G,E.GetScreenSpacePosition()); auto& R=Owner->Stage->Run;
    if(R.Phase==ECardRoomPhase::Exploring) return FReply::Unhandled();
    if(R.Phase==ECardRoomPhase::Seated&&In(Mouse,Match)) R.Match();
    if(R.Phase==ECardRoomPhase::Victory&&In(Mouse,Collect)) { Owner->Interact();return FReply::Handled(); }
    if(R.Phase==ECardRoomPhase::Choosing)
    {
        if(In(Mouse,Confirm)) Owner->ConfirmCards();
        else if(R.Round==4&&In(Mouse,All))
        {
            const bool Clear=R.CanConfirm();
            for(int32 i=0;i<R.Hand.Num();++i) { if(Clear) R.Unstage(i); else R.Stage(i); }
            if(Owner->Stage->AudioOverride.IsBound())Owner->Stage->Sound(TEXT("S_CardSelect"),.3f);
        }
        else for(int32 i=R.Hand.Num()-1;i>=0;--i) if(In(Mouse,CardRect(i,R)))
        {
            if(R.Staged.Contains(i)) R.Unstage(i); else R.Stage(i);
            if(Owner->Stage->AudioOverride.IsBound())Owner->Stage->Sound(TEXT("S_CardSelect"),.3f);
            break;
        }
    }
    return FReply::Handled().SetUserFocus(SharedThis(this));
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
