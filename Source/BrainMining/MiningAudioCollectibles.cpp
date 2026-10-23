#include "MiningAudioComponent.h"
#include "MiningCollection.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"

FName UMiningAudioComponent::CollectibleSoundName(int32 ItemId)
{
    static const FName Names[] = {
        TEXT("S_Collect_Toilet"), TEXT("S_Collect_AntComputer"),
        TEXT("S_Collect_Reagent"), TEXT("S_Collect_Pigeon"),
        TEXT("S_Collect_Calculator"), TEXT("S_Collect_Cassette"),
        TEXT("S_Collect_Note"), TEXT("S_Collect_Photo"),
        TEXT("S_Collect_Cola"), TEXT("S_Collect_Flower"),
        TEXT("S_Collect_Fireflies"), TEXT("S_Collect_Ribbon"),
        TEXT("S_Collect_Copper"),TEXT("S_Collect_Iron"),TEXT("S_Collect_Malachite"),TEXT("S_Collect_Diamond")
    };
    static_assert(UE_ARRAY_COUNT(Names) == AMiningCollectible::ItemTypeCount);
    return ItemId >= 0 && ItemId < UE_ARRAY_COUNT(Names) ? Names[ItemId] : NAME_None;
}

void UMiningAudioComponent::LoadCollectibleSounds()
{
    for(int32 ItemId = 0; ItemId < AMiningCollectible::ItemTypeCount; ++ItemId)
    {
        const FName Name = CollectibleSoundName(ItemId);
        const FString Path = FString::Printf(TEXT("/Game/Audio/Collectibles/%s.%s"), *Name.ToString(), *Name.ToString());
        if(auto* Sound = LoadObject<USoundBase>(nullptr, *Path)) Sounds.Add(Name, Sound);
        else UE_LOG(LogTemp, Warning, TEXT("Collectible audio asset missing: %s"), *Path);
    }
}

void UMiningAudioComponent::PlayCollectible(int32 ItemId)
{
    const FName Name = CollectibleSoundName(ItemId);
    if(Name.IsNone() || !CanPlay()) return;
    NextCoinTime = GetWorld()->GetTimeSeconds() + .12;
    Play(Sounds.Contains(Name) ? Name : FName(TEXT("S_Collect")), .65f);
}
