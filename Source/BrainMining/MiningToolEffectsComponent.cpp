#include "MiningToolEffectsComponent.h"
#include "MiningComponent.h"
#include "MiningDebrisBurst.h"
#include "BrainMiningCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

UMiningToolEffectsComponent::UMiningToolEffectsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PickaxeMeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Pickaxe/SM_Pickaxe.SM_Pickaxe")));
	SwingCurveAsset = TSoftObjectPtr<UCurveFloat>(FSoftObjectPath(TEXT("/Game/Pickaxe/CF_PickaxeSwing.CF_PickaxeSwing")));
	DebrisClass = AMiningDebrisBurst::StaticClass();
	for (const TCHAR* Size : { TEXT("Large"), TEXT("Small") })
		for (int32 Index = 1; Index <= 4; ++Index)
		{
			const FString Name = FString::Printf(TEXT("SM_Debris_Concrete_%s_%d"), Size, Index);
			DebrisMeshAssets.Add(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(TEXT("/Game/Debris/Meshes/%s.%s"), *Name, *Name))));
		}
}

void UMiningToolEffectsComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer || !bEnabled) return;
	Mining = GetOwner()->FindComponentByClass<UMiningComponent>();
	const ABrainMiningCharacter* Character = Cast<ABrainMiningCharacter>(GetOwner());
	UCameraComponent* Camera = Character ? Character->GetFirstPersonCameraComponent() : GetOwner()->FindComponentByClass<UCameraComponent>();
	if (!Mining || !Camera) return;
	UStaticMesh* Mesh = PickaxeMeshAsset.LoadSynchronous();
	SwingCurve = SwingCurveAsset.LoadSynchronous();
	LoadedDebrisClass = DebrisClass.LoadSynchronous();
	for (const TSoftObjectPtr<UStaticMesh>& Asset : DebrisMeshAssets)
		if (UStaticMesh* DebrisMesh = Asset.LoadSynchronous()) LoadedDebrisMeshes.Add(DebrisMesh);
	if (!Mesh || !SwingCurve || !LoadedDebrisClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Mining tool effects assets are missing; mining remains available."));
		return;
	}
	PickaxePivot = NewObject<USceneComponent>(GetOwner(), TEXT("MiningPickaxePivot"));
	GetOwner()->AddInstanceComponent(PickaxePivot);
	PickaxePivot->SetupAttachment(Camera);
	PickaxePivot->SetRelativeLocation(FVector(40, 20, -35));
	PickaxePivot->RegisterComponent();
	PickaxeMesh = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("MiningPickaxeMesh"));
	GetOwner()->AddInstanceComponent(PickaxeMesh);
	PickaxeMesh->SetupAttachment(PickaxePivot);
	PickaxeMesh->SetStaticMesh(Mesh);
	// Match test109's camera-mounted mesh pose and material, without replacing the player.
	PickaxeMesh->SetRelativeLocation(FVector(-9.885566, -0.907176, 1.205274));
	PickaxeMesh->SetRelativeRotation(FRotator(-6.922544, 5.243221, -4.019591));
	PickaxeMesh->SetRelativeScale3D(FVector(0.65));
	PickaxeMesh->SetOnlyOwnerSee(true);
	PickaxeMesh->SetCastShadow(false);
	PickaxeMesh->SetCollisionProfileName(TEXT("NoCollision"));
	PickaxeMesh->RegisterComponent();
	Mining->OnMiningAttempt.AddDynamic(this, &UMiningToolEffectsComponent::PlaySwing);
	Mining->OnMiningHit.AddDynamic(this, &UMiningToolEffectsComponent::QueueImpact);
	Mining->OnMiningReset.AddDynamic(this, &UMiningToolEffectsComponent::ResetEffects);
}

bool UMiningToolEffectsComponent::CanShowEffects() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return bEnabled && IsValid(PickaxePivot) && IsValid(Mining) && Pawn && Pawn->IsLocallyControlled();
}

void UMiningToolEffectsComponent::PlaySwing(float MiningInterval)
{
	if (!CanShowEffects()) return;
	GetWorld()->GetTimerManager().ClearTimer(ImpactTimer);
	PendingHit = FHitResult();
	PendingSurfaceMaterial = nullptr;
	// Fit the source curve into the existing dig cadence; the dig timing itself is unchanged.
	SwingDuration = FMath::Min(SourceSwingDuration, FMath::Max(0.1f, FMath::IsFinite(MiningInterval) ? MiningInterval : 0.3f));
	SwingStart = GetWorld()->GetTimeSeconds();
	bSwinging = true;
	++SwingCount;
	PickaxePivot->SetRelativeRotation(FRotator::ZeroRotator);
}

