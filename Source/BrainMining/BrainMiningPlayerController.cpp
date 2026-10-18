// Copyright Epic Games, Inc. All Rights Reserved.


#include "BrainMiningPlayerController.h"
#include "BrainMiningFonts.h"
#include "MiningAudioSettings.h"
#include "MiningAudioComponent.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "BrainMiningCameraManager.h"
#include "BrainMiningCharacter.h"
#include "Blueprint/UserWidget.h"
#include "BrainMining.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "MiningGUIWidgets.h"
#include "MiningInteractionHUDWidget.h"
#include "MobileTouchControls.h"
#include "Engine/GameViewportClient.h"
#include "Components/InputComponent.h"
#include "MiningComponent.h"
#include "MiningProgressionComponent.h"
#include "MiningBombToolComponent.h"
#include "PopupClose/PopupCloseStage.h"
#include "WenSi/WenSiMirrorPuzzle.h"
#include "WenSi/WenSiPuzzle.h"
#include "EchoPassageStage.h"
#include "EchoPassagePlayer.h"
#include "CardRoomStage.h"
#include "MiningCollection.h"
#include "MiningCellUniverse.h"
#include "EndingEmergence.h"
#include "Showcase/MiningExhibitHall.h"
#include "InputKeyEventArgs.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"

ABrainMiningPlayerController::ABrainMiningPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = ABrainMiningCameraManager::StaticClass();
	MiningGUIClass = TSoftClassPtr<UMiningGUIHUDWidget>(FSoftObjectPath(TEXT("/Game/UI/MiningGUI/WBP_MiningHUD.WBP_MiningHUD_C")));
}

void ABrainMiningPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalPlayerController())
	{
		InteractionHUD = CreateWidget<UMiningInteractionHUDWidget>(this);
		if (InteractionHUD) InteractionHUD->AddToPlayerScreen(30);
		if (auto* WidgetClass = MiningGUIClass.LoadSynchronous())
		{
			MiningGUI = CreateWidget<UMiningGUIHUDWidget>(this, WidgetClass);
			if (MiningGUI)
			{
				MiningGUI->AddToPlayerScreen(10);
				if (ShouldUseTouchControls())
				{
					if (MiningGUI->AbilityG) MiningGUI->AbilityG->SetVisibility(ESlateVisibility::Collapsed);
					if (MiningGUI->AbilityV) MiningGUI->AbilityV->SetVisibility(ESlateVisibility::Collapsed);
				}
				if (!ShouldUseTouchControls())
				{
					if (auto* Viewport = GetWorld()->GetGameViewport())
					{
						PreviousMouseCaptureMode = Viewport->GetMouseCaptureMode();
						PreviousMouseLockMode = Viewport->GetMouseLockMode();
					}
					bOwnsDesktopGUIInput = true;
					FInputModeGameOnly Mode;
					Mode.SetConsumeCaptureMouseDown(false);
					SetInputMode(Mode);
					bShowMouseCursor = false;
					if (FSlateApplication::IsInitialized())
						ApplicationActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this, &ThisClass::OnApplicationActivationChanged);
				}
			}
		}
	}

	
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		if (auto* Viewport = GetWorld()->GetGameViewport())
		{
			MobileTouchControls = SNew(SMobileTouchControls).OwnerController(this);
			Viewport->AddViewportWidgetContent(MobileTouchControls.ToSharedRef(), 0);
		}
	}
}

void ABrainMiningPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (IsLocalPlayerController())
	{
		InputComponent->BindKey(EKeys::G, IE_Pressed, this, &ThisClass::UseAbilityG);
		InputComponent->BindKey(EKeys::V, IE_Pressed, this, &ThisClass::UseAbilityV);
	}

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}

bool ABrainMiningPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void ABrainMiningPlayerController::UseInputDefaultsFrom(const ABrainMiningPlayerController* Defaults)
{
    if (!Defaults) return;
    DefaultMappingContexts = Defaults->DefaultMappingContexts;
    MobileExcludedMappingContexts = Defaults->MobileExcludedMappingContexts;
    MobileControlsWidgetClass = Defaults->MobileControlsWidgetClass;
    PlayerCameraManagerClass = Defaults->PlayerCameraManagerClass;
}

