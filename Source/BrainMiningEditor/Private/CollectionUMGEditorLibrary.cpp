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
#include "Engine/Texture2D.h"
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
    FSlateBrush Normal;Normal.TintColor=FLinearColor(0,0,0,0);FSlateBrush Hover=Normal;Hover.TintColor=FLinearColor(0,0,0,0);
    Style.SetNormal(Normal).SetHovered(Hover).SetPressed(Hover);Style.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
    B->SetStyle(Style);B->IsFocusable=false;B->SetVisibility(ESlateVisibility::Visible);Place(Parent,B,X,Y,W,H);return B;
}
}
static bool BuildCollectionInspectWidget(bool bReplace)
{
    using namespace CollectionUMG;
    // This is an explicit editor authoring command, never a runtime rebuild.
    const FString PackageName=TEXT("/Game/CollectionPrototype/UI/WBP_CollectionInspect");
    auto* BP=LoadObject<UWidgetBlueprint>(nullptr,*(PackageName+TEXT(".WBP_CollectionInspect")));
    if(BP&&!bReplace)return true;
    UPackage* Package=BP?BP->GetOutermost():CreatePackage(*PackageName);
    if(!BP)
    {
        auto* Factory=NewObject<UWidgetBlueprintFactory>();Factory->ParentClass=UMiningCollectionWidget::StaticClass();
        BP=Cast<UWidgetBlueprint>(Factory->FactoryCreateNew(UWidgetBlueprint::StaticClass(),Package,TEXT("WBP_CollectionInspect"),RF_Public|RF_Standalone,nullptr,GWarn));
        if(!BP)return false;FAssetRegistryModule::AssetCreated(BP);
    }
    else
    {
        if(BP->WidgetTree)BP->WidgetTree->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors|REN_DoNotDirty);
        BP->WidgetTree=NewObject<UWidgetTree>(BP,TEXT("WidgetTree"),RF_Transactional);
        BP->WidgetVariableNameToGuidMap.Reset();
    }
    const auto Art=[](const TCHAR* Name){return LoadObject<UTexture2D>(nullptr,*(FString(TEXT("/Game/CollectionPrototype/UI/Redesign/T_Collection"))+Name));};
    const auto Picture=[&](UCanvasPanel* Parent,const FString& Name,float X,float Y,float W,float H,const TCHAR* Skin)
    {auto* I=Image(BP,Parent,Name,X,Y,W,H);I->SetBrushFromTexture(Art(Skin));return I;};
    const auto IconButton=[&](UCanvasPanel* Parent,const TCHAR* Name,float X,float Y,float W,float H,const TCHAR* Skin,bool Mirror=false)
    {
        auto* B=Button(BP,Parent,Name,X,Y,W,H);FButtonStyle Style;
        FSlateBrush Normal;Normal.SetResourceObject(Art(Skin));Normal.DrawAs=ESlateBrushDrawType::Image;Normal.ImageSize=FVector2D(W,H);
        if(Mirror)Normal.Mirroring=ESlateBrushMirrorType::Horizontal;
        FSlateBrush Hover=Normal,Pressed=Normal;
        Hover.TintColor=FLinearColor::White;Pressed.TintColor=FLinearColor(2.2f,3.f,.65f);
        Style.SetNormal(Normal).SetHovered(Hover).SetPressed(Pressed).SetDisabled(Normal);
        Style.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));B->SetStyle(Style);return B;
    };
    auto* Overlay=Make<UOverlay>(BP,TEXT("ScreenRoot"));BP->WidgetTree->RootWidget=Overlay;Overlay->SetClipping(EWidgetClipping::ClipToBounds);
    auto* Blur=Make<UBackgroundBlur>(BP,TEXT("SceneBlur"));Blur->SetBlurStrength(12);
    auto* BlurSlot=Overlay->AddChildToOverlay(Blur);BlurSlot->SetHorizontalAlignment(HAlign_Fill);BlurSlot->SetVerticalAlignment(VAlign_Fill);
    auto* Dim=Make<UImage>(BP,TEXT("SceneDim"));Dim->SetColorAndOpacity(FLinearColor::Black);Dim->SetRenderOpacity(.48f);Dim->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* DimSlot=Overlay->AddChildToOverlay(Dim);DimSlot->SetHorizontalAlignment(HAlign_Fill);DimSlot->SetVerticalAlignment(VAlign_Fill);
    auto* Fit=Make<UScaleBox>(BP,TEXT("ResponsiveScale"));Fit->SetStretch(EStretch::ScaleToFit);
    auto* FitSlot=Overlay->AddChildToOverlay(Fit);FitSlot->SetHorizontalAlignment(HAlign_Fill);FitSlot->SetVerticalAlignment(VAlign_Fill);
    auto* Size=Make<USizeBox>(BP,TEXT("DesignSize1600x900"));Size->SetWidthOverride(1600);Size->SetHeightOverride(900);Fit->AddChild(Size);
    auto* Canvas=Make<UCanvasPanel>(BP,TEXT("LayoutCanvas"));Size->AddChild(Canvas);
    auto* PreviewPanel=Make<UBorder>(BP,TEXT("PreviewPanel"));PreviewPanel->SetBrushColor(FLinearColor::Transparent);PreviewPanel->SetPadding(FMargin(0));PreviewPanel->SetVisibility(ESlateVisibility::Visible);Place(Canvas,PreviewPanel,70,62,990,620);
    auto* PreviewImage=Make<UImage>(BP,TEXT("PreviewImage"));PreviewImage->SetVisibility(ESlateVisibility::HitTestInvisible);PreviewPanel->AddChild(PreviewImage);
    Text(BP,Canvas,TEXT("CollectionTitle"),TEXT("收藏"),60,30,300,50,22,FLinearColor(.9f,.92f,.86f));
    auto* Info=Make<UCanvasPanel>(BP,TEXT("InfoPanel"));Place(Canvas,Info,1072,0,528,900);
    Picture(Info,TEXT("InfoBackground"),0,0,528,900,TEXT("Info"));
    Text(BP,Info,TEXT("ItemName"),TEXT("化学试剂"),182,363,320,70,32,FLinearColor(.91f,.93f,.86f));
    Image(BP,Info,TEXT("DescriptionDivider"),190,441,118,1,nullptr,FLinearColor(.58f,.62f,.59f));
    auto* Description=Text(BP,Info,TEXT("ItemDescription"),TEXT("加点这个……再加点那个……"),190,465,300,185,15,FLinearColor(.61f,.68f,.64f));Description->SetAutoWrapText(true);
    IconButton(Info,TEXT("CloseButton"),448,44,30,24,TEXT("Back"));
    auto* Help=Text(BP,Canvas,TEXT("PreviewHelp"),TEXT("拖动旋转   滚轮缩放   双击复位"),285,695,540,24,12,FLinearColor(.64f,.69f,.65f));Help->SetJustification(ETextJustify::Center);
    auto* Bar=Make<UCanvasPanel>(BP,TEXT("CollectionBar"));Place(Canvas,Bar,50,728,1010,100);
    for(int32 I=0;I<4;++I)
    {
        auto* B=Button(BP,Bar,FString::Printf(TEXT("ItemButton%d"),I),51+I*228,0,222,98);
        auto* C=Make<UCanvasPanel>(BP,FString::Printf(TEXT("ItemContent%d"),I));Content(B,C);
        Picture(C,FString::Printf(TEXT("ItemArt%d"),I),0,0,222,98,TEXT("Unknown"));
        auto* Selected=Picture(C,FString::Printf(TEXT("ItemSelected%d"),I),0,0,222,98,TEXT("Selected"));if(I!=0)Selected->SetVisibility(ESlateVisibility::Hidden);
        auto* Label=Text(BP,C,FString::Printf(TEXT("ItemLabel%d"),I),FString(),16,24,190,64,18,FLinearColor(.025f,.045f,.025f));Label->SetAutoWrapText(true);Label->SetJustification(ETextJustify::Center);
    }
    IconButton(Bar,TEXT("PreviousButton"),7,29,32,34,TEXT("Arrow"),true);
    IconButton(Bar,TEXT("NextButton"),975,29,32,34,TEXT("Arrow"));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
    if(BP->Status==BS_Error||!BP->GeneratedClass)return false;
    auto* Defaults=CastChecked<UUserWidget>(BP->GeneratedClass->GetDefaultObject());Defaults->DesignTimeSize=FVector2D(1600,900);Defaults->DesignSizeMode=EDesignPreviewSizeMode::Custom;
    BP->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
    return UPackage::SavePackage(Package,BP,*FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension()),Args);
}

bool UCollectionUMGEditorLibrary::CreateCollectionInspectWidget(){return BuildCollectionInspectWidget(false);}
bool UCollectionUMGEditorLibrary::ApplyCollectionInspectRedesign(){return BuildCollectionInspectWidget(true);}

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
