#include "MiningAudioComponent.h"
#include "MiningAudioSettings.h"
#include "MiningComponent.h"
#include "MiningBombToolComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "DiggableRock.h"
#include "GeneratedMiningRock.h"
#include "StartRoom/MiningStartRoom.h"
#include "AudioMixerBlueprintLibrary.h"
#include "AudioDevice.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	struct FMainGameAudioCapture
	{
		TWeakObjectPtr<UWorld> World;
		FTimerHandle Timer;
		int32 Step = -1;
		int32 Waits = 0;
		int32 Checks = 0;
		int32 Failures = 0;
		int32 BeforeHits = 0;
		int32 BeforeExplosions = 0;
		int32 PreviousUnlocked = 0;
		double StartTime = 0;
		double StartAudioTime = 0;
		TArray<float> Volumes;
		TArray<FString> Events;
		TArray<int32> MineAreas;
		TWeakObjectPtr<AMiningStarterPickup> Pickaxe;
		TWeakObjectPtr<AMiningStarterPickup> Jetpack;

		void Check(bool bPassed, const TCHAR* Label)
		{
			++Checks;
			if (!bPassed) ++Failures;
			UE_LOG(LogTemp, Display, TEXT("MAIN_GAME_AUDIO_CHECK %s %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Label);
		}

		void Finish()
		{
			if (!World.IsValid()) return;
			if (Step >= 0) UAudioMixerBlueprintLibrary::StopRecordingOutput(World.Get(), EAudioRecordingExportType::WavFile, TEXT("MainGameAudioRuntime"), FPaths::ProjectSavedDir());
			if (auto* Settings = World->GetGameInstance()->GetSubsystem<UMiningAudioSettings>())
				for (int32 Index = 0; Index < Volumes.Num(); ++Index) Settings->SetVolume(Index, Volumes[Index]);
			if (auto* Hall = AMiningExhibitHall::Find(World.Get())) Hall->UnlockedArea = PreviousUnlocked;
			const FString Report = FString::Printf(TEXT("{\"map\":\"/Game/MainGame/Maps/Lvl_MainGame\",\"passed\":%s,\"checks\":%d,\"failures\":%d,\"events\":[%s]}"),
				Failures == 0 && Step >= 26 ? TEXT("true") : TEXT("false"), Checks, Failures, *FString::Join(Events, TEXT(",")));
			FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("MainGameAudioRuntime.json")));
			World->GetTimerManager().ClearTimer(Timer);
			FTimerHandle ExitTimer;
			World->GetTimerManager().SetTimer(ExitTimer, []() { FPlatformMisc::RequestExit(false); }, 2.f, false);
		}

		void Tick()
		{
			if (!World.IsValid()) return;
			auto* Controller = Cast<AMiningExhibitController>(World->GetFirstPlayerController());
			auto* Pawn = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
			auto* Audio = Pawn ? Pawn->FindComponentByClass<UMiningAudioComponent>() : nullptr;
			auto* Hall = AMiningExhibitHall::Find(World.Get());
			auto* Room = AMiningStartRoom::Find(World.Get());
			if (!Audio || !Hall || !Hall->bReady || !Room)
			{
				if (++Waits > 25) { Check(false, TEXT("MainGame readiness timed out")); Finish(); }
				return;
			}
			auto* Mining = Pawn->FindComponentByClass<UMiningComponent>();
			auto* Flight = Pawn->FindComponentByClass<UMiningCharacterMovementComponent>();
			auto* Bomb = Pawn->FindComponentByClass<UMiningBombToolComponent>();
			auto* Settings = World->GetGameInstance()->GetSubsystem<UMiningAudioSettings>();
			auto* Camera = Pawn->FindComponentByClass<UCameraComponent>();
			if (MineAreas.IsEmpty())
				for (int32 Index = 0; Index < Hall->Areas.Num(); ++Index)
					if (Hall->Areas[Index].Kind == EMiningExperienceKind::Mining) MineAreas.Add(Index);
			if (!Mining || !Flight || !Bomb || !Settings || !Camera || MineAreas.Num() != 3 || MineAreas[0] != 0 || !Hall->Areas[0].SourceLevel || Hall->Areas[0].MapRoute.Num() < 5)
			{
				Check(false, TEXT("MainGame components or three mining areas missing")); Finish(); return;
			}
			if (Step == -1)
			{
				for (TActorIterator<AMiningStarterPickup> Actor(World.Get()); Actor; ++Actor)
					if (Actor->Equipment == EMiningStarterEquipment::Pickaxe) Pickaxe = *Actor; else Jetpack = *Actor;
				if (!Pickaxe.IsValid() || !Jetpack.IsValid()) { Check(false, TEXT("Introductory pickups missing")); Finish(); return; }
				for (int32 Channel = 0; Channel < 5; ++Channel) Volumes.Add(Settings->GetVolume(Channel));
				PreviousUnlocked = Hall->UnlockedArea;
				Settings->RestoreDefaults();
				StartTime = FPlatformTime::Seconds();
				const auto Device = World->GetAudioDevice();
				if (!Device) { Check(false, TEXT("Recording requires an audio device")); Finish(); return; }
				StartAudioTime = Device->GetAudioTime();
				UAudioMixerBlueprintLibrary::StartRecordingOutput(World.Get(), 65.f);
				Step = 0;
			}
			const double Elapsed = FPlatformTime::Seconds() - StartTime;
			const auto Device = World->GetAudioDevice();
			const double AudioElapsed = Device ? Device->GetAudioTime() - StartAudioTime : -1.;
			Events.Add(FString::Printf(TEXT("{\"step\":%d,\"time\":%.4f,\"audio_time\":%.4f}"), Step, Elapsed, AudioElapsed));
			UE_LOG(LogTemp, Display, TEXT("MAIN_GAME_AUDIO_STEP %d region=%d loops=%d hits=%d"), Step, Audio->GetAudioRegion(), Audio->GetToolLoopCount(), Audio->GetContactCount());
			auto AimPickup = [&](AMiningStarterPickup* Pickup)
			{
				const FVector Target = Pickup->GetActorLocation();
				const double Floor = Hall->Areas[0].SourceLevel->LevelTransform.GetLocation().Z;
				Pawn->SetActorLocation(FVector(Target.X - 125, Target.Y, Floor + 102), false, nullptr, ETeleportType::TeleportPhysics);
				Flight->StopMovementImmediately();
				Controller->SetControlRotation((Target - Camera->GetComponentLocation()).Rotation());
			};
			switch (Step)
			{
			case 0:
				Check(Hall->Areas.Num() > MineAreas.Num(), TEXT("Current MainGame includes interleaved minigames"));
				Check(Pawn->GetClass()->GetName() == TEXT("BP_MainGameCharacter_C") && Audio->UsesMainGameAudio(), TEXT("Actual MainGame character uses profile"));
				Check(Audio->GetAudioRegion() == 0, TEXT("Actual bedroom identified"));
				break;
			case 1: AimPickup(Pickaxe.Get()); break;
			case 2:
				Check(Room->Interact(Controller), TEXT("Pickaxe acquired through actual interaction"));
				Check(Audio->GetLastPlayedSound() == FName(TEXT("S_PickaxePickup")), TEXT("Pickaxe pickup cue"));
				Check(!Room->Interact(Controller), TEXT("Collected pickup cannot retrigger"));
				Check(UMiningAudioComponent::ResolveSurface(Flight->CurrentFloor.HitResult) == EMiningAudioSurface::Wood, TEXT("Actual bedroom collision selects wood"));
				break;
			case 3: AimPickup(Jetpack.Get()); break;
			case 4:
				Check(Room->Interact(Controller), TEXT("Jetpack acquired through actual interaction"));
				Check(Audio->GetLastPlayedSound() == FName(TEXT("S_JetpackEquip")), TEXT("Jetpack pickup cue"));
				break;
			case 5:
				Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F8, IE_Pressed, 1.f));
				Check(Controller->IsAudioSettingsOpen() && Audio->GetToolLoopCount() == 0, TEXT("F8 opens and stops tools in MainGame"));
				Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F8, IE_Pressed, 1.f));
				Check(!Controller->IsAudioSettingsOpen() && !Controller->IsPaused(), TEXT("F8 restores gameplay"));
				break;
			case 6:
				Pawn->SetActorLocation(Hall->Areas[0].MapRoute[4] + FVector(0, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
				Flight->StopMovementImmediately();
				Flight->SetMovementMode(MOVE_Flying);
				AGeneratedMiningRock::PrepareTerrainAt(World.Get(), Pawn->GetActorLocation());
				break;
			case 7:
			{
				Check(Audio->GetAudioRegion() == 1, TEXT("First mine soundscape"));
				BeforeHits = Audio->GetContactCount();
				bool bFoundRock = false;
				for (const FVector Direction : {FVector(0, 1, 0), FVector(0, -1, 0), FVector(0, 0, -1), FVector(0, 0, 1), FVector(1, 0, 0), FVector(-1, 0, 0)})
				{
					FHitResult Hit;
					const FVector Eye = Camera->GetComponentLocation();
					if (World->LineTraceSingleByChannel(Hit, Eye, Eye + Direction * 3000, ECC_Visibility, FCollisionQueryParams(NAME_None, true, Pawn)) && Cast<ADiggableRock>(Hit.GetActor()) && Hit.Distance > 80)
					{
						Pawn->SetActorLocation(Pawn->GetActorLocation() + Direction * (Hit.Distance - FMath::Min(Mining->Reach * .6f, 250.f)), false, nullptr, ETeleportType::TeleportPhysics);
						Controller->SetControlRotation(Direction.Rotation());
						bFoundRock = true;
						break;
					}
				}
				Check(bFoundRock, TEXT("Actual mine wall found within test positioning range"));
				Check(Mining->ActivateDrill(), TEXT("Drill equipped after introduction"));
				Mining->BeginMiningInput();
				break;
			}
			case 8:
				Check(Audio->GetContactCount() > BeforeHits, TEXT("Real mining contact reaches audio"));
				Mining->EndMiningInput(); Mining->SelectPickaxe();
				Flight->SetMovementMode(MOVE_Falling);
				Flight->SetFlightInputHeld(true);
				break;
			case 9:
				Check(Audio->GetToolLoopCount() > 0, TEXT("Jetpack loop starts from real thrust state"));
				Flight->SetFlightInputHeld(false);
				Flight->StopMovementImmediately();
				Flight->SetMovementMode(MOVE_Flying);
				UE_LOG(LogTemp, Display, TEXT("MAIN_GAME_AUDIO_BOMB unlocked=%d moveIgnored=%d lookIgnored=%d suspended=%d blocked=%d position=%s"), Bomb->IsSkillUnlocked(), Controller->IsMoveInputIgnored(), Controller->IsLookInputIgnored(), Mining->IsInputSuspended(), Mining->IsEncounterBlocked(), *Pawn->GetActorLocation().ToString());
				BeforeExplosions = Bomb->ExplosionCount;
				Check(Bomb->ToggleEquipped(), TEXT("Actual bomb equipped"));
				Check(Bomb->ThrowBomb(), TEXT("Actual bomb thrown"));
				break;
			case 11:
				Check(Bomb->ExplosionCount > BeforeExplosions, TEXT("Actual bomb explodes"));
				Bomb->Stow();
				break;
			case 12: Hall->UnlockedArea = Hall->Areas.Num() - 1; Check(Hall->EnterArea(MineAreas[1]), TEXT("Travel to second mine")); break;
			case 13: Check(Audio->GetAudioRegion() == 2, TEXT("Second mine profile")); Check(Hall->EnterArea(MineAreas[2]), TEXT("Travel to third mine")); break;
			case 14: Check(Audio->GetAudioRegion() == 3, TEXT("Third mine profile")); break;
			case 15: Check(Audio->GetAudioRegion() == 3, TEXT("Third mine profile remains stable")); Settings->SetVolume(0, 0); break;
			case 16: Settings->SetVolume(0, 1); break;
			case 17:
				Audio->Deactivate();
				Check(Audio->GetSoundscapeCount() == 0 && Audio->GetToolLoopCount() == 0, TEXT("All continuous players cleaned up"));
				Audio->Activate(true);
				break;
			case 18:
				Check(Audio->GetSoundscapeCount() > 0, TEXT("Soundscape resumes without duplicate players"));
				Settings->SetVolume(2, 0);
				Settings->SetVolume(3, 0);
				Settings->SetVolume(4, 0);
				Check(Hall->EnterArea(MineAreas[0]), TEXT("Travel to first mine for isolated music recording"));
				Pawn->SetActorLocation(Hall->Areas[MineAreas[0]].MapRoute[4] + FVector(0, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
				Flight->StopMovementImmediately();
				Flight->SetMovementMode(MOVE_Flying);
				break;
			case 20:
			case 22:
			case 24:
			{
				TArray<UAudioComponent*> Players;
				Pawn->GetComponents(Players);
				bool bMusicAudible = false;
				for (const auto* Player : Players)
					if (Player->Sound && Player->Sound->GetFName() == FName(TEXT("M_DreamyFlashback_Loop")))
						bMusicAudible |= Player->IsPlaying() && FMath::IsNearlyEqual(Player->VolumeMultiplier, .12f);
				Check(bMusicAudible && Audio->GetAudioRegion() == (Step - 18) / 2, TEXT("Each mine uses first mine music and gain"));
				if (Step < 24) Check(Hall->EnterArea(MineAreas[(Step - 18) / 2]), TEXT("Travel to next isolated music area"));
				else Settings->SetVolume(1, 0);
				break;
			}
			case 25: Settings->SetVolume(1, 1); break;
			case 26: Finish(); return;
			}
			++Step;
		}
	};

	FAutoConsoleCommandWithWorld MainGameCaptureCommand(TEXT("bm.Audio.MainGameCapture"), TEXT("Record and validate MainGame audio in a disposable standalone session, then exit."), FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World || World->WorldType != EWorldType::Game || !UMiningAudioComponent::IsMainGameWorld(World)) return;
		auto Capture = MakeShared<FMainGameAudioCapture>();
		Capture->World = World;
		World->GetTimerManager().SetTimer(Capture->Timer, [Capture]() { Capture->Tick(); }, 2.f, true, 3.f);
	}));
}
#endif
