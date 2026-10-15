#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "Components/ActorComponent.h"
#include "MiningAudioComponent.generated.h"

class ACharacter;
class AMiningExhibitHall;
class UAudioComponent;
class UMiningComponent;
class UMiningWalletSubsystem;
class USoundBase;
class USoundConcurrency;

UCLASS(ClassGroup=(Mining), meta=(BlueprintSpawnableComponent))
class BRAINMINING_API UMiningAudioComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UMiningAudioComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	void PlayPanel(bool bOpen);
	void PlayPurchase(bool bSuccess);
	static TArray<FName> AssetNames();

private:
	UFUNCTION() void OnSwing(float Interval);
	void OnContact(const FHitResult& Hit, int32 Stage);
	void OnBreak(const FVector& Position);
	void OnCoins(int64 Amount);
	void Play(FName Name, float Gain = 1.f, float Pitch = 1.f, bool bUI = false);
	void PlayVariant(const FString& Prefix, int32 Count, float Gain);
	bool CanPlay() const;
	bool IsMiningArea() const;
	bool HasFootsteps() const;
	void UpdateMusic(float DeltaTime);

	UPROPERTY(Transient) TMap<FName, TObjectPtr<USoundBase>> Sounds;
	UPROPERTY(Transient) TObjectPtr<USoundConcurrency> EffectsConcurrency;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Music;
	TWeakObjectPtr<ACharacter> Character;
	TWeakObjectPtr<UMiningComponent> Mining;
	TWeakObjectPtr<UMiningWalletSubsystem> Wallet;
	TWeakObjectPtr<AMiningExhibitHall> Hall;
	TMap<FString, int32> PreviousVariants;
	FVector PreviousLocation = FVector::ZeroVector;
	float StepDistance = 0;
	float PreviousVerticalSpeed = 0;
	float MusicGain = 0;
	double NextCoinTime = 0;
	double NextHallSearch = 0;
	bool bWasGrounded = false;
	bool bTrackingMovement = false;
};
