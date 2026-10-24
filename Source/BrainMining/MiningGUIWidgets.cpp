#include "MiningGUIWidgets.h"
#include "MiningSaveSubsystem.h"
#include "MiningCollection.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "StartRoom/MiningStartRoom.h"
#include "BrainMiningFonts.h"
#include "MiningGamepadSubsystem.h"
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
#include "Materials/MaterialInterface.h"

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
    if (RechargeBar) RechargeBar->SetVisibility(ESlateVisibility::Collapsed);
    if (AbilityIcon)
    {
        AbilityIcon->SetColorAndOpacity(FLinearColor::White);
        if(auto* Material=AbilityIcon->GetDynamicMaterial())
        {
            Material->SetScalarParameterValue(TEXT("RechargeActive"),bRecharging?1.f:0.f);
            Material->SetScalarParameterValue(TEXT("RechargeProgress"),FMath::IsFinite(Progress)?FMath::Clamp(Progress,0.f,1.f):0.f);
        }
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
    CompletionSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/UI/S_TaskComplete.S_TaskComplete"));
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
		AbilityG->CooldownText->SetFont(BrainMiningFonts::Body(18));
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
	Credits->SetFont(BrainMiningFonts::Body(12));
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
			Label->SetFont(Row == 0 ? BrainMiningFonts::Title(22) : BrainMiningFonts::Body(17));
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
    RefreshTutorial(DeltaTime);
    TickCollectionPickup(Geometry,DeltaTime);
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
    const auto SetMineralIcon = [](UImage* Image, EMiningMineral Type)
    {
        if (!Image) return;
        const FLinearColor Colors[] = {
            FLinearColor(.9f,.43f,.19f), FLinearColor(.72f,.09f,.045f),
            FLinearColor(.16f,.78f,.43f), FLinearColor(.4f,.88f,1.f)};
        const int32 Index = Type == EMiningMineral::Copper ? 0 : Type == EMiningMineral::Iron ? 1 : Type == EMiningMineral::Malachite ? 2 : 3;
        FSlateBrush Brush; Brush.DrawAs = ESlateBrushDrawType::Box; Brush.ImageSize = FVector2D(24,24);
        Brush.TintColor = Colors[Index]; Image->SetBrush(Brush); Image->SetRenderTransformAngle(45.f);
    };
    if (!MineralPanel)
    {
        auto* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget);
        if (!Root || !PickaxeUpgrade) return;
        bMineralProgression = true;
        bUpgradePanelOpen = false;
        if (auto* Currency = GetWidgetFromName(TEXT("CurrencyPanel"))) Currency->SetVisibility(ESlateVisibility::Collapsed);
        UpgradePanel->SetVisibility(ESlateVisibility::Collapsed);
        MineralPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MineralHUD"));
        MineralPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
        auto* PanelSlot = Root->AddChildToCanvas(MineralPanel);
        // Stretch the transparent host across the viewport; each compact panel
        // anchors independently to its own top corner through DPI changes.
        PanelSlot->SetAnchors(FAnchors(0,0,1,1));
        PanelSlot->SetOffsets(FMargin(0));

        const auto AddPanel = [&](const TCHAR* Name, FVector2D Size, bool bLeft)
        {
            auto* Panel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name);
            Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
            Panel->SetRenderTransformPivot(FVector2D(bLeft?0:1,0));
            // The two outside edges mirror each other toward the screen centre.
            Panel->SetRenderShear(FVector2D(0,bLeft?2.f:-2.f));
            auto* Slot = MineralPanel->AddChildToCanvas(Panel);
            Slot->SetAnchors(FAnchors(bLeft?0:1,0));
            Slot->SetAlignment(FVector2D(bLeft?0:1,0));
            Slot->SetPosition(FVector2D(bLeft?24:-24,24)); Slot->SetSize(Size);
            return Panel;
        };
        auto* Inventory = AddPanel(TEXT("MineralInventoryPanel"), FVector2D(248,140), true);
        auto* Pickaxe = AddPanel(TEXT("PickaxeSummaryPanel"), FVector2D(340,178), false);
        const auto AddImage = [&](UCanvasPanel* Parent, const TCHAR* Name, const TCHAR* Art, FVector2D Position, FVector2D Size, int32 Z = 0)
        {
            auto* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
            Image->SetVisibility(ESlateVisibility::HitTestInvisible);
            auto* Slot = Parent->AddChildToCanvas(Image);
            Slot->SetPosition(Position); Slot->SetSize(Size); Slot->SetZOrder(Z);
            return Image;
        };
        MineralBackplate = AddImage(Inventory, TEXT("MineralInventoryBackplate"), TEXT(""),
            FVector2D::ZeroVector, FVector2D(248,140), -10);
        PickaxeBackplate = AddImage(Pickaxe, TEXT("PickaxeSummaryBackplate"), TEXT(""),
            FVector2D::ZeroVector, FVector2D(340,178), -10);
        for (auto* Plate : {MineralBackplate.Get(), PickaxeBackplate.Get()})
        {
            if (auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/UI/MiningGUI/Immersive/MI_UpgradeBackplate.MI_UpgradeBackplate"))) Plate->SetBrushFromMaterial(Material);
            if (auto* Material = Plate->GetDynamicMaterial())
            {
                const bool bLeft=Plate==MineralBackplate;
                Material->SetVectorParameterValue(TEXT("PanelSize"), bLeft?FLinearColor(248,140,0,0):FLinearColor(340,178,0,0));
                Material->SetScalarParameterValue(TEXT("OuterSide"), bLeft?0.f:1.f);
                Material->SetScalarParameterValue(TEXT("BackdropOpacity"), .58f);
            }
        }
        const auto AddRule = [&](UCanvasPanel* Parent, const TCHAR* Name, FVector2D Position, FVector2D Size)
        {
            auto* Rule = AddImage(Parent, Name, TEXT(""), Position, Size);
            FSlateBrush Brush; Brush.TintColor = FLinearColor(.72f,.78f,.73f,.25f); Rule->SetBrush(Brush);
        };
        AddRule(Pickaxe, TEXT("PickaxeColumnRule"), FVector2D(128,14), FVector2D(1,148));
        AddRule(Pickaxe, TEXT("PickaxeTabRule"), FVector2D(146,132), FVector2D(178,1));
        const auto AddLabel = [&](UCanvasPanel* Parent, FName Name, const FString& Text, FVector2D Position, FVector2D Size, int32 FontSize, bool bTitle = false)
        {
            auto* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
            Label->SetFont(bTitle ? BrainMiningFonts::Title(FontSize) : BrainMiningFonts::Body(FontSize));
            Label->SetText(FText::FromString(Text));
            Label->SetColorAndOpacity(FLinearColor(.94f,.95f,.92f));
            Label->SetShadowOffset(FVector2D(0,1));
            Label->SetShadowColorAndOpacity(FLinearColor(0,0,0,.45f));
            Label->SetVisibility(ESlateVisibility::HitTestInvisible);
            auto* Slot = Parent->AddChildToCanvas(Label);
            Slot->SetPosition(Position); Slot->SetSize(Size);
            return Label;
        };
        for (int32 I = 0; I < 4; ++I)
        {
            auto* Icon = AddImage(Inventory, *FString::Printf(TEXT("InventoryMineralIcon%d"), I), TEXT(""),
                FVector2D(18,20+I*28), FVector2D(14,14));
            SetMineralIcon(Icon, Types[I]);
            AddLabel(Inventory, *FString::Printf(TEXT("InventoryMineralName%d"), I), FMiningMineralCost::Name(Types[I]),
                FVector2D(46,12+I*28), FVector2D(108,26), 18);
            auto* Count = AddLabel(Inventory, *FString::Printf(TEXT("InventoryMineralCount%d"), I), TEXT("0"),
                FVector2D(156,12+I*28), FVector2D(76,26), 18);
            Count->SetJustification(ETextJustify::Right);
            MineralCounts.Add(Count);
        }
        PickaxeUpgrade->RemoveFromParent();
        auto* CardSlot = Pickaxe->AddChildToCanvas(PickaxeUpgrade);
        CardSlot->SetPosition(FVector2D(16,18)); CardSlot->SetSize(FVector2D(148,148));
        // Keep the shared artwork's 148-unit layout and spring animation, while
        // presenting the complete disc at 65% (roughly 96 units) in this HUD.
        PickaxeUpgrade->SetRenderTransformPivot(FVector2D::ZeroVector);
        PickaxeUpgrade->SetRenderScale(FVector2D(.65f));
        PickaxeUpgrade->bPreviewUpgrades = false;
        PickaxeUpgrade->SetInteractionAllowed(false);
        PickaxeUpgrade->SetUnlocked(true);
        PickaxeUpgrade->CoinButton->SetVisibility(ESlateVisibility::Collapsed);
        PickaxeLevelLabel = AddLabel(Pickaxe, TEXT("PickaxeCurrentLevel"), TEXT(""),
            FVector2D(12,122), FVector2D(104,26), 18, true);
        PickaxeLevelLabel->SetJustification(ETextJustify::Center);
        for (int32 Row = 0; Row < 4; ++Row)
        {
            auto* Icon = AddImage(Pickaxe, *FString::Printf(TEXT("PickaxeRequirementIcon%d"), Row), TEXT(""),
                FVector2D(146,22+Row*28), FVector2D(12,12));
            SetMineralIcon(Icon, Types[Row]);
            PickaxeRequirementIcons.Add(Icon);
            auto* Requirement = AddLabel(Pickaxe, *FString::Printf(TEXT("PickaxeRequirementName%d"), Row), TEXT(""),
                FVector2D(172,14+Row*28), FVector2D(76,26), 17);
            PickaxeRequirementRows.Add(Requirement);
            auto* Count = AddLabel(Pickaxe, *FString::Printf(TEXT("PickaxeRequirementCount%d"), Row), TEXT(""),
                FVector2D(248,14+Row*28), FVector2D(76,26), 17);
            Count->SetJustification(ETextJustify::Right);
            PickaxeRequirementCounts.Add(Count);
        }
        PickaxeUpgradeHint = AddLabel(Pickaxe, TEXT("PickaxeTabUpgrade"), TEXT(""),
            FVector2D(146,138), FVector2D(178,26), 17);
    }
    const auto* StartRoom=AMiningStartRoom::Find(GetWorld());
    MineralPanel->SetVisibility(!StartRoom||StartRoom->bPickaxeUnlocked?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden);
    const auto SetText = [](UTextBlock* Label, const FText& Text)
    { if (Label && !Label->GetText().EqualTo(Text)) Label->SetText(Text); };
    for (int32 I = 0; I < 4; ++I) SetText(MineralCounts[I], FText::AsNumber(Wallet->CollectedMineralTotals.FindRef(Types[I])));
    const int32 Level = FMath::Clamp(Progression->GetLevel(0), 0, 4);
    PickaxeUpgrade->SetUpgradeLevel(Level, true);
    SetText(PickaxeLevelLabel, FText::FromString(FString::Printf(TEXT("等级 %d"), Level)));
    Visible(PickaxeRequirements, Level < 4);
    const auto Costs = UMiningPickaxeSettings::ResolveLevel(Level+1).MaterialCosts;
    for (int32 Row = 0; Row < PickaxeRequirementRows.Num(); ++Row)
    {
        const bool bHasCost = Level < 4 && Costs.IsValidIndex(Row);
        auto* Label = PickaxeRequirementRows[Row].Get();
        auto* Count = PickaxeRequirementCounts[Row].Get();
        auto* Icon = PickaxeRequirementIcons[Row].Get();
        Visible(Label, bHasCost); Visible(Count, bHasCost); Visible(Icon, bHasCost);
        if (bHasCost)
        {
            const auto& Cost = Costs[Row];
            const int32 Owned = Wallet->GetMineralCount(Cost.Mineral);
            SetMineralIcon(Icon, Cost.Mineral);
            SetText(Label, FText::FromString(FMiningMineralCost::Name(Cost.Mineral)));
            SetText(Count, FText::FromString(Owned>=Cost.Count?TEXT("✓"):FString::Printf(TEXT("还需%d"),Cost.Count-Owned)));
            Count->SetColorAndOpacity(Owned >= Cost.Count ? FLinearColor(.42f,.95f,.65f) : FLinearColor(.89f,.9f,.87f));
        }
        else
        {
            SetText(Label, FText::GetEmpty());
            SetText(Count, FText::GetEmpty());
        }
    }
    const bool bReady = Progression->CanUpgradePickaxe();
    SetText(PickaxeUpgradeHint, FText::FromString(Level >= 4 ? TEXT("已满级") : (UMiningGamepadSubsystem::UsesGamepad(GetOwningPlayer()) ? TEXT("[ LT ] 升级") : TEXT("[ Tab ] 升级"))));
    PickaxeUpgradeHint->SetColorAndOpacity(bReady ? FLinearColor(.42f,.95f,.65f) : FLinearColor(.82f,.85f,.83f));
}

