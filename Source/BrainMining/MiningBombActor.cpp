#include "MiningBombActor.h"
#include "MiningAudioComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "MiningBombPresentationComponent.h"
#include "DiggableRock.h"
#include "GeneratedMiningRock.h"
#include "MiningDebrisBurst.h"
#include "MiningDrillDustBurst.h"
#include "MobileRuntimeSettings.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "TimerManager.h"

AMiningBombActor::AMiningBombActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	Tags.Add(TEXT("BM_PlayerEffect"));
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("BombCollision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(10.f);
	Collision->SetCollisionProfileName(TEXT("PhysicsActor"));
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision->SetEnableGravity(true);
	Collision->BodyInstance.bUseCCD = true;
	Collision->SetNotifyRigidBodyCollision(true);
	Collision->OnComponentHit.AddDynamic(this, &ThisClass::Contact);
	BounceMaterial = CreateDefaultSubobject<UPhysicalMaterial>(TEXT("BombBounceMaterial"));
	BounceMaterial->Friction = .45f;
	BounceMaterial->Restitution = 0.f;
	Collision->SetPhysMaterialOverride(BounceMaterial);
	Model = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BombModel"));
	Model->SetupAttachment(Collision);
	Model->SetCollisionProfileName(TEXT("NoCollision"));
}

void AMiningBombActor::Launch(UStaticMesh* Mesh, UNiagaraSystem* Effect, UMiningBombToolComponent* Tool,
	const FMiningBombSettings& InSettings, const FVector& Velocity)
{
	Settings = InSettings;
	AGeneratedMiningRock::PrepareTerrainAt(GetWorld(), GetActorLocation(), 1000.f);
	SetActorTickEnabled(true);
	Explosion = Effect;
	SourceTool = Tool;
	Model->SetStaticMesh(Mesh);
	Model->SetRelativeLocation(-Mesh->GetBounds().Origin);
	Collision->SetSphereRadius(FMath::Clamp(Settings.CollisionRadius, 1.f, 50.f));
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Collision->SetMassOverrideInKg(NAME_None, FMath::Clamp(Settings.Mass, .1f, 100.f), true);
	Collision->SetLinearDamping(.08f);
	Collision->SetAngularDamping(.25f);
	Collision->SetSimulatePhysics(true);
	Collision->SetPhysicsLinearVelocity(Velocity);
	Collision->SetPhysicsAngularVelocityInDegrees(FVector(250, 180, 90));
	LaunchedAt = GetWorld()->GetTimeSeconds();
	if (GetNetMode() != NM_DedicatedServer)
	{
		AudioAttenuation = NewObject<USoundAttenuation>(this);
		AudioAttenuation->Attenuation.bAttenuate = true;
		AudioAttenuation->Attenuation.bSpatialize = true;
		AudioAttenuation->Attenuation.AttenuationShapeExtents = FVector(180.f);
		AudioAttenuation->Attenuation.FalloffDistance = 3200.f;
		PlayBombSound(TEXT("S_BombThrow"), .4f);
		if (auto* Sound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Pilot/S_BombFuse_Loop.S_BombFuse_Loop")))
			FuseAudio = UGameplayStatics::SpawnSoundAttached(Sound, Collision, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true, .22f, 1.f, 0.f, AudioAttenuation, nullptr, false);
	}
	auto* Presentation = NewObject<UMiningBombPresentationComponent>(this, TEXT("BombCountdownPresentation"));
	Presentation->RegisterComponent();
	Presentation->Initialize(Model, LaunchedAt, Settings.FuseSeconds);
	GetWorldTimerManager().SetTimer(FuseTimer, this, &ThisClass::Explode, FMath::Clamp(Settings.FuseSeconds, .1f, 30.f), false);
	GetWorldTimerManager().SetTimer(PawnGraceTimer, [this]()
	{
		if (!bExploded && !bStuck) Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}, .15f, false);
}

void AMiningBombActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bExploded && !bStuck)
		AGeneratedMiningRock::PrepareTerrainAt(GetWorld(), GetActorLocation(),
			1000.f + Collision->GetPhysicsLinearVelocity().Size() * FMath::Max(0.f, DeltaSeconds));
}

