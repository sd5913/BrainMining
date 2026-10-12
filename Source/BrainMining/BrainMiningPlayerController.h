// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BrainMiningPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UMiningGUIHUDWidget;
class UMiningInteractionHUDWidget;

/**
 *  Simple first person Player Controller
 *  Manages the input mapping context.
 *  Overrides the Player Camera Manager class.
 */
UCLASS(abstract, config="Game")
class BRAINMINING_API ABrainMiningPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	ABrainMiningPlayerController();
	void UseInputDefaultsFrom(const ABrainMiningPlayerController* Defaults);
 void SetMiningGUIEnabled(bool bEnabled);
 bool IsMiningGUIEnabled() const { return bMiningGUIEnabled; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining GUI")
	TSoftClassPtr<UMiningGUIHUDWidget> MiningGUIClass;
	UPROPERTY(BlueprintReadOnly, Category="Mining GUI")
	TObjectPtr<UMiningGUIHUDWidget> MiningGUI;
	UPROPERTY(BlueprintReadOnly, Category="Mining GUI")
	TObjectPtr<UMiningInteractionHUDWidget> InteractionHUD;
	UPROPERTY(BlueprintReadOnly, Category="Mining GUI")
	bool bUpgradeMenuHeld = false;
	/** Tab and the focused HUD share this entry point, so mouse clicks cannot swallow release. */
	UFUNCTION(BlueprintCallable, Category="Mining GUI")
	void SetUpgradeMenuHeld(bool bHeld);
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	void UseAbilityG();
	void UseAbilityV();
	void OnApplicationActivationChanged(bool bActive);
	bool bOwnsDesktopGUIInput = false;
 bool bMiningGUIEnabled = true;
	FDelegateHandle ApplicationActivationHandle;
	EMouseCaptureMode PreviousMouseCaptureMode = EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown;
	EMouseLockMode PreviousMouseLockMode = EMouseLockMode::LockOnCapture;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;
};