void ABrainMiningPlayerController::SetMiningGUIEnabled(bool bEnabled)
{
 if (!bEnabled) SetUpgradeMenuHeld(false);
 bMiningGUIEnabled = bEnabled;
 if (MiningGUI) MiningGUI->SetVisibility(bEnabled ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void ABrainMiningPlayerController::UseAbilityG()
{
	if (!bMiningGUIEnabled || IsMoveInputIgnored() || IsLookInputIgnored() || IsPaused() || bUpgradeMenuHeld) return;
	if (MiningGUI && MiningGUI->AbilityG && !MiningGUI->AbilityG->bAvailable) return;
	auto* Bomb = GetPawn() ? GetPawn()->FindComponentByClass<UMiningBombToolComponent>() : nullptr;
	if (Bomb && Bomb->ToggleEquipped() && MiningGUI) MiningGUI->OnAbilityRequested.Broadcast(TEXT("G"));
}
void ABrainMiningPlayerController::UseAbilityV()
{
 if (IsMoveInputIgnored() || IsLookInputIgnored() || IsPaused() || bUpgradeMenuHeld) return;
	if (MiningGUI && MiningGUI->AbilityV && !MiningGUI->AbilityV->bAvailable) return;
	UMiningComponent* Mining = GetPawn() ? GetPawn()->FindComponentByClass<UMiningComponent>() : nullptr;
	if (Mining && Mining->ActivateDrill() && MiningGUI)
		MiningGUI->OnAbilityRequested.Broadcast(TEXT("V"));
}

void ABrainMiningPlayerController::HandleMobileLook(const FVector2D& Delta)
{
	if (IsPaused() || IsLookInputIgnored() || !GetPawn()) return;
	const float Sensitivity=0.09f;
	AddYawInput(Delta.X*Sensitivity);
	AddPitchInput(Delta.Y*Sensitivity);
}

void ABrainMiningPlayerController::HandleMobileMove(const FVector2D& Axis)
{
	SetMobileVirtualKey(EKeys::W, Axis.Y > .2f);
	SetMobileVirtualKey(EKeys::S, Axis.Y < -.2f);
	SetMobileVirtualKey(EKeys::D, Axis.X > .2f);
	SetMobileVirtualKey(EKeys::A, Axis.X < -.2f);
	if (IsPaused() || IsMoveInputIgnored() || !GetPawn()) return;
	const FVector Forward=GetPawn()->GetActorForwardVector();
	const FVector Right=GetPawn()->GetActorRightVector();
	GetPawn()->AddMovementInput(Forward, FMath::Clamp(Axis.Y,-1.f,1.f));
	GetPawn()->AddMovementInput(Right, FMath::Clamp(Axis.X,-1.f,1.f));
}

void ABrainMiningPlayerController::HandleMobileMine(bool bPressed)
{
	if (IsPaused() || IsMoveInputIgnored() || IsLookInputIgnored()) return;
	if (auto* Mining=GetPawn()?GetPawn()->FindComponentByClass<UMiningComponent>():nullptr)
	{
		if (bPressed) Mining->BeginMiningInput(); else Mining->EndMiningInput();
	}
}

void ABrainMiningPlayerController::HandleMobileBomb()
{
	if (!bMiningGUIEnabled || IsPaused() || IsMoveInputIgnored() || IsLookInputIgnored()) return;
	if (auto* Bomb=GetPawn()?GetPawn()->FindComponentByClass<UMiningBombToolComponent>():nullptr)
	{
		if (!Bomb->bEquipped && !Bomb->ToggleEquipped()) return;
		if (Bomb->ThrowBomb() && MiningGUI) MiningGUI->OnAbilityRequested.Broadcast(TEXT("G"));
	}
}

void ABrainMiningPlayerController::HandleMobileDrill()
{
	if (IsPaused() || IsMoveInputIgnored() || IsLookInputIgnored()) return;
	if (auto* Mining=GetPawn()?GetPawn()->FindComponentByClass<UMiningComponent>():nullptr)
		if (Mining->ActivateDrill() && MiningGUI) MiningGUI->OnAbilityRequested.Broadcast(TEXT("V"));
}

void ABrainMiningPlayerController::HandleMobileJump(bool bPressed)
{
	SetMobileVirtualKey(EKeys::SpaceBar, bPressed);
	auto* PawnCharacter = Cast<ABrainMiningCharacter>(GetPawn());
	if (!PawnCharacter) return;
	if (bPressed)
	{
		if (IsPaused() || IsMoveInputIgnored() || bUpgradeMenuHeld) return;
		PawnCharacter->DoJumpStart();
	}
	else
	{
		PawnCharacter->DoJumpEnd();
	}
}

void ABrainMiningPlayerController::HandleMobileInteract()
{
	for (TActorIterator<APopupCloseStage> It(GetWorld()); It; ++It)
	{
		if (It->IsSessionActive())
		{
			It->HandleInteractionInput(this);
			return;
		}
	}
	if (IsPaused() || IsMoveInputIgnored() || IsLookInputIgnored() || bUpgradeMenuHeld) return;
	InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E, IE_Pressed, 1.f));
}

