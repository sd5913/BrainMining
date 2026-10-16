#include "MiningRoomAudioSubsystem.h"
#include "MiningAudioComponent.h"
#include "MiningRoomEscape.h"
#include "Showcase/MiningExhibitHall.h"
#include "CardRoomStage.h"
#include "EchoPassageStage.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace
{
    FString AudioKey(const UObject* Source, FName Cue)
    {
        return FString::Printf(TEXT("%u:%s"), Source->GetUniqueID(), *Cue.ToString());
    }
    FVector AudioPosition(const UObject* Source)
    {
        if (const auto* Actor = Cast<AActor>(Source)) return Actor->GetActorLocation();
        if (const auto* Component = Cast<USceneComponent>(Source)) return Component->GetComponentLocation();
        return FVector::ZeroVector;
    }
    float EffectsGain(FName Cue, float Gain)
    {
        const auto* Volume = IConsoleManager::Get().FindConsoleVariable(TEXT("bm.Audio.SFXVolume"));
        return FMath::Max(0.f, Gain) * (Cue.ToString().StartsWith(TEXT("A_")) ? 1.f : Volume ? FMath::Clamp(Volume->GetFloat(), 0.f, 1.f) : .7f);
    }
}

bool UMiningRoomAudioSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UMiningRoomAudioSubsystem::CanPlay() const
{
    if (!GetWorld() || !UMiningAudioComponent::IsMainGameWorld(GetWorld()) || UGameplayStatics::IsGamePaused(GetWorld())) return false;
    const auto* Controller = GetWorld()->GetFirstPlayerController();
    if (!Controller || !Controller->IsLocalController() || !Controller->GetPawn()) return false;
    if (const auto* Audio = Controller->GetPawn()->FindComponentByClass<UMiningAudioComponent>(); Audio && !Audio->IsActive()) return false;
    const auto* Enabled = IConsoleManager::Get().FindConsoleVariable(TEXT("bm.Audio.Enabled"));
    return !Enabled || Enabled->GetInt() != 0;
}

USoundBase* UMiningRoomAudioSubsystem::Resolve(FName Cue)
{
    if (const auto* Found = Sounds.Find(Cue)) return Found->Get();
    const FString Path = FString::Printf(TEXT("/Game/Audio/Rooms/%s.%s"), *Cue.ToString(), *Cue.ToString());
    auto* Sound = LoadObject<USoundBase>(nullptr, *Path);
    Sounds.Add(Cue, Sound);
    if (!Sound) UE_LOG(LogTemp, Warning, TEXT("Room audio asset missing: %s"), *Path);
    return Sound;
}

void UMiningRoomAudioSubsystem::Emit(const UObject* Source, FName Cue, float Gain, bool Spatial, float Cooldown)
{
    if (Source && Source->GetWorld())
        if (auto* Audio = Source->GetWorld()->GetSubsystem<UMiningRoomAudioSubsystem>()) Audio->Play(Source, Cue, Gain, Spatial, Cooldown);
}

void UMiningRoomAudioSubsystem::Loop(const UObject* Source, FName Cue, float Gain, bool Spatial)
{
    if (Source && Source->GetWorld())
        if (auto* Audio = Source->GetWorld()->GetSubsystem<UMiningRoomAudioSubsystem>()) Audio->UpdateLoop(Source, Cue, Gain, Spatial);
}

void UMiningRoomAudioSubsystem::StopSource(const UObject* Source)
{
    if (!Source || !Source->GetWorld()) return;
    auto* Audio = Source->GetWorld()->GetSubsystem<UMiningRoomAudioSubsystem>();
    if (!Audio) return;
    for (auto Iterator = Audio->Players.CreateIterator(); Iterator; ++Iterator)
        if (Audio->Owners.FindRef(Iterator.Key()).Get() == Source)
        {
            if (Iterator.Value()) { Iterator.Value()->Stop(); Iterator.Value()->DestroyComponent(); }
            Audio->RefreshedAt.Remove(Iterator.Key()); Audio->Targets.Remove(Iterator.Key()); Audio->Owners.Remove(Iterator.Key());
            Iterator.RemoveCurrent();
        }
}

