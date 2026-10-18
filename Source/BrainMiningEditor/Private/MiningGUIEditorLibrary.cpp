#include "MiningGUIEditorLibrary.h"
#include "EndingEmergence.h"
#include "MiningExperienceSubsystem.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "BrainMiningFonts.h"
#include "MiningGUIWidgets.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"
#include "Materials/MaterialInterface.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Styling/CoreStyle.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Widgets/Input/SButton.h"
#include "Editor.h"
#include "Engine/GameViewportClient.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Widgets/SWindow.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"

bool UMiningGUIEditorLibrary::PrepareEndingCreditsPreview(APlayerController* Controller, const TArray<int32>& Inventory)
{
    UWorld* World=Controller?Controller->GetWorld():nullptr;
    if(!World || World->WorldType!=EWorldType::PIE)return false;
    auto* Session=World->GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>();
    for(TActorIterator<AEndingEmergence> It(World);It;++It)
    {
        if(!Session)return false;
        Session->CampaignCollection=Inventory;It->bComplete=true;return true;
    }
    return false;
}

namespace MiningGUIEditor
{
	const FString Folder = TEXT("/Game/UI/MiningGUI/");
	UTexture2D* Texture(const TCHAR* Name)
	{
		return LoadObject<UTexture2D>(nullptr, *(Folder + TEXT("Art/") + Name + TEXT(".") + Name));
	}
	template<typename T> T* Widget(UWidgetBlueprint* BP, const TCHAR* Name)
	{
		T* Result = BP->WidgetTree->ConstructWidget<T>(T::StaticClass(), FName(Name));
		Result->bIsVariable = true;
		if (!BP->WidgetVariableNameToGuidMap.Contains(Result->GetFName()))
			BP->WidgetVariableNameToGuidMap.Add(Result->GetFName(), FGuid::NewGuid());
		Result->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Result->SetRenderTransformPivot(FVector2D(0.5f));
		return Result;
	}
	void Place(UCanvasPanel* Parent, UWidget* Child, FVector2D Position, FVector2D Size,
		FVector2D Anchor = FVector2D::ZeroVector, FVector2D Alignment = FVector2D::ZeroVector)
	{
		auto* Slot = Parent->AddChildToCanvas(Child);
		Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		Slot->SetAlignment(Alignment);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
	}
	UImage* Image(UWidgetBlueprint* BP, UCanvasPanel* Parent, const TCHAR* Name, const TCHAR* Art,
		FVector2D Position, FVector2D Size)
	{
		auto* Result = Widget<UImage>(BP, Name);
		Result->SetBrushFromTexture(Texture(Art));
		Result->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(Parent, Result, Position, Size);
		return Result;
	}
	UButton* Button(UWidgetBlueprint* BP, UCanvasPanel* Parent, const TCHAR* Name, FVector2D Position, FVector2D Size)
	{
		auto* Result = Widget<UButton>(BP, Name);
		Result->SetVisibility(ESlateVisibility::Visible);
		// Focus stays with the game: clicking an upgrade never steals WASD or G/V.
		FindFProperty<FBoolProperty>(UButton::StaticClass(), TEXT("IsFocusable"))->SetPropertyValue_InContainer(Result, false);
		FButtonStyle Style;
		FSlateBrush Transparent;
		Transparent.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.SetNormal(Transparent).SetHovered(Transparent).SetPressed(Transparent).SetDisabled(Transparent);
		Style.SetNormalPadding(FMargin(0.f)).SetPressedPadding(FMargin(0.f));
		Result->SetStyle(Style);
		Place(Parent, Result, Position, Size);
		return Result;
	}
	UTextBlock* Text(UWidgetBlueprint* BP, UCanvasPanel* Parent, const TCHAR* Name, const TCHAR* Value,
		FVector2D Position, FVector2D Size, int32 FontSize)
	{
		auto* Result = Widget<UTextBlock>(BP, Name);
		Result->SetText(FText::FromString(Value));
		Result->SetFont(BrainMiningFonts::Body(FontSize));
		Result->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Result->SetJustification(ETextJustify::Center);
		Result->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(Parent, Result, Position, Size);
		return Result;
	}
	UWidgetBlueprint* NewBlueprint(const TCHAR* Name, UClass* Parent, bool& bCreated)
	{
		const FString PackageName = Folder + Name;
		if (auto* Existing = LoadObject<UWidgetBlueprint>(nullptr, *(PackageName + TEXT(".") + Name)))
		{
			bCreated = false;
			return Existing;
		}
		auto* Package = CreatePackage(*PackageName);
		auto* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = Parent;
		auto* BP = Cast<UWidgetBlueprint>(Factory->FactoryCreateNew(UWidgetBlueprint::StaticClass(), Package,
			FName(Name), RF_Public | RF_Standalone, nullptr, GWarn));
		bCreated = BP != nullptr;
		if (BP)
		{
			BP->WidgetTree->RootWidget = Widget<UCanvasPanel>(BP, TEXT("RootCanvas"));
			FAssetRegistryModule::AssetCreated(BP);
		}
		return BP;
	}
	bool Complete(UWidgetBlueprint* BP, FVector2D DesignSize)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		FKismetEditorUtilities::CompileBlueprint(BP);
		if (BP->Status == BS_Error || !BP->GeneratedClass) return false;
		auto* Defaults = Cast<UUserWidget>(BP->GeneratedClass->GetDefaultObject());
		Defaults->DesignTimeSize = DesignSize;
		Defaults->DesignSizeMode = EDesignPreviewSizeMode::Custom;
		BP->MarkPackageDirty();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(BP->GetOutermost(), BP,
			*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
	}
	UWidgetBlueprint* Ability(const TCHAR* Name, const TCHAR* Key)
	{
		bool bNew = false;
		auto* BP = NewBlueprint(Name, UMiningGUIAbilityWidget::StaticClass(), bNew);
		if (!BP || !bNew) return BP;
		auto* Root = CastChecked<UCanvasPanel>(BP->WidgetTree->RootWidget);
		auto* Content = Widget<UCanvasPanel>(BP, TEXT("AbilityContent"));
		Place(Root, Content, FVector2D::ZeroVector, FVector2D(100.f, 138.f));
		Image(BP, Content, TEXT("AbilityIcon"), *(FString(TEXT("T_Ability")) + Key), FVector2D::ZeroVector, FVector2D(100.f));
		Image(BP, Content, TEXT("Keycap"), *(FString(TEXT("T_Key")) + Key), FVector2D(31.f, 108.f), FVector2D(38.f, 25.f));
		FKismetEditorUtilities::CompileBlueprint(BP);
		auto* Defaults = CastChecked<UMiningGUIAbilityWidget>(BP->GeneratedClass->GetDefaultObject());
		Defaults->Ability = FName(Key);
		Defaults->NormalTexture = Texture(*(FString(TEXT("T_Ability")) + Key));
		Defaults->UsedTexture = Texture(*(FString(TEXT("T_Ability")) + Key + TEXT("Used")));
		return Complete(BP, FVector2D(100.f, 138.f)) ? BP : nullptr;
	}
	UWidgetBlueprint* Card(const TCHAR* Name, const TCHAR* Icon, int32 Index, bool bUnlocked)
	{
		bool bNew = false;
		auto* BP = NewBlueprint(Name, UMiningGUIUpgradeWidget::StaticClass(), bNew);
		if (!BP || !bNew) return BP;
		auto* Root = CastChecked<UCanvasPanel>(BP->WidgetTree->RootWidget);
		auto* Ring = Widget<UCanvasPanel>(BP, TEXT("RingContent"));
		Place(Root, Ring, FVector2D::ZeroVector, FVector2D(148.f));
		Image(BP, Ring, TEXT("RingGrey"), TEXT("T_RingGrey"), FVector2D::ZeroVector, FVector2D(148.f));
		Image(BP, Ring, TEXT("ItemIcon"), Icon, FVector2D::ZeroVector, FVector2D(148.f));
		Image(BP, Ring, TEXT("LockedRing"), TEXT("T_RingLocked"), FVector2D::ZeroVector, FVector2D(148.f))->SetVisibility(bUnlocked ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
		Image(BP, Ring, TEXT("LockIcon"), TEXT("T_IconLocked"), FVector2D::ZeroVector, FVector2D(148.f))->SetVisibility(bUnlocked ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
		Image(BP, Ring, TEXT("GreenSegment1"), TEXT("T_RingGreen1"), FVector2D::ZeroVector, FVector2D(148.f))->SetVisibility(ESlateVisibility::Hidden);
		Image(BP, Ring, TEXT("GreenSegment2"), TEXT("T_RingGreen2"), FVector2D::ZeroVector, FVector2D(148.f))->SetVisibility(ESlateVisibility::Hidden);
		Image(BP, Ring, TEXT("GreenSegment3"), TEXT("T_RingGreen3"), FVector2D::ZeroVector, FVector2D(148.f))->SetVisibility(ESlateVisibility::Hidden);
		if (!bUnlocked) { Ring->GetChildAt(0)->SetVisibility(ESlateVisibility::Hidden); Ring->GetChildAt(1)->SetVisibility(ESlateVisibility::Hidden); }
		auto* Coin = Button(BP, Root, TEXT("CoinButton"), FVector2D(28.f, 168.f), FVector2D(92.f, 36.f));
		auto* Price = Widget<UCanvasPanel>(BP, TEXT("PriceContent"));
		Coin->AddChild(Price);
		Image(BP, Price, TEXT("PriceBackground"), TEXT("T_PriceButton"), FVector2D::ZeroVector, FVector2D(92.f, 36.f));
		Text(BP, Price, TEXT("PriceText"), TEXT(""), FVector2D(28.f, 1.f), FVector2D(58.f, 30.f), 24);
		FKismetEditorUtilities::CompileBlueprint(BP);
		auto* Defaults = CastChecked<UMiningGUIUpgradeWidget>(BP->GeneratedClass->GetDefaultObject());
		Defaults->ItemIndex = Index;
		Defaults->bUnlocked = bUnlocked;
		return Complete(BP, FVector2D(148.f, 208.f)) ? BP : nullptr;
	}
	template<typename T> T* Child(UWidgetBlueprint* BP, UCanvasPanel* Parent, UWidgetBlueprint* ChildBP,
		const TCHAR* Name, FVector2D Position, FVector2D Size)
	{
		auto* Result = BP->WidgetTree->ConstructWidget<T>(ChildBP->GeneratedClass.Get(), FName(Name));
		Result->bIsVariable = true;
		Result->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Place(Parent, Result, Position, Size);
		return Result;
	}
}

bool UMiningGUIEditorLibrary::UpgradeGUIToFourLevels()
{
	using namespace MiningGUIEditor;
	const FString MaterialFolder = Folder + TEXT("FourLevel/");
	auto Material = [&](const FString& Name)
	{
		return LoadObject<UMaterialInterface>(nullptr, *(MaterialFolder + Name + TEXT(".") + Name));
	};
	auto* White = Material(TEXT("MI_UpgradeRingWhite"));
	auto* Locked = Material(TEXT("MI_UpgradeRingLocked"));
	if (!White || !Locked) return false;
	for (const TCHAR* Name : {TEXT("WBP_UpgradePickaxe"), TEXT("WBP_UpgradeVortex"), TEXT("WBP_UpgradeBlast")})
	{
		auto* BP = LoadObject<UWidgetBlueprint>(nullptr, *(Folder + Name + TEXT(".") + Name));
		if (!BP || !BP->WidgetTree) return false;
		auto* Ring = Cast<UCanvasPanel>(BP->WidgetTree->FindWidget(TEXT("RingContent")));
		auto* GreyImage = Cast<UImage>(BP->WidgetTree->FindWidget(TEXT("RingGrey")));
		auto* LockedImage = Cast<UImage>(BP->WidgetTree->FindWidget(TEXT("LockedRing")));
		auto* First = Cast<UImage>(BP->WidgetTree->FindWidget(TEXT("GreenSegment1")));
		auto* TemplateSlot = First ? Cast<UCanvasPanelSlot>(First->Slot) : nullptr;
		if (!Ring || !GreyImage || !LockedImage || !TemplateSlot) return false;
		GreyImage->SetBrushFromMaterial(White);
		LockedImage->SetBrushFromMaterial(Locked);
		for (int32 Index = 1; Index <= 4; ++Index)
		{
			const FString SegmentName = FString::Printf(TEXT("GreenSegment%d"), Index);
			auto* SegmentMaterial = Material(FString::Printf(TEXT("MI_UpgradeRingGreen%d"), Index));
			if (!SegmentMaterial) return false;
			auto* Segment = Cast<UImage>(BP->WidgetTree->FindWidget(FName(*SegmentName)));
			if (!Segment)
			{
				Segment = Widget<UImage>(BP, *SegmentName);
				Place(Ring, Segment, TemplateSlot->GetPosition(), TemplateSlot->GetSize());
				auto* Slot = CastChecked<UCanvasPanelSlot>(Segment->Slot);
				Slot->SetAnchors(TemplateSlot->GetAnchors());
				Slot->SetAlignment(TemplateSlot->GetAlignment());
				Slot->SetZOrder(TemplateSlot->GetZOrder());
				Segment->SetVisibility(ESlateVisibility::Hidden);
			}
			Segment->SetBrushFromMaterial(SegmentMaterial);
		}
		const FVector2D Size = CastChecked<UUserWidget>(BP->GeneratedClass->GetDefaultObject())->DesignTimeSize;
		if (!Complete(BP, Size)) return false;
	}
	return true;
}

bool UMiningGUIEditorLibrary::AddDrillCooldownOverlay()
{
	using namespace MiningGUIEditor;
	auto* BP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/UI/MiningGUI/WBP_AbilityV.WBP_AbilityV"));
	if (!BP || !BP->WidgetTree) return false;
	if (BP->WidgetTree->FindWidget(TEXT("CooldownText")))
	{
		if (!BP->WidgetVariableNameToGuidMap.Contains(TEXT("CooldownText")))
			BP->WidgetVariableNameToGuidMap.Add(TEXT("CooldownText"), FGuid::NewGuid());
		return Complete(BP, FVector2D(100, 138));
	}
	auto* Content = Cast<UCanvasPanel>(BP->WidgetTree->FindWidget(TEXT("AbilityContent")));
	if (!Content) return false;
	auto* Overlay = Text(BP, Content, TEXT("CooldownText"), TEXT(""), FVector2D(0, 23), FVector2D(100, 54), 32);
	auto Font = Overlay->GetFont();
	Font.OutlineSettings.OutlineSize = 2;
	Font.OutlineSettings.OutlineColor = FLinearColor(.035f, .015f, .01f, 1);
	Overlay->SetFont(Font);
	CastChecked<UCanvasPanelSlot>(Overlay->Slot)->SetZOrder(10);
	return Complete(BP, FVector2D(100, 138));
}

bool UMiningGUIEditorLibrary::AddJetpackHeatOverlay()
{
	using namespace MiningGUIEditor;
	auto* HUD = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/UI/MiningGUI/WBP_MiningHUD.WBP_MiningHUD"));
	auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/UI/MiningGUI/MI_JetpackHeat.MI_JetpackHeat"));
	if (!HUD || !HUD->WidgetTree || !Material) return false;
	bool bNew = false;
	auto* BP = NewBlueprint(TEXT("WBP_JetpackHeat"), UMiningGUIJetpackHeatWidget::StaticClass(), bNew);
	if (!BP) return false;
	if (bNew)
	{
		auto* Root = CastChecked<UCanvasPanel>(BP->WidgetTree->RootWidget);
		auto* Bar = Widget<UImage>(BP, TEXT("HeatBar"));
		Bar->SetBrushFromMaterial(Material);
		Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(Root, Bar, FVector2D::ZeroVector, FVector2D(360.f, 26.f));
		if (!Complete(BP, FVector2D(360.f, 26.f))) return false;
	}
	if (!HUD->WidgetTree->FindWidget(TEXT("JetpackHeatBar")))
	{
		auto* Root = Cast<UCanvasPanel>(HUD->WidgetTree->RootWidget);
		auto* Abilities = HUD->WidgetTree->FindWidget(TEXT("AbilityPanel"));
		auto* Slot = Abilities ? Cast<UCanvasPanelSlot>(Abilities->Slot) : nullptr;
		if (!Root || !Slot) return false;
		// Add underneath the existing icons; never move the artist's ability panel.
		auto* Bar = Child<UMiningGUIJetpackHeatWidget>(HUD, Root, BP, TEXT("JetpackHeatBar"),
			Slot->GetPosition() + FVector2D(0.f, 9.f), FVector2D(360.f, 26.f));
		auto* BarSlot = CastChecked<UCanvasPanelSlot>(Bar->Slot);
		BarSlot->SetAnchors(Slot->GetAnchors());
		BarSlot->SetAlignment(FVector2D(0.f, 0.f));
		BarSlot->SetZOrder(Slot->GetZOrder());
	}
	return Complete(HUD, FVector2D(1920.f, 1080.f));
}

bool UMiningGUIEditorLibrary::UpgradeJetpackToArcGauge()
{
	using namespace MiningGUIEditor;
	auto* HUD = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/UI/MiningGUI/WBP_MiningHUD.WBP_MiningHUD"));
	auto* BP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/UI/MiningGUI/WBP_JetpackHeat.WBP_JetpackHeat"));
	auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/UI/MiningGUI/JetpackGauge/MI_JetpackArc.MI_JetpackArc"));
	auto* WarningArt = Texture(TEXT("T_JetpackHeatWarning"));
	if (!HUD || !HUD->WidgetTree || !BP || !BP->WidgetTree || !Material || !WarningArt) return false;
	auto* Bar = Cast<UImage>(BP->WidgetTree->FindWidget(TEXT("HeatBar")));
	auto* Root = Cast<UCanvasPanel>(HUD->WidgetTree->RootWidget);
	auto* ChildWidget = HUD->WidgetTree->FindWidget(TEXT("JetpackHeatBar"));
	auto* ChildSlot = ChildWidget ? Cast<UCanvasPanelSlot>(ChildWidget->Slot) : nullptr;
	if (!Bar || !Root || !ChildSlot) return false;
	Bar->SetBrushFromMaterial(Material);
	CastChecked<UCanvasPanelSlot>(Bar->Slot)->SetPosition(FVector2D::ZeroVector);
	CastChecked<UCanvasPanelSlot>(Bar->Slot)->SetSize(FVector2D(210.f));
	Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!Complete(BP, FVector2D(210.f))) return false;
	// Position beside the crosshair. Designer slot remains the source of runtime placement.
	ChildSlot->SetAnchors(FAnchors(.35f, .5f));
	ChildSlot->SetAlignment(FVector2D(.5f));
	ChildSlot->SetPosition(FVector2D::ZeroVector);
	ChildSlot->SetSize(FVector2D(210.f));
	ChildSlot->SetZOrder(30);
	auto* Overlay = Cast<UImage>(HUD->WidgetTree->FindWidget(TEXT("JetpackHeatWarning")));
	if (!Overlay)
	{
		Overlay = Widget<UImage>(HUD, TEXT("JetpackHeatWarning"));
		Root->AddChildToCanvas(Overlay);
	}
	Overlay->SetBrushFromTexture(WarningArt);
	Overlay->SetRenderOpacity(0.f);
	Overlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	auto* OverlaySlot = CastChecked<UCanvasPanelSlot>(Overlay->Slot);
	OverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	OverlaySlot->SetOffsets(FMargin(0.f));
	OverlaySlot->SetAlignment(FVector2D::ZeroVector);
	OverlaySlot->SetZOrder(-100); // Below controls; never intercepts pointer or Tab.
	return Complete(HUD, FVector2D(1920.f, 1080.f));
}

bool UMiningGUIEditorLibrary::AddDrillEnergyBar()
{
	using namespace MiningGUIEditor;
	auto* HUD = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/UI/MiningGUI/WBP_MiningHUD.WBP_MiningHUD"));
	auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/UI/MiningGUI/DrillEnergy/M_DrillEnergy.M_DrillEnergy"));
	if (!HUD || !HUD->WidgetTree || !Material) return false;
	auto* Root = Cast<UCanvasPanel>(HUD->WidgetTree->RootWidget);
	if (!Root) return false;
	auto* Bar = Cast<UImage>(HUD->WidgetTree->FindWidget(TEXT("DrillEnergyBar")));
	if (!Bar)
	{
		Bar = Widget<UImage>(HUD, TEXT("DrillEnergyBar"));
		Place(Root, Bar, FVector2D(-38.f, 0.f), FVector2D(56.f, 196.f), FVector2D(1.f, .5f), FVector2D(1.f, .5f));
		CastChecked<UCanvasPanelSlot>(Bar->Slot)->SetZOrder(30);
	}
	Bar->SetBrushFromMaterial(Material);
	Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Complete(HUD, FVector2D(1920.f, 1080.f));
}

bool UMiningGUIEditorLibrary::CreateMiningGUIAssets()
{
	using namespace MiningGUIEditor;
	if (!Texture(TEXT("T_Plus")) || !Texture(TEXT("T_RingGreen3"))) return false;
	auto* G = Ability(TEXT("WBP_AbilityG"), TEXT("G"));
	auto* V = Ability(TEXT("WBP_AbilityV"), TEXT("V"));
	auto* Pickaxe = Card(TEXT("WBP_UpgradePickaxe"), TEXT("T_IconPickaxe"), 0, true);
	auto* Vortex = Card(TEXT("WBP_UpgradeVortex"), TEXT("T_IconVortex"), 1, true);
	auto* Blast = Card(TEXT("WBP_UpgradeBlast"), TEXT("T_IconBlast"), 2, false);
	if (!G || !V || !Pickaxe || !Vortex || !Blast) return false;
	bool bNew = false;
	auto* BP = NewBlueprint(TEXT("WBP_MiningHUD"), UMiningGUIHUDWidget::StaticClass(), bNew);
	if (!BP || !bNew) return BP != nullptr;
	auto* Root = CastChecked<UCanvasPanel>(BP->WidgetTree->RootWidget);
	auto* Currency = Widget<UCanvasPanel>(BP, TEXT("CurrencyPanel"));
	Place(Root, Currency, FVector2D(-32.f, 24.f), FVector2D(232.f, 64.f), FVector2D(1.f, 0.f), FVector2D(1.f, 0.f));
	Image(BP, Currency, TEXT("CoinBar"), TEXT("T_CoinBar"), FVector2D::ZeroVector, FVector2D(220.f, 64.f));
	Text(BP, Currency, TEXT("CoinText"), TEXT(""), FVector2D(61.f, 16.f), FVector2D(130.f, 36.f), 26);
	auto* Plus = Button(BP, Currency, TEXT("PlusButton"), FVector2D(190.f, 6.f), FVector2D(48.f));
	auto* PlusContent = Widget<UCanvasPanel>(BP, TEXT("PlusContent"));
	Plus->AddChild(PlusContent);
	Image(BP, PlusContent, TEXT("PlusIcon"), TEXT("T_Plus"), FVector2D::ZeroVector, FVector2D(48.f));
	auto* Panel = Widget<UCanvasPanel>(BP, TEXT("UpgradePanel"));
	Panel->SetRenderTransformPivot(FVector2D(1.f, 0.f));
	Place(Root, Panel, FVector2D(-36.f, 120.f), FVector2D(492.f, 208.f), FVector2D(1.f, 0.f), FVector2D(1.f, 0.f));
	Child<UMiningGUIUpgradeWidget>(BP, Panel, Pickaxe, TEXT("PickaxeUpgrade"), FVector2D(0.f, 0.f), FVector2D(148.f, 208.f));
	Child<UMiningGUIUpgradeWidget>(BP, Panel, Vortex, TEXT("VortexUpgrade"), FVector2D(172.f, 0.f), FVector2D(148.f, 208.f));
	Child<UMiningGUIUpgradeWidget>(BP, Panel, Blast, TEXT("BlastUpgrade"), FVector2D(344.f, 0.f), FVector2D(148.f, 208.f));
	auto* Abilities = Widget<UCanvasPanel>(BP, TEXT("AbilityPanel"));
	Place(Root, Abilities, FVector2D(64.f, -40.f), FVector2D(222.f, 138.f), FVector2D(0.f, 1.f), FVector2D(0.f, 1.f));
	Child<UMiningGUIAbilityWidget>(BP, Abilities, G, TEXT("AbilityG"), FVector2D::ZeroVector, FVector2D(100.f, 138.f));
	Child<UMiningGUIAbilityWidget>(BP, Abilities, V, TEXT("AbilityV"), FVector2D(122.f, 0.f), FVector2D(100.f, 138.f));
	return Complete(BP, FVector2D(1920.f, 1080.f));
}

void UMiningGUIEditorLibrary::SendPreviewKey(APlayerController* Controller, FName Key, bool bPressed)
{
	if (Controller && Controller->GetWorld()->WorldType == EWorldType::PIE)
		Controller->InputKey(FInputKeyEventArgs::CreateSimulated(FKey(Key), bPressed ? IE_Pressed : IE_Released, bPressed ? 1.f : 0.f));
}

bool UMiningGUIEditorLibrary::SendPreviewFocusedKey(APlayerController* Controller, FName Key, bool bPressed)
{
	if (!Controller || Controller->GetWorld()->WorldType != EWorldType::PIE || !FSlateApplication::IsInitialized()) return false;
	const FKeyEvent Event(FKey(Key), FModifierKeysState(), 0, false, 0, 0);
	return bPressed ? FSlateApplication::Get().ProcessKeyDownEvent(Event) : FSlateApplication::Get().ProcessKeyUpEvent(Event);
}

void UMiningGUIEditorLibrary::SendPreviewMouseAxis(APlayerController* Controller, float DeltaX, float DeltaY)
{
	if (!Controller || Controller->GetWorld()->WorldType != EWorldType::PIE) return;
	Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseX, IE_Axis, DeltaX, 1));
	Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseY, IE_Axis, DeltaY, 1));
}