void ABrainMiningPlayerController::HandleMobilePrimary()
{
	for (TActorIterator<APopupCloseStage> It(GetWorld()); It; ++It)
	{
		if (It->IsSessionActive())
		{
			It->HandleMobilePrimary(this);
			return;
		}
	}
	if (auto* EchoInteraction = FindComponentByClass<UEchoPassageInteractionComponent>())
	{
		if (EchoInteraction->Stage && EchoInteraction->Stage->bLanded && !EchoInteraction->Stage->bCompleted)
		{
			EchoInteraction->Knock();
			return;
		}
	}
	TriggerMobileKey(EKeys::LeftMouseButton);
}

void ABrainMiningPlayerController::HandleMobileConfirm()
{
	TriggerMobileKey(EKeys::SpaceBar);
}

void ABrainMiningPlayerController::HandleMobileReset()
{
	TriggerMobileKey(EKeys::R);
}

void ABrainMiningPlayerController::HandleMobileThoughtLeft()
{
	TriggerMobileKey(EKeys::Q);
}

void ABrainMiningPlayerController::HandleMobileThoughtRight()
{
	TriggerMobileKey(EKeys::R);
}

void ABrainMiningPlayerController::HandleMobileMap()
{
	TriggerMobileKey(EKeys::M);
}

void ABrainMiningPlayerController::HandleMobileCatalogue()
{
	if (AMiningExhibitHall::Find(GetWorld())) TriggerMobileKey(EKeys::I);
	else TriggerMobileKey(EKeys::Tab);
}

void ABrainMiningPlayerController::HandleMobileBack()
{
	TriggerMobileKey(EKeys::Escape);
}

void ABrainMiningPlayerController::HandleMobileEdit()
{
	TriggerMobileKey(EKeys::G);
}

void ABrainMiningPlayerController::SetMobileVirtualKey(const FKey& Key, bool bPressed)
{
	const bool bWasPressed = MobileVirtualKeys.Contains(Key);
	if (bWasPressed == bPressed) return;
	const FInputKeyEventArgs Args = FInputKeyEventArgs::CreateSimulated(Key, bPressed ? IE_Pressed : IE_Released, bPressed ? 1.f : 0.f);
	Super::InputKey(Args);
	if (bPressed) MobileVirtualKeys.Add(Key);
	else MobileVirtualKeys.Remove(Key);
}

void ABrainMiningPlayerController::TriggerMobileKey(const FKey& Key)
{
	InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.f));
	InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.f));
}

bool ABrainMiningPlayerController::IsMobileActionVisible() const
{
	for (TActorIterator<AEchoPassageStage> It(GetWorld()); It; ++It) if (It->bLanded && !It->bCompleted) return true;
	for (TActorIterator<APopupCloseStage> It(GetWorld()); It; ++It) if (It->IsSessionActive()) return true;
	return false;
}

bool ABrainMiningPlayerController::IsMobileConfirmVisible() const
{
	for (TActorIterator<ACardRoomStage> It(GetWorld()); It; ++It)
		if (It->Run.Phase == ECardRoomPhase::Choosing) return true;
	return false;
}

