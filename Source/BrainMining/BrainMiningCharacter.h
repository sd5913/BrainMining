// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "BrainMiningCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UMiningComponent;
class UMiningAudioComponent;
class UMiningToolEffectsComponent;
class UMiningBombToolComponent;
class UMiningProgressionComponent;
class USpotLightComponent;
class UPointLightComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class ABrainMiningCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMiningComponent> MiningComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMiningAudioComponent> MiningAudio;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMiningToolEffectsComponent> MiningToolEffects;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMiningBombToolComponent> BombTool;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMiningProgressionComponent> Progression;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpotLightComponent> MiningLight;
	UPROPERTY(EditDefaultsOnly, Category="Mining|Lighting")
	TSoftObjectPtr<UMaterialInterface> HeadlampPattern = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Campaign/Materials/M_MinerHeadlamp.M_MinerHeadlamp")));

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPointLightComponent> MiningFillLight;

	/** Artist-editable camera mounts. Animation is applied to separate child components. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tools", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> PickaxeMount;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tools", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> PickaxeMotion;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tools", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> PickaxeModel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tools", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> DrillRightMount;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tools", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> DrillRightMotion;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tools", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> DrillRightModel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tools", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> DrillLeftMount;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tools", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> DrillLeftMotion;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tools", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> DrillLeftModel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tools", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> BombMount;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tools", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> BombMotion;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tools", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> BombModel;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;
	
public:
	ABrainMiningCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	void ConfigureMiningLighting(float HeadlampBrightness, float FillBrightness);
	virtual void PostInitializeComponents() override;
	virtual void UnPossessed() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Player Scale")
	float AppliedPlayerScale = 1;

private:
	bool bPlayerScaleInitialized = false;
	void InitializePlayerScale();
	void InitializePlayerFlight();
	void InitializePlayerMining();

protected:

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
	USceneComponent* GetPickaxeMotion() const { return PickaxeMotion; }
	UStaticMeshComponent* GetPickaxeModel() const { return PickaxeModel; }
	USceneComponent* GetDrillRightMotion() const { return DrillRightMotion; }
	UStaticMeshComponent* GetDrillRightModel() const { return DrillRightModel; }
	USceneComponent* GetDrillLeftMotion() const { return DrillLeftMotion; }
	UStaticMeshComponent* GetDrillLeftModel() const { return DrillLeftModel; }
	UStaticMeshComponent* GetBombModel() const { return BombModel; }
	USceneComponent* GetBombMotion() const { return BombMotion; }

};
