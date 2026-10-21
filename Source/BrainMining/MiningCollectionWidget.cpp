#include "MiningCollectionWidget.h"
#include "MiningCollection.h"
#include "MiningMenuStyle.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/BackgroundBlur.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"

void UMiningCollectionWidget::NativeConstruct()
{
    Super::NativeConstruct();SetIsFocusable(true);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("ItemButton0"))))B->OnClicked.AddUniqueDynamic(this,&UMiningCollectionWidget::Select0);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("ItemButton1"))))B->OnClicked.AddUniqueDynamic(this,&UMiningCollectionWidget::Select1);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("ItemButton2"))))B->OnClicked.AddUniqueDynamic(this,&UMiningCollectionWidget::Select2);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("ItemButton3"))))B->OnClicked.AddUniqueDynamic(this,&UMiningCollectionWidget::Select3);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("PreviousButton"))))B->OnClicked.AddUniqueDynamic(this,&UMiningCollectionWidget::Previous);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("NextButton"))))B->OnClicked.AddUniqueDynamic(this,&UMiningCollectionWidget::Next);
    if(auto* B=Cast<UButton>(GetWidgetFromName(TEXT("CloseButton"))))B->OnClicked.AddUniqueDynamic(this,&UMiningCollectionWidget::CloseCollection);
    for(const TCHAR* Name:{TEXT("PreviousButton"),TEXT("NextButton"),TEXT("CloseButton"),TEXT("ItemButton0"),TEXT("ItemButton1"),TEXT("ItemButton2"),TEXT("ItemButton3")})
        if(auto* B=Cast<UButton>(GetWidgetFromName(Name)))
        {
            auto Style=B->GetStyle();Style.SetHovered(Style.Normal);Style.SetPressed(Style.Normal);
            if(B->GetFName()==TEXT("PreviousButton") || B->GetFName()==TEXT("NextButton"))
            {
                Style.Normal.TintColor=FSlateColor(FLinearColor(.65f,.45f,.015f));
                Style.Hovered.TintColor=FSlateColor(FLinearColor(1.f,.80f,.12f));
                Style.Pressed.TintColor=FSlateColor(FLinearColor(1.f,.95f,.65f));
                Style.SetPressedPadding(Style.NormalPadding+FMargin(0,2,0,-2));
            }
            Style.SetDisabled(Style.Normal);B->SetStyle(Style);
        }
    RefreshCollection();
}
void UMiningCollectionWidget::SetCollection(AMiningCollectionField* Field){Collection=Field;CurrentPage=Collection?FMath::Max(0,Collection->CatalogueIds().IndexOfByKey(Collection->SelectedId))/4:0;RefreshCollection();}
int32 UMiningCollectionWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool Enabled) const
{
    Layer=Super::NativePaint(Args,G,Clip,Elements,Layer,Style,Enabled);
    for(int32 I=0;I<4;++I)
        if(auto* B=Cast<UButton>(GetWidgetFromName(FName(*FString::Printf(TEXT("ItemButton%d"),I))));
           B && B->IsVisible() && B->GetIsEnabled() && (B->IsHovered() || B->HasKeyboardFocus()))
        {
            const auto& BG=B->GetCachedGeometry();const FVector2D Size=BG.GetLocalSize();
            const float Cut=10.f,Inset=1.5f;
            TArray<FVector2D> Points={{Inset,Inset},{Size.X-Cut,Inset},{Size.X-Inset,Cut},{Size.X-Inset,Size.Y-Inset},
                {Cut,Size.Y-Inset},{Inset,Size.Y-Cut},{Inset,Inset}};
            for(auto& P:Points)P=G.AbsoluteToLocal(BG.LocalToAbsolute(P));
            FSlateDrawElement::MakeLines(Elements,++Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,
                B->IsPressed()?MiningMenuStyle::Gold:FLinearColor::White,true,2.5f);
        }
    return Layer;
}
void UMiningCollectionWidget::RevealCollection()
{
    CurrentPage=IsValid(Collection)?FMath::Max(0,Collection->CatalogueIds().IndexOfByKey(Collection->SelectedId))/4:0;
    bDragging=false;RevealElapsed=0;bRevealing=true;
    if(auto* Panel=GetWidgetFromName(TEXT("InfoPanel")))Panel->SetRenderTranslation(FVector2D(PanelSlideDistance,0));
    if(auto* Blur=Cast<UBackgroundBlur>(GetWidgetFromName(TEXT("SceneBlur"))))Blur->SetBlurStrength(0);
    if(auto* Dim=GetWidgetFromName(TEXT("SceneDim")))Dim->SetRenderOpacity(0);
    RefreshCollection();
}
void UMiningCollectionWidget::NativeTick(const FGeometry& G,float Dt)
{
    Super::NativeTick(G,Dt);
    if(!bRevealing)return;
    RevealElapsed+=Dt;
    const float T=FMath::Clamp(RevealElapsed/FMath::Max(.05f,PanelSlideDuration),0.f,1.f);
    const float Ease=1-FMath::Pow(1-T,3.f);
    // Render translation preserves the panel's position authored in UMG.
    if(auto* Panel=GetWidgetFromName(TEXT("InfoPanel")))Panel->SetRenderTranslation(FVector2D(PanelSlideDistance*(1-Ease),0));
    if(auto* Blur=Cast<UBackgroundBlur>(GetWidgetFromName(TEXT("SceneBlur"))))Blur->SetBlurStrength(BackgroundBlurStrength*Ease);
    if(auto* Dim=GetWidgetFromName(TEXT("SceneDim")))Dim->SetRenderOpacity(BackgroundDimOpacity*Ease);
    bRevealing=T<1;
}
void UMiningCollectionWidget::RefreshCollection()
{
    if(!IsValid(Collection))return;
    const int32 Id=Collection->SelectedId;
    const bool Owned=Collection->Inventory.IsValidIndex(Id)&&Collection->Inventory[Id]>0;
    if(auto* I=Cast<UImage>(GetWidgetFromName(TEXT("PreviewImage"))))
    {
        if(Collection->PreviewMaterial)I->SetBrushFromMaterial(Collection->PreviewMaterial);
        I->SetVisibility(Owned?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden);
    }
    if(auto* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("ItemName"))))T->SetText(FText::FromString(Owned?AMiningCollectible::ItemName(Id):TEXT("尚未发现")));
    if(auto* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("ItemDescription"))))T->SetText(FText::FromString(Owned?AMiningCollectible::Description(Id):TEXT("")));
    if(auto* W=GetWidgetFromName(TEXT("PreviewHelp")))W->SetVisibility(ESlateVisibility::Collapsed);
    const auto Ids=Collection->CatalogueIds();
    const int32 Page=CurrentPage;
    if(auto* W=GetWidgetFromName(TEXT("PreviousButton")))W->SetVisibility(Page>0?ESlateVisibility::Visible:ESlateVisibility::Hidden);
    if(auto* W=GetWidgetFromName(TEXT("NextButton")))W->SetVisibility((Page+1)*4<Ids.Num()?ESlateVisibility::Visible:ESlateVisibility::Hidden);
    for(int32 I=0;I<4;++I)
    {
        const int32 Item=Ids.IsValidIndex(Page*4+I)?Ids[Page*4+I]:INDEX_NONE;
        const bool Has=Collection->Inventory.IsValidIndex(Item)&&Collection->Inventory[Item]>0;
        if(auto* Image=Cast<UImage>(GetWidgetFromName(FName(*FString::Printf(TEXT("ItemArt%d"),I)))))
        {
            Image->SetBrush(*MiningMenuStyle::Brush(1));
            Image->SetColorAndOpacity(Has?FLinearColor::White:FLinearColor(.5f,.5f,.5f,.65f));
        }
        if(auto* T=Cast<UTextBlock>(GetWidgetFromName(FName(*FString::Printf(TEXT("ItemLabel%d"),I)))))
        {
            // The card is 222 units wide. Auto-wrap on an auto-sized canvas
            // label can reuse a collapsed width and stack a short name vertically.
            T->SetAutoWrapText(false);T->SetWrapTextAt(208.f);T->SetJustification(ETextJustify::Center);
            T->SetText(FText::FromString(Has?AMiningCollectible::ItemName(Item):TEXT("尚未发现")));
        }
        if(auto* W=Cast<UImage>(GetWidgetFromName(FName(*FString::Printf(TEXT("ItemSelected%d"),I)))))
        {
            W->SetBrush(*MiningMenuStyle::Brush(2));
            W->SetVisibility(Has&&Id==Item?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden);
        }
        if(auto* W=GetWidgetFromName(FName(*FString::Printf(TEXT("ItemButton%d"),I))))
        {
            W->SetVisibility(Item==INDEX_NONE?ESlateVisibility::Hidden:ESlateVisibility::Visible);
            W->SetIsEnabled(Has);
        }
        if(auto* T=Cast<UTextBlock>(GetWidgetFromName(FName(*FString::Printf(TEXT("ItemLabel%d"),I)))))T->SetColorAndOpacity(Has&&Id==Item?FLinearColor(.025f,.02f,.005f):Has?MiningMenuStyle::Cream:FLinearColor(.42f,.42f,.40f));
    }
}
void UMiningCollectionWidget::SelectCollectionItem(int32 Id)
{
    if(IsValid(Collection)&&Collection->Inventory.IsValidIndex(Id)&&Collection->Inventory[Id]>0)
    {CurrentPage=FMath::Max(0,Collection->CatalogueIds().IndexOfByKey(Id))/4;Collection->SelectItem(Id);}
}
void UMiningCollectionWidget::TurnCollectionPage(int32 Direction)
{
    if(!IsValid(Collection))return;
    const auto Ids=Collection->CatalogueIds(); if(Ids.IsEmpty())return;
    const int32 Pages=FMath::DivideAndRoundUp(Ids.Num(),4);
    const int32 Page=FMath::Clamp(CurrentPage+(Direction<0?-1:1),0,Pages-1);
    if(Page==CurrentPage)return;
    CurrentPage=Page;
    int32 Selection=INDEX_NONE;
    for(int32 I=Page*4;I<FMath::Min((Page+1)*4,Ids.Num());++I)
        if(Collection->Inventory.IsValidIndex(Ids[I])&&Collection->Inventory[Ids[I]]>0){Selection=Ids[I];break;}
    Collection->SelectItem(Selection);
    RefreshCollection();
}
void UMiningCollectionWidget::CloseCollection(){bDragging=false;if(IsValid(Collection)&&Collection->bCatalogueOpen)Collection->ToggleCatalogue();}
void UMiningCollectionWidget::Select0(){SelectSlot(0);}void UMiningCollectionWidget::Select1(){SelectSlot(1);}
void UMiningCollectionWidget::Select2(){SelectSlot(2);}void UMiningCollectionWidget::Select3(){SelectSlot(3);}
void UMiningCollectionWidget::Previous(){TurnCollectionPage(-1);}void UMiningCollectionWidget::Next(){TurnCollectionPage(1);}
bool UMiningCollectionWidget::InPreview(const FVector2D& Position) const
{
    const auto* W=GetWidgetFromName(TEXT("PreviewPanel"));
    return W&&W->GetCachedGeometry().IsUnderLocation(Position);
}
FReply UMiningCollectionWidget::NativeOnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
    if(E.GetEffectingButton()==EKeys::LeftMouseButton&&InPreview(E.GetScreenSpacePosition()))
    {bDragging=true;LastMouse=E.GetScreenSpacePosition();return FReply::Handled().CaptureMouse(TakeWidget());}
    return Super::NativeOnMouseButtonDown(G,E);
}
FReply UMiningCollectionWidget::NativeOnMouseButtonUp(const FGeometry& G,const FPointerEvent& E)
{
    if(E.GetEffectingButton()==EKeys::LeftMouseButton&&bDragging){bDragging=false;return FReply::Handled().ReleaseMouseCapture();}
    return Super::NativeOnMouseButtonUp(G,E);
}
FReply UMiningCollectionWidget::NativeOnMouseButtonDoubleClick(const FGeometry& G,const FPointerEvent& E)
{
    if(E.GetEffectingButton()==EKeys::LeftMouseButton&&InPreview(E.GetScreenSpacePosition())&&IsValid(Collection)){Collection->ResetPreview();return FReply::Handled();}
    return Super::NativeOnMouseButtonDoubleClick(G,E);
}
FReply UMiningCollectionWidget::NativeOnMouseMove(const FGeometry& G,const FPointerEvent& E)
{
    if(bDragging&&IsValid(Collection))
    {
        // Convert screen motion through the designer panel, so moving/scaling it keeps input aligned.
        auto* Panel=GetWidgetFromName(TEXT("PreviewPanel"));
        const auto& Geometry=Panel?Panel->GetCachedGeometry():G;
        const auto D=Geometry.AbsoluteToLocal(E.GetScreenSpacePosition())-Geometry.AbsoluteToLocal(LastMouse);
        Collection->RotatePreview(D.X*.45f,D.Y*.4f);LastMouse=E.GetScreenSpacePosition();return FReply::Handled();
    }
    return Super::NativeOnMouseMove(G,E);
}
FReply UMiningCollectionWidget::NativeOnMouseWheel(const FGeometry& G,const FPointerEvent& E)
{
    if(InPreview(E.GetScreenSpacePosition())&&IsValid(Collection)){Collection->ZoomPreview(E.GetWheelDelta());return FReply::Handled();}
    return Super::NativeOnMouseWheel(G,E);
}
FReply UMiningCollectionWidget::NativeOnKeyDown(const FGeometry& G,const FKeyEvent& E)
{
    if(E.GetKey()==EKeys::Escape||(IsValid(Collection)&&E.GetKey()==Collection->CatalogueKey)){CloseCollection();return FReply::Handled();}
    if(E.GetKey()==EKeys::F&&IsValid(Collection)){Collection->ResetPreview();return FReply::Handled();}
    if(E.GetKey()==EKeys::Left){Previous();return FReply::Handled();}
    if(E.GetKey()==EKeys::Right){Next();return FReply::Handled();}
    return Super::NativeOnKeyDown(G,E);
}
void UMiningCollectionWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& E){bDragging=false;Super::NativeOnMouseCaptureLost(E);}

void UMiningCollectionWidget::SelectSlot(int32 SlotIndex)
{
    if(!IsValid(Collection))return;
    const auto Ids=Collection->CatalogueIds();
    const int32 Index=CurrentPage*4+SlotIndex;
    if(Ids.IsValidIndex(Index)) { SelectCollectionItem(Ids[Index]); RefreshCollection(); }
}
