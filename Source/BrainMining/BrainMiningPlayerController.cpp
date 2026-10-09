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
				if (bEnableGUICursor && !ShouldUseTouchControls())
				{
					FInputModeGameAndUI Mode;
					Mode.SetHideCursorDuringCapture(true).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
					if (auto* Viewport = GetWorld()->GetGameViewport())
					{
						PreviousMouseCaptureMode = Viewport->GetMouseCaptureMode();
						PreviousMouseLockMode = Viewport->GetMouseLockMode();
					}
					SetInputMode(Mode);
					bShowMouseCursor = true;
					if (auto* Viewport = GetWorld()->GetGameViewport())
						Viewport->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringRightMouseDown);
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
void ABrainMiningPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (MiningGUI) MiningGUI->RemoveFromParent();
	if (MiningGUI && bEnableGUICursor && IsLocalPlayerController())
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