void AMiningBombActor::Contact(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent* OtherComponent, FVector, const FHitResult& Result)
{
	++ContactCount;
	if (bExploded || bStuck || !IsValid(OtherComponent) || Other == this) return;
	// Latch before stopping physics because one physics step may report several
	// contacts. Do not restart the fuse: it still counts from the throw.
	bStuck = true;
	PlayBombSound(TEXT("S_BombStick"), .55f);
	SetActorTickEnabled(false);
	StuckAt = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().ClearTimer(PawnGraceTimer);
	Collision->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Collision->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Collision->SetSimulatePhysics(false);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	const FVector Normal = Result.ImpactNormal.GetSafeNormal();
	if (Result.bBlockingHit && !Result.ImpactPoint.ContainsNaN() && !Normal.IsNearlyZero())
	{
		// Position the centered model by its actual half thickness, not its spherical flight collider.
		const float HalfThickness=Model->GetStaticMesh()?Model->GetStaticMesh()->GetBounds().BoxExtent.Z*Model->GetComponentScale().Z:Collision->GetScaledSphereRadius();
		SetActorLocation(Result.ImpactPoint + Normal * (HalfThickness + .1f), false, nullptr, ETeleportType::TeleportPhysics);
		// The broad C4 base is the face that should sit on the contacted plane.
		// C4 meshes are authored with their thin axis as local Z, so use the
		// negative thin-axis normal and preserve the incoming roll around it.
		// This keeps the large bottom face flush on floors, walls and ceilings.
		const FVector LocalBaseNormal = FVector::DownVector;
		const FVector CurrentBase = GetActorQuat().RotateVector(LocalBaseNormal);
		const FQuat Align = FQuat::FindBetweenNormals(CurrentBase, -Normal);
		SetActorRotation(Align * GetActorQuat(), ETeleportType::TeleportPhysics);
	}
	// Follow moving components and skeletal bones with the aligned base
	// without welding the bomb into the contacted body's physics simulation.
	AttachToComponent(OtherComponent, FAttachmentTransformRules::KeepWorldTransform, Result.BoneName);
}

void AMiningBombActor::ApplyShockwave(const FVector& Location)
{
	const bool bMobile = UMobileRuntimeSettings::IsMobileRuntime();
	const float Radius = FMath::Clamp(Settings.BlastRadius, 50.f, 3000.f);
	FCollisionObjectQueryParams Types;
	Types.AddObjectTypesToQuery(ECC_Pawn);
	if (!bMobile)
	{
		Types.AddObjectTypesToQuery(ECC_PhysicsBody);
		Types.AddObjectTypesToQuery(ECC_WorldDynamic);
	}
	TArray<FOverlapResult> Hits;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BombBlast), false, this);
	GetWorld()->OverlapMultiByObjectType(Hits, Location, FQuat::Identity, Types, FCollisionShape::MakeSphere(Radius), Query);
	TSet<ACharacter*> Characters;
	TSet<UPrimitiveComponent*> Bodies;
	for (const auto& Hit : Hits)
	{
		if (auto* Character = Cast<ACharacter>(Hit.GetActor())) Characters.Add(Character);
		else if (auto* Body = Hit.GetComponent(); Body && Body->IsSimulatingPhysics()) Bodies.Add(Body);
	}
	const auto VisibleFromBlast = [&](AActor* Actor, const FVector& Target)
	{
		FCollisionQueryParams Ray(SCENE_QUERY_STAT(BombBlastOcclusion), false, this);
		Ray.AddIgnoredActor(Actor);
		return !GetWorld()->LineTraceTestByChannel(Location, Target, ECC_Visibility, Ray);
	};
	for (auto* Character : Characters)
	{
		const FVector Target = Character->GetActorLocation();
		const float Weight = FMath::Clamp(1.f - FVector::Dist(Location, Target) / Radius, 0.f, 1.f);
		if (Weight <= 0 || !VisibleFromBlast(Character, Target)) continue;
		FVector Direction = (Target - Location).GetSafeNormal();
		if (Direction.IsNearlyZero()) Direction = FVector::UpVector;
		const FVector Impulse = (Direction * FMath::Clamp(Settings.BlastVelocity, 0.f, 3000.f)
			+ FVector::UpVector * FMath::Clamp(Settings.BlastLift, 0.f, 1000.f)) * Weight;
		// AddImpulse enters CharacterMovement's force integration and collision solver.
		Character->GetCharacterMovement()->AddImpulse(Impulse, true);
	}
	for (auto* Body : Bodies)
	{
		if (!VisibleFromBlast(Body->GetOwner(), Body->GetComponentLocation())) continue;
		Body->AddRadialImpulse(Location, Radius, FMath::Clamp(Settings.BlastVelocity, 0.f, 3000.f), ERadialImpulseFalloff::RIF_Linear, true);
	}
}