bool ABrainMiningPlayerController::IsMobileResetVisible() const
{
	if (IsMobileActionVisible() || IsMobileThoughtVisible() || IsMobileCatalogueVisible()) return true;
	if (AMiningExhibitHall::Find(GetWorld())) return true;
	TActorIterator<AMiningCellUniverse> CellIt(GetWorld());
	if (CellIt) return true;
	TActorIterator<AEndingEmergence> EndingIt(GetWorld());
	if (EndingIt) return true;
	return false;
}

bool ABrainMiningPlayerController::IsMobileThoughtVisible() const
{
	for (TActorIterator<AWenSiPuzzle> It(GetWorld()); It; ++It) if (It->bAreaActive) return true;
	for (TActorIterator<AWenSiMirrorPuzzle> It(GetWorld()); It; ++It) if (It->bAreaActive) return true;
	return false;
}

bool ABrainMiningPlayerController::IsMobileMapVisible() const
{
	const AMiningExhibitHall* Hall = AMiningExhibitHall::Find(GetWorld());
	return Hall && Hall->bReady;
}

bool ABrainMiningPlayerController::IsMobileCatalogueVisible() const
{
	for (TActorIterator<AMiningCollectionField> It(GetWorld()); It; ++It) if (It->IsAreaActive()) return true;
	return false;
}

bool ABrainMiningPlayerController::IsMobileBackVisible() const
{
	const AMiningExhibitHall* Hall = AMiningExhibitHall::Find(GetWorld());
	if (Hall && Hall->IsMapOpen()) return true;
	for (TActorIterator<AMiningCollectionField> It(GetWorld()); It; ++It) if (It->bCatalogueOpen) return true;
	return false;
}

bool ABrainMiningPlayerController::IsMobileEditVisible() const
{
	for (TActorIterator<AWenSiPuzzle> It(GetWorld()); It; ++It) if (It->bAreaActive) return true;
	for (TActorIterator<AWenSiMirrorPuzzle> It(GetWorld()); It; ++It) if (It->bAreaActive) return true;
	return false;
}

FString ABrainMiningPlayerController::GetMobileActionLabel() const
{
	for (TActorIterator<AEchoPassageStage> It(GetWorld()); It; ++It) if (It->bLanded && !It->bCompleted) return TEXT("HIT");
	for (TActorIterator<APopupCloseStage> It(GetWorld()); It; ++It) if (It->IsSessionActive()) return TEXT("CLICK");
	return TEXT("TAP");
}

bool ABrainMiningPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (Params.Key == EKeys::F8 || (IsAudioSettingsOpen() && Params.Key == EKeys::Escape))
	{
		if (Params.Event == IE_Pressed) ToggleAudioSettings();
		return true;
	}
	if (IsAudioSettingsOpen()) return true;
	if (Params.Key == EKeys::E && Params.Event == IE_Pressed && !bUpgradeMenuHeld)
	{
		for (TActorIterator<APopupCloseStage> It(GetWorld()); It; ++It)
			if (It->HandleInteractionInput(this)) return true;
		for (TActorIterator<AWenSiPuzzle> It(GetWorld()); It; ++It)
			if (It->HandleInteractionInput(this)) return true;
		for (TActorIterator<AWenSiMirrorPuzzle> It(GetWorld()); It; ++It)
			if (It->HandleInteractionInput(this)) return true;
	}
    auto* Progression = GetPawn() ? GetPawn()->FindComponentByClass<UMiningProgressionComponent>() : nullptr;
    if (Params.Key == EKeys::Tab && Progression && Progression->UsesMineralInventory())
    {
        if (Params.Event == IE_Pressed && bMiningGUIEnabled && !IsMoveInputIgnored() && !IsLookInputIgnored() && !IsPaused() && !bShowMouseCursor)
        {
            const bool bUpgraded = Progression->Purchase(0, Progression->GetLevel(0) + 1);
            if (auto* Audio = GetPawn()->FindComponentByClass<UMiningAudioComponent>()) Audio->PlayPurchase(bUpgraded);
        }
        return true; // Repeat/release never purchase, show a cursor, or suspend mining.
    }
	// Consume the throw before Enhanced Input sees a mining press. A held throw
	// therefore cannot turn into a pickaxe swing after the bomb leaves the hand.
	if (Params.Key == EKeys::LeftMouseButton && Params.Event == IE_Pressed)
	{
		if (auto* Bomb = GetPawn() ? GetPawn()->FindComponentByClass<UMiningBombToolComponent>() : nullptr; Bomb && Bomb->bEquipped)
		{
			Bomb->ThrowBomb();
			return true;
		}
	}
	if (bOwnsDesktopGUIInput && bMiningGUIEnabled)
	{
		if (Params.Key == EKeys::Tab && (bUpgradeMenuHeld || (!IsMoveInputIgnored() && !IsLookInputIgnored() && !IsPaused())))
		{
			if (Params.Event == IE_Pressed || Params.Event == IE_Repeat) SetUpgradeMenuHeld(true);
			else if (Params.Event == IE_Released) SetUpgradeMenuHeld(false);
			return true;
		}
		// UI handles its buttons first. Unhandled pointer events must not reach mining or recapture RMB.
		if (bUpgradeMenuHeld && (Params.Key.IsMouseButton() || Params.Key == EKeys::MouseX || Params.Key == EKeys::MouseY))
			return true;
	}
	return Super::InputKey(Params);
}

