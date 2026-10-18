#include "MiningGUIWidgets.h"
#include "MiningBombToolComponent.h"
#include "MiningComponent.h"
#include "MiningAudioComponent.h"
#include "MiningProgressionComponent.h"
#include "MiningPickaxeSettings.h"
#include "MiningWalletSubsystem.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/Texture2D.h"
#include "BrainMiningPlayerController.h"
#include "GameFramework/Pawn.h"
#include "MiningCharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	void Visible(UWidget* Widget, bool bVisible)
	{
		if (Widget) Widget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	void Scale(UWidget* Widget, float Value)
	{
		if (Widget) Widget->SetRenderScale(FVector2D(Value));
	}
	void AbilityArtwork(UImage* Image, UTexture2D* Texture)
	{
		if (!Image || !Texture) return;
		// Keep the designer's edge treatment when switching between normal/used art.
		if (auto* Material = Image->GetDynamicMaterial()) Material->SetTextureParameterValue(TEXT("Artwork"), Texture);
		else Image->SetBrushFromTexture(Texture, false);
	}
}

void UMiningGUIJetpackHeatWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	HeatMaterial = HeatBar ? HeatBar->GetDynamicMaterial() : nullptr;
	SetHeatState(PreviewHeat, bPreviewOverheated);
	SetRenderOpacity(IsDesignTime() ? 1.f : 0.f);
}

void UMiningGUIJetpackHeatWidget::NativeConstruct()
{
	Super::NativeConstruct();
	GaugeFade = IdleSeconds = GaugeOpacity = WarningOpacity = 0.f;
	SetRenderOpacity(0.f);
}

void UMiningGUIJetpackHeatWidget::SetHeatState(float Heat, bool bOverheated, bool bThrustActive)
{
	DisplayedHeat = FMath::Clamp(Heat, 0.f, 1.f);
	bDisplayedOverheated = bOverheated;
	bDisplayedThrustActive = bThrustActive;
	if (HeatMaterial)
	{
		HeatMaterial->SetScalarParameterValue(TEXT("Heat"), DisplayedHeat);
		HeatMaterial->SetScalarParameterValue(TEXT("Overheated"), bOverheated ? 1.f : 0.f);
		HeatMaterial->SetScalarParameterValue(TEXT("WarningThreshold"), FMath::Clamp(WarningThreshold, .1f, .95f));
	}
}

void UMiningGUIJetpackHeatWidget::AdvancePresentation(float DeltaSeconds)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) return;
	const bool bActive = bDisplayedThrustActive || bDisplayedOverheated || DisplayedHeat > KINDA_SMALL_NUMBER;
	IdleSeconds = bActive ? 0.f : IdleSeconds + DeltaSeconds;
	const bool bShow = bActive || (GaugeFade > 0.f && IdleSeconds < FMath::Max(0.f, HideDelaySeconds));
	const float Duration = FMath::Max(.01f, bShow ? GaugeFadeInSeconds : GaugeFadeOutSeconds);
	GaugeFade = FMath::FInterpConstantTo(GaugeFade, bShow ? 1.f : 0.f, DeltaSeconds, 1.f / Duration);
	GaugeOpacity = FMath::SmoothStep(0.f, 1.f, GaugeFade);
	SetRenderOpacity(GaugeOpacity);
	// Heat warnings stay on the gauge. The full-screen red wash reads as damage.
	WarningOpacity = 0.f;
}

void UMiningGUIAbilityWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (AbilityIcon && (bUsed ? UsedTexture : NormalTexture))
		AbilityArtwork(AbilityIcon, bUsed ? UsedTexture : NormalTexture);
	SetVisibility(bAvailable ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SetCooldownRemaining(CooldownRemaining);
}
void UMiningGUIAbilityWidget::SetUsed(bool bNewUsed)
{
	if (bUsed == bNewUsed) return;
	bUsed = bNewUsed;
	if (AbilityIcon && (bUsed ? UsedTexture : NormalTexture))
		AbilityArtwork(AbilityIcon, bUsed ? UsedTexture : NormalTexture);
	Pulse.Value = 0.84f;
	Pulse.Velocity = 3.f;
}
void UMiningGUIAbilityWidget::SetCooldownRemaining(float Seconds)
{
	CooldownRemaining = FMath::IsFinite(Seconds) ? FMath::Max(0.f, Seconds) : 0.f;
	if (CooldownText)
	{
		CooldownText->SetVisibility(CooldownRemaining > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		CooldownText->SetText(CooldownRemaining > 0 ? FText::AsNumber(FMath::CeilToInt(CooldownRemaining)) : FText::GetEmpty());
	}
}
void UMiningGUIAbilityWidget::SetAvailable(bool bNewAvailable)
{
	bAvailable = bNewAvailable;
	SetVisibility(bAvailable ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
void UMiningGUIAbilityWidget::SetRechargeState(bool bRecharging, float Progress)
{
	SetUsed(bRecharging);
	SetCooldownRemaining(0.f);
	if (AbilityIcon) AbilityIcon->SetColorAndOpacity(bRecharging ? FLinearColor(1.f,.16f,.12f,1.f) : FLinearColor::White);
	if (!RechargeBar && AbilityContent && WidgetTree)
	{
		RechargeBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("BombRecharge"));
		RechargeBar->SetBarFillType(EProgressBarFillType::BottomToTop);
		FProgressBarStyle Style;
		Style.SetBackgroundImage(FSlateColorBrush(FLinearColor(.08f,.012f,.012f,.9f)));
		Style.SetFillImage(FSlateColorBrush(FLinearColor::White));
		RechargeBar->SetWidgetStyle(Style);
		RechargeBar->SetFillColorAndOpacity(FLinearColor(1.f,.12f,.07f,1.f));
		auto* BarSlot = AbilityContent->AddChildToCanvas(RechargeBar);
		BarSlot->SetPosition(FVector2D(104.f,0.f));
		BarSlot->SetSize(FVector2D(9.f,100.f));
	}
	if (RechargeBar)
	{
		RechargeBar->SetPercent(FMath::IsFinite(Progress) ? FMath::Clamp(Progress,0.f,1.f) : 0.f);
		RechargeBar->SetVisibility(bRecharging ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
void UMiningGUIAbilityWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	Pulse.Step(DeltaTime, SpringStiffness, SpringDamping);
	Scale(AbilityContent, Pulse.Value);
}

void UMiningGUIUpgradeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	CoinButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestUpgrade);
	CoinButton->OnPressed.AddUniqueDynamic(this, &ThisClass::PressCoin);
	CoinButton->OnReleased.AddUniqueDynamic(this, &ThisClass::ReleaseCoin);
}
void UMiningGUIUpgradeWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshVisuals();
}
void UMiningGUIUpgradeWidget::RefreshVisuals()
{
	Visible(RingGrey, bUnlocked);
	Visible(ItemIcon, bUnlocked);
	Visible(LockedRing, !bUnlocked);
	Visible(LockIcon, !bUnlocked);
	Visible(GreenSegment1, bUnlocked && UpgradeLevel >= 1);
	Visible(GreenSegment2, bUnlocked && UpgradeLevel >= 2);
	Visible(GreenSegment3, bUnlocked && UpgradeLevel >= 3);
	Visible(GreenSegment4, bUnlocked && UpgradeLevel >= 4);
	if (CoinButton) CoinButton->SetIsEnabled(bUnlocked && bInteractionAllowed && UpgradeLevel < 4);
}
void UMiningGUIUpgradeWidget::SetUpgradeLevel(int32 NewLevel, bool bAnimate)
{
	const int32 Previous = UpgradeLevel;
	UpgradeLevel = FMath::Clamp(NewLevel, 0, 4);
	RefreshVisuals();
	if (bAnimate && Previous != UpgradeLevel) { RingSpring.Value = 0.91f; RingSpring.Velocity = 3.2f; }
	if (Previous != UpgradeLevel) OnLevelChanged.Broadcast(ItemIndex, UpgradeLevel);
}
void UMiningGUIUpgradeWidget::SetUnlocked(bool bNewUnlocked)
{
	bUnlocked = bNewUnlocked;
	if (!bUnlocked) ClickSpring = FMiningGUISpring();
	RefreshVisuals();
}
void UMiningGUIUpgradeWidget::SetPriceDisplay(FText NewText) { if (PriceText) PriceText->SetText(NewText); }
void UMiningGUIUpgradeWidget::RequestUpgrade()
{
	if (!bUnlocked || !bInteractionAllowed || UpgradeLevel >= 4) return;
	ClickSpring.Target = 1.f;
	ClickSpring.Velocity = 2.8f;
	const int32 RequestedLevel = FMath::Min(UpgradeLevel + 1, 4);
	OnUpgradeRequested.Broadcast(ItemIndex, RequestedLevel);
	if (bPreviewUpgrades) SetUpgradeLevel(RequestedLevel, true);
}
void UMiningGUIUpgradeWidget::SetInteractionAllowed(bool bAllowed)
{
	bInteractionAllowed = bAllowed;
	if (!bAllowed) ClickSpring = FMiningGUISpring();
	RefreshVisuals();
}
void UMiningGUIUpgradeWidget::PressCoin() { if (bUnlocked && bInteractionAllowed) { ClickSpring.Target = 0.86f; ClickSpring.Velocity = -2.f; } }
void UMiningGUIUpgradeWidget::ReleaseCoin() { if (bUnlocked && bInteractionAllowed) { ClickSpring.Target = 1.f; ClickSpring.Velocity = 2.8f; } }
void UMiningGUIUpgradeWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	ClickSpring.Step(DeltaTime, SpringStiffness, SpringDamping);
	RingSpring.Step(DeltaTime, SpringStiffness, SpringDamping);
	Scale(PriceContent, ClickSpring.Value);
	Scale(RingContent, RingSpring.Value);
}

void UMiningGUIHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (JetpackHeatWarning)
	{
		JetpackHeatWarning->SetRenderOpacity(0.f);
		JetpackHeatWarning->SetVisibility(ESlateVisibility::Collapsed);
		JetpackHeatWarning->RemoveFromParent();
	}
	DrillEnergyFade = 0.f;
	if (DrillEnergyBar)
	{
		DrillEnergyBar->SetRenderOpacity(0.f);
		Visible(DrillEnergyBar, false);
	}
	SetIsFocusable(true);
	// The supplied plus artwork remains; Tab owns the popup instead of mouse toggling.
	PlusButton->SetVisibility(ESlateVisibility::HitTestInvisible);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (auto* Card = Item(Index))
		{
			Card->ItemIndex = Index;
			Card->SetInteractionAllowed(false);
			Card->OnUpgradeRequested.AddUniqueDynamic(this, &ThisClass::ForwardUpgrade);
			Card->OnLevelChanged.AddUniqueDynamic(this, &ThisClass::ForwardLevel);
		}
	}
	PanelSpring.Value = PanelSpring.Target = 0.f;
	bUpgradePanelOpen = false;
	UpgradePanel->SetVisibility(ESlateVisibility::Collapsed);
}

FReply UMiningGUIHUDWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Tab)
	{
		if (auto* Controller = GetOwningPlayer<ABrainMiningPlayerController>()) Controller->SetUpgradeMenuHeld(true);
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply UMiningGUIHUDWidget::NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Tab)
	{
		if (auto* Controller = GetOwningPlayer<ABrainMiningPlayerController>()) Controller->SetUpgradeMenuHeld(false);
		return FReply::Handled();
	}
	return Super::NativeOnKeyUp(Geometry, Event);
}
UMiningGUIUpgradeWidget* UMiningGUIHUDWidget::Item(int32 Index) const
{
	return Index == 0 ? PickaxeUpgrade.Get() : Index == 1 ? VortexUpgrade.Get() : Index == 2 ? BlastUpgrade.Get() : Index == 3 ? DrillUpgrade.Get() : nullptr;
}