void AMiningBombActor::Explode()
{
	if (bExploded) return;
	bExploded = true;
	if (FuseAudio) FuseAudio->Stop();
	PlayBombSound(TEXT("S_BombExplosion"), .85f);
	const FVector Location = GetActorLocation();
	Model->SetVisibility(false);
	Collision->SetSimulatePhysics(false);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ApplyShockwave(Location);
	ExcavateTerrain(Location);
	FMiningBombSettings Presentation = Settings;
	if (UMobileRuntimeSettings::IsMobileRuntime())
	{
		Presentation.ExplosionScale = FMath::Min(Presentation.ExplosionScale, .9f);
		Presentation.DebrisCount = FMath::Min(Presentation.DebrisCount, 24);
		Presentation.FireStrength *= .55f;
		Presentation.SmokeStrength *= .45f;
		Presentation.DustStrength = 0;
	}
	if (auto* Burst = GetWorld()->SpawnActor<AMiningBombExplosion>(Location, FRotator::ZeroRotator))
		Burst->Initialize(Explosion, Presentation);
	if (SourceTool.IsValid()) SourceTool->RecordExplosion(Location);
	Destroy();
}

void AMiningBombActor::ExcavateTerrain(const FVector& Location)
{
	const bool bMobile = UMobileRuntimeSettings::IsMobileRuntime();
	const float Radius = FMath::Clamp(Settings.ExcavationRadius, 100.f, 2000.f);
	const float Reach = Radius * 1.45f;
	AGeneratedMiningRock::PrepareTerrainAt(GetWorld(), Location, Reach + 1.f);
	struct FSurface { FHitResult Hit; UMaterialInterface* Material; };
	TArray<FSurface> Surfaces;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BombTerrainSample), true, this);
	Query.bReturnFaceIndex = true;
	Query.AddIgnoredActor(GetOwner());
	// Sample before removing the rock, from the final physical bomb location.
	// Fibonacci directions cover floor, walls and roof without a forward-only bias.
	const int32 SampleCount = bMobile ? 48 : 96;
	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const double Z = 1. - 2. * (Index + .5) / SampleCount;
		const double Angle = Index * 2.399963229728653;
		const double R = FMath::Sqrt(1.-Z*Z);
		const FVector Direction(R*FMath::Cos(Angle), R*FMath::Sin(Angle), Z);
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Location, Location+Direction*Reach, ECC_Visibility, Query)
			|| !Cast<ADiggableRock>(Hit.GetActor()) || !Hit.GetComponent()) continue;
		int32 Section = 0;
		UMaterialInterface* Material = Hit.GetComponent()->GetMaterialFromCollisionFaceIndex(Hit.FaceIndex, Section);
		if (!Material) Material = Hit.GetComponent()->GetMaterial(0);
		if (!Material) Material = Cast<ADiggableRock>(Hit.GetActor())->RockMaterial;
		if (Material) Surfaces.Add({Hit, Material});
	}
	TSet<AActor*> Excavated;
	const int32 Seed = GetUniqueID();
	for (TActorIterator<ADiggableRock> Rock(GetWorld()); Rock; ++Rock)
	{
		if (Rock->IsHidden() || Rock->TriangleCount == 0) continue;
		FVector Origin, Extent;
		Rock->GetActorBounds(true, Origin, Extent);
		if (FBox(Origin-Extent, Origin+Extent).ComputeSquaredDistanceToPoint(Location) > Reach*Reach) continue;
		if (Rock->MineBlast(Location, Radius, FMath::Clamp(Settings.CraterRoughness, 0.f, .45f), Seed)) Excavated.Add(*Rock);
	}
	if (SourceTool.IsValid())
	{
		SourceTool->LastExcavatedRocks = Excavated.Num();
		SourceTool->LastDebrisCount = 0;
	}
	if (Excavated.IsEmpty()) return;
	Surfaces.RemoveAll([&](const FSurface& Surface) { return !Excavated.Contains(Surface.Hit.GetActor()); });
	// Keep a few spatially separated contact patches instead of concentrating
	// all fragments in a single point. Different layers retain their own material.
	TArray<FSurface> Patches;
	for (const FSurface& Surface : Surfaces)
	{
		if (Patches.ContainsByPredicate([&](const FSurface& Other) {
			return Surface.Material == Other.Material && FVector::DistSquared(Surface.Hit.ImpactPoint, Other.Hit.ImpactPoint) < 150*150;
		})) continue;
		Patches.Add(Surface);
		if (Patches.Num() == (bMobile ? 3 : 8)) break;
	}
	TArray<TObjectPtr<UStaticMesh>> Meshes;
	TArray<TObjectPtr<UStaticMesh>> CaveMeshes;
	for (const TCHAR* Size : {TEXT("Large"), TEXT("Small")})
		for (int32 I=1; I<=4; ++I)
		{
			if (auto* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Debris/Meshes/SM_Debris_Concrete_%s_%d.SM_Debris_Concrete_%s_%d"), Size,I,Size,I))) Meshes.Add(Mesh);
			if (auto* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Bomb/FX/SM_Debris_Concrete_%s_%d_CaveUV.SM_Debris_Concrete_%s_%d_CaveUV"), Size,I,Size,I))) CaveMeshes.Add(Mesh);
		}
	const int32 Total = FMath::Clamp(bMobile ? FMath::Min(Settings.DebrisCount, 24) : Settings.DebrisCount, 0, 96);
	for (int32 Index=0; Index<Patches.Num(); ++Index)
	{
		FSurface& Surface = Patches[Index];
		UMaterialInterface* Material = Surface.Material;
		float GuideCount = 0;
		if (Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("GuideCount")), GuideCount) && GuideCount > 0)
		{
			// World-position guide shaders must be frozen at the detached surface:
			// moving fragments must not change from purple ore to ordinary soil.
			bool bGuide = false;
			const FVector Point = Surface.Hit.ImpactPoint;
			for (int32 Guide=0; Guide<FMath::Min(16, int32(GuideCount)); ++Guide)
			{
				FLinearColor Start=FLinearColor::Black, End=FLinearColor::Black;
				if (!Material->GetVectorParameterValue(FMaterialParameterInfo(*FString::Printf(TEXT("GuideStart%d"),Guide)),Start)
					|| !Material->GetVectorParameterValue(FMaterialParameterInfo(*FString::Printf(TEXT("GuideEnd%d"),Guide)),End)
					|| Point.X < Start.R-.1 || Point.X > End.R+.1 || End.R <= Start.R) continue;
				const double Alpha = FMath::Clamp((Point.X-Start.R)/(End.R-Start.R),0.,1.);
				const FVector2D Delta(Point.Y-FMath::Lerp(Start.G,End.G,Alpha), Point.Z-FMath::Lerp(Start.B,End.B,Alpha));
				const double Angle = (FMath::Floor(FMath::Atan2(Delta.Y,Delta.X)/(PI/8))+.5)*(PI/8);
				bGuide |= FVector2D::DotProduct(Delta,FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))) <= Start.A*.9807852804+.1;
			}
			FLinearColor Color;
			if (Material->GetVectorParameterValue(FMaterialParameterInfo(bGuide ? TEXT("GuideColor") : TEXT("OrdinaryColor")),Color))
			{
				auto* Frozen = UMaterialInstanceDynamic::Create(Material, GetWorld());
				Frozen->SetScalarParameterValue(TEXT("GuideCount"), 0);
				Frozen->SetVectorParameterValue(TEXT("OrdinaryColor"), Color);
				Material = Frozen;
			}
		}
		FActorSpawnParameters Spawn;
		Spawn.Owner = GetInstigator() ? GetInstigator()->GetController() : GetOwner();
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector Position = Surface.Hit.ImpactPoint + Surface.Hit.ImpactNormal * 15;
		const int32 Count = Total/Patches.Num() + (Index < Total%Patches.Num() ? 1 : 0);
		if (Count > 0)
			if (auto* Debris = GetWorld()->SpawnActor<AMiningDebrisBurst>(Position,FRotator::ZeroRotator,Spawn))
			{
				// This cave currently uses a mannequin texture on UV-less generated
				// rock. Preserve its UV-zero color, not a robot texture atlas on chips.
				const bool bCaveUV = Surface.Material->GetPathName().Contains(TEXT("/Mannequins/Materials/M_Mannequin")) && CaveMeshes.Num()==8;
				Debris->InitializeBlast(bCaveUV ? CaveMeshes : Meshes,Material,Surface.Hit.ImpactNormal,Radius,Count,Settings.DebrisLifetime);
				if (SourceTool.IsValid()) SourceTool->LastDebrisCount += Debris->ChunkCount;
			}
		if (!bMobile && Settings.DustStrength > 0)
			if (auto* Dust = GetWorld()->SpawnActor<AMiningDrillDustBurst>(Position,FRotator::ZeroRotator,Spawn))
			{
				Dust->Tags.AddUnique(TEXT("BM_PlayerEffect"));
				FMiningDrillContactSettings Contact;
				Contact.DustCount = 12;
				Contact.DustSize = 2.4f;
				Contact.DustStrength = FMath::Clamp(Settings.DustStrength,0.f,3.f);
				Contact.SmokeCount = Contact.SparkCount = 0;
				Contact.LifetimeScale = 1.1f;
				Dust->Initialize(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")),
					LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Drill/M_DrillDust.M_DrillDust")),nullptr,nullptr,
					Material,Surface.Hit.ImpactNormal,Radius,FLinearColor(.22f,.17f,.11f),Contact);
			}
	}
}

