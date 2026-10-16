#include "MiningDrillDustBurst.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"

AMiningDrillDustBurst::AMiningDrillDustBurst()
{
	PrimaryActorTick.bCanEverTick = true;
	Sprites = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DustSprites"));
	RootComponent = Sprites;
	Sprites->SetCollisionProfileName(TEXT("NoCollision"));
	Sprites->SetCastShadow(false);
	Sprites->SetCanEverAffectNavigation(false);
	SmokeSprites = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ContactSmoke"));
	SparkSprites = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ContactSparks"));
	for (UInstancedStaticMeshComponent* Component : { SmokeSprites.Get(), SparkSprites.Get() })
	{
		Component->SetupAttachment(Sprites);
		Component->SetCollisionProfileName(TEXT("NoCollision"));
		Component->SetCastShadow(false);
		Component->SetCanEverAffectNavigation(false);
	}
}

void AMiningDrillDustBurst::Initialize(UStaticMesh* Plane, UMaterialInterface* DustMaterial, UMaterialInterface* SmokeMaterial, UMaterialInterface* SparkMaterial, UMaterialInterface* SoilMaterial, const FVector& Normal, float CutRadius, const FLinearColor& FallbackTint, const FMiningDrillContactSettings& Settings)
{
	if (!Plane || (!DustMaterial && !SmokeMaterial && !SparkMaterial)) { Destroy(); return; }
	const float LifetimeScale = FMath::Clamp(Settings.LifetimeScale, .25f, 2.f);
	const auto VaryCount = [](int32 Average, int32 Limit)
	{
		return Average > 0 ? FMath::Clamp(FMath::RoundToInt(Average * FMath::FRandRange(.8f, 1.2f)), 1, Limit) : 0;
	};
	SourceMaterial = SoilMaterial;
	DustTint = FallbackTint;
	if (SoilMaterial)
		for (const TCHAR* Name : {TEXT("BaseColor"), TEXT("SoilColor"), TEXT("Color"), TEXT("Tint")})
			if (SoilMaterial->GetVectorParameterValue(FMaterialParameterInfo(Name), DustTint)) break;
	if (Settings.bUseCoolPalette) DustTint=FLinearColor(.12f,.17f,.19f);
	// Dust is slightly less saturated than a solid clod, but keeps the soil hue.
	const float Luminance = DustTint.GetLuminance();
	DustTint = FMath::Lerp(DustTint, FLinearColor(Luminance, Luminance, Luminance, 1), 0.15f);
	Material = DustMaterial ? UMaterialInstanceDynamic::Create(DustMaterial, this) : nullptr;
	if (Material)
    {
        Material->SetVectorParameterValue(TEXT("SoilTint"), DustTint);
        if(Settings.bUseCoolPalette)Material->SetVectorParameterValue(TEXT("AmbientDustFill"),FLinearColor(.025f,.035f,.04f));
    }
	Sprites->SetStaticMesh(Plane);
	Sprites->SetMaterial(0, Material);
	Sprites->SetNumCustomDataFloats(3);
	FVector Tangent, Bitangent;
	Normal.FindBestAxisVectors(Tangent, Bitangent);
	const float Scale = FMath::Clamp(CutRadius / 160.f, 0.65f, 1.6f);
	const int32 Count = Material && Settings.DustStrength > 0 ? VaryCount(Settings.DustCount, 64) : 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FPuff Puff;
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const FVector Radial = Tangent * FMath::Cos(Angle) + Bitangent * FMath::Sin(Angle);
		Puff.Position = GetActorLocation() + Normal * FMath::FRandRange(8.f, 20.f) + Radial * FMath::FRandRange(0.f, 26.f) * Scale;
		Puff.Velocity = Normal * FMath::FRandRange(35.f, 85.f) + Radial * FMath::FRandRange(15.f, 65.f) + FVector::UpVector * FMath::FRandRange(12.f, 35.f);
		Puff.Size = FMath::FRandRange(32.f, 56.f) * Scale * FMath::Clamp(Settings.DustSize, .25f, 3.f);
		Puff.Lifetime = FMath::FRandRange(0.8f, 1.35f) * LifetimeScale;
		Puff.Rotation = FMath::FRandRange(-PI, PI);
		Puff.Opacity = FMath::FRandRange(0.28f, 0.4f) * FMath::Clamp(Settings.DustStrength, 0.f, 2.f);
		Puffs.Add(Puff);
		Sprites->AddInstance(FTransform(FQuat::Identity, Puff.Position, FVector(Puff.Size / 100.f)), true);
		const int32 Tile = FMath::RandRange(0, 3);
		Sprites->SetCustomDataValue(Index, 1, (Tile % 2) * 0.5f);
		Sprites->SetCustomDataValue(Index, 2, (Tile / 2) * 0.5f);
	}
	if (SmokeMaterial && Settings.SmokeStrength > 0 && Settings.SmokeCount > 0)
	{
		SmokeMID = UMaterialInstanceDynamic::Create(SmokeMaterial, this);
		SmokeMID->SetVectorParameterValue(TEXT("SoilTint"), FMath::Lerp(DustTint, FLinearColor(.3f,.28f,.24f), .3f));
		SmokeSprites->SetStaticMesh(Plane);
		SmokeSprites->SetMaterial(0, SmokeMID);
		SmokeSprites->SetNumCustomDataFloats(3);
		const int32 SmokeCount = VaryCount(Settings.SmokeCount, 24);
		for (int32 Index=0; Index<SmokeCount; ++Index)
		{
			FPuff Puff;
			Puff.Position = GetActorLocation() + Normal * FMath::FRandRange(15.f,35.f) + FMath::VRand()*12;
			Puff.Velocity = Normal*FMath::FRandRange(30.f,55.f) + FVector::UpVector*FMath::FRandRange(24.f,45.f) + FMath::VRand()*15;
			Puff.Size = FMath::FRandRange(55.f,85.f)*Scale*FMath::Clamp(Settings.SmokeSize,.25f,3.f);
			Puff.Lifetime = FMath::FRandRange(1.1f,1.6f)*LifetimeScale;
			Puff.Opacity = FMath::FRandRange(.32f,.5f)*FMath::Clamp(Settings.SmokeStrength,0.f,2.f);
			Puff.Rotation=FMath::FRandRange(-.65f,.65f);
			Smoke.Add(Puff);
			SmokeSprites->AddInstance(FTransform(FQuat::Identity,Puff.Position,FVector(Puff.Size/100)),true);
		}
	}
	// Short, bright contact flecks with varied trajectories, falling and bouncing once.
	if (SparkMaterial && Settings.SparkStrength > 0 && Settings.SparkCount > 0)
	{
		SparkMID = UMaterialInstanceDynamic::Create(SparkMaterial, this);
		if(Settings.bUseCoolPalette)SparkMID->SetVectorParameterValue(TEXT("SparkTint"),FLinearColor(.32f,.65f,.8f));
		SparkMID->SetScalarParameterValue(TEXT("Intensity"),FMath::Clamp(Settings.SparkBrightness,0.f,50.f));
		SparkSprites->SetStaticMesh(Plane);
		SparkSprites->SetMaterial(0,SparkMID);
		SparkSprites->SetNumCustomDataFloats(1);
		const int32 SparkCount = VaryCount(FMath::RoundToInt(Settings.SparkCount*FMath::Clamp(Settings.SparkStrength,0.f,2.f)),32);
		for (int32 Index=0; Index<SparkCount; ++Index)
		{
			FSpark Spark;
			Spark.Position=GetActorLocation()+Normal*10+FMath::VRand()*8;
			Spark.Velocity=(Normal*FMath::FRandRange(140.f,260.f)+FMath::VRand()*130+FVector::UpVector*FMath::FRandRange(60.f,180.f))*FMath::Clamp(Settings.SparkSpeed,.25f,3.f);
			Spark.Lifetime=FMath::FRandRange(.3f,.65f)*LifetimeScale;
			Spark.Length=FMath::FRandRange(3.5f,7.f)*FMath::Clamp(Settings.SparkSize,.25f,4.f);
			Spark.Width=FMath::FRandRange(.25f,.5f)*FMath::Clamp(Settings.SparkSize,.25f,4.f);
			Sparks.Add(Spark);
			SparkSprites->AddInstance(FTransform(FQuat::Identity,Spark.Position,FVector(.01)),true);
		}
	}
	SetLifeSpan(1.6f*LifetimeScale+.15f);
	Tick(0);
}

