#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "PopupOfficeExplosionShake.generated.h"

/** Small deterministic camera shake used by the staged office collapse. */
UCLASS(MinimalAPI)
class UPopupOfficeExplosionPattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:
	float Duration = .48f;
	float Elapsed = 0.f;
	float Amplitude = 1.f;

protected:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override { return Elapsed >= Duration; }
};

UCLASS(MinimalAPI)
class UPopupOfficeExplosionShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UPopupOfficeExplosionShake(const FObjectInitializer& ObjectInitializer);
};