void ABrainMiningPlayerController::SetUpgradeMenuHeld(bool bHeld)
{
    if (GetPawn()) if (auto* Progression = GetPawn()->FindComponentByClass<UMiningProgressionComponent>(); Progression && Progression->UsesMineralInventory()) return;
	if (!bOwnsDesktopGUIInput || !MiningGUI || bUpgradeMenuHeld == bHeld) return;
 if (bHeld && (!bMiningGUIEnabled || IsMoveInputIgnored() || IsLookInputIgnored() || IsPaused())) return;
	bUpgradeMenuHeld = bHeld;
	SetIgnoreLookInput(bHeld);
	RotationInput = FRotator::ZeroRotator;
	if (APawn* ControlledPawn = GetPawn())
		if (auto* Mining = ControlledPawn->FindComponentByClass<UMiningComponent>()) Mining->SetInputSuspended(bHeld);
	MiningGUI->SetUpgradePanelOpen(bHeld);
	bShowMouseCursor = bHeld;
	if (bHeld)
	{
		// Clear any held mining click before handing the pointer to Slate.
		Super::InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 0.f));
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(MiningGUI->TakeWidget());
		Mode.SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		if (auto* Viewport = GetWorld()->GetGameViewport()) Viewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
		int32 Width = 0, Height = 0;
		GetViewportSize(Width, Height);
		if (Width > 0 && Height > 0) SetMouseLocation(Width / 2, Height / 2);
	}
	else
	{
		FInputModeGameOnly Mode;
		Mode.SetConsumeCaptureMouseDown(false);
		SetInputMode(Mode);
	}
}

void ABrainMiningPlayerController::OnApplicationActivationChanged(bool bActive)
{
	if (!bActive) SetUpgradeMenuHeld(false);
}

void ABrainMiningPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	for (const FKey& Key : MobileVirtualKeys)
		Super::InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.f));
	MobileVirtualKeys.Reset();
	if (IsAudioSettingsOpen()) ToggleAudioSettings();
	if (ApplicationActivationHandle.IsValid() && FSlateApplication::IsInitialized())
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ApplicationActivationHandle);
	SetUpgradeMenuHeld(false);
	if (MiningGUI) MiningGUI->RemoveFromParent();
	if (InteractionHUD) InteractionHUD->RemoveFromParent();
	if (MobileTouchControls.IsValid())
	{
		if (auto* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
			Viewport->RemoveViewportWidgetContent(MobileTouchControls.ToSharedRef());
		MobileTouchControls.Reset();
	}
	if (bOwnsDesktopGUIInput && IsLocalPlayerController())
	{
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
		if (auto* Viewport = GetWorld()->GetGameViewport())
		{
			Viewport->SetMouseCaptureMode(PreviousMouseCaptureMode);
			Viewport->SetMouseLockMode(PreviousMouseLockMode);
		}
	}
	Super::EndPlay(Reason);
}

