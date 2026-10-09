#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningTunnelGenerator.h"
#include "DiggableRock.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"

UMiningComponent::UMiningComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UMiningComponent::BeginPlay()
{
	Super::BeginPlay();
	InitialTransform = GetOwner()->GetActorTransform();
	bHasInitialTransform = true;
}

void UMiningComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (InputSubsystem.IsValid() && MiningContext)
	{
		InputSubsystem->RemoveMappingContext(MiningContext);
	}
	Super::EndPlay(EndPlayReason);
}

void UMiningComponent::BindInput(UEnhancedInputComponent* Input)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!Input || !Controller || !Controller->IsLocalController())
	{
		return;
	}
	ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}
	InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!InputSubsystem.IsValid())
	{
		return;
	}
	if (!MiningContext)
	{
		MiningContext = NewObject<UInputMappingContext>(this);
		MineAction = NewObject<UInputAction>(this);
		ResetAction = NewObject<UInputAction>(this);
		PreviewAction = NewObject<UInputAction>(this);
		MiningContext->MapKey(MineAction, EKeys::LeftMouseButton);
		MiningContext->MapKey(ResetAction, EKeys::R);
		MiningContext->MapKey(PreviewAction, EKeys::F1);
	}
	if(!IsEncounterBlocked())InputSubsystem->AddMappingContext(MiningContext, 1);
	Input->BindAction(MineAction, ETriggerEvent::Started, this, &UMiningComponent::StartMining);
	Input->BindAction(MineAction, ETriggerEvent::Completed, this, &UMiningComponent::StopMining);
	Input->BindAction(MineAction, ETriggerEvent::Canceled, this, &UMiningComponent::StopMining);
	Input->BindAction(ResetAction, ETriggerEvent::Started, this, &UMiningComponent::ResetField);
	Input->BindAction(PreviewAction, ETriggerEvent::Started, this, &UMiningComponent::TogglePreview);
}

void UMiningComponent::RefreshTarget()
{
	TargetRock = nullptr;
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(MiningTrace), true, GetOwner());
	if (GetWorld()->LineTraceSingleByChannel(TargetHit, ViewLocation, ViewLocation + ViewRotation.Vector() * Reach, ECC_Visibility, Query))
	{
		TargetRock = Cast<ADiggableRock>(TargetHit.GetActor());
	}
}

void UMiningComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if(IsEncounterBlocked())return;
	RefreshTarget();
	if (bShowPreview && IsValid(TargetRock))
	{
		const FVector Center = GetCutCenter();
		DrawDebugSphere(GetWorld(), Center, Radius, 16, FColor(60, 220, 210), false, -1.0f, 0, 0.6f);
	}
	if (bMiningHeld && GetWorld()->GetTimeSeconds() >= NextMineTime)
	{
		TryMine();
	}
	if (bHasInitialTransform && FallResetDistance > 0.0f && GetOwner()->GetActorLocation().Z < InitialTransform.GetLocation().Z - FallResetDistance)
	{
		ResetField();
	}
}

bool UMiningComponent::TryMine()
{
    if(IsEncounterBlocked())return false;
	if (GetWorld()->GetTimeSeconds() < NextMineTime)
	{
		return false;
	}
	NextMineTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, Interval);
	RefreshTarget();
	if (!IsValid(TargetRock))
	{
		Status = TEXT("Aim at rock within reach");
		return false;
	}
	LastMinedRock = TargetRock;
	const FVector Center = GetCutCenter();
	const bool bResult = TargetRock->MineSphere(Center, Radius);
	Status = TargetRock->LastStatus;
	return bResult;
}

void UMiningComponent::StartMining()
{
    if(IsEncounterBlocked())return;
	bMiningHeld = true;
	TryMine();
}

FVector UMiningComponent::GetCutCenter() const
{
	return TargetHit.ImpactPoint - TargetHit.ImpactNormal * Radius * InsetFraction - FVector(0.0, 0.0, DownwardOffset);
}

void UMiningComponent::StopMining()
{
	bMiningHeld = false;
}

void UMiningComponent::TogglePreview()
{
    if(IsEncounterBlocked())return;
	bShowPreview = !bShowPreview;
}

void UMiningComponent::ResetField()
{
    if(IsEncounterBlocked())return;
	StopMining();
	if (bHasInitialTransform)
	{
		GetOwner()->SetActorTransform(InitialTransform, false, nullptr, ETeleportType::TeleportPhysics);
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			Character->StopJumping();
			if (UMiningCharacterMovementComponent* Movement = Cast<UMiningCharacterMovementComponent>(Character->GetCharacterMovement()))
			{
				Movement->SetFlightInputHeld(false);
			}
			Character->GetCharacterMovement()->StopMovementImmediately();
			if (AController* Controller = Character->GetController())
			{
				Controller->SetControlRotation(InitialTransform.Rotator());
			}
		}
	}
	for (TActorIterator<ADiggableRock> Rock(GetWorld()); Rock; ++Rock)
	{
		Rock->ResetRock();
	}
	for (TActorIterator<AMiningTunnelGenerator> Generator(GetWorld()); Generator; ++Generator)
	{
		Generator->ResetProgression();
	}
	NextMineTime = 0.0;
	LastMinedRock = nullptr;
	TargetRock = nullptr;
	Status = TEXT("Field reset");
}

void UMiningComponent::PushEncounterBlock()
{
    ++EncounterBlockCount;CancelMining();TargetRock=nullptr;
    if(InputSubsystem.IsValid() && MiningContext)InputSubsystem->RemoveMappingContext(MiningContext);
}
void UMiningComponent::PopEncounterBlock()
{
    if(EncounterBlockCount==0)return;
    --EncounterBlockCount;
    if(!IsEncounterBlocked() && InputSubsystem.IsValid() && MiningContext)InputSubsystem->AddMappingContext(MiningContext,1);
}