void UMiningGUIHUDWidget::EnableProgressionPanel()
{
	if (AbilityG && AbilityG->AbilityContent && !AbilityG->CooldownText)
	{
		AbilityG->CooldownText = WidgetTree->ConstructWidget<UTextBlock>();
		AbilityG->CooldownText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 18));
		AbilityG->CooldownText->SetColorAndOpacity(FLinearColor::White);
		AbilityG->CooldownText->SetJustification(ETextJustify::Center);
		AbilityG->CooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* CountSlot = AbilityG->AbilityContent->AddChildToCanvas(AbilityG->CooldownText);
		CountSlot->SetPosition(FVector2D(0, -32));
		CountSlot->SetSize(FVector2D(100, 28));
	}
    if (const auto* Pawn = GetOwningPlayerPawn())
        if (const auto* Progression = Pawn->FindComponentByClass<UMiningProgressionComponent>(); Progression && Progression->UsesMineralInventory())
        { RefreshMineralProgression(); return; }
	if (DrillUpgrade || !PickaxeUpgrade || !UpgradePanel) return;
	DrillUpgrade = CreateWidget<UMiningGUIUpgradeWidget>(GetOwningPlayer(), PickaxeUpgrade->GetClass());
	if (!DrillUpgrade) return;
	UpgradePanel->AddChildToCanvas(DrillUpgrade);
	DrillUpgrade->ItemIndex = 3;
	DrillUpgrade->OnUpgradeRequested.AddUniqueDynamic(this, &ThisClass::ForwardUpgrade);
	DrillUpgrade->OnLevelChanged.AddUniqueDynamic(this, &ThisClass::ForwardLevel);
	if (AbilityV && AbilityV->NormalTexture) DrillUpgrade->ItemIcon->SetBrushFromTexture(AbilityV->NormalTexture);
	if (auto* PanelSlot = Cast<UCanvasPanelSlot>(UpgradePanel->Slot)) PanelSlot->SetSize(FVector2D(724, 540));
	auto* Credits = WidgetTree->ConstructWidget<UTextBlock>();
	Credits->SetText(FText::FromString(TEXT("Music: Dreamy Flashback — Kevin MacLeod (incompetech.com)\nCC BY 4.0 — creativecommons.org/licenses/by/4.0/\nSource: incompetech.com/music/royalty-free/index.html?isrc=USUAN1100532\nEdited: 48 kHz conversion, loop crossfade and volume adjustment.\nSound effects: Kenney / rubberduck — CC0; project-synthesized tool sounds\n音量设置：松开 Tab 后按 F8；关闭时自动保存")));
	Credits->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 12));
	Credits->SetColorAndOpacity(FLinearColor(.8f,.84f,.87f));
	Credits->SetAutoWrapText(true);
	Credits->SetVisibility(ESlateVisibility::HitTestInvisible);
	auto* CreditSlot = UpgradePanel->AddChildToCanvas(Credits);
	CreditSlot->SetPosition(FVector2D(0, 390));
	CreditSlot->SetSize(FVector2D(724, 150));
	for (int32 Index = 0; Index < 4; ++Index)
	{
		auto* Card = Item(Index);
		Card->bPreviewUpgrades = false;
		Card->SetUnlocked(true);
		Card->SetInteractionAllowed(bUpgradePanelOpen);
		if (auto* CardSlot = Cast<UCanvasPanelSlot>(Card->Slot))
		{
			CardSlot->SetPosition(FVector2D(Index*184, 32));
			CardSlot->SetSize(FVector2D(148, 208));
		}
		auto PriceFont = Card->PriceText->GetFont(); PriceFont.Size = 16; Card->PriceText->SetFont(PriceFont);
		if (auto* ButtonSlot = Cast<UCanvasPanelSlot>(Card->CoinButton->Slot))
		{
			ButtonSlot->SetPosition(FVector2D(0, 168));
			ButtonSlot->SetSize(FVector2D(148, 60));
		}
		if (auto* TextSlot = Cast<UCanvasPanelSlot>(Card->PriceText->Slot))
		{
			TextSlot->SetPosition(FVector2D(54, 14));
			TextSlot->SetSize(FVector2D(90, 30));
		}
		if (auto* Background = Cast<UImage>(Card->GetWidgetFromName(TEXT("PriceBackground"))))
		{
			if (auto* BackgroundSlot = Cast<UCanvasPanelSlot>(Background->Slot)) BackgroundSlot->SetSize(FVector2D(148, 60));
		}
		for (int32 Row = 0; Row < 2; ++Row)
		{
			auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
			Label->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), Row == 0 ? 22 : 17));
			Label->SetColorAndOpacity(FLinearColor::White);
			Label->SetJustification(ETextJustify::Center);
			Label->SetAutoWrapText(true);
			Label->SetVisibility(ESlateVisibility::HitTestInvisible);
			auto* LabelSlot = UpgradePanel->AddChildToCanvas(Label);
			LabelSlot->SetPosition(FVector2D(Index*184-10, Row == 0 ? 0 : 272));
			LabelSlot->SetSize(FVector2D(168, Row == 0 ? 30 : 110));
			(Row == 0 ? UpgradeNames : UpgradeDescriptions).Add(Label);
		}
	}
}

