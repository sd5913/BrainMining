#include "CollectionUMGEditorLibrary.h"
#include "BrainMiningFonts.h"
#include "MiningCollectionWidget.h"
#include "MiningCollection.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInterface.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Styling/CoreStyle.h"
#include "Engine/World.h"
#include "Input/Events.h"

namespace CollectionUMG
{
template<class T>T* Make(UWidgetBlueprint* BP,const FString& Name)
{
    auto* W=BP->WidgetTree->ConstructWidget<T>(T::StaticClass(),FName(*Name));W->bIsVariable=true;
    BP->WidgetVariableNameToGuidMap.Add(W->GetFName(),FGuid::NewGuid());
    W->SetVisibility(ESlateVisibility::SelfHitTestInvisible);return W;
}
void Place(UCanvasPanel* Parent,UWidget* Child,float X,float Y,float W,float H)
{
    auto* S=Parent->AddChildToCanvas(Child);S->SetPosition(FVector2D(X,Y));S->SetSize(FVector2D(W,H));
}
void Content(UButton* Button,UWidget* Child)
{
    auto* Slot=CastChecked<UButtonSlot>(Button->AddChild(Child));Slot->SetHorizontalAlignment(HAlign_Fill);Slot->SetVerticalAlignment(VAlign_Fill);Slot->SetPadding(FMargin(0));
}
UImage* Image(UWidgetBlueprint* BP,UCanvasPanel* Parent,const FString& Name,float X,float Y,float W,float H,const TCHAR* Art=nullptr,FLinearColor Tint=FLinearColor::White)
{
    auto* I=Make<UImage>(BP,Name);if(Art)I->SetBrushFromMaterial(LoadObject<UMaterialInterface>(nullptr,Art));
    I->SetColorAndOpacity(Tint);I->SetVisibility(ESlateVisibility::HitTestInvisible);Place(Parent,I,X,Y,W,H);return I;
}
UTextBlock* Text(UWidgetBlueprint* BP,UCanvasPanel* Parent,const FString& Name,const FString& Value,float X,float Y,float W,float H,int32 Size,FLinearColor Color=FLinearColor::White)
{
    auto* T=Make<UTextBlock>(BP,Name);T->SetText(FText::FromString(Value));T->SetFont((Name.Contains(TEXT("Title")) || Name.Contains(TEXT("Header")) || Name == TEXT("ItemName")) ? BrainMiningFonts::Title(Size) : BrainMiningFonts::Body(Size));
    T->SetColorAndOpacity(Color);T->SetVisibility(ESlateVisibility::HitTestInvisible);Place(Parent,T,X,Y,W,H);return T;
}
UButton* Button(UWidgetBlueprint* BP,UCanvasPanel* Parent,const FString& Name,float X,float Y,float W,float H)
{
    auto* B=Make<UButton>(BP,Name);FButtonStyle Style;
    FSlateBrush Normal;Normal.TintColor=FLinearColor(0,0,0,0);FSlateBrush Hover=Normal;Hover.TintColor=FLinearColor(.56f,1.f,.29f,.12f);
    Style.SetNormal(Normal).SetHovered(Hover).SetPressed(Hover);Style.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
    B->SetStyle(Style);B->IsFocusable=false;B->SetVisibility(ESlateVisibility::Visible);Place(Parent,B,X,Y,W,H);return B;
}
}
bool UCollectionUMGEditorLibrary::CreateCollectionInspectWidget()
{
    using namespace CollectionUMG;
    const FString PackageName=TEXT("/Game/CollectionPrototype/UI/WBP_CollectionInspect");
    if(LoadObject<UWidgetBlueprint>(nullptr,*(PackageName+TEXT(".WBP_CollectionInspect"))))return true;
    auto* Package=CreatePackage(*PackageName);auto* Factory=NewObject<UWidgetBlueprintFactory>();Factory->ParentClass=UMiningCollectionWidget::StaticClass();
    auto* BP=Cast<UWidgetBlueprint>(Factory->FactoryCreateNew(UWidgetBlueprint::StaticClass(),Package,TEXT("WBP_CollectionInspect"),RF_Public|RF_Standalone,nullptr,GWarn));if(!BP)return false;
    auto* Overlay=Make<UOverlay>(BP,TEXT("ScreenRoot"));BP->WidgetTree->RootWidget=Overlay;
    auto* Blur=Make<UBackgroundBlur>(BP,TEXT("SceneBlur"));Blur->SetBlurStrength(10);Overlay->AddChildToOverlay(Blur)->SetHorizontalAlignment(HAlign_Fill);CastChecked<UOverlaySlot>(Blur->Slot)->SetVerticalAlignment(VAlign_Fill);
    auto* Fit=Make<UScaleBox>(BP,TEXT("ResponsiveScale"));Fit->SetStretch(EStretch::ScaleToFit);auto* FitSlot=Overlay->AddChildToOverlay(Fit);FitSlot->SetHorizontalAlignment(HAlign_Fill);FitSlot->SetVerticalAlignment(VAlign_Fill);
    auto* Size=Make<USizeBox>(BP,TEXT("DesignSize1600x900"));Size->SetWidthOverride(1600);Size->SetHeightOverride(900);Fit->AddChild(Size);
    auto* Canvas=Make<UCanvasPanel>(BP,TEXT("LayoutCanvas"));Size->AddChild(Canvas);
    Image(BP,Canvas,TEXT("SceneDim"),0,0,1600,900,nullptr,FLinearColor(0,0,0,.48f));
    auto* Info=Make<UCanvasPanel>(BP,TEXT("InfoPanel"));Place(Canvas,Info,1170,0,430,900);
    auto* Background=Image(BP,Info,TEXT("InfoBackground"),0,0,600,900,nullptr,FLinearColor(0,0,0,.94f));Background->SetRenderShear(FVector2D(-.18f,0));
    Text(BP,Info,TEXT("ItemName"),TEXT("天堂马桶"),74,373,326,54,32)->SetJustification(ETextJustify::Center);
    Image(BP,Info,TEXT("DescriptionDivider"),90,438,118,1);
    auto* Description=Text(BP,Info,TEXT("ItemDescription"),TEXT("收集藏品后在这里显示介绍。\n可在设计器中调整文字位置。"),90,466,310,190,17,FLinearColor(.62f,.69f,.66f));Description->SetAutoWrapText(true);
    auto* PreviewPanel=Make<UBorder>(BP,TEXT("PreviewPanel"));PreviewPanel->SetBrushColor(FLinearColor::Transparent);PreviewPanel->SetPadding(FMargin(0));PreviewPanel->SetVisibility(ESlateVisibility::Visible);Place(Canvas,PreviewPanel,40,80,1030,650);
    auto* PreviewImage=Make<UImage>(BP,TEXT("PreviewImage"));PreviewImage->SetBrushFromMaterial(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionCataloguePreview")));PreviewImage->SetVisibility(ESlateVisibility::HitTestInvisible);PreviewPanel->AddChild(PreviewImage);
    Image(BP,Canvas,TEXT("TitleImage"),40,28,320,48,TEXT("/Game/CollectionPrototype/UI/MI_CatalogueTitle"));
    auto* Close=Button(BP,Canvas,TEXT("CloseButton"),1508,25,72,75);auto* Back=Make<UImage>(BP,TEXT("BackImage"));Back->SetBrushFromMaterial(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/UI/MI_CatalogueBack")));Back->SetVisibility(ESlateVisibility::HitTestInvisible);Content(Close,Back);
    auto* Help=Text(BP,Canvas,TEXT("PreviewHelp"),TEXT("拖动旋转   滚轮缩放   双击复位"),280,698,550,24,13,FLinearColor(.7f,.74f,.72f));Help->SetJustification(ETextJustify::Center);
    auto* Bar=Make<UCanvasPanel>(BP,TEXT("CollectionBar"));Place(Canvas,Bar,40,720,1030,110);
    for(int32 I=0;I<4;++I)
    {
        auto* B=Button(BP,Bar,FString::Printf(TEXT("ItemButton%d"),I),61+I*228,4,222,98);
        auto* SlotCanvas=Make<UCanvasPanel>(BP,FString::Printf(TEXT("ItemContent%d"),I));Content(B,SlotCanvas);
        auto* Selected=Image(BP,SlotCanvas,FString::Printf(TEXT("ItemSelected%d"),I),-2,-2,226,102);Selected->SetRenderShear(FVector2D(-.15f,0));if(I!=0)Selected->SetVisibility(ESlateVisibility::Hidden);
        Image(BP,SlotCanvas,FString::Printf(TEXT("ItemArt%d"),I),0,0,222,98,TEXT("/Game/CollectionPrototype/UI/MI_CatalogueUnknown"));
        Text(BP,SlotCanvas,FString::Printf(TEXT("ItemLabel%d"),I),FString(),6,31,205,38,23,FLinearColor(.005f,.01f,.005f))->SetJustification(ETextJustify::Center);
    }
    auto Arrow=[&](const TCHAR* Name,const TCHAR* Label,float X){auto* B=Button(BP,Bar,Name,X,22,50,57);auto* T=Make<UTextBlock>(BP,FString(Name)+TEXT("Label"));T->SetText(FText::FromString(Label));T->SetFont(BrainMiningFonts::Body(32));T->SetJustification(ETextJustify::Center);T->SetColorAndOpacity(FLinearColor(.6f,.62f,.62f));T->SetVisibility(ESlateVisibility::HitTestInvisible);Content(B,T);};
    Arrow(TEXT("PreviousButton"),TEXT("❮"),5);Arrow(TEXT("NextButton"),TEXT("❯"),975);
    FAssetRegistryModule::AssetCreated(BP);FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
    if(BP->Status==BS_Error||!BP->GeneratedClass)return false;
    auto* Defaults=CastChecked<UUserWidget>(BP->GeneratedClass->GetDefaultObject());Defaults->DesignTimeSize=FVector2D(1600,900);Defaults->DesignSizeMode=EDesignPreviewSizeMode::Custom;
    BP->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
    return UPackage::SavePackage(Package,BP,*FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension()),Args);
}

bool UCollectionUMGEditorLibrary::ExerciseInspectPointer(UMiningCollectionWidget* Widget,bool bZoom)
{
    if(!Widget||!Widget->GetWorld()||Widget->GetWorld()->WorldType!=EWorldType::PIE)return false;
    auto* Panel=Widget->GetWidgetFromName(TEXT("PreviewPanel"));if(!Panel)return false;
    const auto& G=Widget->GetCachedGeometry();const auto& P=Panel->GetCachedGeometry();
    const FVector2D Start=P.LocalToAbsolute(P.GetLocalSize()*.5f),End=P.LocalToAbsolute(P.GetLocalSize()*.5f+FVector2D(65,25));
    auto Slate=Widget->TakeWidget();TSet<FKey> Buttons;Buttons.Add(EKeys::LeftMouseButton);
    if(bZoom){FPointerEvent Wheel(0,Start,Start,TSet<FKey>(),EKeys::Invalid,1,FModifierKeysState());return Slate->OnMouseWheel(G,Wheel).IsEventHandled();}
    FPointerEvent Down(0,Start,Start,Buttons,EKeys::LeftMouseButton,0,FModifierKeysState());
    const bool Handled=Slate->OnMouseButtonDown(G,Down).IsEventHandled();
    FPointerEvent Move(0,End,Start,Buttons,EKeys::Invalid,0,FModifierKeysState());Slate->OnMouseMove(G,Move);
    FPointerEvent Up(0,End,End,TSet<FKey>(),EKeys::LeftMouseButton,0,FModifierKeysState());Slate->OnMouseButtonUp(G,Up);
    return Handled;
}
UWidget* UCollectionUMGEditorLibrary::FindInspectControl(UMiningCollectionWidget* Widget,FName Name){return Widget?Widget->GetWidgetFromName(Name):nullptr;}
