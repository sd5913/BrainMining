// Copyright Epic Games, Inc. All Rights Reserved.


#include "BrainMiningPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "BrainMiningCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "BrainMining.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "MiningGUIWidgets.h"
#include "Engine/GameViewportClient.h"
#include "Components/InputComponent.h"
#include "MiningComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"

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
		if (auto* WidgetClass = MiningGUIClass.LoadSynchronous())
		{
			MiningGUI = CreateWidget<UMiningGUIHUDWidget>(this, WidgetClass);
			if (MiningGUI)
			{
				MiningGUI->AddToPlayerScreen(10);
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

	
	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogBrainMining, Error, TEXT("Could not spawn mobile controls widget."));

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

void ABrainMiningPlayerController::UseAbilityG() { if (MiningGUI) MiningGUI->TriggerAbility(TEXT("G")); }
void ABrainMiningPlayerController::UseAbilityV() { if (MiningGUI) MiningGUI->TriggerAbility(TEXT("V")); }

bool ABrainMiningPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (bOwnsDesktopGUIInput)
	{
		if (Params.Key == EKeys::Tab)
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
	if (!bOwnsDesktopGUIInput || !MiningGUI || bUpgradeMenuHeld == bHeld) return;
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
	if (ApplicationActivationHandle.IsValid() && FSlateApplication::IsInitialized())
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ApplicationActivationHandle);
	SetUpgradeMenuHeld(false);
	if (MiningGUI) MiningGUI->RemoveFromParent();
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
