#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningBombToolComponent.h"
#include "MiningBombActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPhysicalMaterial;
class UNiagaraComponent;
class UPointLightComponent;
class UInstancedStaticMeshComponent;
class UAudioComponent;
class USoundAttenuation;

/** Physical flight until first contact, then attached until the throw-started fuse expires. */
UCLASS()
class BRAINMINING_API AMiningBombActor : public AActor
{
	GENERATED_BODY()
public:
	AMiningBombActor();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	void Launch(UStaticMesh* Mesh, UNiagaraSystem* Effect, UMiningBombToolComponent* Tool,
		const FMiningBombSettings& InSettings, const FVector& Velocity);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bomb") TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bomb") TObjectPtr<UStaticMeshComponent> Model;
	UPROPERTY(BlueprintReadOnly, Category="Bomb") double LaunchedAt = -1;
	UPROPERTY(BlueprintReadOnly, Category="Bomb") int32 ContactCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Bomb") bool bExploded = false;
	UPROPERTY(BlueprintReadOnly, Category="Bomb") bool bStuck = false;
	UPROPERTY(BlueprintReadOnly, Category="Bomb") double StuckAt = -1;
	UFUNCTION(BlueprintCallable, Category="Bomb") void Explode();
private:
	void PlayBombSound(const TCHAR* Name, float Gain);
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> FuseAudio;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> AudioAttenuation;
	UPROPERTY() TObjectPtr<UPhysicalMaterial> BounceMaterial;
	UPROPERTY() TObjectPtr<UNiagaraSystem> Explosion;
	TWeakObjectPtr<UMiningBombToolComponent> SourceTool;
	FMiningBombSettings Settings;
	FTimerHandle FuseTimer, PawnGraceTimer;
	UFUNCTION() void Contact(UPrimitiveComponent* Hit, AActor* Other, UPrimitiveComponent* OtherComponent,
		FVector Impulse, const FHitResult& Result);
	void ApplyShockwave(const FVector& Location);
	void ExcavateTerrain(const FVector& Location);
};

/** Independent finite effect actor so smoke survives the destruction of the bomb. */
UCLASS()
class BRAINMINING_API AMiningBombExplosion : public AActor
{
	GENERATED_BODY()
public:
	AMiningBombExplosion();
	void Initialize(UNiagaraSystem* System, const FMiningBombSettings& Settings);
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Explosion") TObjectPtr<UNiagaraComponent> Effect;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Explosion") TObjectPtr<UPointLightComponent> Flash;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Explosion") TObjectPtr<UInstancedStaticMeshComponent> Fire;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Explosion") TObjectPtr<UInstancedStaticMeshComponent> Smoke;
private:
	float Age = 0;
	float FlashStrength = 1;
	struct FPuff { FVector Position, Velocity; FQuat Rotation; float Lifetime, Size, Delay, Opacity; };
	TArray<FPuff> Flames, Clouds;
};
