#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningDrillDustBurst.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMesh;

/** Per-impact appearance, copied from the editable character effects component. */
struct FMiningDrillContactSettings
{
	int32 DustCount = 24, SmokeCount = 10, SparkCount = 12;
	float DustStrength = 1, SmokeStrength = 1.3f, SparkStrength = 1;
	float DustSize = 1.2f, SmokeSize = 1.25f, SparkSize = 2;
	float LifetimeScale = 1.35f, SparkSpeed = 1, SparkBrightness = 18;
};

/** Local, short-lived lit dust sprites; one instanced draw per impact. */
UCLASS()
class BRAINMINING_API AMiningDrillDustBurst : public AActor
{
	GENERATED_BODY()
public:
	AMiningDrillDustBurst();
	void Initialize(UStaticMesh* Plane, UMaterialInterface* DustMaterial, UMaterialInterface* SmokeMaterial, UMaterialInterface* SparkMaterial, UMaterialInterface* SoilMaterial, const FVector& Normal, float CutRadius, const FLinearColor& FallbackTint, const FMiningDrillContactSettings& Settings);
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Drill|Dust")
	TObjectPtr<UInstancedStaticMeshComponent> Sprites;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Drill|Dust")
	TObjectPtr<UInstancedStaticMeshComponent> SmokeSprites;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Drill|Dust")
	TObjectPtr<UInstancedStaticMeshComponent> SparkSprites;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Drill|Dust")
	TObjectPtr<UMaterialInterface> SourceMaterial;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Drill|Dust")
	FLinearColor DustTint;
private:
	struct FPuff
	{
		FVector Position, Velocity;
		float Age = 0, Lifetime = 1, Size = 40, Rotation = 0, Opacity = 0.15f;
	};
	TArray<FPuff> Puffs;
	TArray<FPuff> Smoke;
	struct FSpark { FVector Position, Velocity; float Age=0, Lifetime=.5f, Length=5, Width=.4f; bool bBounced=false; };
	TArray<FSpark> Sparks;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SmokeMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SparkMID;
};
