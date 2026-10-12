#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MiningBombToolComponent.generated.h"

class UStaticMesh;
class UNiagaraSystem;
class AMiningBombActor;

USTRUCT(BlueprintType)
struct FMiningBombSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1", Units="s")) float FuseSeconds = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="100", Units="cm/s")) float ThrowSpeed = 1100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", Units="cm/s")) float ThrowUpSpeed = 220.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float InheritedVelocity = .5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", Units="cm")) float CollisionRadius = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1", Units="kg")) float Mass = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="50", Units="cm")) float BlastRadius = 700.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", Units="cm/s")) float BlastVelocity = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", Units="cm/s")) float BlastLift = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1", ClampMax="4")) float ExplosionScale = 1.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Excavation", meta=(ClampMin="100", ClampMax="1000", Units="cm")) float ExcavationRadius = 520.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Excavation", meta=(ClampMin="0", ClampMax="0.45")) float CraterRoughness = .35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Effects", meta=(ClampMin="0", ClampMax="96")) int32 DebrisCount = 72;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Effects", meta=(ClampMin="0.5", ClampMax="4", Units="s")) float DebrisLifetime = 3.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Effects", meta=(ClampMin="0", ClampMax="3")) float DustStrength = 1.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Effects", meta=(ClampMin="0", ClampMax="3")) float FireStrength = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Effects", meta=(ClampMin="0", ClampMax="3")) float SmokeStrength = 1.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMiningBombExplosionEvent, FVector, Location);

UCLASS(ClassGroup=(Mining), meta=(BlueprintSpawnableComponent))
class BRAINMINING_API UMiningBombToolComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UMiningBombToolComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Bomb") FMiningBombSettings Settings;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Bomb") bool bUseInventory = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Bomb", meta=(ClampMin="1")) int32 MaxBombs = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Bomb", meta=(ClampMin="0.1")) float RechargeSeconds = 10;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Bomb") int32 BombCount = 3;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Bomb") float RechargeProgress = 0;
	void UpdateInventory(float DeltaSeconds);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Bomb|Assets") TSoftObjectPtr<UNiagaraSystem> ExplosionAsset;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Bomb") bool bEquipped = false;
	UPROPERTY(BlueprintReadOnly, Category="Mining|Bomb") TWeakObjectPtr<AMiningBombActor> LastThrownBomb;
	UPROPERTY(BlueprintReadOnly, Category="Mining|Bomb") FVector LastExplosionLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category="Mining|Bomb") double LastExplosionTime = -1;
	UPROPERTY(BlueprintReadOnly, Category="Mining|Bomb") int32 ExplosionCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Mining|Bomb") int32 LastExcavatedRocks = 0;
	UPROPERTY(BlueprintReadOnly, Category="Mining|Bomb") int32 LastDebrisCount = 0;
	UPROPERTY(BlueprintAssignable, Category="Mining|Bomb") FMiningBombExplosionEvent OnBombExploded;
	UFUNCTION(BlueprintCallable, Category="Mining|Bomb") bool ToggleEquipped();
	UFUNCTION(BlueprintCallable, Category="Mining|Bomb") void Stow();
	UFUNCTION(BlueprintCallable, Category="Mining|Bomb") bool ThrowBomb();
	UFUNCTION(BlueprintPure, Category="Mining|Bomb") bool CanUse() const;
	void RecordExplosion(const FVector& Location);
protected:
	UFUNCTION() void ResetBombs();
	TArray<TWeakObjectPtr<AMiningBombActor>> ThrownBombs;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> LoadedExplosion;
};
