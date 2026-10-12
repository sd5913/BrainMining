#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MiningBombPresentationComponent.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/** Visuals read the existing throw-started fuse; never drive detonation or physics. */
UCLASS(ClassGroup=(Mining), meta=(BlueprintSpawnableComponent))
class BRAINMINING_API UMiningBombPresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UMiningBombPresentationComponent();
	void Initialize(UStaticMeshComponent* Model, double ThrowTime, float FuseDuration);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bomb|Presentation", meta=(ClampMin="0",ClampMax="20")) float LampBrightness = 6.f;
	UPROPERTY(BlueprintReadOnly, Category="Bomb|Presentation") float RemainingSeconds = 3.f;
	UPROPERTY(BlueprintReadOnly, Category="Bomb|Presentation") float BlinkStrength = 0.f;
private:
	double StartedAt = -1;
	float Duration = 3.f;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> LampMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DisplayMaterial;
};