void UMiningRoomAudioSubsystem::Play(const UObject* Source, FName Cue, float Gain, bool Spatial, float Cooldown, const FVector* Position)
{
    if (!CanPlay()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    const FString Key = AudioKey(Source, Cue);
    if (const double* Last = LastEvent.Find(Key); Last && Now - *Last < Cooldown) return;
    FName Asset = Cue;
    if (Cue == TEXT("S_OfficeNotify") || Cue == TEXT("S_Paper") || Cue == TEXT("S_CardSelect") || Cue == TEXT("S_Glass") || Cue == TEXT("S_EchoKnock"))
    {
        int32& Variant = Variants.FindOrAdd(Cue);
        Variant = (Variant + FMath::RandRange(1, 2)) % 3;
        Asset = FName(*FString::Printf(TEXT("%s_%02d"), *Cue.ToString(), Variant));
    }
    auto* Sound = Resolve(Asset);
    if (!Sound) return;
    LastEvent.Add(Key, Now);
    Counts.FindOrAdd(Cue)++;
    const float Volume = EffectsGain(Cue, Gain);
    if (Spatial)
    {
        if (!Attenuation)
        {
            Attenuation = NewObject<USoundAttenuation>(this);
            Attenuation->Attenuation.bAttenuate = true;
            Attenuation->Attenuation.bSpatialize = true;
            Attenuation->Attenuation.AttenuationShapeExtents = FVector(250.f);
            Attenuation->Attenuation.FalloffDistance = 2400.f;
        }
        UGameplayStatics::PlaySoundAtLocation(this, Sound, Position ? *Position : AudioPosition(Source), Volume, 1.f, 0.f, Attenuation);
    }
    else UGameplayStatics::PlaySound2D(this, Sound, Volume, 1.f, 0.f, nullptr, nullptr, false);
}

void UMiningRoomAudioSubsystem::UpdateLoop(const UObject* Source, FName Cue, float Gain, bool Spatial)
{
    if (!CanPlay() || Gain <= 0) return;
    const FString Key = AudioKey(Source, Cue);
    auto* Existing = Players.Find(Key);
    if (!Existing)
    {
        auto* Sound = Resolve(Cue);
        if (!Sound) return;
        auto* Player = NewObject<UAudioComponent>(this);
        Player->bAutoActivate = false;
        Player->bAutoDestroy = false;
        Player->bAllowSpatialization = Spatial;
        Player->bOverrideAttenuation = Spatial;
        Player->AttenuationOverrides.bAttenuate = Spatial;
        Player->AttenuationOverrides.bSpatialize = Spatial;
        Player->AttenuationOverrides.AttenuationShapeExtents = FVector(250.f);
        Player->AttenuationOverrides.FalloffDistance = 2400.f;
        Player->SetSound(Sound);
        Player->SetVolumeMultiplier(0.f);
        Player->RegisterComponentWithWorld(GetWorld());
        Player->SetWorldLocation(AudioPosition(Source));
        Player->Play();
        Players.Add(Key, Player);
        Existing = Players.Find(Key);
    }
    (*Existing)->SetWorldLocation(AudioPosition(Source));
    RefreshedAt.Add(Key, GetWorld()->GetTimeSeconds());
    Targets.Add(Key, EffectsGain(Cue, Gain));
    Owners.Add(Key, const_cast<UObject*>(Source));
}

void UMiningRoomAudioSubsystem::ConfigurePlugins()
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now < NextDiscovery) return;
    NextDiscovery = Now + 1.;
    const TWeakObjectPtr<UMiningRoomAudioSubsystem> WeakThis(this);
    if (!Cards.IsValid())
        for (TActorIterator<ACardRoomStage> Actor(GetWorld()); Actor; ++Actor)
        {
            Cards = *Actor;
            Cards->AudioOverride.BindLambda([WeakThis](FName Name, float Gain)
            {
                auto* Audio = WeakThis.Get();
                if (!Audio || !Audio->Cards.IsValid()) return;
                static const TMap<FName,FName> Mapping = {
                    {TEXT("S_Arrival"),TEXT("S_CardArrival")},{TEXT("S_Draw"),TEXT("S_CardDraw")},
                    {TEXT("S_Laser"),TEXT("S_CardLaser")},{TEXT("S_Poison"),TEXT("S_CardPoison")},
                    {TEXT("S_Finale"),TEXT("S_CardFinale")},{TEXT("S_Impact"),TEXT("S_CardImpact")},
                    {TEXT("S_Victory"),TEXT("S_CardVictory")},{TEXT("S_Reward"),TEXT("S_CardReward")}};
                const auto* Cue = Mapping.Find(Name);
                Audio->Play(Audio->Cards.Get(), Cue ? *Cue : Name, Gain, false, .07f);
            });
            break;
        }
    if (!Echo.IsValid())
        for (TActorIterator<AEchoPassageStage> Actor(GetWorld()); Actor; ++Actor)
        {
            Echo = *Actor;
            Echo->AudioOverride.BindLambda([WeakThis](FName Name, FVector Position, float Gain)
            {
                auto* Audio = WeakThis.Get();
                if (!Audio || !Audio->Echo.IsValid()) return;
                const FName Cue = Name == TEXT("S_Knock") ? TEXT("S_EchoKnock") : Name == TEXT("S_Landing") ? TEXT("S_EchoLanding") : Name == TEXT("S_Crumble") ? TEXT("S_EchoCrumble") : TEXT("S_EchoExit");
                const bool Spatial = Name == TEXT("S_Knock") || Name == TEXT("S_Landing");
                Audio->Play(Audio->Echo.Get(), Cue, Gain, Spatial, .1f, &Position);
            });
            break;
        }
}