void AMiningBombActor::EndPlay(const EEndPlayReason::Type Reason)
{
	if (FuseAudio) { FuseAudio->Stop(); FuseAudio->DestroyComponent(); FuseAudio = nullptr; }
	GetWorldTimerManager().ClearTimer(FuseTimer);
	GetWorldTimerManager().ClearTimer(PawnGraceTimer);
	Super::EndPlay(Reason);
}

void AMiningBombActor::PlayBombSound(const TCHAR* Name, float Gain)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	const TCHAR* Folder = UMiningAudioComponent::IsMainGameWorld(GetWorld()) && FCString::Strcmp(Name, TEXT("S_BombExplosion")) == 0 ? TEXT("MainGame") : TEXT("Pilot");
	if (auto* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/%s/%s.%s"), Folder, Name, Name)))
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), Gain, 1.f, 0.f, AudioAttenuation);
}

AMiningBombExplosion::AMiningBombExplosion()
{
	Tags.Add(TEXT("BM_PlayerEffect"));
	PrimaryActorTick.bCanEverTick = true;
	auto* Root = CreateDefaultSubobject<USceneComponent>(TEXT("ExplosionRoot"));
	SetRootComponent(Root);
	Effect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ExplosionFX"));
	Effect->SetupAttachment(Root);
	Effect->SetAutoActivate(false);
	Fire = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StylizedFire"));
	Smoke = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StylizedSmoke"));
	for (auto* Component : { Fire.Get(), Smoke.Get() })
	{
		Component->SetupAttachment(Root);
		Component->SetCollisionProfileName(TEXT("NoCollision"));
		Component->SetCastShadow(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetNumCustomDataFloats(2);
	}
	Flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("ExplosionFlash"));
	Flash->SetupAttachment(Root);
	Flash->SetIntensityUnits(ELightUnits::Lumens);
	Flash->SetLightColor(FLinearColor(1.f, .25f, .06f));
	Flash->SetAttenuationRadius(1300.f);
	Flash->SetCastShadows(false);
	Flash->SetIntensity(0);
}

