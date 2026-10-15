#include "MiningToolEffectsComponent.h"
#include "MiningComponent.h"
#include "MiningBombToolComponent.h"
#include "MiningDebrisBurst.h"
#include "MiningDrillDustBurst.h"
#include "BrainMiningCharacter.h"
#include "EchoPassagePlayer.h"
#include "GameFramework/Controller.h"
#include "Materials/MaterialInterface.h"
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
	EchoPickaxeBlackAsset=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Passages/Echo/Materials/M_SharedPickaxeBlack.M_SharedPickaxeBlack")));
	EchoPickaxeLinesAsset=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Passages/Echo/Materials/M_SharedPickaxeLines.M_SharedPickaxeLines")));
	PickaxeMeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Pickaxe/SM_Pickaxe.SM_Pickaxe")));
	SwingCurveAsset = TSoftObjectPtr<UCurveFloat>(FSoftObjectPath(TEXT("/Game/Pickaxe/CF_PickaxeSwing.CF_PickaxeSwing")));
	DrillMeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Drill/Stylized/SM_DrillStylized.SM_DrillStylized")));
	DrillSpinCurveAsset = TSoftObjectPtr<UCurveFloat>(FSoftObjectPath(TEXT("/Game/Drill/CF_DrillSpin.CF_DrillSpin")));
	DrillDustMaterialAsset = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Drill/M_DrillDust.M_DrillDust")));
	DrillSmokeMaterialAsset = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Drill/M_DrillSmoke.M_DrillSmoke")));
	DrillSparkMaterialAsset = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Drill/M_DrillSparks.M_DrillSparks")));
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
	// Introductory pickups can enable this component after BeginPlay. Initialize
	// its meshes and delegates even while presentation is temporarily locked.
	if (GetNetMode() == NM_DedicatedServer) return;
	Mining = GetOwner()->FindComponentByClass<UMiningComponent>();
	const ABrainMiningCharacter* Character = Cast<ABrainMiningCharacter>(GetOwner());
	UCameraComponent* Camera = Character ? Character->GetFirstPersonCameraComponent() : GetOwner()->FindComponentByClass<UCameraComponent>();
	if (!Mining || !Camera) return;
	UStaticMesh* Mesh = Character ? Character->GetPickaxeModel()->GetStaticMesh().Get() : PickaxeMeshAsset.LoadSynchronous();
	SwingCurve = SwingCurveAsset.LoadSynchronous();
	DrillSpinCurve = DrillSpinCurveAsset.LoadSynchronous();
	LoadedDebrisClass = DebrisClass.LoadSynchronous();
	LoadedDustMaterial = DrillDustMaterialAsset.LoadSynchronous();
	LoadedSmokeMaterial = DrillSmokeMaterialAsset.LoadSynchronous();
	LoadedSparkMaterial = DrillSparkMaterialAsset.LoadSynchronous();
	DustPlane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	for (const TSoftObjectPtr<UStaticMesh>& Asset : DebrisMeshAssets)
		if (UStaticMesh* DebrisMesh = Asset.LoadSynchronous()) LoadedDebrisMeshes.Add(DebrisMesh);
	if (!Mesh || !SwingCurve || !LoadedDebrisClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Mining tool effects assets are missing; mining remains available."));
		return;
	}
	if (Character)
	{
		bUsesCharacterComponents = true;
		PickaxePivot = Character->GetPickaxeMotion();
		PickaxeMesh = Character->GetPickaxeModel();
		BombPivot = Character->GetBombMotion();
		DrillPivot = Character->GetDrillRightMotion();
		DrillMesh = Character->GetDrillRightModel();
		LeftDrillPivot = Character->GetDrillLeftMotion();
		LeftDrillMesh = Character->GetDrillLeftModel();
	}
	else
	{
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
		if (UStaticMesh* Drill = DrillMeshAsset.LoadSynchronous())
		{
			DrillSpinCurve = DrillSpinCurveAsset.LoadSynchronous();
			DrillPivot = NewObject<USceneComponent>(GetOwner(), TEXT("MiningDrillPivot"));
			GetOwner()->AddInstanceComponent(DrillPivot);
			DrillPivot->SetupAttachment(Camera);
			DrillPivot->SetRelativeLocation(DrillLocation);
			DrillPivot->SetRelativeRotation(DrillRotation);
			DrillPivot->RegisterComponent();
			DrillMesh = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("MiningDrillMesh"));
			GetOwner()->AddInstanceComponent(DrillMesh);
			DrillMesh->SetupAttachment(DrillPivot);
			DrillMesh->SetStaticMesh(Drill);
			DrillMesh->SetRelativeScale3D(FVector(DrillScale));
			DrillMesh->SetOnlyOwnerSee(true);
			DrillMesh->SetCastShadow(false);
			DrillMesh->SetCollisionProfileName(TEXT("NoCollision"));
			DrillMesh->RegisterComponent();
			LeftDrillPivot = NewObject<USceneComponent>(GetOwner(), TEXT("MiningLeftDrillPivot"));
			GetOwner()->AddInstanceComponent(LeftDrillPivot);
			LeftDrillPivot->SetupAttachment(Camera);
			LeftDrillPivot->RegisterComponent();
			LeftDrillMesh = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("MiningLeftDrillMesh"));
			GetOwner()->AddInstanceComponent(LeftDrillMesh);
			LeftDrillMesh->SetupAttachment(LeftDrillPivot);
			LeftDrillMesh->SetStaticMesh(Drill);
			LeftDrillMesh->SetRelativeScale3D(FVector(DrillScale, -DrillScale, DrillScale));
			LeftDrillMesh->SetOnlyOwnerSee(true);
			LeftDrillMesh->SetCastShadow(false);
			LeftDrillMesh->SetCollisionProfileName(TEXT("NoCollision"));
			LeftDrillMesh->RegisterComponent();
			LastDrillAssetPath = DrillMeshAsset.ToSoftObjectPath();
		}
	}
	for (UStaticMeshComponent* Model : { PickaxeMesh.Get(), DrillMesh.Get(), LeftDrillMesh.Get() })
		if (Model)
		{
			Model->SetOnlyOwnerSee(true);
			Model->SetHiddenInGame(false);
		}
	Mining->OnMiningAttempt.AddDynamic(this, &UMiningToolEffectsComponent::PlaySwing);
	Mining->OnMiningHit.AddDynamic(this, &UMiningToolEffectsComponent::QueueImpact);
	Mining->OnMiningReset.AddDynamic(this, &UMiningToolEffectsComponent::ResetEffects);
	Mining->OnToolModeChanged.AddDynamic(this, &UMiningToolEffectsComponent::UpdateToolMode);
	UpdateToolMode(Mining->bDrillMode);
	EquipElapsed = ToolEquipDuration;
	LastPresentedTool = Mining->bDrillMode ? 1 : 0;
	EchoPickaxeBlack=EchoPickaxeBlackAsset.LoadSynchronous();
	EchoPickaxeLines=EchoPickaxeLinesAsset.LoadSynchronous();
}

