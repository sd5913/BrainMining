#include "MiningBombToolComponent.h"
#include "MiningBombActor.h"
#include "MiningComponent.h"
#include "BrainMiningCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraSystem.h"

UMiningBombToolComponent::UMiningBombToolComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Default presentation is our compact faceted blast. Artists may assign an
	// optional Niagara system; the realistic showcase explosion is not the default.
}

void UMiningBombToolComponent::BeginPlay()
{
	Super::BeginPlay();
	LoadedExplosion = ExplosionAsset.LoadSynchronous();
	if (auto* Mining = GetOwner()->FindComponentByClass<UMiningComponent>())
		Mining->OnMiningReset.AddUniqueDynamic(this, &ThisClass::ResetBombs);
	if (auto* Character = Cast<ABrainMiningCharacter>(GetOwner()))
	{
		Character->GetBombModel()->SetOnlyOwnerSee(true);
		Character->GetBombModel()->SetHiddenInGame(false);
		Character->GetBombModel()->SetVisibility(false);
	}
}

bool UMiningBombToolComponent::CanUse() const
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	const auto* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const auto* Mining = GetOwner()->FindComponentByClass<UMiningComponent>();
	return PC && PC->IsLocalController() && !PC->IsPaused() && !PC->IsMoveInputIgnored()
		&& !PC->IsLookInputIgnored() && Mining && !Mining->IsInputSuspended() && !Mining->IsEncounterBlocked();
}

bool UMiningBombToolComponent::ToggleEquipped()
{
	if (!CanUse()) return false;
	if (bEquipped) { Stow(); return true; }
	GetOwner()->FindComponentByClass<UMiningComponent>()->SelectPickaxe();
	bEquipped = true;
	return true;
}

void UMiningBombToolComponent::Stow()
{
	bEquipped = false;
	if (auto* Character = Cast<ABrainMiningCharacter>(GetOwner())) Character->GetBombModel()->SetVisibility(false);
}

bool UMiningBombToolComponent::ThrowBomb()
{
	if (!bEquipped || !CanUse()) return false;
	auto* Character = Cast<ABrainMiningCharacter>(GetOwner());
	UStaticMesh* Mesh = Character ? Character->GetBombModel()->GetStaticMesh() : nullptr;
	if (!Mesh) return false;
	auto* PC = Cast<APlayerController>(Character->GetController());
	FVector Eye; FRotator View;
	PC->GetPlayerViewPoint(Eye, View);
	const FVector Forward = View.Vector();
	const float Radius = FMath::Clamp(Settings.CollisionRadius, 1.f, 50.f);
	const FVector Desired = Eye + Forward * 105.f + View.RotateVector(FVector(0, 10, -7));
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BombThrowClearance), false, Character);
	FHitResult Hit;
	FVector Position = Desired;
	if (GetWorld()->SweepSingleByChannel(Hit, Eye, Desired, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Radius), Query))
	{
		if (Hit.bStartPenetrating || FVector::Dist(Eye, Hit.Location) < Radius * 2.f) return false;
		Position = Hit.Location - Forward * 2.f;
	}
	FActorSpawnParameters Spawn;
	Spawn.Owner = Character;
	Spawn.Instigator = Character;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Bomb = GetWorld()->SpawnActor<AMiningBombActor>(Position, View, Spawn);
	if (!Bomb) return false;
	const FVector Velocity = Forward * FMath::Clamp(Settings.ThrowSpeed, 100.f, 4000.f)
		+ FVector::UpVector * FMath::Clamp(Settings.ThrowUpSpeed, 0.f, 1000.f)
		+ Character->GetVelocity() * FMath::Clamp(Settings.InheritedVelocity, 0.f, 1.f);
	Bomb->Launch(Mesh, LoadedExplosion, this, Settings, Velocity);
	LastThrownBomb = Bomb;
	ThrownBombs.RemoveAll([](const auto& Entry) { return !Entry.IsValid(); });
	ThrownBombs.Add(Bomb);
	Stow();
	return true;
}

void UMiningBombToolComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (auto* Character = Cast<ABrainMiningCharacter>(GetOwner())) Character->GetBombModel()->SetVisibility(bEquipped && CanUse());
}

void UMiningBombToolComponent::RecordExplosion(const FVector& Location)
{
	LastExplosionLocation = Location;
	LastExplosionTime = GetWorld()->GetTimeSeconds();
	++ExplosionCount;
	OnBombExploded.Broadcast(Location);
}

void UMiningBombToolComponent::ResetBombs()
{
	Stow();
	for (const auto& Bomb : ThrownBombs) if (Bomb.IsValid()) Bomb->Destroy();
	ThrownBombs.Reset();
}

void UMiningBombToolComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ResetBombs();
	Super::EndPlay(Reason);
}
