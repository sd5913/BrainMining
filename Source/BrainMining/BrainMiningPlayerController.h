// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BrainMiningPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UMiningGUIHUDWidget;
class UMiningInteractionHUDWidget;
class SWidget;
class SMobileTouchControls;

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
    virtual void PlayerTick(float DeltaTime) override;
    virtual void AddYawInput(float Value) override;
    virtual void AddPitchInput(float Value) override;
	void UseInputDefaultsFrom(const ABrainMiningPlayerController* Defaults);
    // Touch controls call these small controller-side adapters so the same
    // mining components are used on desktop and mobile input paths.
    void HandleMobileLook(const FVector2D& Delta);
    void HandleMobileMove(const FVector2D& Axis);
    void HandleMobileMine(bool bPressed);
    void HandleMobileBomb();
    void HandleMobileDrill();
    void HandleMobileJump(bool bPressed);
    void HandleMobileInteract();
    void HandleMobilePrimary();
    void HandleMobileConfirm();
    void HandleMobileReset();
    void HandleMobileThoughtLeft();
    void HandleMobileThoughtRight();
    void HandleMobileMap();
    void HandleMobileCatalogue();
    void HandleMobileBack();
    void HandleMobileEdit();
    void HandleMobileUpgrade();
    bool IsMobileActionVisible() const;
    bool IsMobileInteractVisible() const;
    bool IsMobileConfirmVisible() const;
    bool IsMobileResetVisible() const;
    bool IsMobileThoughtVisible() const;
    bool IsMobileMapVisible() const;
    bool IsMobileCatalogueVisible() const;
    bool IsMobileBackVisible() const;
    bool IsMobileEditVisible() const;
    bool IsMobileUpgradeVisible() const;
    FString GetMobileActionLabel() const;
    bool IsUsingTouchControls() const { return ShouldUseTouchControls(); }
 UFUNCTION(BlueprintCallable, Category="Mining GUI")
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
	bool IsAudioSettingsOpen() const { return AudioSettingsPanel.IsValid(); }
	void ToggleAudioSettings();

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
	TSharedPtr<SWidget> AudioSettingsPanel;
	TSharedPtr<SMobileTouchControls> MobileTouchControls;
	bool bAudioPausedWorld = false;
	bool bAudioPreviousMiningSuspended = false;
	EMouseCaptureMode PreviousMouseCaptureMode = EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown;
	EMouseLockMode PreviousMouseLockMode = EMouseLockMode::LockOnCapture;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;
	void SetMobileVirtualKey(const FKey& Key, bool bPressed);
	void TriggerMobileKey(const FKey& Key);
	TSet<FKey> MobileVirtualKeys;
};