void UMiningToolEffectsComponent::UpdateEchoPickaxeStyle()
{
	if(!PickaxeMesh)return;
	const auto* Pawn=Cast<APawn>(GetOwner());
	const auto* Controller=Pawn?Pawn->GetController():nullptr;
	const auto* Echo=Controller?Controller->FindComponentByClass<UEchoPassageInteractionComponent>():nullptr;
	// GiantRoomEntrance already turns exploration off at the warm doorway and
	// back on when walking back into the passage. Share that exact boundary.
	const bool bLines=Echo && Echo->Stage && Echo->bExplorationActive && EchoPickaxeBlack && EchoPickaxeLines;
	if(bLines==bEchoPickaxeLines)return;
	if(bLines)
	{
		NormalPickaxeMaterials.Reset();
		for(int Slot=0;Slot<PickaxeMesh->GetNumMaterials();++Slot)
		{NormalPickaxeMaterials.Add(PickaxeMesh->GetMaterial(Slot));PickaxeMesh->SetMaterial(Slot,EchoPickaxeBlack);}
		NormalPickaxeOverlay=PickaxeMesh->GetOverlayMaterial();
		PickaxeMesh->SetOverlayMaterial(EchoPickaxeLines);
	}
	else
	{
		for(int Slot=0;Slot<NormalPickaxeMaterials.Num();++Slot)PickaxeMesh->SetMaterial(Slot,NormalPickaxeMaterials[Slot]);
		PickaxeMesh->SetOverlayMaterial(NormalPickaxeOverlay);
		NormalPickaxeMaterials.Reset();NormalPickaxeOverlay=nullptr;
	}
	bEchoPickaxeLines=bLines;
}