void AMiningBombExplosion::Initialize(UNiagaraSystem* System, const FMiningBombSettings& Settings)
{
	const float Scale = FMath::Clamp(Settings.ExplosionScale, .1f, 4.f);
	if (System)
	{
		Effect->SetAsset(System);
		Effect->SetRelativeScale3D(FVector(Scale));
		Effect->Activate(true);
	}
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Bomb/FX/SM_BombPuff.SM_BombPuff"));
	Fire->SetStaticMesh(Mesh);
	Smoke->SetStaticMesh(Mesh);
	Fire->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Bomb/FX/M_BombFire.M_BombFire")));
	Smoke->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Bomb/FX/M_BombSmoke.M_BombSmoke")));
	FlashStrength = FMath::Clamp(Settings.FireStrength,0.f,3.f);
	for (int32 Kind=0; Kind<2; ++Kind)
	{
		const bool bFire = Kind == 0;
		auto* Component = bFire ? Fire.Get() : Smoke.Get();
		auto& Puffs = bFire ? Flames : Clouds;
		const float Strength = FMath::Clamp(bFire ? Settings.FireStrength : Settings.SmokeStrength,0.f,3.f);
		const int32 Count = Strength > 0 ? FMath::RoundToInt((bFire ? 15 : 18)*FMath::Sqrt(Strength)) : 0;
		for (int32 Index=0; Index<Count; ++Index)
		{
			FPuff Puff;
			const FVector Direction = FMath::VRand();
			Puff.Position = GetActorLocation()+Direction*FMath::FRandRange(10.f,70.f)*Scale;
			Puff.Velocity = Direction*FMath::FRandRange(bFire?230.f:90.f,bFire?380.f:170.f)*Scale;
			Puff.Velocity.Z += bFire ? 60 : 55;
			Puff.Rotation = FRotator(FMath::FRand()*360,FMath::FRand()*360,FMath::FRand()*360).Quaternion();
			Puff.Lifetime = bFire ? FMath::FRandRange(.28f,.52f) : FMath::FRandRange(1.1f,2.1f);
			Puff.Delay = bFire ? FMath::FRandRange(0,.04f) : FMath::FRandRange(.12f,.24f);
			Puff.Size = FMath::FRandRange(bFire?65.f:70.f,bFire?115.f:130.f)*Scale;
			Puff.Opacity = (bFire ? .9f : .6f)*FMath::Min(Strength,1.5f);
			Puffs.Add(Puff);
			Component->AddInstance(FTransform(Puff.Rotation,Puff.Position,FVector(.001f)),true);
		}
	}
	SetLifeSpan(System ? 12.f : 3.5f);
}

