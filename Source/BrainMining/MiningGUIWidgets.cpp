#include "MiningGUIWidgets.h"
#include "MiningComponent.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
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
}

void UMiningGUIJetpackHeatWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	HeatMaterial = HeatBar ? HeatBar->GetDynamicMaterial() : nullptr;
	SetHeatState(PreviewHeat, bPreviewOverheated);
}

void UMiningGUIJetpackHeatWidget::SetHeatState(float Heat, bool bOverheated)
{
	DisplayedHeat = FMath::Clamp(Heat, 0.f, 1.f);
	bDisplayedOverheated = bOverheated;
	if (HeatMaterial)
	{
		HeatMaterial->SetScalarParameterValue(TEXT("Heat"), DisplayedHeat);
		HeatMaterial->SetScalarParameterValue(TEXT("Overheated"), bOverheated ? 1.f : 0.f);
	}
}

void UMiningGUIAbilityWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (AbilityIcon && (bUsed ? UsedTexture : NormalTexture))
		AbilityIcon->SetBrushFromTexture(bUsed ? UsedTexture : NormalTexture, false);
	SetVisibility(bAvailable ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SetCooldownRemaining(CooldownRemaining);
}
void UMiningGUIAbilityWidget::SetUsed(bool bNewUsed)
{
	if (bUsed == bNewUsed) return;
	bUsed = bNewUsed;
	if (AbilityIcon && (bUsed ? UsedTexture : NormalTexture))
		AbilityIcon->SetBrushFromTexture(bUsed ? UsedTexture : NormalTexture, false);
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
	if (CoinButton) CoinButton->SetIsEnabled(bUnlocked && bInteractionAllowed);
}
void UMiningGUIUpgradeWidget::SetUpgradeLevel(int32 NewLevel, bool bAnimate)
{
	const int32 Previous = UpgradeLevel;
	UpgradeLevel = FMath::Clamp(NewLevel, 0, 3);
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
	if (!bUnlocked || !bInteractionAllowed) return;
	ClickSpring.Target = 1.f;
	ClickSpring.Velocity = 2.8f;
	const int32 RequestedLevel = FMath::Min(UpgradeLevel + 1, 3);
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
	return Index == 0 ? PickaxeUpgrade.Get() : Index == 1 ? VortexUpgrade.Get() : Index == 2 ? BlastUpgrade.Get() : nullptr;
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
	for (int32 Index = 0; Index < 3; ++Index)
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
	if (JetpackHeatBar)
	{
		const auto* Pawn = GetOwningPlayerPawn();
		const auto* Flight = Pawn ? Pawn->FindComponentByClass<UMiningCharacterMovementComponent>() : nullptr;
		JetpackHeatBar->SetHeatState(Flight ? Flight->JetpackHeat : 0.f, Flight && Flight->IsJetpackOverheated());
	}
	if (AbilityV)
	{
		const APawn* Pawn = GetOwningPlayerPawn();
		const UMiningComponent* Mining = Pawn ? Pawn->FindComponentByClass<UMiningComponent>() : nullptr;
		const float Cooldown = Mining ? Mining->GetDrillCooldownRemaining() : 0.f;
		AbilityV->SetUsed(Cooldown > 0);
		AbilityV->SetCooldownRemaining(Cooldown);
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
