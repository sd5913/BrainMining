#include "MiningSaveSubsystem.h"

#include "DiggableRock.h"
#include "GeneratedMiningRock.h"
#include "MiningWalletSubsystem.h"
#include "MiningProgressionComponent.h"
#include "MiningBombToolComponent.h"
#include "Showcase/MiningTreasure.h"
#include "MiningCollection.h"
#include "PopupClose/PopupCloseStage.h"
#include "MiningRoomEscape.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"
#include "EchoPassageStage.h"

bool UMiningSaveSubsystem::bApplyingReplay = false;

void UMiningSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UMiningSaveSubsystem::OnWorldCleanup);
	PreExitHandle = FCoreDelegates::OnPreExit.AddUObject(this, &UMiningSaveSubsystem::OnPreExit);
}

void UMiningSaveSubsystem::Deinitialize()
{
	if (WorldCleanupHandle.IsValid()) FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	if (PreExitHandle.IsValid()) FCoreDelegates::OnPreExit.Remove(PreExitHandle);
	if (ActiveWorld.IsValid() && bWorldLoaded) SaveNow();
	SaveData = nullptr;
	Super::Deinitialize();
}

TStatId UMiningSaveSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMiningSaveSubsystem, STATGROUP_Tickables);
}

FString UMiningSaveSubsystem::GetWorldKey(const UWorld* World) const
{
	if (!World || !World->GetOutermost()) return FString();
	return UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
}

void UMiningSaveSubsystem::Tick(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld()) return;
	if (ActiveWorld.Get() != World)
	{
		if (ActiveWorld.IsValid() && bWorldLoaded) SaveNow();
		ActiveWorld = World;
		bWorldLoaded = false;
		SaveTimer = 20.f;
	}
	if (!bWorldLoaded)
	{
		if (!World->GetFirstPlayerController() || !World->GetFirstPlayerController()->GetPawn()) return;
		LoadLatest();
		bWorldLoaded = true;
	}
	SaveTimer -= FMath::Max(0.f, DeltaSeconds);
	if (SaveTimer <= 0.f)
	{
		SaveNow();
		SaveTimer = 20.f;
	}
}

bool UMiningSaveSubsystem::HasSave() const
{
	return UGameplayStatics::DoesSaveGameExist(SlotName, 0);
}

void UMiningSaveSubsystem::DeleteSave()
{
	UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	SaveData = nullptr;
}

bool UMiningSaveSubsystem::LoadLatest()
{
	if (!GetWorld()) return false;
	SaveData = Cast<UMiningSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!SaveData) return false;
	const FString WorldKey = GetWorldKey(GetWorld());
	if (auto* Wallet = GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>())
	{
		Wallet->Minerals = SaveData->Minerals;
		Wallet->Balance = SaveData->Balance;
		Wallet->UpgradeLevels = SaveData->UpgradeLevels;
		Wallet->bBombSkillUnlocked = SaveData->bBombSkillUnlocked;
		Wallet->bBombSkillRequiresUnlock = SaveData->bBombSkillRequiresUnlock;
	}
	APawn* LoadPawn = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn().Get() : nullptr;
	if (LoadPawn)
	{
		UMiningProgressionComponent* Progression = LoadPawn->FindComponentByClass<UMiningProgressionComponent>();
		if (Progression && Progression->bInitialized) Progression->ApplyUpgrades();
		if (UMiningBombToolComponent* Bomb = LoadPawn->FindComponentByClass<UMiningBombToolComponent>()) Bomb->BombCount = SaveData->BombCount;
	}
	if (SaveData->MapPackage == WorldKey)
	{
		ApplyLoadedWorld();
		ReplayCuts();
	}
	return true;
}

