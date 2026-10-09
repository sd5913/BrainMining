// Copyright Epic Games, Inc. All Rights Reserved.

#include "BrainMiningCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BrainMining.h"
#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningTunnelGenerator.h"
#include "EngineUtils.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"

ABrainMiningCharacter::ABrainMiningCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UMiningCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	MiningComponent = CreateDefaultSubobject<UMiningComponent>(TEXT("MiningComponent"));
	MiningLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("MiningLight"));
	MiningLight->SetupAttachment(FirstPersonCameraComponent);
	MiningLight->SetRelativeLocation(FVector(20.0, 0.0, -10.0));
	MiningLight->SetIntensity(500.0f);
	MiningLight->SetAttenuationRadius(2000.0f);
	MiningLight->SetInnerConeAngle(30.0f);
	MiningLight->SetOuterConeAngle(65.0f);
	MiningLight->SetCastShadows(false);
	MiningFillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MiningFillLight"));
	MiningFillLight->SetupAttachment(GetCapsuleComponent());
	MiningFillLight->SetRelativeLocation(FVector(0, 0, 25));
	MiningFillLight->SetIntensityUnits(ELightUnits::Candelas);
	MiningFillLight->SetIntensity(0);
	MiningFillLight->SetAttenuationRadius(1400);
	MiningFillLight->SetCastShadows(false);
	MiningFillLight->SetVisibility(false);

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
}

void ABrainMiningCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (GetWorld() && GetWorld()->IsGameWorld() && !bPlayerScaleInitialized)
	{
		bPlayerScaleInitialized = true;
		InitializePlayerMining();
		InitializePlayerScale();
		InitializePlayerFlight();
	}
}

void ABrainMiningCharacter::InitializePlayerScale()
{
	const AMiningTunnelGenerator* ScaleSource = nullptr;
	for (TActorIterator<AMiningTunnelGenerator> Generator(GetWorld()); Generator; ++Generator)
	{
		if (!Generator->PlayerScaleSettings.bEnabled) continue;
		if (ScaleSource)
		{
			UE_LOG(LogBrainMining, Warning, TEXT("Multiple enabled player scale sources; keeping original character settings. Enable Player Scale on only one generator."));
			return;
		}
		ScaleSource = *Generator;
	}
	if (!ScaleSource) return;
	const FMiningPlayerScaleSettings& Settings = ScaleSource->PlayerScaleSettings;
	const auto SafeValue = [](float Value, float Minimum, float Maximum)
	{
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, Minimum, Maximum) : 1.0f;
	};
	const float Scale = SafeValue(Settings.Scale, 0.25f, 2.0f);
	const float SpeedScale = Scale * SafeValue(Settings.MovementSpeedMultiplier, 0.25f, 3.0f);
	const float OriginalHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	SetActorScale3D(GetActorScale3D() * Scale);
	AddActorWorldOffset(FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - OriginalHalfHeight), false, nullptr, ETeleportType::TeleportPhysics);
	BaseEyeHeight *= Scale;
	CrouchedEyeHeight *= Scale;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed *= SpeedScale;
	Movement->MaxWalkSpeedCrouched *= SpeedScale;
	Movement->MaxAcceleration *= SpeedScale;
	Movement->BrakingDecelerationWalking *= SpeedScale;
	Movement->BrakingDecelerationFalling *= SpeedScale;
	Movement->MaxStepHeight *= Scale * SafeValue(Settings.StepHeightMultiplier, 0.25f, 3.0f);
	Movement->PerchAdditionalHeight *= Scale;
	Movement->PerchRadiusThreshold *= Scale;
	Movement->JumpZVelocity *= Scale * FMath::Sqrt(SafeValue(Settings.JumpHeightMultiplier, 0.25f, 3.0f));
	Movement->GravityScale *= Scale;
	MiningComponent->Radius *= Scale * SafeValue(Settings.MiningRadiusMultiplier, 0.25f, 3.0f);
	MiningComponent->Reach *= Scale * SafeValue(Settings.MiningReachMultiplier, 0.25f, 3.0f);
	MiningComponent->DownwardOffset *= Scale * SafeValue(Settings.MiningDownwardOffsetMultiplier, 0.0f, 3.0f);
	AppliedPlayerScale = Scale;
	if (UMiningCharacterMovementComponent* MiningMovement = Cast<UMiningCharacterMovementComponent>(Movement))
	{
		MiningMovement->FlightSizeScale = Scale;
		MiningMovement->FlightMovementScale = SpeedScale;
	}
}

