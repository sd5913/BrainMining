#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "MiningSaveGame.h"
#include "MiningSaveSubsystem.generated.h"

class ADiggableRock;

UCLASS()
class BRAINMINING_API UMiningSaveSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableWhenPaused() const override { return true; }

	UFUNCTION(BlueprintCallable, Category="Mining|Save") bool SaveNow();
	UFUNCTION(BlueprintCallable, Category="Mining|Save") bool LoadLatest();
	UFUNCTION(BlueprintCallable, Category="Mining|Save") bool HasSave() const;
	UFUNCTION(BlueprintCallable, Category="Mining|Save") void DeleteSave();

	// Terrain edits are journaled as deterministic operations and replayed after
	// a map is loaded. This keeps the save small while preserving tunnel layout.
	static void RecordSphere(ADiggableRock* Rock, FVector Center, float Radius);
	static void RecordPickaxe(ADiggableRock* Rock, FTransform Frame, FVector HalfSize, int32 Seed, float Roughness);
	static void RecordBlast(ADiggableRock* Rock, FVector Center, float Radius, float Roughness, int32 Seed);
	static bool IsApplyingReplay();

private:
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void OnPreExit();
	void CaptureWorld();
	void ApplyLoadedWorld();
	void ReplayCuts();
	FString GetWorldKey(const UWorld* World) const;
	void RecordCut(const FMiningSavedCut& Cut);

	static constexpr const TCHAR* SlotName = TEXT("BrainMining_Auto");
	TObjectPtr<UMiningSaveGame> SaveData;
	TWeakObjectPtr<UWorld> ActiveWorld;
	FDelegateHandle WorldCleanupHandle;
	FDelegateHandle PreExitHandle;
	float SaveTimer = 20.f;
	bool bWorldLoaded = false;
	bool bApplying = false;
	static bool bApplyingReplay;
};