void UMiningSaveSubsystem::CaptureWorld()
{
	UWorld* World = GetWorld();
	if (!World) return;
	if (!SaveData) SaveData = Cast<UMiningSaveGame>(UGameplayStatics::CreateSaveGameObject(UMiningSaveGame::StaticClass()));
	SaveData->MapPackage = GetWorldKey(World);
	SaveData->ActorStates.Reset();
	if (auto* Wallet = GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>())
	{
		SaveData->Minerals = Wallet->Minerals;
		SaveData->Balance = Wallet->Balance;
		SaveData->UpgradeLevels = Wallet->UpgradeLevels;
		SaveData->bBombSkillUnlocked = Wallet->bBombSkillUnlocked;
		SaveData->bBombSkillRequiresUnlock = Wallet->bBombSkillRequiresUnlock;
	}
	APawn* SavePawn = World->GetFirstPlayerController() ? World->GetFirstPlayerController()->GetPawn().Get() : nullptr;
	if (SavePawn)
		if (UMiningBombToolComponent* Bomb = SavePawn->FindComponentByClass<UMiningBombToolComponent>()) SaveData->BombCount = Bomb->BombCount;
	for (TActorIterator<AMiningCollectionField> It(World); It; ++It)
	{
		if (!It->Inventory.IsEmpty()) { SaveData->CollectionInventory = It->Inventory; break; }
	}
	if (auto* PC = World->GetFirstPlayerController()) if (APawn* Pawn = PC->GetPawn().Get())
	{
		SaveData->bHasPlayerTransform = true;
		SaveData->PlayerTransform = Pawn->GetActorTransform();
		SaveData->ControlRotation = PC->GetControlRotation();
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor)) continue;
		FMiningSavedActorState State; State.ActorPath = Actor->GetPathName();
		if (auto* Treasure = Cast<AMiningTreasure>(Actor))
		{
			if (!Treasure->bCollected) continue;
			State.Id = Treasure->TreasureId; State.Flags = 1; State.IntValue = Treasure->CollectedValue; SaveData->ActorStates.Add(State);
		}
		else if (auto* Collectible = Cast<AMiningCollectible>(Actor))
		{
			if (!Collectible->bCollected) continue;
			State.IntValue = Collectible->ItemId; State.Flags = 1; SaveData->ActorStates.Add(State);
		}
		else if (auto* Echo = Cast<AEchoPassageStage>(Actor))
		{
			State.Id = Echo->PassageId; State.IntValue = Echo->KnockCount;
			State.Flags = (Echo->bCollapsed ? 1 : 0) | (Echo->bLanded ? 2 : 0) | (Echo->bCompleted ? 4 : 0); SaveData->ActorStates.Add(State);
		}
		else if (auto* Escape = Cast<AMiningRoomEscape>(Actor))
		{
			State.IntValue = Escape->ClearedNodes; State.FloatValue = Escape->WaterZ;
			State.Flags = (uint8)Escape->Phase | (Escape->bWallBroken ? 0x10 : 0) | (Escape->bEnteredComputer ? 0x20 : 0) | (Escape->bExitGuidance ? 0x40 : 0) | (Escape->bComputerCheckpoint ? 0x80 : 0); SaveData->ActorStates.Add(State);
		}
		else if (auto* Office = Cast<APopupCloseStage>(Actor))
		{
			State.Flags = (Office->IsOfficeEscaped() ? 1 : 0) | (Office->IsOfficeDestroyed() ? 2 : 0); SaveData->ActorStates.Add(State);
		}
	}
}

bool UMiningSaveSubsystem::SaveNow()
{
	if (!GetWorld() || !GetWorld()->IsGameWorld() || !bWorldLoaded) return false;
	CaptureWorld();
	return SaveData && UGameplayStatics::SaveGameToSlot(SaveData, SlotName, 0);
}

void UMiningSaveSubsystem::ApplyLoadedWorld()
{
	if (!SaveData || !GetWorld()) return;
	const TMap<FString, const FMiningSavedActorState*> States = [&]()
	{
		TMap<FString, const FMiningSavedActorState*> Result;
		for (const FMiningSavedActorState& State : SaveData->ActorStates) Result.Add(State.ActorPath, &State);
		return Result;
	}();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It; const FMiningSavedActorState* State = States.FindRef(Actor->GetPathName());
		if (!State) continue;
		if (auto* Treasure = Cast<AMiningTreasure>(Actor))
		{
			Treasure->bCollected = true; Treasure->CollectedValue = State->IntValue; Treasure->SetDropActive(false); Treasure->SetHighlighted(false); Treasure->SetActorEnableCollision(false); if (Treasure->Model) Treasure->Model->SetVisibility(false); if (Treasure->GlowLight) Treasure->GlowLight->SetVisibility(false);
		}
		else if (auto* Collectible = Cast<AMiningCollectible>(Actor))
		{
			Collectible->bCollected = true; Collectible->SetActorEnableCollision(false); Collectible->SetActorHiddenInGame(true);
		}
		else if (auto* Echo = Cast<AEchoPassageStage>(Actor))
		{
			Echo->KnockCount = State->IntValue; Echo->bLanded = (State->Flags & 2) != 0; if (State->Flags & 1) Echo->Collapse(); if (State->Flags & 4) Echo->Complete();
		}
		else if (auto* Escape = Cast<AMiningRoomEscape>(Actor))
		{
			Escape->ClearedNodes = State->IntValue; Escape->WaterZ = State->FloatValue; Escape->Phase = (EMiningEscapePhase)(State->Flags & 0x0f); Escape->bWallBroken = (State->Flags & 0x10) != 0; Escape->bEnteredComputer = (State->Flags & 0x20) != 0; Escape->bExitGuidance = (State->Flags & 0x40) != 0; Escape->bComputerCheckpoint = (State->Flags & 0x80) != 0;
		}
	}
	if (!SaveData->CollectionInventory.IsEmpty())
		for (TActorIterator<AMiningCollectionField> It(GetWorld()); It; ++It) It->Inventory = SaveData->CollectionInventory;
	if (SaveData->bHasPlayerTransform) if (auto* PC = GetWorld()->GetFirstPlayerController()) if (APawn* Pawn = PC->GetPawn().Get())
	{
		Pawn->SetActorTransform(SaveData->PlayerTransform, false, nullptr, ETeleportType::TeleportPhysics); PC->SetControlRotation(SaveData->ControlRotation);
	}
}