bool UMiningGUIEditorLibrary::SimulatePreviewButton(UButton* Button, bool bPressed)
{
	if (!Button || !Button->GetIsEnabled() || !FSlateApplication::IsInitialized() || !Button->GetCachedWidget().IsValid()) return false;
	if (!Button->GetWorld() || Button->GetWorld()->WorldType != EWorldType::PIE) return false;
	auto& Slate = FSlateApplication::Get();
	const auto& Geometry = Button->GetCachedGeometry();
	const FVector2D Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
	// Check real hit testing, then use Slate's supported automation click entry point.
	// Routing an OS pointer reentrantly from a post-tick Python callback is unsafe.
	const FWidgetPath Hit = Slate.LocateWindowUnderMouse(Position, Slate.GetInteractiveTopLevelWindows(), false);
	if (!Hit.IsValid() || !Hit.ContainsWidget(Button->GetCachedWidget().Get())) return false;
	if (bPressed) Button->OnPressed.Broadcast();
	else
	{
		Button->OnReleased.Broadcast();
		StaticCastSharedPtr<SButton>(Button->GetCachedWidget())->SimulateClick();
	}
	return true;
}

void UMiningGUIEditorLibrary::StartFloatingPreview()
{
	if (!GEditor || GEditor->PlayWorld || IsRunningCommandlet()) return;
	FRequestPlaySessionParams Params;
	Params.EditorPlaySettings = DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage());
	Params.EditorPlaySettings->NewWindowWidth = 1600;
	Params.EditorPlaySettings->NewWindowHeight = 900;
	Params.bAllowOnlineSubsystem = false;
	// No DestinationSlateViewport: use an independent game window with normal UMG.
	GEditor->RequestPlaySession(Params);
}

