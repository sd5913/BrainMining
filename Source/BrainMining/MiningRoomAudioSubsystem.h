#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MiningRoomAudioSubsystem.generated.h"

class UAudioComponent;
class USoundBase;
class USoundAttenuation;
class ACardRoomStage;
class AEchoPassageStage;

UCLASS()
class BRAINMINING_API UMiningRoomAudioSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    static void Emit(const UObject* Source, FName Cue, float Gain = .5f, bool Spatial = false, float Cooldown = .12f);
    static void Loop(const UObject* Source, FName Cue, float Gain, bool Spatial = false);
    static void StopSource(const UObject* Source);
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual void Deinitialize() override;
    int32 EventCount(FName Cue) const { return Counts.FindRef(Cue); }
    int32 LoopCount() const { return Players.Num(); }
    USoundBase* Resolve(FName Cue);
protected:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
private:
    bool CanPlay() const;
    void ConfigurePlugins();
    void Play(const UObject* Source, FName Cue, float Gain, bool Spatial, float Cooldown, const FVector* Position = nullptr);
    void UpdateLoop(const UObject* Source, FName Cue, float Gain, bool Spatial);
    void StopAll();
    UPROPERTY(Transient) TMap<FName,TObjectPtr<USoundBase>> Sounds;
    UPROPERTY(Transient) TMap<FString,TObjectPtr<UAudioComponent>> Players;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> Attenuation;
    TMap<FString,double> LastEvent;
    TMap<FString,double> RefreshedAt;
    TMap<FString,float> Targets;
    TMap<FString,TWeakObjectPtr<UObject>> Owners;
    TMap<FName,int32> Counts;
    TMap<FName,int32> Variants;
    TWeakObjectPtr<ACardRoomStage> Cards;
    TWeakObjectPtr<AEchoPassageStage> Echo;
    double NextDiscovery = 0;
};