void UMiningSaveSubsystem::ReplayCuts()
{
	if (!SaveData || !GetWorld() || SaveData->Cuts.IsEmpty()) return;
	bApplying = bApplyingReplay = true;
	for (const FMiningSavedCut& Cut : SaveData->Cuts)
	{
		AActor* Found = nullptr;
		for (TActorIterator<AActor> It(GetWorld()); It; ++It) if (It->GetPathName() == Cut.ActorPath) { Found = *It; break; }
		ADiggableRock* Rock = Cast<ADiggableRock>(Found); if (!Rock) continue;
		switch (Cut.Operation)
		{
		case 0: Rock->MineSphere(Cut.Center, Cut.Radius); break;
		case 1: Rock->MinePickaxe(Cut.WorldFrame, Cut.HalfSize, Cut.Seed, Cut.Roughness); break;
		case 2: Rock->MineBlast(Cut.Center, Cut.Radius, Cut.Roughness, Cut.Seed); break;
		default: break;
		}
	}
	bApplyingReplay = bApplying = false;
}

void UMiningSaveSubsystem::RecordCut(const FMiningSavedCut& Cut)
{
	if (bApplying || bApplyingReplay || !bWorldLoaded) return;
	if (!SaveData) SaveData = Cast<UMiningSaveGame>(UGameplayStatics::CreateSaveGameObject(UMiningSaveGame::StaticClass()));
	SaveData->Cuts.Add(Cut);
}

void UMiningSaveSubsystem::RecordSphere(ADiggableRock* Rock, FVector Center, float Radius)
{
	if (!Rock || !Rock->GetWorld()) return;
	if (auto* GameInstance = Rock->GetGameInstance())
		if (auto* S = GameInstance->GetSubsystem<UMiningSaveSubsystem>()) { FMiningSavedCut C; C.ActorPath=Rock->GetPathName(); C.Operation=0; C.Center=Center; C.Radius=Radius; S->RecordCut(C); }
}
void UMiningSaveSubsystem::RecordPickaxe(ADiggableRock* Rock, FTransform Frame, FVector HalfSize, int32 Seed, float Roughness)
{
	if (!Rock || !Rock->GetWorld()) return;
	if (auto* GameInstance = Rock->GetGameInstance())
		if (auto* S = GameInstance->GetSubsystem<UMiningSaveSubsystem>()) { FMiningSavedCut C; C.ActorPath=Rock->GetPathName(); C.Operation=1; C.WorldFrame=Frame; C.HalfSize=HalfSize; C.Seed=Seed; C.Roughness=Roughness; S->RecordCut(C); }
}
void UMiningSaveSubsystem::RecordBlast(ADiggableRock* Rock, FVector Center, float Radius, float Roughness, int32 Seed)
{
	if (!Rock || !Rock->GetWorld()) return;
	if (auto* GameInstance = Rock->GetGameInstance())
		if (auto* S = GameInstance->GetSubsystem<UMiningSaveSubsystem>()) { FMiningSavedCut C; C.ActorPath=Rock->GetPathName(); C.Operation=2; C.Center=Center; C.Radius=Radius; C.Roughness=Roughness; C.Seed=Seed; S->RecordCut(C); }
}

void UMiningSaveSubsystem::OnWorldCleanup(UWorld* World, bool, bool)
{
	if (World == ActiveWorld.Get() && bWorldLoaded) SaveNow();
}
void UMiningSaveSubsystem::OnPreExit() { if (ActiveWorld.IsValid() && bWorldLoaded) SaveNow(); }