void AMiningDrillDustBurst::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* Player = Cast<APlayerController>(GetOwner());
	if (!Player || !Sprites) return;
	FVector ViewLocation;
	FRotator ViewRotation;
	Player->GetPlayerViewPoint(ViewLocation, ViewRotation);
	for (int32 Index = 0; Index < Puffs.Num(); ++Index)
	{
		FPuff& Puff = Puffs[Index];
		Puff.Age += DeltaSeconds;
		const float Age = FMath::Clamp(Puff.Age / Puff.Lifetime, 0.f, 1.f);
		Puff.Position += Puff.Velocity * DeltaSeconds;
		Puff.Velocity *= FMath::Exp(-2.2f * DeltaSeconds);
		Puff.Velocity.Z += 10 * DeltaSeconds;
		const float Size = Puff.Size * (1 + 1.9f * (1 - FMath::Pow(1 - Age, 2.f)));
		const FQuat Facing = FRotationMatrix::MakeFromZ((ViewLocation - Puff.Position).GetSafeNormal()).ToQuat();
		const FQuat Spin(FVector::UpVector, Puff.Rotation + Age * 0.35f);
		const float Opacity = Puff.Opacity * FMath::Min(Puff.Age / 0.055f, 1.f) * FMath::Pow(1 - Age, 1.6f);
		Sprites->SetCustomDataValue(Index, 0, Opacity);
		Sprites->UpdateInstanceTransform(Index, FTransform(Facing * Spin, Puff.Position, FVector(Size / 100.f)), true, Index == Puffs.Num()-1, true);
	}
	for (int32 Index=0; Index<Smoke.Num(); ++Index)
	{
		FPuff& Puff=Smoke[Index];
		Puff.Age+=DeltaSeconds;
		const float Age=FMath::Clamp(Puff.Age/Puff.Lifetime,0.f,1.f);
		Puff.Position+=Puff.Velocity*DeltaSeconds;
		Puff.Velocity*=FMath::Exp(-1.4f*DeltaSeconds);
		Puff.Velocity.Z+=7*DeltaSeconds;
		const int32 Frame=FMath::Clamp(8+FMath::FloorToInt(Age*48),8,56);
		SmokeSprites->SetCustomDataValue(Index,0,Puff.Opacity*FMath::Min(Puff.Age/.09f,1.f)*FMath::Pow(1-Age,1.15f));
		SmokeSprites->SetCustomDataValue(Index,1,(Frame%8)/8.f);
		SmokeSprites->SetCustomDataValue(Index,2,(Frame/8)/8.f);
		const FQuat Facing=FRotationMatrix::MakeFromZ((ViewLocation-Puff.Position).GetSafeNormal()).ToQuat();
		SmokeSprites->UpdateInstanceTransform(Index,FTransform(Facing*FQuat(FVector::UpVector,Puff.Rotation),Puff.Position,FVector(Puff.Size*(1+Age*1.3f)/100)),true,Index==Smoke.Num()-1,true);
	}
	for (int32 Index=0; Index<Sparks.Num(); ++Index)
	{
		FSpark& Spark=Sparks[Index];
		Spark.Age+=DeltaSeconds;
		const float Age=FMath::Clamp(Spark.Age/Spark.Lifetime,0.f,1.f);
		Spark.Velocity.Z-=800*DeltaSeconds;
		const FVector Next=Spark.Position+Spark.Velocity*DeltaSeconds;
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(DrillSpark),false,this);
		Query.AddIgnoredActor(GetOwner());
		if (Age<1 && !Spark.bBounced && GetWorld()->LineTraceSingleByObjectType(Hit,Spark.Position,Next,FCollisionObjectQueryParams(ECC_WorldStatic),Query))
		{
			Spark.Position=Hit.ImpactPoint+Hit.ImpactNormal*2;
			Spark.Velocity=Spark.Velocity.MirrorByVector(Hit.ImpactNormal)*.3f;
			Spark.bBounced=true;
		}
		else Spark.Position=Next;
		const FVector Normal=(ViewLocation-Spark.Position).GetSafeNormal();
		const FVector Direction=FVector::VectorPlaneProject(Spark.Velocity,Normal).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		const FQuat Facing=FRotationMatrix::MakeFromYZ(Direction,Normal).ToQuat();
		SparkSprites->SetCustomDataValue(Index,0,FMath::Pow(1-Age,1.3f));
		SparkSprites->UpdateInstanceTransform(Index,FTransform(Facing,Spark.Position,FVector(Spark.Width/100,Spark.Length*(1-Age*.6f)/100,1)),true,Index==Sparks.Num()-1,true);
	}
}