void ABrainMiningPlayerController::ToggleAudioSettings()
{
	auto* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	auto* Settings = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMiningAudioSettings>() : nullptr;
	if (!Viewport || !Settings) return;
	auto* Mining = GetPawn() ? GetPawn()->FindComponentByClass<UMiningComponent>() : nullptr;
	if (AudioSettingsPanel.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(AudioSettingsPanel.ToSharedRef());
		AudioSettingsPanel.Reset();
		Settings->Save();
		if (bAudioPausedWorld) SetPause(false);
		bAudioPausedWorld = false;
		SetIgnoreMoveInput(false);
		SetIgnoreLookInput(false);
		if (Mining) Mining->SetInputSuspended(bAudioPreviousMiningSuspended);
		bShowMouseCursor = false;
		FInputModeGameOnly Mode;
		Mode.SetConsumeCaptureMouseDown(false);
		SetInputMode(Mode);
		return;
	}
	if (bShowMouseCursor || IsPaused()) return;
	bAudioPreviousMiningSuspended = Mining && Mining->IsInputSuspended();
	if (Mining) Mining->SetInputSuspended(true);
	if (auto* Audio = GetPawn() ? GetPawn()->FindComponentByClass<UMiningAudioComponent>() : nullptr) Audio->StopTools();
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	bAudioPausedWorld = SetPause(true);
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	Rows->AddSlot().AutoHeight().Padding(0, 0, 0, 20)[SNew(STextBlock).Font(BrainMiningFonts::Style("Bold", 22)).Text(FText::FromString(TEXT("音量设置 · F8 / Esc 关闭")))];
	const TCHAR* Labels[] = {TEXT("总音量"), TEXT("音乐"), TEXT("音效"), TEXT("环境声"), TEXT("语音")};
	TWeakObjectPtr<UMiningAudioSettings> WeakSettings(Settings);
	for (int32 Channel = 0; Channel < 5; ++Channel)
	{
		const FString Label(Labels[Channel]);
		Rows->AddSlot().AutoHeight().Padding(0, 8)[SNew(STextBlock).Font(BrainMiningFonts::Style("Regular", 16)).Text_Lambda([WeakSettings, Channel, Label]()
		{
			return FText::FromString(FString::Printf(TEXT("%s   %d%%"), *Label, WeakSettings.IsValid() ? FMath::RoundToInt(100 * WeakSettings->GetVolume(Channel)) : 0));
		})];
		Rows->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SSlider)
			.Value_Lambda([WeakSettings, Channel]() { return WeakSettings.IsValid() ? WeakSettings->GetVolume(Channel) : 0.f; })
			.OnValueChanged_Lambda([WeakSettings, Channel](float Value) { if (WeakSettings.IsValid()) WeakSettings->SetVolume(Channel, Value); })];
	}
	Rows->AddSlot().AutoHeight().Padding(0, 12)[SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(12, 8)).OnClicked_Lambda([WeakSettings]()
	{
		if (WeakSettings.IsValid()) WeakSettings->RestoreDefaults();
		return FReply::Handled();
	})[SNew(STextBlock).Font(BrainMiningFonts::Style("Regular", 16)).Text(FText::FromString(TEXT("恢复默认音量")))]];
	Rows->AddSlot().AutoHeight()[SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(12, 8)).OnClicked_Lambda([this]() { ToggleAudioSettings(); return FReply::Handled(); })
		[SNew(STextBlock).Font(BrainMiningFonts::Style("Regular", 16)).Text(FText::FromString(TEXT("保存并返回游戏")))]];
	AudioSettingsPanel = SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SBorder).BorderBackgroundColor(FLinearColor(0, 0, 0, .85f))]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride(540)[SNew(SBorder).Padding(28)[Rows]]];
	Viewport->AddViewportWidgetContent(AudioSettingsPanel.ToSharedRef(), 1000);
	bShowMouseCursor = true;
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(AudioSettingsPanel).SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	Viewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
}