void UMiningToolEffectsComponent::UpdateToolMode(bool bDrillMode)
{
	PendingHit = FHitResult();
	PendingSurfaceMaterial = nullptr;
	bSwinging = false;
	bDrillRunning = false;
	EquipElapsed = 0;
	ApplyToolMotion(PickaxePivot, FVector(40, 20, -35), FRotator::ZeroRotator, FVector::ZeroVector, FRotator::ZeroRotator);
	if (PickaxeMesh) PickaxeMesh->SetVisibility(CanShowEffects() && !bDrillMode);
	if (DrillMesh) DrillMesh->SetVisibility(CanShowEffects() && bDrillMode);
	if (LeftDrillMesh) LeftDrillMesh->SetVisibility(CanShowEffects() && bDrillMode && bDualDrills);
}

bool UMiningToolEffectsComponent::CanShowEffects() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return bEnabled && IsValid(PickaxePivot) && IsValid(Mining) && Pawn && Pawn->IsLocallyControlled();
}

void UMiningToolEffectsComponent::PlaySwing(float MiningInterval)
{
	if (!CanShowEffects()) return;
	PendingHit = FHitResult();
	PendingSurfaceMaterial = nullptr;
	if (Mining->bDrillMode) return;
	SwingDuration = FMath::Min(SourceSwingDuration, FMath::Max(0.1f, FMath::IsFinite(MiningInterval) ? MiningInterval : 0.3f));
	SwingStart = GetWorld()->GetTimeSeconds();
	bSwinging = true;
	++SwingCount;
	ApplyToolMotion(PickaxePivot, FVector(40, 20, -35), FRotator::ZeroRotator, FVector::ZeroVector, FRotator::ZeroRotator);
}

void UMiningToolEffectsComponent::QueueImpact(const FHitResult& Hit)
{
	if (!CanShowEffects() || !Hit.bBlockingHit || Hit.ImpactPoint.ContainsNaN() || Hit.ImpactNormal.ContainsNaN()) return;
	PendingHit = Hit;
	PendingSurfaceMaterial = Hit.GetComponent() ? Hit.GetComponent()->GetMaterial(0) : nullptr;
	bPendingDrillImpact = Mining->bDrillMode;
	SpawnImpact();
}