bool UMiningGUIEditorLibrary::RepairMissingGUIFonts()
{
	using namespace MiningGUIEditor;
	UFont* Font = BrainMiningFonts::Asset();
	if (!Font) return false;
	for (const TCHAR* Name : {TEXT("WBP_UpgradePickaxe"), TEXT("WBP_UpgradeVortex"), TEXT("WBP_UpgradeBlast"), TEXT("WBP_MiningHUD")})
	{
		auto* BP = LoadObject<UWidgetBlueprint>(nullptr, *(Folder + Name + TEXT(".") + Name));
		if (!BP || !BP->WidgetTree) return false;
		bool bChanged = false;
		BP->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (auto* Label = Cast<UTextBlock>(Widget); Label && !Label->GetFont().FontObject)
			{
				Label->SetFont(BrainMiningFonts::Body(Label->GetName() == TEXT("CoinText") ? 26 : 24));
				bChanged = true;
			}
		});
		if (bChanged)
		{
			const FVector2D Size = CastChecked<UUserWidget>(BP->GeneratedClass->GetDefaultObject())->DesignTimeSize;
			if (!Complete(BP, Size)) return false;
		}
	}
	return true;
}

bool UMiningGUIEditorLibrary::CapturePreview(APlayerController* Controller, const FString& Filename)
{
	if (!Controller || Controller->GetWorld()->WorldType != EWorldType::PIE) return false;
	UGameViewportClient* Client = Controller->GetWorld()->GetGameViewport();
	if (!Client || !Client->GetWindow().IsValid() || !FSlateApplication::IsInitialized()) return false;
	FIntVector Size;
	TArray<FColor> Pixels;
	if (!FSlateApplication::Get().TakeScreenshot(Client->GetWindow().ToSharedRef(), Pixels, Size)) return false;
	for (FColor& Pixel : Pixels) Pixel.A = 255;
	TArray64<uint8> Compressed;
	FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Compressed);
	return FFileHelper::SaveArrayToFile(Compressed, *Filename);
}

bool UMiningGUIEditorLibrary::ClearGUIPlaceholderNumbers()
{
	using namespace MiningGUIEditor;
	for (const TCHAR* Name : {TEXT("WBP_UpgradePickaxe"), TEXT("WBP_UpgradeVortex"), TEXT("WBP_UpgradeBlast"), TEXT("WBP_MiningHUD")})
	{
		auto* BP = LoadObject<UWidgetBlueprint>(nullptr, *(Folder + Name + TEXT(".") + Name));
		if (!BP || !BP->WidgetTree) return false;
		BP->WidgetTree->ForEachWidget([](UWidget* Widget)
		{
			if (auto* Label = Cast<UTextBlock>(Widget); Label && (Label->GetFName() == TEXT("PriceText") || Label->GetFName() == TEXT("CoinText")))
				Label->SetText(FText::GetEmpty());
		});
		const FVector2D Size = CastChecked<UUserWidget>(BP->GeneratedClass->GetDefaultObject())->DesignTimeSize;
		if (!Complete(BP, Size)) return false;
	}
	return true;
}
