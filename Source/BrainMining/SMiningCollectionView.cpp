#include "SMiningCollectionView.h"
#include "BrainMiningFonts.h"
#include "MiningCollection.h"
#include "MiningInteractionHUDWidget.h"
#include "Engine/World.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"

void SMiningCollectionView::Construct(const FArguments& Args) { Owner=Args._Owner; PreviewBrush.DrawAs=ESlateBrushDrawType::Image; PreviewBrush.ImageSize=FVector2D(1536,1024); }
FVector2D SMiningCollectionView::ToVirtual(const FGeometry& G,const FVector2D& P) const
{
    const FVector2D Size=G.GetLocalSize(); const float Scale=FMath::Min(Size.X/1600,Size.Y/900);
    return (G.AbsoluteToLocal(P)-(Size-FVector2D(1600,900)*Scale)*.5)/Scale;
}
int32 SMiningCollectionView::OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const
{
    if(!Owner.IsValid()) return Layer;
    const auto* F=Owner.Get();
    const bool HeavenRoom=F->bUseAuthoredPlacements && F->AuthoredPlacements.Num()==1 && F->AuthoredPlacements[0].ItemId==0;
    const TArray<int32> Ids=F->CatalogueIds();
    const int32 Total=Ids.Num();
    if (!F->bCatalogueOpen && !F->bShowExplorationHUD) return Layer;
    const FVector2D Size=G.GetLocalSize(); const float S=FMath::Min(Size.X/1600,Size.Y/900); const FVector2D O=(Size-FVector2D(1600,900)*S)*.5;
    const FLinearColor Ink(.018f,.034f,.038f,1),Panel(.035f,.057f,.061f,1),Cream(.89f,.91f,.81f,1),Muted(.47f,.58f,.54f,1),Gold(.93f,.62f,.23f,1),Mint(.24f,.72f,.57f,1);
    const auto* Brush=FCoreStyle::Get().GetBrush("WhiteBrush"); int L=Layer;
    auto Geo=[&](float X,float Y,float W,float H) { return G.ToPaintGeometry(FVector2f(W*S,H*S),FSlateLayoutTransform(FVector2f(O+FVector2D(X,Y)*S))); };
    auto Box=[&](float X,float Y,float W,float H,FLinearColor C) { FSlateDrawElement::MakeBox(Out,++L,Geo(X,Y,W,H),Brush,ESlateDrawEffect::None,C); };
    auto Text=[&](FString T,float X,float Y,int Font,FLinearColor C,bool bTitle=false,bool bItalic=false) { auto Info=BrainMiningFonts::Style(bTitle ? TEXT("Bold") : TEXT("Medium"),FMath::Max(8,FMath::RoundToInt(Font*S)));Info.SkewAmount=bItalic?.18f:0.f;FSlateDrawElement::MakeText(Out,++L,Geo(X,Y,1500,90),T,Info,ESlateDrawEffect::None,C); };
    if(!F->bCatalogueOpen)
    {
        Box(40,35,395,104,Ink); Box(40,35,4,104,Gold);
        Text(HeavenRoom?TEXT("天堂马桶  /  穹顶秘室"):TEXT("地下奇物  /  收集试验场"),63,52,23,Cream,true); Text(HeavenRoom?TEXT("HEAVEN    /    FOLLOW THE PIPE"):F->bDeepField?FString(TEXT("DEEP COLLECTION     ·     "))+F->DisplayVersion:TEXT("COLLECTION FIELD     ·     V0.1"),64,91,12,Gold);
        Box(1280,36,280,83,Ink); Text(FString::Printf(TEXT("图鉴  %02d / %02d"),F->UniqueCount(),Total),1304,50,23,Cream); Text(TEXT("[ Tab ]  查看我的收藏"),1304,86,13,Mint);
        Box(50,762,940,88,Ink);
        Text(TEXT("左键 挖掘    E 收集    Tab 图鉴    按住空格 上升 / 松开 下落"),70,775,18,Cream);
        Text(HeavenRoom?TEXT("WASD 移动    沿外露水管挖穿土层，前往半球大厅中央"):F->bExpandedCollection?TEXT("WASD 移动    R 重置    右侧通道：新藏品与红幕布舞台"):TEXT("WASD 移动    R 新一轮（清空收集）    高处鸟巢可用飞行接近"),70,814,14,Muted);
        const bool SharedHUD = UMiningInteractionHUDWidget::IsActiveFor(F->GetWorld()->GetFirstPlayerController());
        if (!SharedHUD) Box(798,448,4,4,F->Target?Gold:Cream);
        if(F->Target && !SharedHUD)
        {
            Box(570,568,460,58,Ink); Text(TEXT("[ E ]  收集  ")+AMiningCollectible::ItemName(F->Target->ItemId),600,580,23,Gold);
        }
        if(F->ToastRemaining>0) { Box(320,665,960,61,Panel); Text(F->Toast.ToString(),343,681,18,Cream); }
        if(!HeavenRoom && Total>0 && F->UniqueCount()==Total) { Text(TEXT("本区奇物已集齐 · 打开图鉴，看看它们的另一面"),470,169,22,Gold); }
        if(HeavenRoom && F->UniqueCount()==1) { Text(TEXT("已找到")+AMiningCollectible::ItemName(0)+TEXT(" · Tab 打开图鉴检视"),525,169,22,Gold); }
        return L;
    }
    FSlateDrawElement::MakeBox(Out,++L,G.ToPaintGeometry(),Brush,ESlateDrawEffect::None,FLinearColor::Black);
    const bool Owned=F->Inventory.IsValidIndex(F->SelectedId)&&F->Inventory[F->SelectedId]>0;
    if(Owned&&F->PreviewTexture)
    {
        auto* Self=const_cast<SMiningCollectionView*>(this); Self->PreviewBrush.SetResourceObject(F->PreviewTexture);
        // SceneColorHDR stores inverse opacity in alpha; this preview has an opaque black backdrop.
        FSlateDrawElement::MakeBox(Out,++L,Geo(0,8,1155,770),&PreviewBrush,ESlateDrawEffect::IgnoreTextureAlpha,FLinearColor::White);
    }
    Text(TEXT("奇物收藏"),60,37,24,Cream,true);
    Text(FString::Printf(TEXT("%02d / %02d"),F->UniqueCount(),Total),230,48,12,Muted);
    Box(1408,35,132,44,Panel); Text(FString::Printf(TEXT("返回  %s"),*F->CatalogueKey.GetDisplayName().ToString()),1426,47,14,Cream);
    Box(1178,140,1,585,FLinearColor(.12f,.17f,.16f));
    if(Owned || AMiningCollectible::IsMineralReward(F->SelectedId))
    {
        Text(AMiningCollectible::ItemName(F->SelectedId),1220,280,32,Cream,true);
        Box(1223,344,46,2,Gold);
        // Authoring stores the exact description. Wrap its presentation to the
        // info column so longer Chinese sentences do not run off the screen.
        TArray<FString> Lines; FString Current;
        const auto Font=BrainMiningFonts::Body(FMath::Max(8,FMath::RoundToInt(15*S)));
        const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
        for(const TCHAR Character:(Owned?AMiningCollectible::Description(F->SelectedId):F->UnlockCondition(F->SelectedId)))
        {
            if(Character==TEXT('\n')) { Lines.Add(Current); Current.Empty(); continue; }
            const FString Next=Current+FString::Chr(Character);
            if(!Current.IsEmpty() && Measure->Measure(Next,Font).X>330*S) { Lines.Add(Current); Current.Empty(); }
            Current.AppendChar(Character);
        }
        if(!Current.IsEmpty())Lines.Add(Current);
        bool bQuote=false;
        for(int i=0;i<Lines.Num();++i)
        {
            if(Lines[i].IsEmpty())bQuote=true;
            Text(Lines[i],1223,379+i*25,15,Muted,false,bQuote);
        }
    }
    else
    {
        Text(TEXT("尚未发现"),1220,280,32,Muted,true);
        Text(TEXT("收藏后即可检视"),1223,380,16,Muted);
    }
    const float MaxScroll=FMath::Max(0.f,Total*267.f-25.f-1480.f);
    Out.PushClip(FSlateClippingZone(G.MakeChild(FVector2D(1480*S,90*S),FSlateLayoutTransform(FVector2f(O+FVector2D(60,780)*S)))));
    for(int Slot=0;Slot<Total;++Slot)
    {
        const int i=Ids[Slot];const float X=60+Slot*267-ListScroll;
        if(X+242<60 || X>1540)continue;
        const bool Has=F->Inventory.IsValidIndex(i)&&F->Inventory[i]>0,Selected=F->SelectedId==i;
        Box(X,790,242,75,Has?FLinearColor(.07f,.17f,.13f):Panel);
        Box(X,790,4,75,Has?Mint:Muted);
        if(Selected)Box(X,790,242,3,Gold);
        Text(FString::Printf(TEXT("%02d"),i+1),X+17,811,15,Has?Gold:Muted);
        Text(Has?AMiningCollectible::ItemName(i):TEXT("未发现"),X+55,800,19,Has?Cream:Muted);
        Text(Has?TEXT("已收集"):TEXT("等待探索"),X+55,833,12,Has?Mint:Muted);
    }
    Out.PopClip();
    Box(60,877,1480,3,Panel);
    const float Thumb=1480*FMath::Min(1.f,1480.f/FMath::Max(1.f,Total*267.f-25));
    Box(60+(MaxScroll>0?ListScroll/MaxScroll*(1480-Thumb):0),876,Thumb,5,Muted);
    Text(TEXT("拖动藏品列表或使用滚轮浏览"),60,755,12,Muted);
    Box(1220,661,252,44,Panel); Text(TEXT("F   恢复展示角度"),1240,672,15,Muted);
    return L;
}
FReply SMiningCollectionView::OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
    if(!Owner.IsValid()||!Owner->bCatalogueOpen) return FReply::Unhandled();
    const auto Ids=Owner->CatalogueIds();
    const int32 Total=Ids.Num();
    FVector2D P=ToVirtual(G,E.GetScreenSpacePosition());
    if(E.GetEffectingButton()!=EKeys::LeftMouseButton) return FReply::Handled();
    if(P.X>=1408&&P.X<=1540&&P.Y>=35&&P.Y<=79) { Owner->ToggleCatalogue(); return FReply::Handled(); }
    if(P.X>=60 && P.X<=1540 && P.Y>=790 && P.Y<=885)
    {
        bListDragging=true;ListPress=P;ListDragStart=ListScroll;
        return FReply::Handled().CaptureMouse(AsShared());
    }
    if(P.X>=1220&&P.X<=1472&&P.Y>=661&&P.Y<=705) { Owner->ResetPreview(); return FReply::Handled(); }
    if(P.X>=30&&P.X<=1155&&P.Y>=90&&P.Y<=770) { bDragging=true; LastMouse=P; return FReply::Handled().CaptureMouse(AsShared()); }
    return FReply::Handled();
}
FReply SMiningCollectionView::OnMouseButtonUp(const FGeometry& G,const FPointerEvent& E)
{
    if(E.GetEffectingButton()==EKeys::LeftMouseButton)
    {
        const auto P=ToVirtual(G,E.GetScreenSpacePosition());
        if(bListDragging&&Owner.IsValid()&&FVector2D::Distance(P,ListPress)<6)
        {
            const auto Ids=Owner->CatalogueIds();const int Index=FMath::FloorToInt((P.X-60+ListScroll)/267);
            if(Ids.IsValidIndex(Index)&&P.Y>=790&&P.Y<=865&&FMath::Fmod(float(P.X-60+ListScroll),267.f)<242)Owner->SelectItem(Ids[Index]);
        }
        bDragging=bListDragging=false;return FReply::Handled().ReleaseMouseCapture();
    }
    return FReply::Handled();
}
FReply SMiningCollectionView::OnMouseButtonDoubleClick(const FGeometry& G,const FPointerEvent& E)
{
    const auto P=ToVirtual(G,E.GetScreenSpacePosition());
    if(Owner.IsValid()&&P.X>=30&&P.X<=1155&&P.Y>=90&&P.Y<=770) Owner->ResetPreview();
    return FReply::Handled();
}
FReply SMiningCollectionView::OnMouseMove(const FGeometry& G,const FPointerEvent& E)
{
    const auto P=ToVirtual(G,E.GetScreenSpacePosition());
    if(bListDragging&&Owner.IsValid())ListScroll=FMath::Clamp(ListDragStart+float(ListPress.X-P.X),0.f,FMath::Max(0.f,Owner->CatalogueIds().Num()*267.f-25-1480));
    if(bDragging&&Owner.IsValid()) { const auto D=P-LastMouse; Owner->RotatePreview(D.X*.45f,D.Y*.4f); LastMouse=P; }
    return FReply::Handled();
}
FReply SMiningCollectionView::OnMouseWheel(const FGeometry& G,const FPointerEvent& E)
{
    const auto P=ToVirtual(G,E.GetScreenSpacePosition());
    if(Owner.IsValid()&&P.Y>=780&&P.Y<=890)ListScroll=FMath::Clamp(ListScroll-E.GetWheelDelta()*150,0.f,FMath::Max(0.f,Owner->CatalogueIds().Num()*267.f-25-1480));
    else if(Owner.IsValid()&&P.X>=30&&P.X<=1155&&P.Y>=90&&P.Y<=770) Owner->ZoomPreview(E.GetWheelDelta());
    return FReply::Handled();
}
FReply SMiningCollectionView::OnKeyDown(const FGeometry&,const FKeyEvent& E)
{
    if(!Owner.IsValid()) return FReply::Unhandled();
    if(E.GetKey()==Owner->CatalogueKey||E.GetKey()==EKeys::Escape) { bDragging=false; Owner->ToggleCatalogue(); return FReply::Handled().ReleaseMouseCapture(); }
    if(E.GetKey()==EKeys::F) Owner->ResetPreview();
    return FReply::Handled();
}
