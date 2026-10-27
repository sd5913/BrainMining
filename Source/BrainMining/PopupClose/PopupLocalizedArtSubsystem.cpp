#include "PopupClose/PopupLocalizedArtSubsystem.h"
#include "PopupClose/PopupCloseContent.h"
#include "BrainMiningFonts.h"
#include "MiningLocalizationSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "RenderingThread.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif
#include "Slate/WidgetRenderer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void UPopupLocalizedArtSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UMiningLocalizationSubsystem>();
    GetGameInstance()->GetSubsystem<UMiningLocalizationSubsystem>()->OnLanguageChanged.AddUObject(this, &UPopupLocalizedArtSubsystem::RefreshLanguage);
}

void UPopupLocalizedArtSubsystem::Deinitialize()
{
    if (auto* Localization = GetGameInstance()->GetSubsystem<UMiningLocalizationSubsystem>()) Localization->OnLanguageChanged.RemoveAll(this);
    if (Renderer) BeginCleanup(Renderer);
    Renderer = nullptr;
    Materials.Reset();
    Backgrounds.Reset();
    Targets.Reset();
    Super::Deinitialize();
}

UMaterialInterface* UPopupLocalizedArtSubsystem::GetArtwork(int32 Variant)
{
    Variant = (Variant % PopupCloseContent::ArtCount + PopupCloseContent::ArtCount) % PopupCloseContent::ArtCount;
    if (const auto* Existing = Materials.Find(Variant)) return *Existing;
    const auto& Skin = PopupCloseContent::GetArt(Variant);
    auto* Parent = LoadObject<UMaterialInterface>(nullptr, *(FString(TEXT("/Game/PopupClose/PopupArt/M_Popup")) + Skin.AssetName));
    if (!Parent || !Skin.bUseWorkText || !FApp::CanEverRender() || !FSlateApplication::IsInitialized()) return Parent;
    auto* Background = LoadObject<UTexture2D>(nullptr, *(FString(TEXT("/Game/PopupClose/PopupArt/T_Popup")) + Skin.AssetName));
    if (!Background) return Parent;
#if WITH_EDITOR
    FTextureCompilingManager::Get().FinishCompilation({Background});
#endif
    Background->SetForceMipLevelsToBeResident(60.f);
    Background->WaitForStreaming();
    FlushRenderingCommands();
    Backgrounds.Add(Variant, Background);
    Targets.Add(Variant, FWidgetRenderer::CreateTargetFor(Skin.Size * 2.f, TF_Bilinear, false));
    auto* Material = UMaterialInstanceDynamic::Create(Parent, this, FName(*(FString(TEXT("M_PopupLocalized_")) + Skin.AssetName)));
    Material->SetTextureParameterValue(TEXT("PopupArtwork"), Targets[Variant]);
    Materials.Add(Variant, Material);
    RenderArtwork(Variant);
    return Material;
}

void UPopupLocalizedArtSubsystem::RefreshLanguage(const FString& Culture)
{
    for (const auto& Pair : Targets) RenderArtwork(Pair.Key);
}

void UPopupLocalizedArtSubsystem::RenderArtwork(int32 Variant)
{
    if (!Renderer) Renderer = new FWidgetRenderer(false);
    const auto& Skin = PopupCloseContent::GetArt(Variant);
    const FVector4 Region = Skin.TextRect;
    const float Width = Region.Z * Skin.Size.X;
    FSlateBrush Brush;
    Brush.SetResourceObject(Backgrounds[Variant]);
    Brush.ImageSize = Skin.Size;
    const FString Prefix = FString::Printf(TEXT("Office.Physical.%d"), Variant);
    auto Artwork = SNew(SConstraintCanvas)
        + SConstraintCanvas::Slot().Offset(FMargin(0, 0, Skin.Size.X, Skin.Size.Y)).Alignment(FVector2D::ZeroVector)
        [SNew(SImage).Image(&Brush)]
        + SConstraintCanvas::Slot().Offset(FMargin(Region.X * Skin.Size.X, Region.Y * Skin.Size.Y, Width, Region.W * Skin.Size.Y)).Alignment(FVector2D::ZeroVector)
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly).HAlign(HAlign_Left).VAlign(VAlign_Top)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
                [SNew(STextBlock).Text(MiningText::Get(*(Prefix + TEXT(".Title")))).Font(BrainMiningFonts::Title(18)).WrapTextAt(Width).ColorAndOpacity(FLinearColor(.035f, .035f, .04f))]
                + SVerticalBox::Slot().AutoHeight()
                [SNew(STextBlock).Text(MiningText::Get(*(Prefix + TEXT(".Body")))).Font(BrainMiningFonts::Body(Variant == 2 ? 15 : 17)).WrapTextAt(Width).ColorAndOpacity(FLinearColor(.035f, .035f, .04f))]
            ]
        ];
    Renderer->DrawWidget(Targets[Variant], Artwork, 2.f, Skin.Size * 2.f, 0.f);
}
