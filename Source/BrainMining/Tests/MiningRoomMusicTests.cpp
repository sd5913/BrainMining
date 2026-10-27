#include "MiningAudioComponent.h"
#include "MiningRoomEscape.h"
#include "Showcase/MiningExhibitHall.h"
#include "CardRoomStage.h"
#include "EchoPassageStage.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoomMusicAssetsTest, "BrainMining.RoomMusic.ImportedAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRoomMusicAssetsTest::RunTest(const FString&)
{
	const auto Names = UMiningAudioComponent::RoomMusicAssetNames();
	TestEqual(TEXT("Two selected room tracks"), Names.Num(), 2);
	auto* MusicClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/Mix/SC_Music.SC_Music"));
	if (!TestNotNull(TEXT("Music mix exists"), MusicClass)) return false;
	for (const FName Name : Names)
	{
		const FString Path = FString::Printf(TEXT("/Game/Audio/RoomMusic/%s.%s"), *Name.ToString(), *Name.ToString());
		auto* Sound = LoadObject<USoundWave>(nullptr, *Path);
		if (!TestNotNull(*Path, Sound)) continue;
		TestTrue(TEXT("Music loops"), Sound->bLooping);
		TestEqual(TEXT("Music slider controls every track"), Sound->SoundClassObject.Get(), MusicClass);
		TestEqual(TEXT("Music remains stereo"), Sound->NumChannels, 2);
		TestTrue(TEXT("Full-length music imported"), Sound->Duration > 90.f && Sound->Duration < 390.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoomMusicRoutingTest, "BrainMining.RoomMusic.StageTransitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRoomMusicRoutingTest::RunTest(const FString&)
{
	auto* PawnClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/MainGame/Blueprints/BP_MainGameCharacter.BP_MainGameCharacter_C"));
	if (!TestNotNull(TEXT("Actual MainGame pawn loads"), PawnClass)) return false;
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("Lvl_MainGame"), CreatePackage(TEXT("/Temp/RoomMusicTests/Lvl_MainGame")));
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Enabled = IConsoleManager::Get().FindConsoleVariable(TEXT("bm.Audio.Enabled"));
	auto* Trim = IConsoleManager::Get().FindConsoleVariable(TEXT("bm.Audio.MusicVolume"));
	const int32 OldEnabled = Enabled->GetInt();
	const float OldTrim = Trim->GetFloat();
	Enabled->SetWithCurrentPriority(1);
	Trim->SetWithCurrentPriority(.12f);
	UMiningAudioComponent* Audio = nullptr;
	ON_SCOPE_EXIT
	{
		if (Audio && Audio->HasBegunPlay()) Audio->EndPlay(EEndPlayReason::Destroyed);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		Enabled->SetWithCurrentPriority(OldEnabled);
		Trim->SetWithCurrentPriority(OldTrim);
	};
	auto* Pawn = World->SpawnActor<ACharacter>(PawnClass);
	auto* Controller = World->SpawnActor<AMiningExhibitController>();
	World->AddController(Controller);
	Controller->SetAsLocalPlayerController();
	Controller->Possess(Pawn);
	Controller->EncounterPolicy->Release();
	auto* Hall = World->SpawnActor<AMiningExhibitHall>();
	Hall->Areas.SetNum(4);
	Hall->Areas[0].Kind = EMiningExperienceKind::Office;
	Hall->Areas[1].Kind = EMiningExperienceKind::Journey;
	Hall->Areas[2].Kind = EMiningExperienceKind::Museum;
	Hall->Areas[3].Kind = EMiningExperienceKind::Mining;
	Hall->bReady = true;
	Hall->ActiveArea = 0;
	Hall->Journey = World->SpawnActor<AMiningRoomEscape>();
	Hall->Journey->CardStage = World->SpawnActor<ACardRoomStage>();
	auto* Echo = World->SpawnActor<AEchoPassageStage>();
	Audio = Pawn->FindComponentByClass<UMiningAudioComponent>();
	if (!TestNotNull(TEXT("Shared audio component"), Audio)) return false;
	Audio->BeginPlay();
	Audio->Activate(true);
	Audio->Hall = Hall;
	Audio->RoomMusicEcho = Echo;
	auto Advance = [Audio]()
	{
		for (int32 Frame = 0; Frame < 80; ++Frame) Audio->TickComponent(.05f, LEVELTICK_All, nullptr);
	};
	auto CheckSelection = [this, Audio, &Advance](const TCHAR* Label, FName Expected)
	{
		TestEqual(Label, Audio->FindRoomMusic(), Expected);
		Advance();
		int32 AudibleMusic = 0;
		for (const auto& Pair : Audio->Soundscape)
			if (Pair.Key.ToString().StartsWith(TEXT("M_")) && Pair.Value->VolumeMultiplier > .001f)
			{
				++AudibleMusic;
				TestEqual(TEXT("Only the selected music remains after fade"), Pair.Key, Expected);
			}
		TestEqual(TEXT("No overlapping music after transition"), AudibleMusic, Expected.IsNone() ? 0 : 1);
	};
	CheckSelection(TEXT("Office has no added background music"), NAME_None);
	Hall->ActiveArea = 1;
	Echo->bCompleted = false;
	CheckSelection(TEXT("Echo gameplay retains its silence"), NAME_None);
	Echo->bCompleted = true;
	for (const auto Phase : {ECardRoomPhase::Exploring, ECardRoomPhase::Seated, ECardRoomPhase::Matching, ECardRoomPhase::Arrival})
	{
		Hall->Journey->CardStage->Run.Phase = Phase;
		CheckSelection(TEXT("Exploration stays on Enchanted Journey"), TEXT("M_Room_CardExplore_Loop"));
	}
	Hall->Journey->CardStage->Run.Phase = ECardRoomPhase::Choosing;
	CheckSelection(TEXT("First hand retains the room music"), TEXT("M_Room_CardExplore_Loop"));
	auto* BattlePlayer = Audio->Soundscape.FindRef(TEXT("M_Room_CardExplore_Loop")).Get();
	Hall->Journey->CardStage->Run.Phase = ECardRoomPhase::Resolving;
	CheckSelection(TEXT("Attacks keep battle music"), TEXT("M_Room_CardExplore_Loop"));
	TestTrue(TEXT("Resolving does not recreate the player"), BattlePlayer == Audio->Soundscape.FindRef(TEXT("M_Room_CardExplore_Loop")).Get());
	Hall->Journey->CardStage->Run.Phase = ECardRoomPhase::Choosing;
	Advance();
	TestTrue(TEXT("Next hand keeps the same player"), BattlePlayer == Audio->Soundscape.FindRef(TEXT("M_Room_CardExplore_Loop")).Get());
	Hall->Journey->CardStage->Run.Phase = ECardRoomPhase::Victory;
	CheckSelection(TEXT("Victory retains card-room music"), TEXT("M_Room_CardExplore_Loop"));
	Hall->Journey->Phase = EMiningEscapePhase::Warning;
	CheckSelection(TEXT("Escape warning retains room music"), TEXT("M_Room_CardExplore_Loop"));
	auto* EscapePlayer = Audio->Soundscape.FindRef(TEXT("M_Room_CardExplore_Loop")).Get();
	TestTrue(TEXT("Battle into escape never recreates music"), EscapePlayer==BattlePlayer);
	Hall->Journey->Phase = EMiningEscapePhase::Flooding;
	CheckSelection(TEXT("Flood and television use the same track"), TEXT("M_Room_CardExplore_Loop"));
	TestTrue(TEXT("Warning into flood preserves playback"), EscapePlayer == Audio->Soundscape.FindRef(TEXT("M_Room_CardExplore_Loop")).Get());
	Hall->Journey->Phase = EMiningEscapePhase::Failed;
	CheckSelection(TEXT("Failure retains room music"), TEXT("M_Room_CardExplore_Loop"));
	Hall->Journey->Phase = EMiningEscapePhase::Flooding;
	CheckSelection(TEXT("Retry retains room music"), TEXT("M_Room_CardExplore_Loop"));
	Hall->Journey->Phase = EMiningEscapePhase::Escaped;
	CheckSelection(TEXT("Success ends escape music"), NAME_None);
	Hall->ActiveArea = 2;
	CheckSelection(TEXT("Museum uses Meditation Impromptu"), TEXT("M_Room_Museum_Loop"));
	Trim->SetWithCurrentPriority(0.f);
	Advance();
	TestFalse(TEXT("Zero music trim releases the room player"), Audio->Soundscape.Contains(TEXT("M_Room_Museum_Loop")));
	Trim->SetWithCurrentPriority(.12f);
	CheckSelection(TEXT("Restoring volume restores music"), TEXT("M_Room_Museum_Loop"));
	Hall->ActiveArea = 3;
	Advance();
	auto* Exploration = Audio->Soundscape.FindRef(TEXT("M_DreamyFlashback_Loop")).Get();
	if(TestNotNull(TEXT("Returning to mining restores Dreamy Flashback"),Exploration))
	{
		TestTrue(TEXT("Returned exploration volume is audible"),Exploration->VolumeMultiplier>.1f);
		Hall->ActiveArea=2;Advance();
		TestTrue(TEXT("Room entry retains the exploration player"),Audio->Soundscape.FindRef(TEXT("M_DreamyFlashback_Loop")).Get()==Exploration);
		TestEqual(TEXT("Exploration is muted inside museum"),Exploration->VolumeMultiplier,0.f);
		Hall->ActiveArea=3;Advance();
		TestTrue(TEXT("Returning twice restores the same exploration player"),Audio->Soundscape.FindRef(TEXT("M_DreamyFlashback_Loop")).Get()==Exploration);
		TestTrue(TEXT("Exploration gain recovers after repeated room visits"),Exploration->VolumeMultiplier>.1f);
	}
	for (const FName Name : Audio->RoomMusicAssetNames()) TestFalse(TEXT("Departed room players released"), Audio->Soundscape.Contains(Name));
	Hall->ActiveArea = 2;
	Advance();
	Enabled->SetWithCurrentPriority(0);
	Advance();
	TestEqual(TEXT("Global disable releases all music"), Audio->GetSoundscapeCount(), 0);
	Enabled->SetWithCurrentPriority(1);
	Advance();
	Audio->Deactivate();
	TestEqual(TEXT("Deactivation releases all music"), Audio->GetSoundscapeCount(), 0);
	return true;
}
#endif
