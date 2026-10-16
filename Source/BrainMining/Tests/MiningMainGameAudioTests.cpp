#include "MiningAudioComponent.h"
#include "MiningComponent.h"
#include "Showcase/MiningExhibitHall.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainGameAudioAssetsTest, "BrainMining.AudioPilot.MainGameAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMainGameAudioAssetsTest::RunTest(const FString&)
{
	const auto Names = UMiningAudioComponent::MainGameAssetNames();
	TestEqual(TEXT("MainGame selection is complete"), Names.Num(), 19);
	for (FName Name : Names)
	{
		const FString Path = FString::Printf(TEXT("/Game/Audio/MainGame/%s.%s"), *Name.ToString(), *Name.ToString());
		auto* Sound = LoadObject<USoundWave>(nullptr, *Path);
		if (!TestNotNull(*Path, Sound)) continue;
		const FString Category = Name.ToString().StartsWith(TEXT("M_")) ? TEXT("Music") : Name.ToString().StartsWith(TEXT("A_")) ? TEXT("Ambience") : TEXT("Effects");
		if (TestNotNull(TEXT("Category exists"), Sound->SoundClassObject.Get()))
			TestEqual(TEXT("Correct mix routing"), Sound->SoundClassObject->GetName(), TEXT("SC_") + Category);
		TestEqual(TEXT("Loop metadata"), bool(Sound->bLooping), Name.ToString().EndsWith(TEXT("_Loop")));
		TestTrue(TEXT("Nonempty audio"), Sound->Duration > .05f && Sound->Duration < 300.f);
		if (Category == TEXT("Effects")) TestEqual(TEXT("Effects are mono"), Sound->NumChannels, 1);
		if (Sound->bLooping) TestEqual(TEXT("Muted loops retain their playback timeline"), Sound->VirtualizationMode, EVirtualizationMode::PlayWhenSilent);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainGameAudioSurfaceTest, "BrainMining.AudioPilot.MainGameSurfaces", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMainGameAudioSurfaceTest::RunTest(const FString&)
{
	auto* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MainGame/Materials/DirtTransition/MI_ClayBlockage.MI_ClayBlockage"));
	if (!TestNotNull(TEXT("Actual clay material exists"), Parent)) return false;
	auto* Material = UMaterialInstanceDynamic::Create(Parent, GetTransientPackage());
	auto* Mesh = NewObject<UStaticMeshComponent>();
	Mesh->SetMaterial(0, Material);
	FHitResult Hit;
	Hit.Component = Mesh;
	Hit.ImpactPoint = FVector(50, 0, 0);
	TestTrue(TEXT("Blockage core sounds like soil"), UMiningAudioComponent::ResolveSurface(Hit) == EMiningAudioSurface::Dirt);
	Material->SetScalarParameterValue(TEXT("MudCore"), 0);
	Material->SetScalarParameterValue(TEXT("MudCount"), 1);
	Material->SetScalarParameterValue(TEXT("MudReach"), 240);
	Material->SetVectorParameterValue(TEXT("MudStart0"), FLinearColor(0, 0, 0, 50));
	Material->SetVectorParameterValue(TEXT("MudEnd0"), FLinearColor(100, 0, 0, 50));
	TestTrue(TEXT("Local dirt coverage sounds like soil"), UMiningAudioComponent::ResolveSurface(Hit) == EMiningAudioSurface::Dirt);
	Hit.ImpactPoint = FVector(50, 500, 0);
	TestTrue(TEXT("Same mixed material outside dirt stays stone"), UMiningAudioComponent::ResolveSurface(Hit) == EMiningAudioSurface::Stone);
	Mesh->ComponentTags.Add(TEXT("BM_Audio_Wood"));
	TestTrue(TEXT("Authored floor override selects wood"), UMiningAudioComponent::ResolveSurface(Hit) == EMiningAudioSurface::Wood);
	Mesh->ComponentTags.Empty();
	Mesh->ComponentTags.Add(TEXT("BM_Audio_Gravel"));
	TestTrue(TEXT("Gravel override is independent"), UMiningAudioComponent::ResolveSurface(Hit) == EMiningAudioSurface::Gravel);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainGameAudioLifecycleTest, "BrainMining.AudioPilot.MainGameLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMainGameAudioLifecycleTest::RunTest(const FString&)
{
	auto* PawnClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/MainGame/Blueprints/BP_MainGameCharacter.BP_MainGameCharacter_C"));
	if (!TestNotNull(TEXT("Actual MainGame character loads"), PawnClass)) return false;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("Lvl_MainGame"), CreatePackage(TEXT("/Temp/AudioTests/Lvl_MainGame")));
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UMiningAudioComponent* Audio = nullptr;
	auto* Enabled = IConsoleManager::Get().FindConsoleVariable(TEXT("bm.Audio.Enabled"));
	const int32 PreviousEnabled = Enabled->GetInt();
	Enabled->SetWithCurrentPriority(1);
	auto* MusicTrim = IConsoleManager::Get().FindConsoleVariable(TEXT("bm.Audio.MusicVolume"));
	const float PreviousMusicTrim = MusicTrim->GetFloat();
	MusicTrim->SetWithCurrentPriority(.12f);
	ON_SCOPE_EXIT
	{
		if (Audio && Audio->HasBegunPlay()) Audio->EndPlay(EEndPlayReason::Destroyed);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		Enabled->SetWithCurrentPriority(PreviousEnabled);
		MusicTrim->SetWithCurrentPriority(PreviousMusicTrim);
	};
	TestTrue(TEXT("Fixture identifies MainGame"), UMiningAudioComponent::IsMainGameWorld(World));
	auto* Pawn = World->SpawnActor<ACharacter>(PawnClass);
	auto* Controller = World->SpawnActor<AMiningExhibitController>();
	World->AddController(Controller);
	Controller->SetAsLocalPlayerController();
	Controller->Possess(Pawn);
	Controller->EncounterPolicy->Release();
	auto* Hall = World->SpawnActor<AMiningExhibitHall>();
	Hall->Tick(.01f);
	Hall->Areas.SetNum(4);
	for (auto& Area : Hall->Areas) Area.Kind = EMiningExperienceKind::Mining;
	Hall->Areas[0].SourceLevel = NewObject<ULevelStreamingDynamic>(World);
	Hall->ActiveArea = 0;
	Hall->bReady = true;
	Pawn->SetActorLocation(FVector(0, 0, 100));
	Audio = Pawn->FindComponentByClass<UMiningAudioComponent>();
	if (!TestNotNull(TEXT("MainGame inherits audio"), Audio)) return false;
	Audio->BeginPlay();
	Audio->Activate(true);
	Audio->TickComponent(1, LEVELTICK_All, nullptr);
	TestTrue(TEXT("MainGame profile selected"), Audio->UsesMainGameAudio());
	TestEqual(TEXT("Bedroom distinguished from first mine"), Audio->GetAudioRegion(), 0);
	Audio->PlayStarterPickup(false);
	TestEqual(TEXT("Pickaxe pickup has its own cue"), Audio->GetLastPlayedSound(), FName(TEXT("S_PickaxePickup")));
	Audio->PlayStarterPickup(true);
	TestEqual(TEXT("Jetpack pickup has its own cue"), Audio->GetLastPlayedSound(), FName(TEXT("S_JetpackEquip")));
	Pawn->SetActorLocation(FVector(1000, 0, 100));
	Audio->TickComponent(.1f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Boundary changes are debounced"), Audio->GetAudioRegion(), 0);
	Audio->TickComponent(.6f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Leaving bedroom selects mine ambience"), Audio->GetAudioRegion(), 1);
	auto FindSharedMusic = [Pawn]() -> UAudioComponent*
	{
		TArray<UAudioComponent*> Players;
		Pawn->GetComponents(Players);
		for (auto* Player : Players)
			if (Player->Sound && Player->Sound->GetFName() == FName(TEXT("M_DreamyFlashback_Loop"))) return Player;
		return nullptr;
	};
	const auto* FirstMineMusic = FindSharedMusic();
	if (!TestNotNull(TEXT("First mine music player exists"), FirstMineMusic)) return false;
	const float FirstMineGain = FirstMineMusic->VolumeMultiplier;
	for (int32 Index = 1; Index < 4; ++Index)
	{
		Hall->ActiveArea = Index;
		Audio->TickComponent(.1f, LEVELTICK_All, nullptr);
		Audio->TickComponent(.6f, LEVELTICK_All, nullptr);
		TestEqual(TEXT("Each mine selects its profile"), Audio->GetAudioRegion(), Index + 1);
		for (int32 Frame = 0; Frame < 60; ++Frame) Audio->TickComponent(.025f, LEVELTICK_All, nullptr);
		const auto* SharedMusic = FindSharedMusic();
		if (TestNotNull(TEXT("Later areas retain the first mine music"), SharedMusic))
		{
			TestTrue(TEXT("Area transitions reuse the same music player"), SharedMusic == FirstMineMusic);
			TestTrue(TEXT("Later music gain equals first mine gain"), FMath::IsNearlyEqual(SharedMusic->VolumeMultiplier, FirstMineGain));
		}
		TArray<UAudioComponent*> Players;
		Pawn->GetComponents(Players);
		for (const auto* Player : Players)
			TestFalse(TEXT("Cave Theme is not layered over the shared music"), Player->Sound && Player->Sound->GetFName() == FName(TEXT("M_CaveTheme_Loop")));
	}
	Hall->Areas.InsertDefaulted(1);
	Hall->Areas[1].Kind = EMiningExperienceKind::Office;
	Hall->Areas.InsertDefaulted(3);
	Hall->Areas[3].Kind = EMiningExperienceKind::Journey;
	for (const int32 AreaIndex : {2, 4, 5})
	{
		Hall->ActiveArea = AreaIndex;
		Audio->TickComponent(.1f, LEVELTICK_All, nullptr);
		Audio->TickComponent(.6f, LEVELTICK_All, nullptr);
		TestEqual(TEXT("Minigames do not shift mining audio profiles"), Audio->GetAudioRegion(), AreaIndex == 2 ? 2 : AreaIndex - 1);
		if (const auto* SharedMusic = FindSharedMusic())
			TestTrue(TEXT("Interleaved mines retain first mine music gain"), FMath::IsNearlyEqual(SharedMusic->VolumeMultiplier, FirstMineGain));
	}
	Hall->ActiveArea = 1;
	Audio->TickComponent(.1f, LEVELTICK_All, nullptr);
	Audio->TickComponent(1.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Office retains its own audio instead of a mining profile"), Audio->GetAudioRegion(), INDEX_NONE);
	Hall->ActiveArea = 4;
	Audio->TickComponent(1.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Returning to the third mine restores its profile"), Audio->GetAudioRegion(), 3);
	MusicTrim->SetWithCurrentPriority(0.f);
	Audio->TickComponent(2.f, LEVELTICK_All, nullptr);
	if (const auto* SharedMusic = FindSharedMusic()) TestEqual(TEXT("Shared music respects zero trim"), SharedMusic->VolumeMultiplier, 0.f);
	MusicTrim->SetWithCurrentPriority(.12f);
	TestTrue(TEXT("Transitions have a bounded number of players"), Audio->GetSoundscapeCount() <= 5);
	auto* Mining = Pawn->FindComponentByClass<UMiningComponent>();
	Mining->bDrillMode = true;
	Audio->TickComponent(.1f, LEVELTICK_All, nullptr);
	TArray<UAudioComponent*> Components;
	Pawn->GetComponents(Components);
	bool bUsesRecording = false;
	for (const auto* Component : Components)
		if (Component->Sound && Component->Sound->GetPathName().StartsWith(TEXT("/Game/Audio/MainGame/S_DrillIdle_Loop"))) bUsesRecording = true;
	TestTrue(TEXT("Drill uses MainGame recording override"), bUsesRecording);
	Audio->Deactivate();
	TestEqual(TEXT("Deactivation releases environment and music"), Audio->GetSoundscapeCount(), 0);
	TestEqual(TEXT("Deactivation releases tools"), Audio->GetToolLoopCount(), 0);
	Audio->Activate(true);
	Audio->TickComponent(1, LEVELTICK_All, nullptr);
	Enabled->SetWithCurrentPriority(0);
	Audio->TickComponent(1, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Audio disable leaves no soundscape"), Audio->GetSoundscapeCount(), 0);
	TestEqual(TEXT("Audio disable leaves no tool loop"), Audio->GetToolLoopCount(), 0);
	return true;
}
#endif
