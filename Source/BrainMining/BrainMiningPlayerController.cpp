// Copyright Epic Games, Inc. All Rights Reserved.


#include "BrainMiningPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "BrainMiningCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "BrainMining.h"
#include "Widgets/Input/SVirtualJoystick.h"

ABrainMiningPlayerController::ABrainMiningPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = ABrainMiningCameraManager::StaticClass();
}

void ABrainMiningPlayerController::BeginPlay()
{
	Super::BeginPlay();

	
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
    if(!Defaults)return;
    DefaultMappingContexts=Defaults->DefaultMappingContexts;
    MobileExcludedMappingContexts=Defaults->MobileExcludedMappingContexts;
    MobileControlsWidgetClass=Defaults->MobileControlsWidgetClass;
    PlayerCameraManagerClass=Defaults->PlayerCameraManagerClass;
}
