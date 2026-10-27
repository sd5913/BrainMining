#include "MiningCollectionWidget.h"
#include "MiningLocalizationSubsystem.h"
#include "MiningCollection.h"
#include "MiningMineralRewards.h"
#include "MiningMenuStyle.h"
#include "BrainMiningFonts.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/BackgroundBlur.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "Engine/DataTable.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "Internationalization/TextLocalizationManager.h"

namespace
{
FString FittedQuoteStyle(const FString& Quote, const URichTextBlock* Text)
{
    const UDataTable* Styles=Text->GetTextStyleSet();
    const auto* Row=Styles?Styles->FindRow<FRichTextStyleRow>(TEXT("Quote"),TEXT("Collection quote"),false):nullptr;
    if(!Row || !FSlateApplication::IsInitialized())return TEXT("Quote");
    const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    const float Width=Text->GetWrapTextAt()>0?Text->GetWrapTextAt():286.f;
    const int32 Base=Row->TextStyle.Font.Size, Minimum=FMath::CeilToInt(Base*.82f);
    for(int32 Size=Base;Size>=Minimum;--Size)
    {
        auto Font=Row->TextStyle.Font;Font.Size=Size;
        int32 LineStart=0,Lines=1;
        for(int32 I=1;I<=Quote.Len();++I)
            if(Measure->Measure(FStringView(Quote).Mid(LineStart,I-LineStart),Font).X>Width-3.f)
            {LineStart=I-1;++Lines;}
        const float LastWidth=Measure->Measure(FStringView(Quote).Mid(LineStart),Font).X;
        // Keep the authored alignment and body size. Only shrink a quote when
        // its final line would otherwise contain a few isolated characters.
        if(Lines==1 || LastWidth>=Measure->Measure(FStringView(TEXT("山在心里")),Font).X)
            return Size==Base?TEXT("Quote"):FString::Printf(TEXT("QuoteFit%d"),Size);
    }
    return TEXT("Quote"); // Long quotations remain readable and wrap normally.
}
}

void UMiningCollectionPageButton::Configure(UMiningCollectionWidget* InOwner,int32 InPage)
{
    CollectionWidget=InOwner;Page=InPage;
    OnClicked.AddUniqueDynamic(this,&ThisClass::SelectPage);
}
void UMiningCollectionPageButton::SelectPage()
{
    if(CollectionWidget.IsValid())CollectionWidget->SelectCollectionPage(Page);
}