void UMiningToolEffectsComponent::WarmDebrisPool()
{
	if (LoadedDebrisClass != AMiningDebrisBurst::StaticClass() || LoadedDebrisMeshes.IsEmpty()) return;
	DebrisPool.RemoveAll([](const TObjectPtr<AMiningDebrisBurst>& Burst) { return !IsValid(Burst); });
	for (AMiningDebrisBurst* Burst : DebrisPool)
	{
		if (IsValid(Burst) && !Burst->IsPoolActive() && Burst->GetPreparedChunkCount() < 64)
		{
			Burst->PreparePool(LoadedDebrisMeshes, 4);
			return;
		}
	}
	if (DebrisPool.Num() >= 4) return;
	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Burst = GetWorld()->SpawnActor<AMiningDebrisBurst>(GetOwner()->GetActorLocation(), FRotator::ZeroRotator, Params);
	if (Burst)
	{
		Burst->PreparePool(LoadedDebrisMeshes, 4);
		DebrisPool.Add(Burst);
	}
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
	if (bPendingDrillImpact && DustPlane &&
		((DrillDustStrength > 0 && DrillDustParticleCount > 0 && LoadedDustMaterial) ||
		 (DrillSmokeStrength > 0 && DrillSmokeParticleCount > 0 && LoadedSmokeMaterial) ||
		 (DrillSparkStrength > 0 && DrillSparkParticleCount > 0 && LoadedSparkMaterial)))
	{
		FActorSpawnParameters DustParams = Params;
		DustParams.Owner = Cast<APawn>(GetOwner())->GetController();
		if (AMiningDrillDustBurst* Dust = GetWorld()->SpawnActor<AMiningDrillDustBurst>(Transform.GetLocation(), FRotator::ZeroRotator, DustParams))
		{
			FMiningDrillContactSettings Settings;
			Settings.DustCount = DrillDustParticleCount;
			Settings.SmokeCount = DrillSmokeParticleCount;
			Settings.SparkCount = DrillSparkParticleCount;
			Settings.DustStrength = DrillDustStrength;
			Settings.SmokeStrength = DrillSmokeStrength;
			Settings.SparkStrength = DrillSparkStrength;
			Settings.DustSize = DrillDustSize;
			Settings.SmokeSize = DrillSmokeSize;
			Settings.SparkSize = DrillSparkSize;
			Settings.LifetimeScale = DrillContactLifetimeScale;
			Settings.SparkSpeed = DrillSparkSpeed;
			Settings.SparkBrightness = DrillSparkBrightness;
			Dust->Initialize(DustPlane, LoadedDustMaterial, LoadedSmokeMaterial, LoadedSparkMaterial, PendingSurfaceMaterial, Normal, Mining->GetMiningRadius(), DrillDustColor, Settings);
			DustActors.RemoveAll([](const TWeakObjectPtr<AActor>& Actor) { return !Actor.IsValid(); });
			while (DustActors.Num() >= 12)
			{
				if (DustActors[0].IsValid()) DustActors[0]->Destroy();
				DustActors.RemoveAt(0);
			}
			DustActors.Add(Dust);
		}
	}
	AActor* Burst = nullptr;
	if (LoadedDebrisClass == AMiningDebrisBurst::StaticClass())
	{
		for (AMiningDebrisBurst* Candidate : DebrisPool)
			if (IsValid(Candidate) && !Candidate->IsPoolActive() && Candidate->GetPreparedChunkCount() > 0) { Burst = Candidate; break; }
		if (!Burst && !DebrisPool.IsEmpty()) Burst = DebrisPool[NextDebrisPoolIndex++ % DebrisPool.Num()];
		if (auto* Pooled = Cast<AMiningDebrisBurst>(Burst))
		{
			Pooled->ReleaseToPool();
			Pooled->SetActorTransform(Transform);
		}
	}
	else Burst = GetWorld()->SpawnActor<AActor>(LoadedDebrisClass, Transform, Params);
	if (AMiningDebrisBurst* SoilBurst = Cast<AMiningDebrisBurst>(Burst))
	{
		const bool bLightStrike = !bPendingDrillImpact && !Mining->bLastPickaxeBreak;
		const int32 BaseCount = bPendingDrillImpact ? DrillDebrisChunkCount : (bLightStrike ? 12 + Mining->TargetPickaxeHits * 3 : DebrisChunkCount);
		const int32 Count = FMath::RandRange(FMath::Clamp(BaseCount - 6, 12, 64), FMath::Clamp(BaseCount + 6, 12, 64));
		const float DebrisScale = bLightStrike ? (Mining->TargetPickaxeHits == 1 ? .42f : .65f) : 1.f;
		SoilBurst->Initialize(LoadedDebrisMeshes, PendingSurfaceMaterial, Normal, Mining->GetMiningRadius() * DebrisScale, Count, bPendingDrillImpact ? DrillDebrisLifetime : DebrisLifetime);
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
	DebrisActors.RemoveAll([Burst](const TWeakObjectPtr<AActor>& Actor) { return !Actor.IsValid() || Actor.Get() == Burst; });
	// Bound the physics cost even if a level uses a very short mining interval.
	const int32 MaxBursts = bPendingDrillImpact ? 6 : 8;
	while (DebrisActors.Num() >= MaxBursts)
	{
		if (auto* Pooled = Cast<AMiningDebrisBurst>(DebrisActors[0].Get()); Pooled && Pooled->IsPooled()) Pooled->ReleaseToPool();
		else if (DebrisActors[0].IsValid()) DebrisActors[0]->Destroy();
		DebrisActors.RemoveAt(0);
	}
	DebrisActors.Add(Burst);
	++DebrisBurstCount;
}

FTransform UMiningToolEffectsComponent::BuildDrillSpinTransform(const FTransform& ModelPose,
	const FVector& LocalAxis, const FVector& LocalPivot, float Degrees)
{
	const FVector Axis = ModelPose.TransformVectorNoScale(LocalAxis.ContainsNaN() ? FVector::ForwardVector
		: LocalAxis.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector)).GetSafeNormal();
	const FVector Pivot = ModelPose.TransformPosition(LocalPivot.ContainsNaN() ? FVector::ZeroVector : LocalPivot);
	const FQuat Spin(Axis, FMath::DegreesToRadians(FMath::IsFinite(Degrees) ? Degrees : 0.f));
	// Conjugate rotation with the shaft's position: every point on the shaft stays fixed.
	// Rotating Motion around its origin instead makes an offset Model orbit the mount.
	return FTransform(Spin, Pivot - Spin.RotateVector(Pivot), FVector::OneVector);
}

