#include "MiningGUIWidgets.h"
#include "MiningBombToolComponent.h"
#include "MiningComponent.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
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
	if (DrillUpgrade || !PickaxeUpgrade || !UpgradePanel) return;
	DrillUpgrade = CreateWidget<UMiningGUIUpgradeWidget>(GetOwningPlayer(), PickaxeUpgrade->GetClass());
	if (!DrillUpgrade) return;
	UpgradePanel->AddChildToCanvas(DrillUpgrade);
	DrillUpgrade->ItemIndex = 3;
	DrillUpgrade->OnUpgradeRequested.AddUniqueDynamic(this, &ThisClass::ForwardUpgrade);
	DrillUpgrade->OnLevelChanged.AddUniqueDynamic(this, &ThisClass::ForwardLevel);
	if (AbilityV && AbilityV->NormalTexture) DrillUpgrade->ItemIcon->SetBrushFromTexture(AbilityV->NormalTexture);
	if (auto* PanelSlot = Cast<UCanvasPanelSlot>(UpgradePanel->Slot)) PanelSlot->SetSize(FVector2D(724, 384));
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
	if (bUpgradePanelOpen == bOpen) return;
	bUpgradePanelOpen = bOpen;
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
		AbilityG->SetUsed(Bomb && Bomb->bEquipped);
		AbilityG->SetCooldownRemaining(0.f);
		if (Bomb && Bomb->bUseInventory && AbilityG->CooldownText)
		{
			AbilityG->CooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
			AbilityG->CooldownText->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), Bomb->BombCount, Bomb->MaxBombs)));
			AbilityG->SetUsed(Bomb->BombCount == 0);
		}
	}
	if (AbilityV)
	{
		const APawn* Pawn = GetOwningPlayerPawn();
		const UMiningComponent* Mining = Pawn ? Pawn->FindComponentByClass<UMiningComponent>() : nullptr;
		const float Cooldown = Mining ? Mining->GetDrillCooldownRemaining() : 0.f;
		AbilityV->SetUsed(Cooldown > 0);
		AbilityV->SetCooldownRemaining(Cooldown);
		if (Mining && Mining->bUseDrillEnergy)
		{
			AbilityV->SetCooldownRemaining(0.f);
			AbilityV->SetUsed(!Mining->bInfiniteDrillEnergy && Mining->DrillEnergy <= UE_SMALL_NUMBER);
		}
	}
	PlusSpring.Step(DeltaTime, 420.f, 19.f);
	Scale(PlusContent, PlusSpring.Value);
	PanelSpring.Step(DeltaTime, PanelSpringStiffness, PanelSpringDamping);
	if (UpgradePanel)
	{
		const float Progress = PanelSpring.Value;
		Scale(UpgradePanel, FMath::Max(0.01f, 0.58f + 0.42f * Progress));
		UpgradePanel->SetRenderOpacity(FMath::Clamp(Progress * 2.f, 0.f, 1.f));
		UpgradePanel->SetRenderTranslation(FVector2D(0.f, -30.f * (1.f - Progress)));
		if (!bUpgradePanelOpen && Progress == 0.f) UpgradePanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}
