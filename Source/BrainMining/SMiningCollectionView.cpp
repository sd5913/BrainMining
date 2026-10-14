#include "SMiningCollectionView.h"
#include "MiningCollection.h"
#include "MiningInteractionHUDWidget.h"
#include "Engine/World.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Fonts/FontMeasure.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

void SMiningCollectionView::Construct(const FArguments& Args)
{
 Owner=Args._Owner;PreviewBrush.DrawAs=ESlateBrushDrawType::Image;PreviewBrush.ImageSize=FVector2D(1536,1024);
 TSharedPtr<SConstraintCanvas> Controls;
 ChildSlot[SNew(SOverlay)
  +SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(10.f).Visibility_Lambda([this]{return Owner.IsValid()&&Owner->bCatalogueOpen?EVisibility::HitTestInvisible:EVisibility::Collapsed;})]
  +SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit).Visibility_Lambda([this]{return Owner.IsValid()&&Owner->bCatalogueOpen?EVisibility::Visible:EVisibility::Collapsed;})
   [SNew(SBox).WidthOverride(1600).HeightOverride(900)[SAssignNew(Controls,SConstraintCanvas)]]]];
 Controls->AddSlot().Offset(FMargin(40,90,1030,620)).Anchors(FAnchors(0,0)).Alignment(FVector2D::ZeroVector)
 [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
  .OnMouseButtonDown_Lambda([this](const FGeometry&,const FPointerEvent& E){return OnMouseButtonDown(GetCachedGeometry(),E);})
  .OnMouseButtonUp_Lambda([this](const FGeometry&,const FPointerEvent& E){return OnMouseButtonUp(GetCachedGeometry(),E);})
  [SNew(STextBlock).Text(FText::FromString(TEXT("3D物品旋转检视"))).RenderOpacity(0.f)]];
 auto Button=[&](const FString& Label,float X,float Y,float W,float H,int32 Action)
 {
  Controls->AddSlot().Offset(FMargin(X,Y,W,H)).Anchors(FAnchors(0,0)).Alignment(FVector2D::ZeroVector)
  [SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).IsFocusable(false).RenderOpacity(0.f)
   .OnHovered_Lambda([this,X,Y,W,H]{HoverMouse=FVector2D(X+W*.5f,Y+H*.5f);Invalidate(EInvalidateWidgetReason::Paint);})
   .OnUnhovered_Lambda([this]{HoverMouse=FVector2D(-1,-1);Invalidate(EInvalidateWidgetReason::Paint);})
   .OnClicked_Lambda([this,Action]{if(Owner.IsValid()){if(Action<4)Owner->SelectItem(Action);else if(Action==4)TurnPage(-1);else if(Action==5)TurnPage(1);else Owner->ToggleCatalogue();}return FReply::Handled();})
   [SNew(STextBlock).Text(FText::FromString(Label))]];
 };
 for(int i=0;i<4;++i)Button(FString::Printf(TEXT("收藏槽 %d"),i+1),101+i*228,724,222,98,i);
 Button(TEXT("上一页"),45,742,50,57,4);Button(TEXT("下一页"),1015,742,50,57,5);Button(TEXT("返回游戏"),1508,25,72,75,6);
}
FVector2D SMiningCollectionView::ToVirtual(const FGeometry& G,const FVector2D& P) const
{
    const FVector2D Size=G.GetLocalSize(); const float Scale=FMath::Min(Size.X/1600,Size.Y/900);
    return (G.AbsoluteToLocal(P)-(Size-FVector2D(1600,900)*Scale)*.5)/Scale;
}
int32 SMiningCollectionView::OnPaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const
{
    if(!Owner.IsValid()) return Layer;
    const auto* F=Owner.Get();
    const bool HeavenRoom=F->bUseAuthoredPlacements && F->AuthoredPlacements.Num()==1 && F->AuthoredPlacements[0].ItemId==0;
    const int32 Total=HeavenRoom?1:4;
    if (!F->bCatalogueOpen && !F->bShowExplorationHUD) return Layer;
    const FVector2D Size=G.GetLocalSize(); const float S=FMath::Min(Size.X/1600,Size.Y/900); const FVector2D O=(Size-FVector2D(1600,900)*S)*.5;
    const FLinearColor Ink(.018f,.034f,.038f,1),Panel(.035f,.057f,.061f,1),Cream(.89f,.91f,.81f,1),Muted(.47f,.58f,.54f,1),Gold(.93f,.62f,.23f,1),Mint(.24f,.72f,.57f,1);
    const auto* Brush=FCoreStyle::Get().GetBrush("WhiteBrush"); int L=F->bCatalogueOpen?SCompoundWidget::OnPaint(Args,G,Clip,Out,Layer,Style,Enabled):Layer;
    auto Geo=[&](float X,float Y,float W,float H) { return G.ToPaintGeometry(FVector2f(W*S,H*S),FSlateLayoutTransform(FVector2f(O+FVector2D(X,Y)*S))); };
    auto Box=[&](float X,float Y,float W,float H,FLinearColor C) { FSlateDrawElement::MakeBox(Out,++L,Geo(X,Y,W,H),Brush,ESlateDrawEffect::None,C); };
    auto Text=[&](FString T,float X,float Y,int Font,FLinearColor C) { FSlateDrawElement::MakeText(Out,++L,Geo(X,Y,1500,90),T,FCoreStyle::GetDefaultFontStyle("Regular",FMath::Max(8,FMath::RoundToInt(Font*S))),ESlateDrawEffect::None,C); };
    if(!F->bCatalogueOpen)
    {
        Box(40,35,395,104,Ink); Box(40,35,4,104,Gold);
        Text(HeavenRoom?TEXT("天堂马桶  /  穹顶秘室"):TEXT("地下奇物  /  收集试验场"),63,52,23,Cream); Text(HeavenRoom?TEXT("HEAVEN    /    FOLLOW THE PIPE"):F->bDeepField?FString(TEXT("DEEP COLLECTION     ·     "))+F->DisplayVersion:TEXT("COLLECTION FIELD     ·     V0.1"),64,91,12,Gold);
        Box(1280,36,280,83,Ink); Text(FString::Printf(TEXT("图鉴  %02d / %02d"),F->UniqueCount(),Total),1304,50,23,Cream); Text(TEXT("[ Tab ]  查看我的收藏"),1304,86,13,Mint);
        Box(50,762,940,88,Ink);
        Text(TEXT("左键 挖掘    E 收集    Tab 图鉴    按住空格 上升 / 松开 下落"),70,775,18,Cream);
        Text(HeavenRoom?TEXT("WASD 移动    沿外露水管挖穿土层，前往半球大厅中央"):TEXT("WASD 移动    R 新一轮（清空收集）    高处鸟巢可用飞行接近"),70,814,14,Muted);
        const bool SharedHUD = UMiningInteractionHUDWidget::IsActiveFor(F->GetWorld()->GetFirstPlayerController());
        if (!SharedHUD) Box(798,448,4,4,F->Target?Gold:Cream);
        if(F->Target && !SharedHUD)
        {
            Box(570,568,460,58,Ink); Text(TEXT("[ E ]  收集  ")+AMiningCollectible::ItemName(F->Target->ItemId),600,580,23,Gold);
        }
        if(F->ToastRemaining>0) { Box(320,665,960,61,Panel); Text(F->Toast,343,681,18,Cream); }
        if(F->UniqueCount()==4) { Text(TEXT("四种奇物已集齐 · 打开图鉴，看看它们的另一面"),470,169,22,Gold); }
        if(HeavenRoom && F->UniqueCount()==1) { Text(TEXT("已找到天堂马桶 · Tab 打开图鉴检视"),525,169,22,Gold); }
        return L;
    }
    // Native vector edges remain sharp at every viewport resolution.
    const auto Handle=FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Brush);
    auto Poly=[&](TArray<FVector2D> Points,FLinearColor Color)
    {
        TArray<FSlateVertex> Vertices;TArray<SlateIndex> Indices;
        for(auto P:Points)Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),FVector2f(O+P*S),FVector2f(.5f,.5f),Color.ToFColor(true)));
        if(Points.Num()==6){for(int i=0;i<4;++i){Indices.Add(5);Indices.Add(i);Indices.Add(i+1);}}
        else for(int i=1;i<Points.Num()-1;++i){Indices.Add(0);Indices.Add(i);Indices.Add(i+1);}
        FSlateDrawElement::MakeCustomVerts(Out,++L,Handle,Vertices,Indices,nullptr,0,0);
    };
    auto CenterText=[&](const FString& T,float X,float Y,float W,int Font,FLinearColor C)
    {
        auto Info=FCoreStyle::GetDefaultFontStyle("Bold",FMath::Max(8,FMath::RoundToInt(Font*S)));
        const float Width=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(T,Info).X/S;
        FSlateDrawElement::MakeText(Out,++L,Geo(X+(W-Width)*.5f,Y,W,70),T,Info,ESlateDrawEffect::None,C);
    };
    Poly({{0,0},{1232,0},{1070,900},{0,900}},FLinearColor(.6f,.6f,.6f,.48f));
    Poly({{1232,0},{1600,0},{1600,900},{1070,900}},FLinearColor(0,0,0,.94f));
    const bool Owned=F->Inventory.IsValidIndex(F->SelectedId)&&F->Inventory[F->SelectedId]>0;
    if(Owned&&F->PreviewTexture&&F->PreviewMaterial)
    {
        auto* Self=const_cast<SMiningCollectionView*>(this); Self->PreviewBrush.SetResourceObject(F->PreviewMaterial);
        FSlateDrawElement::MakeBox(Out,++L,Geo(40,80,1030,650),&PreviewBrush,ESlateDrawEffect::None,FLinearColor::White);
    }
    Text(TEXT("✦ 奇物收藏 ✦"),40,28,28,Cream);
    const bool BackHover=HoverMouse.X>1508 && HoverMouse.Y<100;
    const FLinearColor Accent(.56f,1.f,.29f,1),White(.95f,.97f,.93f,1);
    const auto BackColor=BackHover?Accent:FLinearColor(.65f,.65f,.65f,1);
    Poly({{1524,52},{1540,39},{1540,65}},BackColor);
    FSlateDrawElement::MakeCubicBezierSpline(Out,++L,G.ToPaintGeometry(),FVector2f(O+FVector2D(1538,52)*S),FVector2f(O+FVector2D(1553,49)*S),FVector2f(O+FVector2D(1565,55)*S),FVector2f(O+FVector2D(1560,63)*S),8*S,ESlateDrawEffect::None,BackColor);
    Box(1525,75,35,16,BackHover?FLinearColor(.27f,.4f,.2f):FLinearColor(.22f,.22f,.22f));
    CenterText(F->CatalogueKey.GetFName().ToString().ToUpper(),1525,74,35,12,White);
    if(Owned)
    {
        CenterText(AMiningCollectible::ItemName(F->SelectedId),1244,373,326,32,White);
        Box(1260,438,118,1,White);
        TArray<FString> Lines; AMiningCollectible::Description(F->SelectedId).ParseIntoArrayLines(Lines);
        for(int i=0;i<Lines.Num();++i) Text(Lines[i],1260,466+i*31,17,FLinearColor(.62f,.69f,.66f));
    }
    else
    {
        CenterText(TEXT("尚未发现"),1244,373,326,32,White);
        Box(1260,438,118,1,White);Text(TEXT("收集后即可旋转检视"),1260,466,17,Muted);
    }
    if(Owned)Text(TEXT("拖动旋转   滚轮缩放   双击复位"),340,698,13,FLinearColor(.2f,.22f,.2f));
    for(int i=0;i<4;++i)
    {
        const float X=113+i*228;const bool Has=F->Inventory.IsValidIndex(i)&&F->Inventory[i]>0,Selected=F->SelectedId==i,Hover=SlotAt(HoverMouse)==i;
        auto C=Has?FLinearColor(.58f,1.f,.31f):FLinearColor(.055f,.05f,.05f,.9f);
        if(Hover)C=Has?FLinearColor(.75f,1.f,.49f):FLinearColor(.19f,.18f,.18f,.95f);
        if(Selected || Hover)Poly({{X+1,722},{X+213,722},{X+198,824},{X-14,824}},White);
        Poly({{X+3,724},{X+211,724},{X+196,822},{X-12,822}},C);
        CenterText(Has?AMiningCollectible::ItemName(i):TEXT("?"),X-6,Has?755:735,205,Has?23:48,Has?FLinearColor(.005f,.01f,.005f):FLinearColor(.69f,.75f,.79f));
    }
    const bool LeftHover=HoverMouse.X>=45&&HoverMouse.X<=95&&HoverMouse.Y>=742&&HoverMouse.Y<=799,RightHover=HoverMouse.X>=1015&&HoverMouse.X<=1065&&HoverMouse.Y>=742&&HoverMouse.Y<=799;
    Poly({{89,753},{72,753},{58,769},{72,785},{89,785},{75,769}},LeftHover?White:FLinearColor(.3f,.31f,.31f));
    Poly({{1024,753},{1041,753},{1055,769},{1041,785},{1024,785},{1038,769}},RightHover?White:FLinearColor(.3f,.31f,.31f));
    return L;
}
int32 SMiningCollectionView::SlotAt(FVector2D P) const
{
 if(P.Y<724 || P.Y>822)return INDEX_NONE;
 for(int i=0;i<4;++i){const float X=113+i*228-(P.Y-724)*15.f/98.f;if(P.X>=X && P.X<=X+208)return i;}
 return INDEX_NONE;
}
void SMiningCollectionView::TurnPage(int32 Direction)
{
 if(Owner.IsValid()){const int32 Current=FMath::Max(0,Owner->SelectedId);Owner->SelectItem((Current+Direction+4)%4);}
}
FReply SMiningCollectionView::OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
    if(!Owner.IsValid()||!Owner->bCatalogueOpen) return FReply::Unhandled();
    FVector2D P=ToVirtual(G,E.GetScreenSpacePosition());
    if(E.GetEffectingButton()!=EKeys::LeftMouseButton) return FReply::Handled();
    if(P.X>=1508&&P.X<=1580&&P.Y>=25&&P.Y<=100) { Owner->ToggleCatalogue(); return FReply::Handled(); }
    const int32 Slot=SlotAt(P);if(Slot!=INDEX_NONE){Owner->SelectItem(Slot);return FReply::Handled();}
    if(P.Y>=742&&P.Y<=799){if(P.X>=45&&P.X<=95){TurnPage(-1);return FReply::Handled();}if(P.X>=1015&&P.X<=1065){TurnPage(1);return FReply::Handled();}}
    if(P.X>=40&&P.X<=1070&&P.Y>=90&&P.Y<=710) { bDragging=true; LastMouse=P; return FReply::Handled().CaptureMouse(AsShared()); }
    return FReply::Handled();
}
FReply SMiningCollectionView::OnMouseButtonUp(const FGeometry&,const FPointerEvent& E)
{
    if(E.GetEffectingButton()==EKeys::LeftMouseButton) { bDragging=false; return FReply::Handled().ReleaseMouseCapture(); }
    return FReply::Handled();
}
FReply SMiningCollectionView::OnMouseButtonDoubleClick(const FGeometry& G,const FPointerEvent& E)
{
    const auto P=ToVirtual(G,E.GetScreenSpacePosition());
    if(Owner.IsValid()&&P.X>=40&&P.X<=1070&&P.Y>=90&&P.Y<=710) Owner->ResetPreview();
    return FReply::Handled();
}
FReply SMiningCollectionView::OnMouseMove(const FGeometry& G,const FPointerEvent& E)
{
    const auto P=ToVirtual(G,E.GetScreenSpacePosition());
    HoverMouse=P;Invalidate(EInvalidateWidgetReason::Paint);
    if(bDragging&&Owner.IsValid()) { const auto D=P-LastMouse; Owner->RotatePreview(D.X*.45f,D.Y*.4f); LastMouse=P; }
    return FReply::Handled();
}
FReply SMiningCollectionView::OnMouseWheel(const FGeometry& G,const FPointerEvent& E)
{
    const auto P=ToVirtual(G,E.GetScreenSpacePosition());
    if(Owner.IsValid()&&P.X>=40&&P.X<=1070&&P.Y>=90&&P.Y<=710) Owner->ZoomPreview(E.GetWheelDelta());
    return FReply::Handled();
}
FReply SMiningCollectionView::OnKeyDown(const FGeometry&,const FKeyEvent& E)
{
    if(!Owner.IsValid()) return FReply::Unhandled();
    if(E.GetKey()==Owner->CatalogueKey||E.GetKey()==EKeys::Escape) { bDragging=false; Owner->ToggleCatalogue(); return FReply::Handled().ReleaseMouseCapture(); }
    if(E.GetKey()==EKeys::F) Owner->ResetPreview();
    if(E.GetKey()==EKeys::Left)TurnPage(-1);
    if(E.GetKey()==EKeys::Right)TurnPage(1);
    return FReply::Handled();
}
void SMiningCollectionView::OnMouseLeave(const FPointerEvent& E)
{
 HoverMouse=FVector2D(-1,-1);Invalidate(EInvalidateWidgetReason::Paint);SCompoundWidget::OnMouseLeave(E);
}
