#include "MiningCollectionWidget.h"
#include "MiningCollection.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/BackgroundBlur.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

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
    RefreshCollection();
}
void UMiningCollectionWidget::SetCollection(AMiningCollectionField* Field){Collection=Field;RefreshCollection();}
void UMiningCollectionWidget::RevealCollection()
{
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
    if(auto* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("ItemDescription"))))T->SetText(FText::FromString(Owned?AMiningCollectible::Description(Id):TEXT("收集后即可旋转检视")));
    if(auto* W=GetWidgetFromName(TEXT("PreviewHelp")))W->SetVisibility(Owned?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden);
    const auto Ids=Collection->CatalogueIds();
    const int32 Page=FMath::Max(0,Ids.IndexOfByKey(Id))/4;
    for(int32 I=0;I<4;++I)
    {
        const int32 Item=Ids.IsValidIndex(Page*4+I)?Ids[Page*4+I]:INDEX_NONE;
        const bool Has=Collection->Inventory.IsValidIndex(Item)&&Collection->Inventory[Item]>0;
        if(auto* Image=Cast<UImage>(GetWidgetFromName(FName(*FString::Printf(TEXT("ItemArt%d"),I)))))
        {
            const TCHAR* Art=Has?TEXT("/Game/CollectionPrototype/UI/Redesign/T_CollectionSelected"):TEXT("/Game/CollectionPrototype/UI/Redesign/T_CollectionUnknown");
            if(auto* Texture=LoadObject<UTexture2D>(nullptr,Art))Image->SetBrushFromTexture(Texture);
            Image->SetColorAndOpacity(Has&&Id!=Item?FLinearColor(.45f,.55f,.42f):FLinearColor::White);
        }
        if(auto* T=Cast<UTextBlock>(GetWidgetFromName(FName(*FString::Printf(TEXT("ItemLabel%d"),I)))))T->SetText(FText::FromString(Has?AMiningCollectible::ItemName(Item):FString()));
        if(auto* W=GetWidgetFromName(FName(*FString::Printf(TEXT("ItemSelected%d"),I))))W->SetVisibility(Id==Item?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden);
        if(auto* W=GetWidgetFromName(FName(*FString::Printf(TEXT("ItemButton%d"),I))))W->SetVisibility(Item==INDEX_NONE?ESlateVisibility::Hidden:ESlateVisibility::Visible);
    }
}
void UMiningCollectionWidget::SelectCollectionItem(int32 Id){if(IsValid(Collection))Collection->SelectItem(Id);}
void UMiningCollectionWidget::TurnCollectionPage(int32 Direction)
{
    if(!IsValid(Collection))return;
    const auto Ids=Collection->CatalogueIds(); if(Ids.IsEmpty())return;
    const int32 Index=FMath::Max(0,Ids.IndexOfByKey(Collection->SelectedId));
    const int32 Pages=FMath::DivideAndRoundUp(Ids.Num(),4);
    const int32 Page=(Index/4+(Direction<0?-1:1)+Pages)%Pages;
    SelectCollectionItem(Ids[FMath::Min(Page*4+Index%4,Ids.Num()-1)]);
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
    const int32 Index=(FMath::Max(0,Ids.IndexOfByKey(Collection->SelectedId))/4)*4+SlotIndex;
    if(Ids.IsValidIndex(Index)) { SelectCollectionItem(Ids[Index]); RefreshCollection(); }
}
