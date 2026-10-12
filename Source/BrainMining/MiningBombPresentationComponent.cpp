#include "MiningBombPresentationComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

UMiningBombPresentationComponent::UMiningBombPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UMiningBombPresentationComponent::Initialize(UStaticMeshComponent* Model, double ThrowTime, float FuseDuration)
{
	StartedAt = ThrowTime;
	Duration = FMath::Clamp(FuseDuration, .1f, 30.f);
	const int32 LampSlot = Model->GetMaterialIndex(TEXT("BombFuseLamp"));
	const int32 DisplaySlot = Model->GetMaterialIndex(TEXT("BombCountdownDisplay"));
	if (LampSlot != INDEX_NONE) LampMaterial = Model->CreateDynamicMaterialInstance(LampSlot);
	if (DisplaySlot != INDEX_NONE) DisplayMaterial = Model->CreateDynamicMaterialInstance(DisplaySlot);
	TickComponent(0.f, LEVELTICK_All, nullptr);
}

void UMiningBombPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (StartedAt < 0 || !GetWorld()) return;
	const float Age = FMath::Max(0.f, float(GetWorld()->GetTimeSeconds() - StartedAt));
	RemainingSeconds = FMath::Clamp(Duration - Age, 0.f, Duration);
	// Integrated increasing frequency: slow pulses initially, urgent flashes near zero.
	const float Phase = 2.f * PI * (.85f * Age + 2.2f * Age * Age / Duration);
	BlinkStrength = FMath::Pow(FMath::Max(0.f, FMath::Cos(Phase)), 3.f);
	if (LampMaterial) LampMaterial->SetScalarParameterValue(TEXT("BlinkStrength"), .04f + LampBrightness * BlinkStrength);
	if (DisplayMaterial) DisplayMaterial->SetScalarParameterValue(TEXT("RemainingSeconds"), RemainingSeconds);
}
