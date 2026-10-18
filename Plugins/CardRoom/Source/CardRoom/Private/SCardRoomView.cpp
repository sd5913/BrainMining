#include "SCardRoomView.h"
#include "CardRoomSymbols.h"
#include "Rendering/SlateRenderer.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Texture2D.h"
#include "CardRoomUISettings.h"

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
    FSlateBrush ArtBrush(UTexture2D* Texture,const FVector2D& Size)
    {
        FSlateBrush Brush;
        Brush.DrawAs=ESlateBrushDrawType::Image;
        Brush.SetResourceObject(Texture);
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
    const UCardRoomUISettings* UI=Owner.IsValid()&&Owner->Stage?Owner->Stage->UISettings.Get():nullptr;
    const bool Many=Run.Hand.Num()>5; const float Width=Many?(UI?UI->CardManySize.X:64.f):(UI?UI->CardNormalSize.X:138.f);
    const float Height=Many?(UI?UI->CardManySize.Y:96.f):(UI?UI->CardNormalSize.Y:200.f);
    const float Step=Many?(UI?UI->CardManyStep:75.f):(UI?UI->CardNormalStep:160.f);
    const float Center=UI?UI->CardHandCenterX:800.f;
    const float Left=Center-(Run.Hand.Num()*Step- (Step-Width))*.5+Index*Step;
    const float Top=(UI?UI->CardTop:672.f)-(Run.Staged.Contains(Index)?(UI?UI->CardSelectedLift:30.f):0.f);
    return FSlateRect(Left,Top,Left+Width,Top+Height);
}
int32 SCardRoomView::OnPaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Cull,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const
{
    using namespace CardRoomUI;
    if(!Owner.IsValid() || !Owner->Stage) return Layer;
    const FCardRoomRun& R=Owner->Stage->Run;
    const UCardRoomUISettings* UI=Owner->Stage->UISettings;
    if(R.Phase==ECardRoomPhase::Collected)return Layer;
    const FVector2D Size=G.GetLocalSize(); const float S=FMath::Min(Size.X/1600,Size.Y/900); const FVector2D Origin=(Size-FVector2D(1600,900)*S)*.5;
    const FSlateBrush* Brush=FCoreStyle::Get().GetBrush("WhiteBrush"); int32 L=Layer;
    auto Geo=[&](float X,float Y,float W,float H) { return G.ToPaintGeometry(FVector2f(W*S,H*S),FSlateLayoutTransform(FVector2f(Origin+FVector2D(X,Y)*S))); };
    auto Box=[&](float X,float Y,float W,float H,FLinearColor C) { FSlateDrawElement::MakeBox(Out,++L,G.ToPaintGeometry(FVector2f(W*S,H*S),FSlateLayoutTransform(FVector2f(Origin+FVector2D(X,Y)*S))),Brush,ESlateDrawEffect::None,C); };
    auto Text=[&](FString T,float X,float Y,int32 Font,FLinearColor C=FLinearColor::White) { FSlateDrawElement::MakeText(Out,++L,G.ToPaintGeometry(FVector2f(1500*S,70*S),FSlateLayoutTransform(FVector2f(Origin+FVector2D(X,Y)*S))),T,FCoreStyle::GetDefaultFontStyle("Regular",FMath::Max(9,FMath::RoundToInt(Font*S))),ESlateDrawEffect::None,C); };
    auto Button=[&](FSlateRect Rect,FString Label,bool Active=true) { Box(Rect.Left,Rect.Top,Rect.Right-Rect.Left,Rect.Bottom-Rect.Top,Active?(In(Mouse,Rect)?FLinearColor(1,.76f,.4f):Gold):FLinearColor(.23f,.25f,.24f)); Text(Label,Rect.Left+22,Rect.Top+10,20,Active?Ink:Cream); };
    auto UIBox=[&](const FCardRoomUIBox& B,const FSlateRect& Fallback)->FSlateRect { return UI&&B.Size.X>0.f&&B.Size.Y>0.f?FSlateRect(B.Position.X,B.Position.Y,B.Position.X+B.Size.X,B.Position.Y+B.Size.Y):Fallback; };
    const FSlateRect MatchRect=UI?UIBox(UI->SeatedPanel,FSlateRect(465,625,1135,710)):Match;
    const FSlateRect ConfirmRect=UI?UIBox(UI->Confirm,Confirm):Confirm;
    const FSlateRect AllRect=UI?UIBox(UI->SelectAll,All):All;
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
    auto DrawTextureObject=[&](UTexture2D* Texture,const TCHAR* Fallback,float X,float Y,float W,float H)
    {
        FSlateBrush Art=Texture?CardRoomUI::ArtBrush(Texture,FVector2D(W,H)):CardRoomUI::ArtBrush(Fallback,FVector2D(W,H));
        FSlateDrawElement::MakeBox(Out,++L,Geo(X,Y,W,H),&Art,ESlateDrawEffect::None,FLinearColor::White);
    };
    auto Card=[&](int32 Index,FSlateRect B,bool Floating=false)
    {
        const ERoomCard C=R.Hand[Index]; const bool Many=R.Hand.Num()>5&&!Floating; const bool Staged=R.Staged.Contains(Index)&&!Floating;
        const float W=B.Right-B.Left,H=B.Bottom-B.Top;
        UTexture2D* Texture=C==ERoomCard::Laser?(UI?UI->CardLaserArt.Get():nullptr):C==ERoomCard::Poison?(UI?UI->CardPoisonArt.Get():nullptr):(UI?UI->CardFireArt.Get():nullptr);
        const TCHAR* Path=C==ERoomCard::Laser?TEXT("/Game/Rooms/CardDuel/UI/T_Card_Laser.T_Card_Laser"):C==ERoomCard::Poison?TEXT("/Game/Rooms/CardDuel/UI/T_Card_Poison.T_Card_Poison"):TEXT("/Game/Rooms/CardDuel/UI/T_Card_Fire.T_Card_Fire");
        Box(B.Left+5,B.Top+7,W,H,FLinearColor(0,0,0,.48f));
        DrawTextureObject(Texture,Path,B.Left,B.Top,W,H);
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
        const FSlateRect B=UI?FSlateRect(UI->Round.Position.X,UI->Round.Position.Y,UI->Round.Position.X+UI->Round.Size.X,UI->Round.Position.Y+UI->Round.Size.Y):FSlateRect(48,103,370,283);
        DrawTextureObject(UI?UI->RoundArt.Get():nullptr,TEXT("/Game/Rooms/CardDuel/UI/T_RoundHeader.T_RoundHeader"),B.Left,B.Top,B.Right-B.Left,B.Bottom-B.Top);
        Text(FString::Printf(TEXT("第 %02d / 04 回合"),R.Round),B.Left+78,B.Top+29,25,Cream);
        Box(B.Left+78,B.Top+70,245,3,Gold);
        for(int32 i=0;i<4;++i) Box(B.Left+78+i*65,B.Top+86,54,4,i<R.Round?Gold:FLinearColor(.2f,.2f,.2f));
        auto Health=[&](bool Player,float HP,float X,float Y)
        {
            const TCHAR* Path=Player?TEXT("/Game/Rooms/CardDuel/UI/T_Health_Player.T_Health_Player"):TEXT("/Game/Rooms/CardDuel/UI/T_Health_Boss.T_Health_Boss");
            const float W=430.f,H=67.f;
            const float BX=UI?(Player?UI->PlayerHealth.Position.X:UI->BossHealth.Position.X):X;
            const float BY=UI?(Player?UI->PlayerHealth.Position.Y:UI->BossHealth.Position.Y):Y;
            const float BW=UI?(Player?UI->PlayerHealth.Size.X:UI->BossHealth.Size.X):W;
            const float BH=UI?(Player?UI->PlayerHealth.Size.Y:UI->BossHealth.Size.Y):H;
            DrawTextureObject(Player?(UI?UI->PlayerHealthArt.Get():nullptr):(UI?UI->BossHealthArt.Get():nullptr),Path,BX,BY,BW,BH);
            const float Fill=FMath::Clamp(HP/100.f,0.f,1.f);
            if(Fill<1.f) Box(BX+73.f+(BW-80.f)*Fill,BY+8,(BW-80.f)*(1.f-Fill),BH-16,FLinearColor(0.015f,.018f,.02f,.72f));
            Text(FString::Printf(TEXT("%d / 100"),FMath::RoundToInt(HP)),BX+BW-104,BY+24,16,FLinearColor::White);
        };
        Health(true,R.PlayerHP,46,205); Health(false,R.RivalHP,1120,205);
        if(R.bPoisoned) { const FSlateRect P=UI?FSlateRect(UI->Poison.Position.X,UI->Poison.Position.Y,UI->Poison.Position.X+UI->Poison.Size.X,UI->Poison.Position.Y+UI->Poison.Size.Y):FSlateRect(1355,220,1572,256); Box(P.Left,P.Top,P.Right-P.Left,P.Bottom-P.Top,FLinearColor(.1f,.23f,.08f,.9f)); Text(TEXT("中毒 · 可被点燃"),P.Left+15,P.Top+8,14,FLinearColor(.7f,1,.4f)); }
    }
    if(R.Phase==ECardRoomPhase::Seated || R.Phase==ECardRoomPhase::Matching || R.Phase==ECardRoomPhase::Arrival)
    {
        const FSlateRect B=UI?FSlateRect(UI->SeatedPanel.Position.X,UI->SeatedPanel.Position.Y,UI->SeatedPanel.Position.X+UI->SeatedPanel.Size.X,UI->SeatedPanel.Position.Y+UI->SeatedPanel.Size.Y):FSlateRect(465,625,1135,710);
        Box(B.Left,B.Top,B.Right-B.Left,B.Bottom-B.Top,Dark); Text(R.Message,B.Left+35,B.Top+13,22,Cream);
        Text(TEXT("今晚的规则很简单：把你的好牌，全都打出去。"),B.Left+35,B.Top+53,14,Gold);
        if(R.Phase==ECardRoomPhase::Seated) Button(MatchRect,TEXT("开始匹配     →"));
        else Text(R.Phase==ECardRoomPhase::Matching?TEXT("·   ·   ·"):TEXT("请注意头顶！"),B.Left+250,B.Bottom+35,24,Gold);
    }
    if(R.Phase==ECardRoomPhase::Choosing)
    {
        // Prompts live above the hand. The table stays unobstructed.
        const FSlateRect PromptBox=UI?FSlateRect(UI->Prompt.Position.X,UI->Prompt.Position.Y,UI->Prompt.Position.X+UI->Prompt.Size.X,UI->Prompt.Position.Y+UI->Prompt.Size.Y):FSlateRect(48,596,958,651);
        Box(PromptBox.Left,PromptBox.Top,PromptBox.Right-PromptBox.Left,PromptBox.Bottom-PromptBox.Top,Dark);
        FString Prompt=R.CanConfirm()?FString::Printf(TEXT("已选 %d 张 · 按空格，甩向桌心！"),R.Staged.Num()):R.Round==3?TEXT("点击选中「火」和「激光弹」，空格打出组合技"):R.Round==4?TEXT("最后一搏！选中 17 张牌，空格全部甩出"):TEXT("点击手牌选中，再按空格打出");
        Text(Prompt,PromptBox.Left+20,PromptBox.Top+13,19,Cream);
        Button(ConfirmRect,R.CanConfirm()?TEXT("空格   打出选中的牌"):FString::Printf(TEXT("已选 %d / %d 张"),R.Staged.Num(),R.Hand.Num()),R.CanConfirm());
        if(R.Round==4) Button(AllRect,R.CanConfirm()?TEXT("取消全选"):TEXT("全选 17 张"));
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
            const FSlateRect D=UI?FSlateRect(UI->Dialogue.Position.X,UI->Dialogue.Position.Y,UI->Dialogue.Position.X+UI->Dialogue.Size.X,UI->Dialogue.Position.Y+UI->Dialogue.Size.Y):FSlateRect(1018,283,1518,371);
            if(UI&&UI->SpeechBubbleArt)
                DrawTextureObject(UI->SpeechBubbleArt.Get(),TEXT("/Game/Rooms/CardDuel/UI/T_SpeechBubble.T_SpeechBubble"),D.Left,D.Top,D.Right-D.Left,D.Bottom-D.Top);
            else
            {
                Box(D.Left,D.Top,D.Right-D.Left,D.Bottom-D.Top,Ink); Box(D.Left+4,D.Top+4,D.Right-D.Left-8,D.Bottom-D.Top-8,Cream);
                Box(D.Left-13,D.Top+45,20,14,Cream); Box(D.Left-22,D.Top+51,13,7,Cream);
            }
            Text(TEXT("超级人机"),D.Left+44,D.Top+14,12,Ink); Text(Speech,D.Left+44,D.Top+42,18,Ink);
        }
    }

    if(R.Phase==ECardRoomPhase::Resolving)
    {
        const FSlateRect B=UI?UIBox(UI->Prompt,FSlateRect(335,711,1265,825)):FSlateRect(335,711,1265,825);
        Box(B.Left,B.Top,B.Right-B.Left,B.Bottom-B.Top,Dark); Text(R.Message,B.Left+37,B.Top+24,23,Cream);
        Text(TEXT("牌已打出，看看会发生什么……"),B.Left+250,B.Bottom-32,15,Gold);
        if(R.Round==3 && R.PhaseTime>2.15f && R.PhaseTime<2.7f) Box(0,0,1600,900,FLinearColor(.7f,.025f,.008f,.16f));
    }
    if(R.Phase==ECardRoomPhase::Victory || R.Phase==ECardRoomPhase::Collected)
    {
        const FSlateRect B=UI?UIBox(UI->Victory,FSlateRect(48,610,918,770)):FSlateRect(48,610,918,770);
        Box(B.Left,B.Top,B.Right-B.Left,B.Bottom-B.Top,Dark); Box(B.Left,B.Top,B.Right-B.Left,3,Gold);
        Text(R.Phase==ECardRoomPhase::Victory?TEXT("恭喜你获得胜利！"):TEXT("彗尾法球 · 已收藏"),B.Left+34,B.Top+24,32,Cream);
        Text(R.Phase==ECardRoomPhase::Victory?TEXT("超级人机已退场。属于你的星光，留在桌上。"):TEXT("本版奖励为房间内占位，不改变挖掘数值。"),B.Left+34,B.Top+93,18,Gold);
        if(R.Phase==ECardRoomPhase::Victory) Button(Collect,TEXT("E   收集桌上的彗尾法球"));
    }
    const FSlateRect Hint=UI?UIBox(UI->BottomHint,FSlateRect(48,850,430,890)):FSlateRect(48,850,430,890);
    Text(TEXT("点击选牌 / 再点取消     空格出牌"),Hint.Left,Hint.Top,12,Cream);
    return L;
}
FReply SCardRoomView::OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
    using namespace CardRoomUI;
    if(!Owner.IsValid()||!Owner->Stage||E.GetEffectingButton()!=EKeys::LeftMouseButton) return FReply::Unhandled();
    Mouse=ToVirtual(G,E.GetScreenSpacePosition()); auto& R=Owner->Stage->Run;
    const UCardRoomUISettings* UI=Owner->Stage->UISettings;
    auto UIBox=[&](const FCardRoomUIBox& B,const FSlateRect& Fallback)->FSlateRect { return UI&&B.Size.X>0.f&&B.Size.Y>0.f?FSlateRect(B.Position.X,B.Position.Y,B.Position.X+B.Size.X,B.Position.Y+B.Size.Y):Fallback; };
    const FSlateRect MatchRect=UI?UIBox(UI->SeatedPanel,FSlateRect(640,740,960,800)):FSlateRect(640,740,960,800);
    const FSlateRect ConfirmRect=UI?UIBox(UI->Confirm,Confirm):Confirm;
    const FSlateRect AllRect=UI?UIBox(UI->SelectAll,All):All;
    if(R.Phase==ECardRoomPhase::Exploring) return FReply::Unhandled();
    if(R.Phase==ECardRoomPhase::Seated&&In(Mouse,MatchRect)) R.Match();
    if(R.Phase==ECardRoomPhase::Victory&&In(Mouse,Collect)) { Owner->Interact();return FReply::Handled(); }
    if(R.Phase==ECardRoomPhase::Choosing)
    {
        if(In(Mouse,ConfirmRect)) Owner->ConfirmCards();
        else if(R.Round==4&&In(Mouse,AllRect))
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
