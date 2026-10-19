#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Tickable.h"
#include "MiningHapticMixer.h"
#include "MiningHapticsSubsystem.generated.h"

class APlayerController;
class APawn;
class AEchoPassageStage;
class UMiningComponent;
class SWidget;

UCLASS(Config=Game)
class BRAINMINING_API UMiningHapticsSubsystem : public ULocalPlayerSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMiningHapticsSubsystem, STATGROUP_Tickables); }
    virtual bool IsTickable() const override { return !IsTemplate(); }
    virtual bool IsTickableWhenPaused() const override { return true; }
    virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
    static UMiningHapticsSubsystem* Find(const APlayerController* Player);
    static void Emit(const UObject* Source, EMiningHapticCue Cue, float Gain = 1.f);
    static void EmitAt(const UObject* Source, EMiningHapticCue Cue, FVector Location, float Radius, float Gain = 1.f);
    static void MiningContact(UMiningComponent* Mining);
    static void FinaleThrust(const UObject* Source, bool bActive, bool bComfortMode);
    bool Play(EMiningHapticCue Cue, float Gain = 1.f);
    void Stop();
    void SetEnabled(bool Enabled);
    void SetStrength(float Strength);
    void SetContinuousEnabled(bool Enabled);
    void SetOptionalEnabled(bool Enabled);
    void LoadPreferences(const FString& Filename);
    void SavePreferences(const FString& Filename) const;
    TSharedRef<SWidget> MakeSettingsWidget();
    FVector2D GetOutput() const { return Output; }
    bool IsCueActive(EMiningHapticCue Cue) const { return Mixer.IsActive(Cue); }
    uint64 GetAcceptedCueCount() const { return AcceptedCueCount; }
    UPROPERTY(Config, EditAnywhere, Category="Gamepad|Vibration") bool bEnabled = true;
    UPROPERTY(Config, EditAnywhere, Category="Gamepad|Vibration", meta=(ClampMin="0", ClampMax="1")) float Strength = .75f;
    UPROPERTY(Config, EditAnywhere, Category="Gamepad|Vibration") bool bContinuous = true;
    UPROPERTY(Config, EditAnywhere, Category="Gamepad|Vibration") bool bOptional = false;
private:
    APlayerController* Controller() const;
    bool CanPlay() const;
    void SyncIdentity();
    void SampleGameplay(float DeltaTime, bool bAllowed);
    void WriteOutput();
    void OnEcho(AEchoPassageStage* Stage, FName Event, FVector Location);
    void Save() const;
    FMiningHapticMixer Mixer;
    TWeakObjectPtr<APlayerController> LastController;
    TWeakObjectPtr<APawn> LastPawn;
    TWeakObjectPtr<UWorld> LastWorld;
    int32 LastArea = INDEX_NONE;
    int32 LastInputContext = INDEX_NONE;
    uint64 LowHandle = 0;
    uint64 HighHandle = 0;
    uint64 AcceptedCueCount = 0;
    FDelegateHandle EchoHandle;
    FVector2D Output = FVector2D::ZeroVector;
    FVector2D Continuous = FVector2D::ZeroVector;
    FVector PreviousLocation = FVector::ZeroVector;
    float PreviousVerticalSpeed = 0;
    float DrillContactRemaining = 0;
    float FinaleRemaining = 0;
    float FinaleGain = 1;
    float AnimationTime = 0;
    bool bSamplesReady = false;
    bool bWasGrounded = false;
    bool bWasDrill = false;
    bool bWasLow = false;
    bool bWasEmpty = false;
    bool bWasJet = false;
    bool bWasHot = false;
};