void UMiningRoomAudioSubsystem::Tick(float DeltaTime)
{
    if (!UMiningAudioComponent::IsMainGameWorld(GetWorld())) return;
    ConfigurePlugins();
    if (!CanPlay()) { StopAll(); return; }
    if (auto* Hall = AMiningExhibitHall::Find(GetWorld()); Hall && Hall->bReady)
    {
        if (Hall->AreaKind(Hall->ActiveArea) == EMiningExperienceKind::Museum) UpdateLoop(Hall, TEXT("A_Museum_Loop"), .14f, false);
        if (Hall->AreaKind(Hall->ActiveArea) == EMiningExperienceKind::Journey && Echo.IsValid() && Echo->bCompleted && Hall->Journey && Hall->Journey->Phase == EMiningEscapePhase::Idle)
            UpdateLoop(Hall, TEXT("A_Giant_Loop"), .12f, false);
    }
    const double Now = GetWorld()->GetTimeSeconds();
    for (auto Iterator = Players.CreateIterator(); Iterator; ++Iterator)
    {
        const bool Alive = Owners.FindRef(Iterator.Key()).IsValid() && Now - RefreshedAt.FindRef(Iterator.Key()) < .35;
        auto* Player = Iterator.Value().Get();
        const float Gain = FMath::FInterpConstantTo(Player->VolumeMultiplier, Alive ? Targets.FindRef(Iterator.Key()) : 0.f, DeltaTime, .6f);
        Player->SetVolumeMultiplier(Gain);
        if (!Alive && Gain <= 0.f)
        {
            Player->Stop(); Player->DestroyComponent();
            RefreshedAt.Remove(Iterator.Key()); Targets.Remove(Iterator.Key()); Owners.Remove(Iterator.Key());
            Iterator.RemoveCurrent();
        }
    }
}

void UMiningRoomAudioSubsystem::StopAll()
{
    for (auto& Pair : Players) if (Pair.Value) { Pair.Value->Stop(); Pair.Value->DestroyComponent(); }
    Players.Empty(); RefreshedAt.Empty(); Targets.Empty(); Owners.Empty();
}

void UMiningRoomAudioSubsystem::Deinitialize()
{
    StopAll();
    if (Cards.IsValid()) Cards->AudioOverride.Unbind();
    if (Echo.IsValid()) Echo->AudioOverride.Unbind();
    Super::Deinitialize();
}

TStatId UMiningRoomAudioSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UMiningRoomAudioSubsystem, STATGROUP_Tickables);
}
