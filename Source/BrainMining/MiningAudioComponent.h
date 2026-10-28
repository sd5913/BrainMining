#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "Components/ActorComponent.h"
#include "MiningAudioComponent.generated.h"

class ACharacter;
class AMiningExhibitHall;
class AEchoPassageStage;
class UAudioComponent;
class UMiningComponent;
class UMiningWalletSubsystem;
class USoundBase;
class USoundConcurrency;

enum class EMiningAudioSurface : uint8 { Stone, Wood, Dirt, Gravel };

UCLASS(ClassGroup=(Mining), meta=(BlueprintSpawnableComponent))
class BRAINMINING_API UMiningAudioComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UMiningAudioComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Deactivate() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	void PlayPanel(bool bOpen);
	void PlayPurchase(bool bSuccess);
	void PlayCollect();
	void PlayCollectible(int32 ItemId);
	static FName CollectibleSoundName(int32 ItemId);
	void PlayStarterPickup(bool bJetpack);
	UFUNCTION() void StopTools();
	int32 GetToolLoopCount() const { return ToolLoops.Num(); }
	static TArray<FName> AssetNames();
	static TArray<FName> MainGameAssetNames();
	static TArray<FName> RoomMusicAssetNames();
	static bool IsMainGameWorld(const UWorld* World);
	static EMiningAudioSurface ResolveSurface(const FHitResult& Hit);
	int32 GetAudioRegion() const { return AudioRegion; }
	bool UsesMainGameAudio() const { return bMainGameAudio; }
	int32 GetSoundscapeCount() const { return Soundscape.Num(); }
    void BeginEndingMusicTransition();
    void CancelEndingMusicTransition();
	FName GetLastPlayedSound() const { return LastPlayedSound; }
	EMiningAudioSurface GetLastContactSurface() const { return LastContactSurface; }
	int32 GetContactCount() const { return ContactCount; }

private:
	friend class FRoomMusicRoutingTest;
	UFUNCTION() void OnSwing(float Interval);
	UFUNCTION() void OnDrillContact(const FHitResult& Hit);
	UFUNCTION() void OnBlockedContact(const FHitResult& Hit);
	void OnContact(const FHitResult& Hit, int32 Stage);
	void OnBreak(const FVector& Position);
	void OnCoins(int64 Amount);
	void Play(FName Name, float Gain = 1.f, float Pitch = 1.f, bool bUI = false);
	void PlayVariant(const FString& Prefix, int32 Count, float Gain);
	void LoadCollectibleSounds();
	bool CanPlay() const;
	bool IsMiningArea() const;
	bool HasFootsteps() const;
	void UpdateMusic(float DeltaTime);
	void UpdateTools(float DeltaTime);
	void SetToolLoop(FName Name, bool bActive, float Gain, float Pitch = 1.f);
	void UpdateMainGameSoundscape(float DeltaTime);
	void UpdateRoomMusic(float DeltaTime, float Gain);
	FName FindRoomMusic();
	void SetSoundscapeLayer(FName Name, float Target, float DeltaTime);
	void StopSoundscape();
	int32 FindAudioRegion() const;
	void PlayFootstep(const FHitResult& Hit, float Gain);

	UPROPERTY(Transient) TMap<FName, TObjectPtr<USoundBase>> Sounds;
	UPROPERTY(Transient) TObjectPtr<USoundConcurrency> EffectsConcurrency;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Music;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UAudioComponent>> ToolLoops;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UAudioComponent>> Soundscape;
	TWeakObjectPtr<ACharacter> Character;
	TWeakObjectPtr<UMiningComponent> Mining;
	TWeakObjectPtr<UMiningWalletSubsystem> Wallet;
	TWeakObjectPtr<AMiningExhibitHall> Hall;
	TWeakObjectPtr<AEchoPassageStage> RoomMusicEcho;
	TMap<FString, int32> PreviousVariants;
	FVector PreviousLocation = FVector::ZeroVector;
	float StepDistance = 0;
	float PreviousVerticalSpeed = 0;
	float MusicGain = 0;
	double NextCoinTime = 0;
	double NextHallSearch = 0;
	double DrillContactUntil = 0;
	double NextCollectTime = 0;
	double NextBlockedTime = 0;
	int32 AudioRegion = INDEX_NONE;
	int32 PendingAudioRegion = INDEX_NONE;
	float RegionStableSeconds = 0;
	bool bMainGameAudio = false;
    bool bMuseumMusicTransferred = false;
	FName LastPlayedSound;
	int32 ContactCount = 0;
	EMiningAudioSurface LastContactSurface = EMiningAudioSurface::Stone;
	bool bDrillRunning = false;
	bool bJetRunning = false;
	bool bLowDrill = false;
	bool bOverheated = false;
	bool bWasGrounded = false;
	bool bTrackingMovement = false;
};