void AMiningBombExplosion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	Flash->SetIntensity(9000.f * FlashStrength * FMath::Square(FMath::Max(0.f, 1.f - Age / .16f)));
	for (int32 Kind=0; Kind<2; ++Kind)
	{
		const bool bFire = Kind == 0;
		auto* Component = bFire ? Fire.Get() : Smoke.Get();
		auto& Puffs = bFire ? Flames : Clouds;
		for (int32 Index=0; Index<Puffs.Num(); ++Index)
		{
			FPuff& Puff = Puffs[Index];
			const float Time = Age-Puff.Delay;
			if (Time <= 0 || Time >= Puff.Lifetime)
			{
				Component->SetCustomDataValue(Index,0,0);
				Component->UpdateInstanceTransform(Index,FTransform(Puff.Rotation,Puff.Position,FVector(.001f)),true);
				continue;
			}
			Puff.Position += Puff.Velocity*DeltaSeconds;
			Puff.Velocity *= FMath::Exp(-(bFire ? 4.f : 2.f)*DeltaSeconds);
			if (!bFire) Puff.Velocity.Z += DeltaSeconds*18;
			const float T = Time/Puff.Lifetime;
			const float Diameter = Puff.Size*(1+2.2f*(1-FMath::Square(1-T)));
			Component->UpdateInstanceTransform(Index,FTransform(Puff.Rotation,Puff.Position,FVector(Diameter/100)),true);
			Component->SetCustomDataValue(Index,0,Puff.Opacity*FMath::Min(1.f,Time/.06f)*FMath::Pow(1-T,bFire?.6f:1.3f));
			Component->SetCustomDataValue(Index,1,1-T);
		}
		Component->MarkRenderStateDirty();
	}
}