void UMiningGUIHUDWidget::RefreshTutorial(float Dt)
{
    auto* Saves=GetGameInstance()?GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>():nullptr;
    if(!Saves || !MineralPanel)return;
    if(!CompletionToast)
    {
        auto* Root=Cast<UCanvasPanel>(WidgetTree->RootWidget);
        if(Root)
        {
            CompletionToast=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("TaskCompletionToast"));
            CompletionToast->SetFont(BrainMiningFonts::Body(28));CompletionToast->SetJustification(ETextJustify::Center);
            CompletionToast->SetColorAndOpacity(FLinearColor(.65f,1.f,.79f));
            CompletionToast->SetShadowOffset(FVector2D(1,2));CompletionToast->SetShadowColorAndOpacity(FLinearColor(0,0,0,.85f));
            auto* ToastSlot=Root->AddChildToCanvas(CompletionToast);ToastSlot->SetAnchors(FAnchors(.5f,.2f));ToastSlot->SetAlignment(FVector2D(.5f,.5f));ToastSlot->SetSize(FVector2D(720,64));ToastSlot->SetZOrder(100);
            CompletionToast->SetRenderTransformPivot(FVector2D(.5f,.5f));
        }
    }
    if(CompletionToast)
    {
        CompletionToastAge+=Dt;
        if(CompletionToastAge>=2.6f && !Saves->CompletionNotices.IsEmpty())
        {
            CompletionToast->SetText(FText::FromString(TEXT("✓  ")+Saves->CompletionNotices[0]));Saves->CompletionNotices.RemoveAt(0);CompletionToastAge=0;
            if(CompletionSound)UGameplayStatics::PlaySound2D(this,CompletionSound,.65f);
        }
        const float T=CompletionToastAge;
        CompletionToast->SetVisibility(T<2.6f?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
        const float Fade=FMath::Clamp(T/.15f,0.f,1.f)*FMath::Clamp((2.6f-T)/.45f,0.f,1.f);
        CompletionToast->SetRenderOpacity(Fade*(T<.8f?.82f+.18f*FMath::Cos(T*4*PI):1.f));
        CompletionToast->SetRenderScale(FVector2D(1.f+.10f*FMath::Sin(FMath::Clamp(T/.3f,0.f,1.f)*PI)));
        CompletionToast->SetRenderTranslation(FVector2D(0,-12.f*(1-FMath::SmoothStep(0.f,.3f,T))));
    }
    if(!CollectionShortcut)
    {
        CollectionShortcut=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("CollectionShortcut"));
        CollectionShortcut->SetText(FText::FromString(TEXT("[C]查看藏品")));
        CollectionShortcut->SetFont(BrainMiningFonts::Body(18));
        CollectionShortcut->SetColorAndOpacity(FLinearColor(.94f,.95f,.92f));
        CollectionShortcut->SetShadowOffset(FVector2D(0,1));CollectionShortcut->SetShadowColorAndOpacity(FLinearColor(0,0,0,.65f));
        CollectionShortcut->SetVisibility(ESlateVisibility::HitTestInvisible);
        auto* ShortcutSlot=MineralPanel->AddChildToCanvas(CollectionShortcut);ShortcutSlot->SetPosition(FVector2D(40,195));ShortcutSlot->SetSize(FVector2D(275,28));
        CollectionShortcut->SetRenderShear(FVector2D(0,2));
    }
    if(!Saves->TutorialActive)return;
    if(!TutorialPanel)
    {
        TutorialPanel=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("GuidancePanel"));
        TutorialPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
        auto* PanelSlot=MineralPanel->AddChildToCanvas(TutorialPanel);
        PanelSlot->SetPosition(FVector2D(24,184));PanelSlot->SetSize(FVector2D(300,182));
        TutorialPanel->SetRenderShear(FVector2D(0,2));
        TutorialBackplate=WidgetTree->ConstructWidget<UImage>();
        auto* PlateSlot=TutorialPanel->AddChildToCanvas(TutorialBackplate);
        PlateSlot->SetSize(FVector2D(300,182));PlateSlot->SetZOrder(-1);
        TutorialBackplate->SetBrushFromMaterial(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/UI/MiningGUI/Immersive/MI_UpgradeBackplate")));
        if(auto* M=TutorialBackplate->GetDynamicMaterial())
        {M->SetVectorParameterValue(TEXT("PanelSize"),FLinearColor(300,182,0,0));M->SetScalarParameterValue(TEXT("OuterSide"),0);M->SetScalarParameterValue(TEXT("BackdropOpacity"),.58f);}
        for(int32 I=0;I<5;++I)
        {
            auto* Label=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),*FString::Printf(TEXT("GuidanceRow%d"),I));
            Label->SetFont(BrainMiningFonts::Body(18));Label->SetShadowOffset(FVector2D(0,1));Label->SetShadowColorAndOpacity(FLinearColor(0,0,0,.45f));
            auto* RowSlot=TutorialPanel->AddChildToCanvas(Label);RowSlot->SetSize(FVector2D(275,28));
            TutorialRows.Add(Label);
        }
        // A continued game only displays the next outstanding prototype.
        DisplayedPrototype=FMath::Clamp(Saves->PrototypesFound+1,1,5);
        DisplayedUpgrade=(Saves->TutorialCompleted&4)?2:1;
    }
    if(!Saves->TutorialSoilPassed || bTutorialSoilPresented)
    {
        if(!Saves->TutorialSoilPassed){bTutorialSoilPresented=true;TutorialSoilDoneAge=0;}
        else TutorialSoilDoneAge+=Dt;
        if(TutorialSoilDoneAge<1.4f)
        {
            TutorialAge+=Dt;
            for(int32 I=0;I<5;++I)TutorialRows[I]->SetVisibility(I==0?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
            auto* Row=TutorialRows[0].Get();
            Row->SetText(FText::FromString(Saves->TutorialSoilPassed?TEXT("✓  穿过1个泥土层"):TEXT("·  穿过1个泥土层")));
            Row->SetColorAndOpacity(Saves->TutorialSoilPassed?FLinearColor(.42f,.95f,.66f):FLinearColor(.94f,.95f,.92f));
            Row->SetRenderOpacity(Saves->TutorialSoilPassed?FMath::Clamp((1.4f-TutorialSoilDoneAge)/.4f,0.f,1.f):1.f);
            CastChecked<UCanvasPanelSlot>(Row->Slot)->SetPosition(FVector2D(16,14));
            TutorialPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
            TutorialPanel->SetRenderOpacity(TutorialAge<2.4f?.70f+.30f*FMath::Cos(TutorialAge*2.f*PI/.8f):1.f);
            CastChecked<UCanvasPanelSlot>(TutorialBackplate->Slot)->SetSize(FVector2D(300,59));
            if(auto* M=TutorialBackplate->GetDynamicMaterial())M->SetVectorParameterValue(TEXT("PanelSize"),FLinearColor(300,59,0,0));
            CastChecked<UCanvasPanelSlot>(CollectionShortcut->Slot)->SetPosition(FVector2D(40,254));
            return;
        }
        bTutorialSoilPresented=false;TutorialAge=0;
    }
    const bool UpgradeUnlocked=(Saves->TutorialCompleted&3)==3;
    if(UpgradeUnlocked && !bTutorialUpgradeRevealed)
    {
        bTutorialUpgradeRevealed=true;
        if(!(Saves->TutorialCompleted&4)){TutorialAge=0;UpgradeAttentionAge=0;}
    }
    TutorialAge+=Dt;UpgradeAttentionAge+=Dt;
    // Use the same three gentle pulses for the new objective and its destination.
    if(auto* PickaxePanel=GetWidgetFromName(TEXT("PickaxeSummaryPanel")))
        PickaxePanel->SetRenderOpacity(UpgradeAttentionAge<2.4f?.70f+.30f*FMath::Cos(UpgradeAttentionAge*2.f*PI/.8f):1.f);
    if(DisplayedUpgrade==1 && TutorialDoneAge[2]>1.4f)
    {DisplayedUpgrade=2;TutorialDoneAge[2]=0;TutorialAge=0;UpgradeAttentionAge=0;}
    const TCHAR* Basic[]={TEXT("收集1个铜矿"),TEXT("收集1个铁矿"),DisplayedUpgrade==1?TEXT("升级1次矿镐"):TEXT("将矿镐升至2级"),TEXT("找到1个灵感藏品")};
    const TCHAR* Prototype[]={TEXT("发现1个灵感原型"),TEXT("发现第2个灵感原型"),TEXT("发现第3个灵感原型"),TEXT("发现第4个灵感原型")};
    int32 VisibleRows=0;
    for(int32 I=0;I<5;++I)
    {
        auto* Row=TutorialRows[I].Get();
        const bool Done=I<4?(Saves->TutorialCompleted&(I==2 && DisplayedUpgrade==2?16:1<<I))!=0:Saves->PrototypesFound>=DisplayedPrototype;
        if(Done)TutorialDoneAge[I]+=Dt;
        if(I==4 && TutorialDoneAge[I]>1.4f && DisplayedPrototype<5)
        {++DisplayedPrototype;TutorialDoneAge[I]=0;TutorialAge=0;}
        const bool Hidden=(I==2 && !UpgradeUnlocked) || (I<4?TutorialDoneAge[I]>1.4f:DisplayedPrototype>4);
        Row->SetVisibility(Hidden?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
        if(Hidden)continue;
        const bool Checked=I<4?Done:Saves->PrototypesFound>=DisplayedPrototype;
        Row->SetText(FText::FromString(FString(Checked?TEXT("✓  "):TEXT("·  "))+(I<4?Basic[I]:Prototype[DisplayedPrototype-1])));
        Row->SetColorAndOpacity(Checked?FLinearColor(.42f,.95f,.66f):FLinearColor(.94f,.95f,.92f));
        Row->SetRenderOpacity(Checked?FMath::Clamp((1.4f-TutorialDoneAge[I])/.4f,0.f,1.f):1.f);
        CastChecked<UCanvasPanelSlot>(Row->Slot)->SetPosition(FVector2D(16,14+VisibleRows*31));++VisibleRows;
    }
    TutorialPanel->SetVisibility(VisibleRows?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    // Three gentle pulses draw attention without obscuring gameplay.
    TutorialPanel->SetRenderOpacity(TutorialAge<2.4f?.70f+.30f*FMath::Cos(TutorialAge*2.f*PI/.8f):1.f);
    const float Height=28+VisibleRows*31;
    CastChecked<UCanvasPanelSlot>(CollectionShortcut->Slot)->SetPosition(FVector2D(40,195+(VisibleRows?Height:0)));
    CastChecked<UCanvasPanelSlot>(TutorialBackplate->Slot)->SetSize(FVector2D(300,Height));
    if(auto* M=TutorialBackplate->GetDynamicMaterial())M->SetVectorParameterValue(TEXT("PanelSize"),FLinearColor(300,Height,0,0));
}
