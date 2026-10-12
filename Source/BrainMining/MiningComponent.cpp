#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningBombToolComponent.h"
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
	if(!IsEncounterBlocked() && bInputContextEnabled)InputSubsystem->AddMappingContext(MiningContext, 1);
	Input->BindAction(MineAction, ETriggerEvent::Started, this, &UMiningComponent::StartMining);
	Input->BindAction(MineAction, ETriggerEvent::Completed, this, &UMiningComponent::StopMining);
	Input->BindAction(MineAction, ETriggerEvent::Canceled, this, &UMiningComponent::StopMining);
	Input->BindAction(ResetAction, ETriggerEvent::Started, this, &UMiningComponent::ResetField);
	Input->BindAction(PreviewAction, ETriggerEvent::Started, this, &UMiningComponent::TogglePreview);
}

float UMiningComponent::GetMiningInterval() const
{
	const float Base = FMath::IsFinite(Interval) ? FMath::Max(0.1f, Interval) : 0.3f;
	const float Multiplier = FMath::IsFinite(DrillIntervalMultiplier) ? FMath::Clamp(DrillIntervalMultiplier, 0.1f, 1.f) : 0.4f;
	return bDrillMode ? FMath::Max(0.04f, Base * Multiplier) : Base;
}

float UMiningComponent::GetMiningRadius() const
{
	const float Base = FMath::IsFinite(Radius) ? FMath::Max(1.f, Radius) : 160.f;
	const float Multiplier = FMath::IsFinite(DrillRadiusMultiplier) ? FMath::Clamp(DrillRadiusMultiplier, 1.f, 2.f) : 1.2f;
	return bDrillMode ? Base * Multiplier : Base;
}

void UMiningComponent::SetDrillMode(bool bNewDrillMode)
{
	if (bDrillMode == bNewDrillMode) return;
	// Require a fresh left click after switching: selecting a tool never drills
	// automatically or carries a held mining click across the handoff.
	StopMining();
	bDrillMode = bNewDrillMode;
	OnToolModeChanged.Broadcast(bDrillMode);
}

bool UMiningComponent::ActivateDrill()
{
	if (!GetWorld() || bInputSuspended || IsEncounterBlocked()) return false;
	UpdateDrillSession();
	if (bDrillMode || GetDrillCooldownRemaining() > 0) return false;
	if (auto* Bomb = GetOwner()->FindComponentByClass<UMiningBombToolComponent>()) Bomb->Stow();
	const double Now = GetWorld()->GetTimeSeconds();
	const float UseTime = FMath::IsFinite(DrillUseDuration) ? FMath::Max(.1f, DrillUseDuration) : 10.f;
	const float CooldownTime = FMath::IsFinite(DrillCooldownDuration) ? FMath::Max(.1f, DrillCooldownDuration) : 10.f;
	DrillUseEnds = Now + UseTime;
	DrillCooldownEnds = DrillUseEnds + CooldownTime;
	SetDrillMode(true);
	return true;
}

void UMiningComponent::SelectPickaxe()
{
	StopMining();
	SetDrillMode(false);
}

float UMiningComponent::GetDrillUseRemaining() const
{
	return bDrillMode && GetWorld() ? FMath::Max(0.0, DrillUseEnds - GetWorld()->GetTimeSeconds()) : 0.f;
}

float UMiningComponent::GetDrillCooldownRemaining() const
{
	return !bDrillMode && GetWorld() ? FMath::Max(0.0, DrillCooldownEnds - GetWorld()->GetTimeSeconds()) : 0.f;
}

void UMiningComponent::UpdateDrillSession()
{
	if (bDrillMode && GetWorld()->GetTimeSeconds() >= DrillUseEnds) SetDrillMode(false);
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
	UpdateDrillSession();
    if(IsEncounterBlocked())return;
	RefreshTarget();
	if (bShowPreview && IsValid(TargetRock))
	{
		const FVector Center = GetCutCenter();
		DrawDebugSphere(GetWorld(), Center, GetMiningRadius(), 16, FColor(60, 220, 210), false, -1.0f, 0, 0.6f);
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
    if (const auto* Bomb = GetOwner()->FindComponentByClass<UMiningBombToolComponent>(); Bomb && Bomb->bEquipped) return false;
    if(IsEncounterBlocked())return false;
	if (bInputSuspended || GetWorld()->GetTimeSeconds() < NextMineTime)
	{
		return false;
	}
	const float MiningInterval = GetMiningInterval();
	NextMineTime = GetWorld()->GetTimeSeconds() + MiningInterval;
	OnMiningAttempt.Broadcast(MiningInterval);
	RefreshTarget();
	if (!IsValid(TargetRock))
	{
		Status = TEXT("Aim at rock within reach");
		return false;
	}
	LastMinedRock = TargetRock;
	const FVector Center = GetCutCenter();
	const FHitResult MiningHit = TargetHit;
	const bool bResult = TargetRock->MineSphere(Center, GetMiningRadius());
	Status = TargetRock->LastStatus;
	if (bResult) OnMiningHit.Broadcast(MiningHit);
	return bResult;
}

void UMiningComponent::StartMining()
{
    if(IsEncounterBlocked())return;
	if (bInputSuspended) return;
	if (auto* Bomb = GetOwner()->FindComponentByClass<UMiningBombToolComponent>(); Bomb && Bomb->bEquipped)
	{
		Bomb->ThrowBomb();
		return;
	}
	bMiningHeld = true;
	TryMine();
}

FVector UMiningComponent::GetCutCenter() const
{
	return TargetHit.ImpactPoint - TargetHit.ImpactNormal * GetMiningRadius() * InsetFraction - FVector(0.0, 0.0, DownwardOffset);
}

void UMiningComponent::StopMining()
{
	bMiningHeld = false;
}

void UMiningComponent::SetInputSuspended(bool bSuspended)
{
	bInputSuspended = bSuspended;
	if (bSuspended) StopMining();
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
	DrillUseEnds = DrillCooldownEnds = 0;
	SetDrillMode(false);
	if (bHasInitialTransform && !bScopedReset)
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
		if (bScopedReset && !ResetBounds.IsInsideOrOn(Rock->GetActorLocation())) continue;
		Rock->ResetRock();
	}
	for (TActorIterator<AMiningTunnelGenerator> Generator(GetWorld()); Generator; ++Generator)
	{
		if (bScopedReset && !ResetBounds.IsInsideOrOn(Generator->GetActorLocation())) continue;
		Generator->ResetProgression();
	}
	NextMineTime = 0.0;
	LastMinedRock = nullptr;
	TargetRock = nullptr;
	Status = TEXT("Field reset");
	OnMiningReset.Broadcast();
	OnFieldReset.Broadcast();
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
    if(!IsEncounterBlocked() && bInputContextEnabled && InputSubsystem.IsValid() && MiningContext)InputSubsystem->AddMappingContext(MiningContext,1);
}

void UMiningComponent::SetInputContextEnabled(bool bEnabled)
{
    bInputContextEnabled = bEnabled;
    CancelMining();
    if (!InputSubsystem.IsValid() || !MiningContext) return;
    if (bEnabled && !IsEncounterBlocked()) InputSubsystem->AddMappingContext(MiningContext, 1);
    else InputSubsystem->RemoveMappingContext(MiningContext);
}