void UMiningToolEffectsComponent::ApplyToolMotion(USceneComponent* Motion, const FVector& LegacyLocation,
	const FRotator& LegacyRotation, const FVector& CameraOffset, const FRotator& Tilt, float SpinDegrees, UStaticMeshComponent* SpinModel)
{
	if (!Motion) return;
	const FTransform Spin = SpinModel ? BuildDrillSpinTransform(SpinModel->GetRelativeTransform(), DrillSpinAxis, DrillSpinPivot, SpinDegrees)
		: FTransform::Identity;
	if (bUsesCharacterComponents)
	{
		const USceneComponent* Mount = Motion->GetAttachParent();
		if (!Mount) return;
		const FQuat MountRotation = Mount->GetRelativeRotation().Quaternion();
		const FQuat EquipRotation = MountRotation.Inverse() * (Mount->GetRelativeRotation() + Tilt).Quaternion();
		Motion->SetRelativeLocation(Mount->GetRelativeTransform().InverseTransformVector(CameraOffset)
			+ EquipRotation.RotateVector(Spin.GetTranslation()));
		Motion->SetRelativeRotation(EquipRotation * Spin.GetRotation());
	}
	else
	{
		const FQuat EquipRotation = (LegacyRotation + Tilt).Quaternion();
		Motion->SetRelativeLocation(LegacyLocation + CameraOffset + EquipRotation.RotateVector(Spin.GetTranslation()));
		Motion->SetRelativeRotation(EquipRotation * Spin.GetRotation());
	}
}

void UMiningToolEffectsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (CanShowEffects()) WarmDebrisPool();
	UpdateEchoPickaxeStyle();
	if (!CanShowEffects())
	{
		if (bSwinging || !DebrisActors.IsEmpty() || !DustActors.IsEmpty()) ResetEffects();
		if (PickaxeMesh) PickaxeMesh->SetVisibility(false);
		if (DrillMesh) DrillMesh->SetVisibility(false);
		if (LeftDrillMesh) LeftDrillMesh->SetVisibility(false);
		bDrillRunning = false;
		return;
	}
	const auto* Bomb = GetOwner()->FindComponentByClass<UMiningBombToolComponent>();
	const bool bBombEquipped = Bomb && Bomb->bEquipped;
	if (bBombEquipped && bSwinging) ResetEffects();
	const int32 PresentedTool = bBombEquipped ? 2 : Mining->bDrillMode ? 1 : 0;
	if (PresentedTool != LastPresentedTool)
	{
		LastPresentedTool = PresentedTool;
		EquipElapsed = 0;
	}
	EquipElapsed += FMath::Max(0.f, DeltaTime);
	const float EquipAlpha = FMath::Clamp(EquipElapsed / FMath::Max(0.05f, ToolEquipDuration), 0.f, 1.f);
	const float EquipOffset = FMath::Pow(1.f - EquipAlpha, 3.f);
	const FVector Lowered(0, 0, -14.f * EquipOffset);
	const FRotator EquipTilt(-12.f * EquipOffset, 0, 0);
	ApplyToolMotion(BombPivot, FVector(45, 23, -20), FRotator(-12, 0, -8), Lowered, EquipTilt);
	if (bBombEquipped)
	{
		PickaxeMesh->SetVisibility(false);
		if (DrillMesh) DrillMesh->SetVisibility(false);
		if (LeftDrillMesh) LeftDrillMesh->SetVisibility(false);
		bDrillRunning = false;
		return;
	}
	PickaxeMesh->SetVisibility(!Mining->bDrillMode);
	if (DrillMesh) DrillMesh->SetVisibility(Mining->bDrillMode);
	if (LeftDrillMesh) LeftDrillMesh->SetVisibility(Mining->bDrillMode && bDualDrills);
	// Legacy runtime-created tools still accept the old fields. Character tools use their
	// blueprint component transforms/mesh assignments directly, including live PIE edits.
	if (!bUsesCharacterComponents && DrillMeshAsset.ToSoftObjectPath() != LastDrillAssetPath)
	{
		UStaticMesh* UpdatedMesh = DrillMeshAsset.LoadSynchronous();
		if (DrillMesh) DrillMesh->SetStaticMesh(UpdatedMesh);
		if (LeftDrillMesh) LeftDrillMesh->SetStaticMesh(UpdatedMesh);
		LastDrillAssetPath = DrillMeshAsset.ToSoftObjectPath();
	}
	if (!bUsesCharacterComponents)
	{
		const float SafeScale = FMath::IsFinite(DrillScale) ? FMath::Clamp(DrillScale, .1f, 3.f) : .8f;
		if (DrillMesh) DrillMesh->SetRelativeScale3D(FVector(SafeScale));
		if (LeftDrillMesh) LeftDrillMesh->SetRelativeScale3D(FVector(SafeScale, -SafeScale, SafeScale));
	}
	ApplyToolMotion(PickaxePivot, FVector(40, 20, -35), FRotator::ZeroRotator, Lowered, EquipTilt);
	bDrillRunning = Mining->bDrillMode && Mining->IsMiningActive() && IsValid(DrillMesh) && IsValid(DrillSpinCurve);
	if (bDrillRunning)
	{
		float Start = 0, End = 0;
		DrillSpinCurve->GetTimeRange(Start, End);
		const double Duration = FMath::Max(0.001, double(End - Start));
		DrillAnimationTime += FMath::Max(0.f, DeltaTime);
		const double Cycles = FMath::FloorToDouble(DrillAnimationTime / Duration);
		const double Phase = FMath::Fmod(DrillAnimationTime, Duration);
		const double CycleAngle = DrillSpinCurve->GetFloatValue(End) - DrillSpinCurve->GetFloatValue(Start);
		const double Angle = Cycles * CycleAngle + DrillSpinCurve->GetFloatValue(Start + Phase) - DrillSpinCurve->GetFloatValue(Start);
		DrillSpinDegrees = FMath::Fmod(Angle, 360.0);
	}
	const FVector Vibration = bDrillRunning ? FVector(0, FMath::Sin(DrillAnimationTime * 65) * .12, FMath::Sin(DrillAnimationTime * 87) * .1) : FVector::ZeroVector;
	const FVector LeftVibration = bDrillRunning ? FVector(0, -Vibration.Y, FMath::Sin(DrillAnimationTime * 87 + 1) * .1) : FVector::ZeroVector;
	ApplyToolMotion(DrillPivot, DrillLocation, DrillRotation, Lowered + Vibration, EquipTilt, DrillSpinDegrees, DrillMesh);
	ApplyToolMotion(LeftDrillPivot, LeftDrillLocation, LeftDrillRotation, Lowered + LeftVibration, EquipTilt, -DrillSpinDegrees, LeftDrillMesh);
	if (Mining->bDrillMode) return;
	if (!bSwinging)
	{
		return;
	}
	const float Elapsed = GetWorld()->GetTimeSeconds() - SwingStart;
	if (Elapsed >= SwingDuration)
	{
		bSwinging = false;
		ApplyToolMotion(PickaxePivot, FVector(40, 20, -35), FRotator::ZeroRotator, Lowered, FRotator::ZeroRotator);
		return;
	}
	ApplyToolMotion(PickaxePivot, FVector(40, 20, -35), FRotator::ZeroRotator, Lowered,
		FRotator(EquipTilt.Pitch - SwingCurve->GetFloatValue(Elapsed * SourceSwingDuration / SwingDuration), 0, 0));
}

