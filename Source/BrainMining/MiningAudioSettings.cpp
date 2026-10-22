#include "MiningAudioSettings.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace
{
    const TCHAR* ChannelNames[] = {TEXT("Master"), TEXT("Music"), TEXT("Effects"), TEXT("Ambience"), TEXT("Voice")};
    FString AudioSettingsFile() { return FPaths::ProjectSavedDir() / TEXT("Config/BrainMiningAudio.ini"); }
}

void UMiningAudioSettings::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadVolumes(AudioSettingsFile());
#if PLATFORM_WINDOWS
    if(auto* Graphics=UGameUserSettings::GetGameUserSettings();Graphics && Graphics->GetFrameRateLimit()<=0)
    {
        Graphics->SetFrameRateLimit(60.f);
        Graphics->ApplyNonResolutionSettings();
    }
#endif
    for (int32 Channel = 1; Channel < 5; ++Channel)
        Classes.Add(LoadObject<USoundClass>(nullptr, *FString::Printf(TEXT("/Game/Audio/Mix/SC_%s.SC_%s"), ChannelNames[Channel], ChannelNames[Channel])));
    Mix = LoadObject<USoundMix>(nullptr, TEXT("/Game/Audio/Mix/SM_Player.SM_Player"));
}

float UMiningAudioSettings::GetVolume(int32 Channel) const { return Volumes.IsValidIndex(Channel) ? Volumes[Channel] : 0.f; }
USoundClass* UMiningAudioSettings::GetClass(int32 Channel) const { return Classes.IsValidIndex(Channel - 1) ? Classes[Channel - 1].Get() : nullptr; }

void UMiningAudioSettings::SetVolume(int32 Channel, float Value)
{
    if (!Volumes.IsValidIndex(Channel) || !FMath::IsFinite(Value)) return;
    Volumes[Channel] = FMath::Clamp(Value, 0.f, 1.f);
    Apply();
}

void UMiningAudioSettings::RestoreDefaults() { Volumes = {1.f, 1.f, .85f, .8f, 1.f}; LookSensitivity=1.f; FieldOfView=100.f; Apply(); }

void UMiningAudioSettings::LoadVolumes(const FString& Filename)
{
    FConfigFile Config;
    Config.Read(Filename);
    float Fov=100.f;
    if(Config.GetFloat(TEXT("BrainMining.Controls"),TEXT("FieldOfView"),Fov)) SetFieldOfView(Fov);
    float Sensitivity=1.f;
    if(Config.GetFloat(TEXT("BrainMining.Controls"),TEXT("LookSensitivity"),Sensitivity)) SetLookSensitivity(Sensitivity);
    for (int32 Channel = 0; Channel < 5; ++Channel)
    {
        float Value = Volumes[Channel];
        if (Config.GetFloat(TEXT("BrainMining.Audio"), ChannelNames[Channel], Value) && FMath::IsFinite(Value))
            Volumes[Channel] = FMath::Clamp(Value, 0.f, 1.f);
    }
}

void UMiningAudioSettings::SaveVolumes(const FString& Filename) const
{
    FConfigFile Config;
    Config.Read(Filename);
    Config.SetFloat(TEXT("BrainMining.Controls"),TEXT("LookSensitivity"),LookSensitivity);
    Config.SetFloat(TEXT("BrainMining.Controls"),TEXT("FieldOfView"),FieldOfView);
    for (int32 Channel = 0; Channel < 5; ++Channel)
        Config.SetFloat(TEXT("BrainMining.Audio"), ChannelNames[Channel], Volumes[Channel]);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    if (!Config.Write(Filename)) UE_LOG(LogTemp, Warning, TEXT("Could not save audio settings: %s"), *Filename);
}

void UMiningAudioSettings::Save() const { SaveVolumes(AudioSettingsFile()); }

void UMiningAudioSettings::Apply()
{
    if (!GetWorld() || !Mix) return;
    for (int32 Channel = 1; Channel < 5; ++Channel)
        if (auto* SoundClass = GetClass(Channel))
            UGameplayStatics::SetSoundMixClassOverride(this, Mix, SoundClass, Volumes[0] * Volumes[Channel], 1.f, .08f, false);
    if (AppliedWorld != GetWorld())
    {
        UGameplayStatics::PushSoundMixModifier(this, Mix);
        AppliedWorld = GetWorld();
    }
}

void UMiningAudioSettings::Tick(float DeltaTime) { if (AppliedWorld != GetWorld()) Apply(); }

void UMiningAudioSettings::Deinitialize()
{
    Save();
    if (AppliedWorld.IsValid() && Mix) UGameplayStatics::PopSoundMixModifier(AppliedWorld.Get(), Mix);
    Super::Deinitialize();
}