void UMiningGUIHUDWidget::SetUpgradeDetails(int32 Index, FText Name, FText Description, int64 Price, bool bAffordable)
{
    if (bMineralProgression) return;
	if (auto* Card = Item(Index))
	{
		Card->SetPriceDisplay(Price > 0 ? FText::AsNumber(Price) : FText::FromString(TEXT("MAX")));
		Card->PriceText->SetColorAndOpacity(Price > 0 && !bAffordable ? FLinearColor(1,.4f,.3f) : FLinearColor::White);
		Card->CoinButton->SetIsEnabled(bUpgradePanelOpen && Card->bUnlocked && Card->UpgradeLevel < 4 && bAffordable && Price > 0);
		if (UpgradeNames.IsValidIndex(Index)) UpgradeNames[Index]->SetText(Name);
		if (UpgradeDescriptions.IsValidIndex(Index)) UpgradeDescriptions[Index]->SetText(FText::FromString(
			FString::Printf(TEXT("%d / 4\n%s"), Card->UpgradeLevel, *Description.ToString())));
	}
}
UMiningGUIAbilityWidget* UMiningGUIHUDWidget::AbilityWidget(FName Name) const
{
	return Name == TEXT("G") ? AbilityG.Get() : Name == TEXT("V") ? AbilityV.Get() : nullptr;
}
void UMiningGUIHUDWidget::ToggleUpgradePanel() { SetUpgradePanelOpen(!bUpgradePanelOpen); }
void UMiningGUIHUDWidget::SetUpgradePanelOpen(bool bOpen)
{
    if (bMineralProgression) return;
	if (bUpgradePanelOpen == bOpen) return;
	bUpgradePanelOpen = bOpen;
	if (auto* Pawn = GetOwningPlayerPawn())
		if (auto* Audio = Pawn->FindComponentByClass<UMiningAudioComponent>()) Audio->PlayPanel(bOpen);
	for (int32 Index = 0; Index < 4; ++Index)
		if (auto* Card = Item(Index)) Card->SetInteractionAllowed(bOpen);
	PanelSpring.Target = bOpen ? 1.f : 0.f;
	UpgradePanel->SetVisibility(bOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::HitTestInvisible);
	OnPanelChanged.Broadcast(bOpen);
}
void UMiningGUIHUDWidget::SetCoinDisplay(FText NewText) { if (CoinText) CoinText->SetText(NewText); }
void UMiningGUIHUDWidget::SetItemLevel(int32 Index, int32 Level, bool bAnimate) { if (auto* Card = Item(Index)) Card->SetUpgradeLevel(Level, bAnimate); }
void UMiningGUIHUDWidget::SetItemUnlocked(int32 Index, bool bUnlocked) { if (auto* Card = Item(Index)) Card->SetUnlocked(bUnlocked); }
void UMiningGUIHUDWidget::SetAbilityUsed(FName Name, bool bUsed) { if (auto* Widget = AbilityWidget(Name)) Widget->SetUsed(bUsed); }
void UMiningGUIHUDWidget::SetAbilityAvailable(FName Name, bool bAvailable) { if (auto* Widget = AbilityWidget(Name)) Widget->SetAvailable(bAvailable); }
void UMiningGUIHUDWidget::TriggerAbility(FName Name)
{
	if (auto* Widget = AbilityWidget(Name); Widget && Widget->bAvailable)
	{
		OnAbilityRequested.Broadcast(Name);
		if (bPreviewAbilities && Name != TEXT("V")) Widget->SetUsed(true);
	}
}
void UMiningGUIHUDWidget::PressPlus() { PlusSpring.Target = 0.82f; PlusSpring.Velocity = -2.f; }
void UMiningGUIHUDWidget::ReleasePlus() { PlusSpring.Target = 1.f; PlusSpring.Velocity = 3.f; }
void UMiningGUIHUDWidget::ForwardUpgrade(int32 Index, int32 Level) { OnUpgradeRequested.Broadcast(Index, Level); }
void UMiningGUIHUDWidget::ForwardLevel(int32 Index, int32 Level) { OnLevelChanged.Broadcast(Index, Level); }
void UMiningGUIHUDWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	const auto* TouchController = GetOwningPlayer<ABrainMiningPlayerController>();
	const APawn* AimPawn = GetOwningPlayerPawn();
	const auto* AimMining = AimPawn ? AimPawn->FindComponentByClass<UMiningComponent>() : nullptr;
	const auto* AimBomb = AimPawn ? AimPawn->FindComponentByClass<UMiningBombToolComponent>() : nullptr;
	const auto* AimController = GetOwningPlayer();
	const bool bShowAim = AimMining && AimController && !AimController->IsPaused()
		&& !bUpgradePanelOpen && !AimMining->IsInputSuspended()
		&& !AimController->bShowMouseCursor && !AimController->IsLookInputIgnored()
		&& AimController->GetViewTarget() == AimPawn;
	const int32 AimTool = AimBomb && AimBomb->bEquipped ? 2 : AimMining && AimMining->bDrillMode ? 1 : 0;
	const FName ReticleNames[] = { TEXT("ReticlePickaxe"), TEXT("ReticleDrill"), TEXT("ReticleBomb") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ReticleNames); ++Index)
		Visible(GetWidgetFromName(ReticleNames[Index]), bShowAim && AimTool == Index);
	if (DrillEnergyBar)
	{
		const bool bShowEnergy = bShowAim && AimMining && AimMining->bUseDrillEnergy && AimMining->bDrillMode;
		// Match the jetpack's short eased fade, keeping the image visible until fade-out ends.
		DrillEnergyFade = FMath::FInterpConstantTo(DrillEnergyFade, bShowEnergy ? 1.f : 0.f,
			DeltaTime, 1.f / (bShowEnergy ? .16f : .35f));
		DrillEnergyBar->SetRenderOpacity(FMath::SmoothStep(0.f, 1.f, DrillEnergyFade));
		Visible(DrillEnergyBar, DrillEnergyFade > KINDA_SMALL_NUMBER);
		if (auto* EnergyMaterial = DrillEnergyBar->GetDynamicMaterial())
			EnergyMaterial->SetScalarParameterValue(TEXT("Energy"), AimMining ?
				(AimMining->bInfiniteDrillEnergy ? 1.f : FMath::Clamp(AimMining->DrillEnergy / FMath::Max(1.f, AimMining->MaxDrillEnergy), 0.f, 1.f)) : 0.f);
	}
	if (JetpackHeatBar)
	{
		const auto* Pawn = GetOwningPlayerPawn();
		const auto* Flight = Pawn ? Pawn->FindComponentByClass<UMiningCharacterMovementComponent>() : nullptr;
		const bool bHeatEnabled = Flight && Flight->FlightSettings.bEnabled && Flight->FlightSettings.bEnableHeat;
		JetpackHeatBar->SetHeatState(bHeatEnabled ? Flight->JetpackHeat : 0.f, bHeatEnabled && Flight->IsJetpackOverheated(), bHeatEnabled && Flight->IsFlightThrustActive());
		JetpackHeatBar->AdvancePresentation(DeltaTime);
	}
	if (AbilityG)
	{
		const APawn* Pawn = GetOwningPlayerPawn();
		const auto* Bomb = Pawn ? Pawn->FindComponentByClass<UMiningBombToolComponent>() : nullptr;
		AbilityG->SetAvailable(Bomb && Bomb->IsSkillUnlocked());
		SetItemUnlocked(2, Bomb && Bomb->IsSkillUnlocked());
		AbilityG->SetRechargeState(Bomb && Bomb->bUseInventory && Bomb->BombCount == 0, Bomb ? Bomb->RechargeProgress : 0.f);
	}
	if (AbilityV)
	{
		const APawn* Pawn = GetOwningPlayerPawn();
		const UMiningComponent* Mining = Pawn ? Pawn->FindComponentByClass<UMiningComponent>() : nullptr;
		AbilityV->SetAvailable(Mining && Mining->IsDrillUnlocked());
		SetItemUnlocked(3, Mining && Mining->IsDrillUnlocked());
		const float Cooldown = Mining ? Mining->GetDrillCooldownRemaining() : 0.f;
		AbilityV->SetUsed(Cooldown > 0);
		AbilityV->SetCooldownRemaining(Cooldown);
		if (Mining && Mining->bUseDrillEnergy)
		{
			AbilityV->SetCooldownRemaining(0.f);
			AbilityV->SetUsed(!Mining->bInfiniteDrillEnergy && Mining->DrillEnergy <= UE_SMALL_NUMBER);
		}
	}
	if (TouchController && TouchController->IsUsingTouchControls())
	{
		if (AbilityG) AbilityG->SetVisibility(ESlateVisibility::Collapsed);
		if (AbilityV) AbilityV->SetVisibility(ESlateVisibility::Collapsed);
	}
	PlusSpring.Step(DeltaTime, 420.f, 19.f);
	Scale(PlusContent, PlusSpring.Value);
	PanelSpring.Step(DeltaTime, PanelSpringStiffness, PanelSpringDamping);
	if (bMineralProgression) RefreshMineralProgression();
	if (UpgradePanel && !bMineralProgression)
	{
		const float Progress = PanelSpring.Value;
		Scale(UpgradePanel, FMath::Max(0.01f, 0.58f + 0.42f * Progress));
		UpgradePanel->SetRenderOpacity(FMath::Clamp(Progress * 2.f, 0.f, 1.f));
		UpgradePanel->SetRenderTranslation(FVector2D(0.f, -30.f * (1.f - Progress)));
		if (!bUpgradePanelOpen && Progress == 0.f) UpgradePanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMiningGUIHUDWidget::RefreshMineralProgression()
{
    const auto* Pawn = GetOwningPlayerPawn();
    const auto* Progression = Pawn ? Pawn->FindComponentByClass<UMiningProgressionComponent>() : nullptr;
    const auto* Wallet = Progression ? Progression->GetWallet() : nullptr;
    if (!Wallet || !Wallet->UsesMineralInventory()) return;
    const EMiningMineral Types[] = {EMiningMineral::Copper, EMiningMineral::Iron, EMiningMineral::Malachite, EMiningMineral::Diamond};
    if (!MineralPanel)
    {
        auto* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget);
        if (!Root || !PickaxeUpgrade) return;
        bMineralProgression = true;
        bUpgradePanelOpen = false;
        if (auto* Currency = GetWidgetFromName(TEXT("CurrencyPanel"))) Currency->SetVisibility(ESlateVisibility::Collapsed);
        UpgradePanel->SetVisibility(ESlateVisibility::Collapsed);
        MineralPanel = WidgetTree->ConstructWidget<UCanvasPanel>();
        MineralPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
        auto* PanelSlot = Root->AddChildToCanvas(MineralPanel);
        PanelSlot->SetAnchors(FAnchors(1,0)); PanelSlot->SetAlignment(FVector2D(1,0));
        PanelSlot->SetPosition(FVector2D(-32,24)); PanelSlot->SetSize(FVector2D(280,570));
        const auto AddLabel = [&](const FString& Text, FVector2D Position, FVector2D Size, int32 FontSize)
        {
            auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
            auto Font = CoinText->GetFont(); Font.Size = FontSize;
            Label->SetFont(Font); Label->SetText(FText::FromString(Text));
            Label->SetColorAndOpacity(FLinearColor(.94f,.97f,1));
            Label->SetShadowOffset(FVector2D(1,2)); Label->SetShadowColorAndOpacity(FLinearColor(0,0,0,.9f));
            auto* LabelSlot = MineralPanel->AddChildToCanvas(Label); LabelSlot->SetPosition(Position); LabelSlot->SetSize(Size);
            return Label;
        };
        AddLabel(TEXT("矿石背包"), FVector2D(0,0), FVector2D(280,28), 20);
        const FLinearColor Colors[] = {FLinearColor(.9f,.43f,.19f), FLinearColor(.72f,.09f,.045f), FLinearColor(.16f,.78f,.43f), FLinearColor(.4f,.88f,1)};
        for (int32 I = 0; I < 4; ++I)
        {
            // A small faceted crystal glyph, independent of platform emoji fonts.
            for (int32 Facet = 0; Facet < 2; ++Facet)
            {
                auto* Icon = WidgetTree->ConstructWidget<UImage>();
                FSlateBrush Brush; Brush.TintColor = Facet == 0 ? Colors[I] : Colors[I] * 1.35f;
                Icon->SetBrush(Brush); Icon->SetRenderTransformAngle(45);
                auto* IconSlot = MineralPanel->AddChildToCanvas(Icon);
                IconSlot->SetPosition(FVector2D(Facet == 0 ? 7 : 10, 42+I*34));
                IconSlot->SetSize(Facet == 0 ? FVector2D(16,16) : FVector2D(6,6));
            }
            AddLabel(FMiningMineralCost::Name(Types[I]), FVector2D(36,36+I*34), FVector2D(150,30), 20);
            auto* Count = AddLabel(TEXT("0"), FVector2D(198,36+I*34), FVector2D(75,30), 20);
            Count->SetJustification(ETextJustify::Right); MineralCounts.Add(Count);
        }
        PickaxeUpgrade->RemoveFromParent();
        auto* CardSlot = MineralPanel->AddChildToCanvas(PickaxeUpgrade);
        CardSlot->SetPosition(FVector2D(66,190)); CardSlot->SetSize(FVector2D(148,160));
        PickaxeUpgrade->bPreviewUpgrades = false; PickaxeUpgrade->SetInteractionAllowed(false);
        PickaxeUpgrade->SetUnlocked(true); PickaxeUpgrade->CoinButton->SetVisibility(ESlateVisibility::Collapsed);
        PickaxeLevelLabel = AddLabel(TEXT(""), FVector2D(0,355), FVector2D(280,32), 22);
        PickaxeLevelLabel->SetJustification(ETextJustify::Center);
        PickaxeUpgradeHint = AddLabel(TEXT(""), FVector2D(0,391), FVector2D(280,30), 18);
        PickaxeUpgradeHint->SetJustification(ETextJustify::Center);
        PickaxeRequirements = AddLabel(TEXT("升级材料"), FVector2D(20,427), FVector2D(250,24), 17);
        PickaxeRequirements->SetJustification(ETextJustify::Center);
        for (int32 Row=0; Row<4; ++Row)
        {
            auto* Requirement=AddLabel(TEXT(""), FVector2D(20,451+Row*25), FVector2D(250,24), 17);
            Requirement->SetJustification(ETextJustify::Center);
            PickaxeRequirementRows.Add(Requirement);
        }
    }
    const auto SetText = [](UTextBlock* Label, const FText& Text)
    { if (Label && !Label->GetText().EqualTo(Text)) Label->SetText(Text); };
    for (int32 I = 0; I < 4; ++I) SetText(MineralCounts[I], FText::AsNumber(Wallet->GetMineralCount(Types[I])));
    const int32 Level = FMath::Max(0, Progression->GetLevel(0));
    PickaxeUpgrade->SetUpgradeLevel(Level, true);
    SetText(PickaxeLevelLabel, FText::FromString(FString::Printf(TEXT("等级 %d"), Level)));
    PickaxeRequirements->SetVisibility(Level<4 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    const auto Costs=UMiningPickaxeSettings::ResolveLevel(Level+1).MaterialCosts;
    for (int32 Row=0; Row<PickaxeRequirementRows.Num(); ++Row)
    {
        UTextBlock* Label=PickaxeRequirementRows[Row].Get();
        if (!Label) continue;
        if (Level<4 && Costs.IsValidIndex(Row))
        {
            const auto& Cost=Costs[Row];
            const int32 Owned=Wallet->GetMineralCount(Cost.Mineral);
            SetText(Label,FText::FromString(FString::Printf(TEXT("%s  %d / %d"),*FMiningMineralCost::Name(Cost.Mineral),Owned,Cost.Count)));
            Label->SetColorAndOpacity(Owned>=Cost.Count ? FLinearColor(.25f,1.f,.45f) : FLinearColor(.82f,.85f,.9f));
        }
        else SetText(Label,FText::GetEmpty());
    }
    const bool bReady = Progression->CanUpgradePickaxe();
    SetText(PickaxeUpgradeHint, FText::FromString(Level >= 4 ? TEXT("矿镐已满级") : bReady ? TEXT("[ Tab ] 升级矿镐") : TEXT("[ Tab ] 升级 · 材料不足")));
    PickaxeUpgradeHint->SetColorAndOpacity(bReady ? FLinearColor(.35f,1,.65f) : FLinearColor(.85f,.87f,.9f));
}
