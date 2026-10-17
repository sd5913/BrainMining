#include "PopupClose/PopupOfficeExplosionShake.h"

void UPopupOfficeExplosionPattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(Duration);
	OutInfo.BlendIn = .015f;
	OutInfo.BlendOut = .18f;
}

void UPopupOfficeExplosionPattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	Elapsed = 0.f;
	if (Params.bOverrideDuration && Params.DurationOverride > 0.f)
		Duration = Params.DurationOverride;
}

void UPopupOfficeExplosionPattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	Elapsed += FMath::Max(0.f, Params.DeltaTime);
	const float Alpha = FMath::Clamp(1.f - Elapsed / FMath::Max(.01f, Duration), 0.f, 1.f);
	const float Envelope = Alpha * Alpha;
	const float Time = Elapsed * 38.f;
	const float Scale = Amplitude * Envelope * Params.GetTotalScale();
	OutResult.Location += FVector(FMath::Sin(Time * 1.17f), FMath::Cos(Time * .91f), FMath::Sin(Time * 1.63f)) * (2.8f * Scale);
	OutResult.Rotation += FRotator(FMath::Sin(Time * .83f) * 1.7f * Scale,
		FMath::Cos(Time * 1.09f) * 1.3f * Scale, FMath::Sin(Time * 1.41f) * .9f * Scale);
}

UPopupOfficeExplosionShake::UPopupOfficeExplosionShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = false;
	ChangeRootShakePattern<UPopupOfficeExplosionPattern>();
}
