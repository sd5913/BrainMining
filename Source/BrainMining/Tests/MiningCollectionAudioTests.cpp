#include "MiningAudioComponent.h"
#include "MiningCollection.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollectionAudioAssetsTest, "BrainMining.CollectionAudio.ImportedAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCollectionAudioAssetsTest::RunTest(const FString&)
{
    TSet<FName> UniqueSounds;
    auto* Effects = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/Mix/SC_Effects.SC_Effects"));
    TestNotNull(TEXT("Effects mix exists"), Effects);
    for(int32 ItemId = 0; ItemId < AMiningCollectible::ItemTypeCount; ++ItemId)
    {
        const FName Name = UMiningAudioComponent::CollectibleSoundName(ItemId);
        UniqueSounds.Add(Name);
        const FString Path = FString::Printf(TEXT("/Game/Audio/Collectibles/%s.%s"), *Name.ToString(), *Name.ToString());
        auto* Sound = LoadObject<USoundWave>(nullptr, *Path);
        if(!TestNotNull(*Path, Sound)) continue;
        TestFalse(TEXT("Pickup does not loop"), Sound->bLooping);
        TestEqual(TEXT("Pickup uses effects volume"), Sound->SoundClassObject.Get(), Effects);
        TestEqual(TEXT("Pickup is mono"), Sound->NumChannels, 1);
        TestTrue(TEXT("Pickup stays brief"), Sound->Duration > .1f && Sound->Duration < 1.5f);
    }
    TestEqual(TEXT("Every collectible has its own sound"), UniqueSounds.Num(), AMiningCollectible::ItemTypeCount);
    TestTrue(TEXT("Negative item IDs are silent"), UMiningAudioComponent::CollectibleSoundName(-1).IsNone());
    TestTrue(TEXT("Unknown item IDs are silent"), UMiningAudioComponent::CollectibleSoundName(AMiningCollectible::ItemTypeCount).IsNone());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollectionPickupAudioTest, "BrainMining.CollectionAudio.SuccessfulPickupRouting", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCollectionPickupAudioTest::RunTest(const FString&)
{
    auto* PawnClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/Showcase/Blueprints/BP_SharedMiningCharacter.BP_SharedMiningCharacter_C"));
    if(!TestNotNull(TEXT("MainGame shared pawn loads"), PawnClass)) return false;
    auto* Enabled = IConsoleManager::Get().FindConsoleVariable(TEXT("bm.Audio.Enabled"));
    auto* EffectsVolume = IConsoleManager::Get().FindConsoleVariable(TEXT("bm.Audio.SFXVolume"));
    if(!TestNotNull(TEXT("Audio switch exists"), Enabled) || !TestNotNull(TEXT("Effects gain exists"), EffectsVolume)) return false;
    const int32 PreviousEnabled = Enabled->GetInt();
    const float PreviousVolume = EffectsVolume->GetFloat();
    Enabled->SetWithCurrentPriority(1);
    EffectsVolume->SetWithCurrentPriority(1.f);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    UMiningAudioComponent* Audio = nullptr;
    ON_SCOPE_EXIT
    {
        if(Audio && Audio->HasBegunPlay()) Audio->EndPlay(EEndPlayReason::Destroyed);
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
        Enabled->SetWithCurrentPriority(PreviousEnabled);
        EffectsVolume->SetWithCurrentPriority(PreviousVolume);
    };
    auto* Pawn = World->SpawnActor<ACharacter>(PawnClass);
    auto* Controller = World->SpawnActor<APlayerController>();
    World->AddController(Controller);
    Controller->SetAsLocalPlayerController();
    Controller->Possess(Pawn);
    Audio = Pawn->FindComponentByClass<UMiningAudioComponent>();
    if(!TestNotNull(TEXT("Shared pawn owns audio"), Audio)) return false;
    Audio->BeginPlay();
    Audio->Activate(true);
    auto* Field = World->SpawnActor<AMiningCollectionField>();
    Field->Player = Controller;
    Field->Inventory.Init(0, AMiningCollectible::ItemTypeCount);
    FVector Eye;
    FRotator Direction;
    Controller->GetPlayerViewPoint(Eye, Direction);
    for(int32 ItemId = 0; ItemId < AMiningCollectible::ItemTypeCount; ++ItemId)
    {
        auto* Item = World->SpawnActor<AMiningCollectible>(Eye + Direction.Vector() * 200.f, FRotator::ZeroRotator);
        Item->ItemId = ItemId;
        Item->Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        const FName PreviousSound = Audio->GetLastPlayedSound();
        TestFalse(TEXT("Buried collectible cannot be picked up"), Field->TryCollectTarget());
        TestEqual(TEXT("Failed pickup stays silent"), Audio->GetLastPlayedSound(), PreviousSound);
        Item->bReleased = true;
        Field->bCatalogueOpen = true;
        TestFalse(TEXT("Catalogue cannot collect in the background"), Field->TryCollectTarget());
        Field->bCatalogueOpen = false;
        TestTrue(*FString::Printf(TEXT("Item %d collects through production path"), ItemId), Field->TryCollectTarget());
        TestEqual(TEXT("Correct item-specific sound plays immediately"), Audio->GetLastPlayedSound(), UMiningAudioComponent::CollectibleSoundName(ItemId));
        TestEqual(TEXT("Inventory increments once"), Field->Inventory[ItemId], 1);
        TestFalse(TEXT("Repeated interaction does not collect twice"), Field->TryCollectTarget());
        TestEqual(TEXT("Duplicate interaction leaves inventory unchanged"), Field->Inventory[ItemId], 1);
        Item->Destroy();
    }
    const FName PreviousSound = Audio->GetLastPlayedSound();
    Audio->PlayCollectible(-1);
    TestEqual(TEXT("Invalid item does not play a fallback"), Audio->GetLastPlayedSound(), PreviousSound);
    Enabled->SetWithCurrentPriority(0);
    Audio->PlayCollectible(0);
    TestEqual(TEXT("Global audio disable suppresses pickup effects"), Audio->GetLastPlayedSound(), PreviousSound);
    return true;
}
#endif
