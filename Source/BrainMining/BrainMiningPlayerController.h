// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BrainMiningPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UMiningGUIHUDWidget;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining GUI")
	TSoftClassPtr<UMiningGUIHUDWidget> MiningGUIClass;
	UPROPERTY(BlueprintReadOnly, Category="Mining GUI")
	TObjectPtr<UMiningGUIHUDWidget> MiningGUI;
	/** A visible cursor allows direct interaction; hold RMB to look around. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining GUI")
	bool bEnableGUICursor = true;

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
	EMouseCaptureMode PreviousMouseCaptureMode = EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown;
	EMouseLockMode PreviousMouseLockMode = EMouseLockMode::LockOnCapture;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;
};