void UMiningCollectionWidget::NativeConstruct()
{
    Super::NativeConstruct();SetIsFocusable(true);
    if(auto* Title=Cast<UTextBlock>(GetWidgetFromName(TEXT("CollectionTitle"))))
    {
        Title->SetFont(BrainMiningFonts::Title(36));
        Title->SetText(MiningText::Get(TEXT("UI.Collection.Title")));
        if(auto* TitleSlot=Cast<UCanvasPanelSlot>(Title->Slot))
        {
            TitleSlot->SetSize(FVector2D(300,60));
            if(!GetWidgetFromName(TEXT("CollectionCount")))
                if(auto* Canvas=Cast<UCanvasPanel>(Title->GetParent()))
                {
                    auto* Count=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("CollectionCount"));
                    Count->SetFont(BrainMiningFonts::Body(18));Count->SetColorAndOpacity(MiningMenuStyle::Cream);
                    Count->SetVisibility(ESlateVisibility::HitTestInvisible);
                    auto* CountSlot=Canvas->AddChildToCanvas(Count);CountSlot->SetPosition(TitleSlot->GetPosition()+FVector2D(0,56));CountSlot->SetSize(FVector2D(300,32));
                }
        }
    }
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
            if(B->GetFName()==TEXT("PreviousButton") || B->GetFName()==TEXT("NextButton") || B->GetFName()==TEXT("CloseButton"))
            {
                Style.Normal.TintColor=FSlateColor(FLinearColor(.65f,.45f,.015f));
                Style.Hovered.TintColor=FSlateColor(FLinearColor(1.f,.80f,.12f));
                Style.Pressed.TintColor=FSlateColor(FLinearColor(1.f,.95f,.65f));
                Style.SetPressedPadding(Style.NormalPadding+FMargin(0,2,0,-2));
            }
            if(B->GetFName()==TEXT("PreviousButton") || B->GetFName()==TEXT("NextButton"))
            {
                const FName GlyphName(*(FString(Name)+TEXT("Glyph")));
                auto* Glyph=Cast<UImage>(GetWidgetFromName(GlyphName));
                if(!Glyph)Glyph=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),GlyphName);
                FSlateBrush ArrowBrush;ArrowBrush.DrawAs=ESlateBrushDrawType::Image;
                ArrowBrush.SetResourceObject(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/UI/Redesign/M_CollectionArrowTint.M_CollectionArrowTint")));
                ArrowBrush.ImageSize=FVector2D(32,38);
                ArrowBrush.Mirroring=B->GetFName()==TEXT("PreviousButton")?ESlateBrushMirrorType::Horizontal:ESlateBrushMirrorType::NoMirror;
                Glyph->SetBrush(ArrowBrush);Glyph->SetColorAndOpacity(MiningMenuStyle::Gold);Glyph->SetVisibility(ESlateVisibility::HitTestInvisible);
                B->SetContent(Glyph);
                FSlateBrush Empty;Empty.DrawAs=ESlateBrushDrawType::NoDrawType;
                Style.SetNormal(Empty).SetHovered(Empty).SetPressed(Empty);
                // A smaller glyph sits inside the original, generous hit area.
                Style.SetNormalPadding(FMargin(7,8)).SetPressedPadding(FMargin(7,10,7,6));
            }
            Style.SetDisabled(Style.Normal);MiningUISounds::Apply(Style);B->SetStyle(Style);
        }
    // Enlarge only the empty gutters: cards start at x=51 and end at x=957.
    for(const TCHAR* Name:{TEXT("PreviousButton"),TEXT("NextButton")})
        if(auto* B=GetWidgetFromName(Name))if(auto* ButtonSlot=Cast<UCanvasPanelSlot>(B->Slot))
        {ButtonSlot->SetPosition(FVector2D(B->GetFName()==TEXT("PreviousButton")?0:963,20));ButtonSlot->SetSize(FVector2D(46,54));}
    if(auto* Bar=Cast<UCanvasPanel>(GetWidgetFromName(TEXT("CollectionBar"))))
    {
        const int32 Pages=FMath::DivideAndRoundUp(AMiningCollectible::ItemTypeCount,4);
        for(int32 I=0;I<Pages;++I)
        {
            const FName Name(*FString::Printf(TEXT("PageIndicator%d"),I));
            if(GetWidgetFromName(Name))continue;
            auto* Button=WidgetTree->ConstructWidget<UMiningCollectionPageButton>(UMiningCollectionPageButton::StaticClass(),FName(*FString::Printf(TEXT("PageButton%d"),I)));
            Button->Configure(this,I);
            FButtonStyle PageStyle;FSlateBrush Empty;Empty.DrawAs=ESlateBrushDrawType::NoDrawType;
            PageStyle.SetNormal(Empty).SetHovered(Empty).SetPressed(Empty).SetDisabled(Empty);
            PageStyle.SetNormalPadding(FMargin(4,12)).SetPressedPadding(FMargin(4,12));MiningUISounds::Apply(PageStyle);Button->SetStyle(PageStyle);
            auto* Segment=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),Name);
            Segment->SetBrush(*FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")));
            Segment->SetVisibility(ESlateVisibility::HitTestInvisible);
            auto* S=Bar->AddChildToCanvas(Button);
            const float X=(1010.f-(Pages*24.f+(Pages-1)*8.f))*.5f+I*32.f;
            S->SetPosition(FVector2D(X-4.f,104));
            S->SetSize(FVector2D(32,31));
            auto* Visual=Bar->AddChildToCanvas(Segment);Visual->SetPosition(FVector2D(X,116));Visual->SetSize(FVector2D(24,7));Visual->SetZOrder(2);
        }

    }
    for(int32 I=0;I<4;++I)
        if(auto* Label=Cast<UTextBlock>(GetWidgetFromName(FName(*FString::Printf(TEXT("ItemLabel%d"),I)))))
            if(auto* Canvas=Cast<UCanvasPanel>(Label->GetParent()))
            {
                const FName Name(*FString::Printf(TEXT("ItemCondition%d"),I));
                if(GetWidgetFromName(Name))continue;
                auto* Condition=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
                Condition->SetFont(BrainMiningFonts::Body(14));Condition->SetJustification(ETextJustify::Center);
                Condition->SetVisibility(ESlateVisibility::HitTestInvisible);
                auto* ConditionSlot=Canvas->AddChildToCanvas(Condition);ConditionSlot->SetPosition(FVector2D(7,61));ConditionSlot->SetSize(FVector2D(208,28));
            }
    if(auto* Description=Cast<URichTextBlock>(GetWidgetFromName(TEXT("ItemDescription"))))
    {
        Description->SetJustification(ETextJustify::Left);
        if(auto* Styles=Description->GetTextStyleSet())
            if(const auto* Quote=Styles->FindRow<FRichTextStyleRow>(TEXT("Quote"),TEXT("Collection quote"),false))
            {
                const FRichTextStyleRow Original=*Quote;
                auto* LocalStyles=DuplicateObject<UDataTable>(Styles,Description);
                for(int32 Size=Original.TextStyle.Font.Size-1;Size>=FMath::CeilToInt(Original.TextStyle.Font.Size*.82f);--Size)
                {
                    auto Row=Original;Row.TextStyle.Font.Size=Size;
                    LocalStyles->AddRow(FName(*FString::Printf(TEXT("QuoteFit%d"),Size)),Row);
                }
                Description->SetTextStyleSet(LocalStyles);
            }
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
    if(TextRevision!=FTextLocalizationManager::Get().GetTextRevision())RefreshCollection();
    if(auto* Back=Cast<UButton>(GetWidgetFromName(TEXT("CloseButton"))))
        Back->SetRenderScale(FVector2D(Back->IsPressed()?.96f:Back->IsHovered()||Back->HasKeyboardFocus()?1.16f:1.f));
    for(const TCHAR* Name:{TEXT("PreviousButton"),TEXT("NextButton")})
        if(auto* B=Cast<UButton>(GetWidgetFromName(Name)))
            if(auto* Glyph=Cast<UImage>(B->GetContent()))
                Glyph->SetColorAndOpacity(B->IsPressed()?FLinearColor(.72f,.38f,.008f):B->IsHovered()||B->HasKeyboardFocus()?FLinearColor(1.f,.90f,.42f):MiningMenuStyle::Gold);
    for(int32 I=0;I<FMath::DivideAndRoundUp(AMiningCollectible::ItemTypeCount,4);++I)
        if(auto* Button=Cast<UButton>(GetWidgetFromName(FName(*FString::Printf(TEXT("PageButton%d"),I)))))
            if(auto* Segment=Cast<UImage>(GetWidgetFromName(FName(*FString::Printf(TEXT("PageIndicator%d"),I)))))
            {
                const bool Hover=Button->IsHovered() || Button->HasKeyboardFocus();
                Segment->SetColorAndOpacity(I==CurrentPage?(Hover?FLinearColor(1.f,.85f,.15f):MiningMenuStyle::Gold):Hover?MiningMenuStyle::Cream:FLinearColor(.18f,.14f,.035f,.85f));
                Segment->SetRenderScale(FVector2D(1.f,Hover?1.4f:1.f));
            }
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
    TextRevision=FTextLocalizationManager::Get().GetTextRevision();
    if(auto* Count=Cast<UTextBlock>(GetWidgetFromName(TEXT("CollectionCount"))))
        Count->SetText(FText::Format(MiningText::Get(TEXT("UI.Collection.Count")),FFormatNamedArguments{{TEXT("Count"),Collection->UniqueCount()},{TEXT("Total"),AMiningCollectible::ItemTypeCount}}));
    const int32 Id=Collection->SelectedId;
    const bool Owned=Collection->Inventory.IsValidIndex(Id)&&Collection->Inventory[Id]>0;
    if(auto* I=Cast<UImage>(GetWidgetFromName(TEXT("PreviewImage"))))
    {
        if(Collection->PreviewMaterial)I->SetBrushFromMaterial(Collection->PreviewMaterial);
        I->SetVisibility(Owned?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden);
    }
    if(auto* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("ItemName"))))T->SetText(Owned?AMiningCollectible::ItemNameText(Id):MiningText::Get(TEXT("UI.Collection.Undiscovered")));
    const FString Description=Owned?AMiningCollectible::Description(Id):Collection->UnlockCondition(Id);
    if(auto* T=Cast<URichTextBlock>(GetWidgetFromName(TEXT("ItemDescription"))))
    {
        const auto Escape=[](FString Value)
        {return Value.Replace(TEXT("&"),TEXT("&amp;")).Replace(TEXT("<"),TEXT("&lt;")).Replace(TEXT(">"),TEXT("&gt;"));};
        FString Body,Quote;
        const FString Markup=Owned && Description.Split(TEXT("\n\n"),&Body,&Quote)
            ?Escape(Body)+TEXT("\n\n<")+FittedQuoteStyle(Quote,T)+TEXT(">")+Escape(Quote)+TEXT("</>"):Escape(Description);
        if(!T->GetText().EqualTo(FText::FromString(Markup)))
        {
            T->SetText(FText::FromString(Markup));
            if(auto* Scroll=Cast<UScrollBox>(GetWidgetFromName(TEXT("DescriptionScroll"))))Scroll->ScrollToStart();
        }
    }
    else if(auto* PlainText=Cast<UTextBlock>(GetWidgetFromName(TEXT("ItemDescription"))))PlainText->SetText(FText::FromString(Description));
    if(auto* W=GetWidgetFromName(TEXT("PreviewHelp")))W->SetVisibility(ESlateVisibility::Collapsed);
    const auto Ids=Collection->CatalogueIds();
    const int32 Page=CurrentPage;
    for(int32 I=0;I<FMath::DivideAndRoundUp(AMiningCollectible::ItemTypeCount,4);++I)
        if(auto* Segment=Cast<UImage>(GetWidgetFromName(FName(*FString::Printf(TEXT("PageIndicator%d"),I)))))
            Segment->SetColorAndOpacity(I==Page?MiningMenuStyle::Gold:FLinearColor(.18f,.14f,.035f,.85f));
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
            T->SetText(Has?AMiningCollectible::ItemNameText(Item):MiningText::Get(TEXT("UI.Collection.Undiscovered")));
            T->SetRenderTranslation(FVector2D(0,AMiningCollectible::IsMineralReward(Item)?-14:0));
        }
        if(auto* Condition=Cast<UTextBlock>(GetWidgetFromName(FName(*FString::Printf(TEXT("ItemCondition%d"),I)))))
        {
            const bool Show=AMiningCollectible::IsMineralReward(Item);
            Condition->SetText(Show?FText::Format(MiningText::Get(Has?TEXT("UI.Collection.MineralGoalComplete"):TEXT("UI.Collection.MineralGoal")),FFormatNamedArguments{{TEXT("Target"),AMiningCollectible::UnlockThreshold(Item)},{TEXT("Mineral"),FMiningMineralCost::NameText(MineralRewardType(Item))}}):FText::GetEmpty());
            Condition->SetColorAndOpacity(Id==Item?FLinearColor(.12f,.10f,.025f):FLinearColor(.62f,.62f,.58f));
        }
        if(auto* W=Cast<UImage>(GetWidgetFromName(FName(*FString::Printf(TEXT("ItemSelected%d"),I)))))
        {
            W->SetBrush(*MiningMenuStyle::Brush(2));
            W->SetVisibility(Item!=INDEX_NONE&&Id==Item?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden);
        }
        if(auto* W=GetWidgetFromName(FName(*FString::Printf(TEXT("ItemButton%d"),I))))
        {
            W->SetVisibility(Item==INDEX_NONE?ESlateVisibility::Hidden:ESlateVisibility::Visible);
            W->SetIsEnabled(Item!=INDEX_NONE);
        }
        if(auto* T=Cast<UTextBlock>(GetWidgetFromName(FName(*FString::Printf(TEXT("ItemLabel%d"),I)))))T->SetColorAndOpacity(Id==Item?FLinearColor(.025f,.02f,.005f):Has?MiningMenuStyle::Cream:FLinearColor(.42f,.42f,.40f));
    }
}
void UMiningCollectionWidget::SelectCollectionItem(int32 Id)
{
    if(IsValid(Collection)&&Collection->CatalogueIds().Contains(Id))
    {CurrentPage=FMath::Max(0,Collection->CatalogueIds().IndexOfByKey(Id))/4;Collection->SelectItem(Id);}
}
void UMiningCollectionWidget::TurnCollectionPage(int32 Direction)
{
    SelectCollectionPage(CurrentPage+(Direction<0?-1:1));
}
void UMiningCollectionWidget::SelectCollectionPage(int32 RequestedPage)
{
    if(!IsValid(Collection))return;
    const auto Ids=Collection->CatalogueIds(); if(Ids.IsEmpty())return;
    const int32 Pages=FMath::DivideAndRoundUp(Ids.Num(),4);
    const int32 Page=FMath::Clamp(RequestedPage,0,Pages-1);
    if(Page==CurrentPage)return;
    CurrentPage=Page;
    const int32 Selection=Ids[Page*4];
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