void UMiningToolEffectsComponent::QueueImpact(const FHitResult& Hit)
{
	if (!CanShowEffects() || !bSwinging || !Hit.bBlockingHit || Hit.ImpactPoint.ContainsNaN() || Hit.ImpactNormal.ContainsNaN()) return;
	PendingHit = Hit;
	PendingSurfaceMaterial = Hit.GetComponent() ? Hit.GetComponent()->GetMaterial(0) : nullptr;
	GetWorld()->GetTimerManager().SetTimer(ImpactTimer, this, &UMiningToolEffectsComponent::SpawnImpact, SwingDuration * SourceImpactTime / SourceSwingDuration, false);
}

void UMiningToolEffectsComponent::SpawnImpact()
{
	if (!CanShowEffects() || !LoadedDebrisClass || !PendingHit.bBlockingHit) return;
	const FVector Normal = PendingHit.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const FTransform Transform(FRotationMatrix::MakeFromZ(Normal).ToQuat(), PendingHit.ImpactPoint + Normal * 6);
	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.Instigator = Cast<APawn>(GetOwner());
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Burst = GetWorld()->SpawnActor<AActor>(LoadedDebrisClass, Transform, Params);
	if (AMiningDebrisBurst* SoilBurst = Cast<AMiningDebrisBurst>(Burst))
	{
		const int32 Count = FMath::RandRange(FMath::Clamp(DebrisChunkCount - 6, 12, 64), FMath::Clamp(DebrisChunkCount + 6, 12, 64));
		SoilBurst->Initialize(LoadedDebrisMeshes, PendingSurfaceMaterial, Normal, Mining->Radius, Count, DebrisLifetime);
	}
	PendingHit = FHitResult();
	PendingSurfaceMaterial = nullptr;
	if (!IsValid(Burst) || Burst->IsActorBeingDestroyed()) return;
	// Soil chunks collide with the environment, but never with mining traces or the player.
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Burst);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		Primitive->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		Primitive->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Primitive->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	if (Burst->GetLifeSpan() <= 0) Burst->SetLifeSpan(DebrisLifetime);
	DebrisActors.RemoveAll([](const TWeakObjectPtr<AActor>& Actor) { return !Actor.IsValid(); });
	// Bound the physics cost even if a level uses a very short mining interval.
	while (DebrisActors.Num() >= 8)
	{
		if (DebrisActors[0].IsValid()) DebrisActors[0]->Destroy();
		DebrisActors.RemoveAt(0);
	}
	DebrisActors.Add(Burst);
	++DebrisBurstCount;
}

void UMiningToolEffectsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (!CanShowEffects())
	{
		if (bSwinging || GetWorld()->GetTimerManager().IsTimerActive(ImpactTimer) || !DebrisActors.IsEmpty()) ResetEffects();
		if (PickaxeMesh) PickaxeMesh->SetVisibility(false);
		return;
	}
	PickaxeMesh->SetVisibility(true);
	if (!bSwinging) return;
	const float Elapsed = GetWorld()->GetTimeSeconds() - SwingStart;
	if (Elapsed >= SwingDuration)
	{
		bSwinging = false;
		PickaxePivot->SetRelativeRotation(FRotator::ZeroRotator);
		return;
	}
	PickaxePivot->SetRelativeRotation(FRotator(-SwingCurve->GetFloatValue(Elapsed * SourceSwingDuration / SwingDuration), 0, 0));
}

void UMiningToolEffectsComponent::ResetEffects()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ImpactTimer);
	PendingHit = FHitResult();
	PendingSurfaceMaterial = nullptr;
	bSwinging = false;
	if (PickaxePivot) PickaxePivot->SetRelativeRotation(FRotator::ZeroRotator);
	for (const TWeakObjectPtr<AActor>& Actor : DebrisActors) if (Actor.IsValid()) Actor->Destroy();
	DebrisActors.Reset();
}

void UMiningToolEffectsComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ResetEffects();
	if (Mining)
	{
		Mining->OnMiningAttempt.RemoveDynamic(this, &UMiningToolEffectsComponent::PlaySwing);
		Mining->OnMiningHit.RemoveDynamic(this, &UMiningToolEffectsComponent::QueueImpact);
		Mining->OnMiningReset.RemoveDynamic(this, &UMiningToolEffectsComponent::ResetEffects);
	}
	Super::EndPlay(Reason);
}
