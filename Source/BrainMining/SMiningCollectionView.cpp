#include "SMiningCollectionView.h"
#include "MiningCollection.h"
#include "MiningInteractionHUDWidget.h"
#include "Engine/World.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Engine/TextureRenderTarget2D.h"

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
    if (!F->bCatalogueOpen && !F->bShowExplorationHUD) return Layer;
    const FVector2D Size=G.GetLocalSize(); const float S=FMath::Min(Size.X/1600,Size.Y/900); const FVector2D O=(Size-FVector2D(1600,900)*S)*.5;
    const FLinearColor Ink(.018f,.034f,.038f,1),Panel(.035f,.057f,.061f,1),Cream(.89f,.91f,.81f,1),Muted(.47f,.58f,.54f,1),Gold(.93f,.62f,.23f,1),Mint(.24f,.72f,.57f,1);
    const auto* Brush=FCoreStyle::Get().GetBrush("WhiteBrush"); int L=Layer;
    auto Geo=[&](float X,float Y,float W,float H) { return G.ToPaintGeometry(FVector2f(W*S,H*S),FSlateLayoutTransform(FVector2f(O+FVector2D(X,Y)*S))); };
    auto Box=[&](float X,float Y,float W,float H,FLinearColor C) { FSlateDrawElement::MakeBox(Out,++L,Geo(X,Y,W,H),Brush,ESlateDrawEffect::None,C); };
    auto Text=[&](FString T,float X,float Y,int Font,FLinearColor C) { FSlateDrawElement::MakeText(Out,++L,Geo(X,Y,1500,90),T,FCoreStyle::GetDefaultFontStyle("Regular",FMath::Max(8,FMath::RoundToInt(Font*S))),ESlateDrawEffect::None,C); };
    if(!F->bCatalogueOpen)
    {
        Box(40,35,395,104,Ink); Box(40,35,4,104,Gold);
        Text(TEXT("地下奇物  /  收集试验场"),63,52,23,Cream); Text(F->bDeepField?FString(TEXT("DEEP COLLECTION     ·     "))+F->DisplayVersion:TEXT("COLLECTION FIELD     ·     V0.1"),64,91,12,Gold);
        Box(1280,36,280,83,Ink); Text(FString::Printf(TEXT("图鉴  %02d / 04"),F->UniqueCount()),1304,50,23,Cream); Text(TEXT("[ Tab ]  查看我的收藏"),1304,86,13,Mint);
        Box(50,762,940,88,Ink);
        Text(TEXT("左键 挖掘    E 收集    Tab 图鉴    按住空格 上升 / 松开 下落"),70,775,18,Cream);
        Text(TEXT("WASD 移动    R 新一轮（清空收集）    高处鸟巢可用飞行接近"),70,814,14,Muted);
        const bool SharedHUD = UMiningInteractionHUDWidget::IsActiveFor(F->GetWorld()->GetFirstPlayerController());
        if (!SharedHUD) Box(798,448,4,4,F->Target?Gold:Cream);
        if(F->Target && !SharedHUD)
        {
            Box(570,568,460,58,Ink); Text(TEXT("[ E ]  收集  ")+AMiningCollectible::ItemName(F->Target->ItemId),600,580,23,Gold);
        }
        if(F->ToastRemaining>0) { Box(320,665,960,61,Panel); Text(F->Toast,343,681,18,Cream); }
        if(F->UniqueCount()==4) { Text(TEXT("四种奇物已集齐 · 打开图鉴，看看它们的另一面"),470,169,22,Gold); }
        return L;
    }
    FSlateDrawElement::MakeBox(Out,++L,G.ToPaintGeometry(),Brush,ESlateDrawEffect::None,FLinearColor::Black);
    const bool Owned=F->Inventory.IsValidIndex(F->SelectedId)&&F->Inventory[F->SelectedId]>0;
    if(Owned&&F->PreviewTexture)
    {
        auto* Self=const_cast<SMiningCollectionView*>(this); Self->PreviewBrush.SetResourceObject(F->PreviewTexture);
        FSlateDrawElement::MakeBox(Out,++L,Geo(0,8,1155,770),&PreviewBrush,ESlateDrawEffect::None,FLinearColor::White);
    }
    Text(TEXT("奇物收藏"),60,37,24,Cream);
    Text(FString::Printf(TEXT("%02d / 04"),F->UniqueCount()),230,48,12,Muted);
    Box(1408,35,132,44,Panel); Text(FString::Printf(TEXT("返回  %s"),*F->CatalogueKey.GetDisplayName().ToString()),1426,47,14,Cream);
    Box(1178,140,1,585,FLinearColor(.12f,.17f,.16f));
    if(Owned)
    {
        Text(AMiningCollectible::ItemName(F->SelectedId),1220,280,32,Cream);
        Box(1223,344,46,2,Gold);
        TArray<FString> Lines; AMiningCollectible::Description(F->SelectedId).ParseIntoArrayLines(Lines);
        for(int i=0;i<Lines.Num();++i) Text(Lines[i],1223,379+i*35,15,Muted);
    }
    else
    {
        Text(TEXT("尚未发现"),1220,280,32,Muted);
        Text(TEXT("收藏后即可检视"),1223,380,16,Muted);
    }
    Text(TEXT("拖动旋转    滚轮缩放    双击复位"),355,729,14,Muted);
    for(int i=0;i<4;++i)
    {
        const float X=60+i*267; const bool Has=F->Inventory.IsValidIndex(i)&&F->Inventory[i]>0,Selected=F->SelectedId==i;
        Box(X,790,242,75,Selected?FLinearColor(.095f,.135f,.12f):Panel);
        if(Selected) Box(X,790,242,2,Gold);
        Text(FString::Printf(TEXT("0%d"),i+1),X+17,815,15,Selected?Gold:Muted);
        Text(Has?AMiningCollectible::ItemName(i):TEXT("未发现"),X+55,812,20,Has?Cream:Muted);
    }
    Box(1220,661,252,44,Panel); Text(TEXT("F   恢复展示角度"),1240,672,15,Muted);
    return L;
}
FReply SMiningCollectionView::OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
    if(!Owner.IsValid()||!Owner->bCatalogueOpen) return FReply::Unhandled();
    FVector2D P=ToVirtual(G,E.GetScreenSpacePosition());
    if(E.GetEffectingButton()!=EKeys::LeftMouseButton) return FReply::Handled();
    if(P.X>=1408&&P.X<=1540&&P.Y>=35&&P.Y<=79) { Owner->ToggleCatalogue(); return FReply::Handled(); }
    for(int i=0;i<4;++i) if(P.X>=60+i*267&&P.X<=302+i*267&&P.Y>=790&&P.Y<=865) { Owner->SelectItem(i); return FReply::Handled(); }
    if(P.X>=1220&&P.X<=1472&&P.Y>=661&&P.Y<=705) { Owner->ResetPreview(); return FReply::Handled(); }
    if(P.X>=30&&P.X<=1155&&P.Y>=90&&P.Y<=770) { bDragging=true; LastMouse=P; return FReply::Handled().CaptureMouse(AsShared()); }
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
    if(Owner.IsValid()&&P.X>=30&&P.X<=1155&&P.Y>=90&&P.Y<=770) Owner->ResetPreview();
    return FReply::Handled();
}
FReply SMiningCollectionView::OnMouseMove(const FGeometry& G,const FPointerEvent& E)
{
    const auto P=ToVirtual(G,E.GetScreenSpacePosition());
    if(bDragging&&Owner.IsValid()) { const auto D=P-LastMouse; Owner->RotatePreview(D.X*.45f,D.Y*.4f); LastMouse=P; }
    return FReply::Handled();
}
FReply SMiningCollectionView::OnMouseWheel(const FGeometry& G,const FPointerEvent& E)
{
    const auto P=ToVirtual(G,E.GetScreenSpacePosition());
    if(Owner.IsValid()&&P.X>=30&&P.X<=1155&&P.Y>=90&&P.Y<=770) Owner->ZoomPreview(E.GetWheelDelta());
    return FReply::Handled();
}
FReply SMiningCollectionView::OnKeyDown(const FGeometry&,const FKeyEvent& E)
{
    if(!Owner.IsValid()) return FReply::Unhandled();
    if(E.GetKey()==Owner->CatalogueKey||E.GetKey()==EKeys::Escape) { bDragging=false; Owner->ToggleCatalogue(); return FReply::Handled().ReleaseMouseCapture(); }
    if(E.GetKey()==EKeys::F) Owner->ResetPreview();
    return FReply::Handled();
}