void ABrainMiningCharacter::InitializePlayerMining()
{
	const AMiningTunnelGenerator* Source = nullptr;
	for (TActorIterator<AMiningTunnelGenerator> Generator(GetWorld()); Generator; ++Generator)
	{
		if (Source)
		{
			UE_LOG(LogBrainMining, Warning, TEXT("Multiple mining settings sources; preserving character blueprint mining values."));
			return;
		}
		Source = *Generator;
	}
	if (Source) Source->DiggingSettings.ApplyTo(*MiningComponent);
}

void ABrainMiningCharacter::InitializePlayerFlight()
{
	UMiningCharacterMovementComponent* Movement = Cast<UMiningCharacterMovementComponent>(GetCharacterMovement());
	if (!Movement) return;
	const AMiningTunnelGenerator* FlightSource = nullptr;
	for (TActorIterator<AMiningTunnelGenerator> Generator(GetWorld()); Generator; ++Generator)
	{
		if (FlightSource)
		{
			UE_LOG(LogBrainMining, Warning, TEXT("Multiple flight settings sources; keeping character blueprint flight settings. Use one mining generator per level."));
			return;
		}
		FlightSource = *Generator;
	}
	if (FlightSource) Movement->FlightSettings = FlightSource->FlightSettings.Sanitized();
}

void ABrainMiningCharacter::UnPossessed()
{
	DoJumpEnd();
	Super::UnPossessed();
}

void ABrainMiningCharacter::ConfigureMiningLighting(float HeadlampBrightness, float FillBrightness)
{
	MiningLight->SetUseInverseSquaredFalloff(false);
	MiningLight->SetIntensityUnits(ELightUnits::Unitless);
	MiningLight->SetLightFalloffExponent(2);
	MiningLight->SetIntensity(FMath::Clamp(HeadlampBrightness, 0.f, 20.f));
	MiningLight->SetRelativeLocation(FVector(-20, 0, 0));
	MiningLight->SetInnerConeAngle(45);
	MiningLight->SetOuterConeAngle(80);
	MiningLight->SetAttenuationRadius(2200);
	MiningFillLight->SetUseInverseSquaredFalloff(false);
	MiningFillLight->SetIntensityUnits(ELightUnits::Unitless);
	MiningFillLight->SetLightFalloffExponent(2);
	MiningFillLight->SetIntensity(FMath::Clamp(FillBrightness, 0.f, 10.f));
	MiningFillLight->SetVisibility(true);
}

void ABrainMiningCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		MiningComponent->BindInput(EnhancedInputComponent);
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ABrainMiningCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ABrainMiningCharacter::DoJumpEnd);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Canceled, this, &ABrainMiningCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABrainMiningCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABrainMiningCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ABrainMiningCharacter::LookInput);
	}
	else
	{
		UE_LOG(LogBrainMining, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void ABrainMiningCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void ABrainMiningCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void ABrainMiningCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ABrainMiningCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void ABrainMiningCharacter::DoJumpStart()
{
    if(IsMoveInputIgnored())return;
	if (UMiningCharacterMovementComponent* Movement = Cast<UMiningCharacterMovementComponent>(GetCharacterMovement()); Movement && Movement->IsFlightAvailable())
	{
		StopJumping();
		Movement->SetFlightInputHeld(true);
		return;
	}
	// pass Jump to the character
	Jump();
}

void ABrainMiningCharacter::DoJumpEnd()
{
	if (UMiningCharacterMovementComponent* Movement = Cast<UMiningCharacterMovementComponent>(GetCharacterMovement()))
	{
		Movement->SetFlightInputHeld(false);
	}
	// pass StopJumping to the character
	StopJumping();
}