void UMiningToolEffectsComponent::ResetEffects()
{
	PendingHit = FHitResult();
	PendingSurfaceMaterial = nullptr;
	bSwinging = false;
	bDrillRunning = false;
	DrillAnimationTime = 0;
	DrillSpinDegrees = 0;
	EquipElapsed = ToolEquipDuration;
	ApplyToolMotion(PickaxePivot, FVector(40, 20, -35), FRotator::ZeroRotator, FVector::ZeroVector, FRotator::ZeroRotator);
	ApplyToolMotion(DrillPivot, DrillLocation, DrillRotation, FVector::ZeroVector, FRotator::ZeroRotator);
	ApplyToolMotion(LeftDrillPivot, LeftDrillLocation, LeftDrillRotation, FVector::ZeroVector, FRotator::ZeroRotator);
	ApplyToolMotion(BombPivot, FVector(45, 23, -20), FRotator(-12, 0, -8), FVector::ZeroVector, FRotator::ZeroRotator);
	for (const TWeakObjectPtr<AActor>& Actor : DebrisActors) if (Actor.IsValid())
	{
		if (auto* Burst = Cast<AMiningDebrisBurst>(Actor.Get()); Burst && Burst->IsPooled()) Burst->ReleaseToPool();
		else Actor->Destroy();
	}
	DebrisActors.Reset();
	for (const TWeakObjectPtr<AActor>& Actor : DustActors) if (Actor.IsValid()) Actor->Destroy();
	DustActors.Reset();
}

void UMiningToolEffectsComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ResetEffects();
	for (AMiningDebrisBurst* Burst : DebrisPool) if (IsValid(Burst)) Burst->Destroy();
	DebrisPool.Reset();
	if (Mining)
	{
		Mining->OnMiningAttempt.RemoveDynamic(this, &UMiningToolEffectsComponent::PlaySwing);
		Mining->OnMiningHit.RemoveDynamic(this, &UMiningToolEffectsComponent::QueueImpact);
		Mining->OnMiningReset.RemoveDynamic(this, &UMiningToolEffectsComponent::ResetEffects);
		Mining->OnToolModeChanged.RemoveDynamic(this, &UMiningToolEffectsComponent::UpdateToolMode);
	}
	Super::EndPlay(Reason);
}
