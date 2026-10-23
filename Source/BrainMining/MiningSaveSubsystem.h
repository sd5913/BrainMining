#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MiningMineralInventory.h"
#include "Tickable.h"
#include "MiningSaveSubsystem.generated.h"

USTRUCT()
struct FMiningSavedMesh
{
    GENERATED_BODY()
    UPROPERTY() FString ActorKey;
    UPROPERTY() int32 Chunk=INDEX_NONE;
    UPROPERTY() int32 Cuts=0;
    UPROPERTY() int32 ActorCuts=0;
    UPROPERTY() TArray<uint8> Data;
};

UCLASS()
class BRAINMINING_API UMiningSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Version=1;
    UPROPERTY() int32 Area=0;
    UPROPERTY() int32 UnlockedArea=0;
    UPROPERTY() TSet<int32> CompletedAreas;
    UPROPERTY() FTransform PlayerTransform;
    UPROPERTY() FRotator ViewRotation;
    UPROPERTY() int64 Balance=0;
    UPROPERTY() TMap<EMiningMineral,int32> Minerals;
    UPROPERTY() TArray<int32> Upgrades;
    UPROPERTY() TArray<int32> Collection;
    UPROPERTY() TSet<int32> RunCollectedItems;
    UPROPERTY() TSet<int32> CollectedMinerals;
    UPROPERTY() bool Bomb=false;
    UPROPERTY() bool Drill=false;
    UPROPERTY() bool Pickaxe=false;
    UPROPERTY() bool Jetpack=false;
    UPROPERTY() bool Headlamp=false;
    UPROPERTY() TArray<FMiningSavedMesh> Meshes;
    UPROPERTY() bool TutorialActive=false;
    UPROPERTY() bool TutorialSoilPassed=false;
    UPROPERTY() int32 TutorialCompleted=0;
    UPROPERTY() int32 PrototypesFound=0;
};

UCLASS()
class BRAINMINING_API UMiningSaveSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return !IsTemplate(); }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMiningSaveSubsystem,STATGROUP_Tickables); }
    UFUNCTION(BlueprintPure) bool HasSave() const;
    UFUNCTION(BlueprintCallable) bool SaveProgress();
    UFUNCTION(BlueprintCallable) bool LoadProgress();
    void StartNewGame(bool bKeepCollection=false);
    TArray<int32> GetCollectionProgress() const;
    bool HasCollectionProgress() const;
    void EndSession() { bSessionStarted=false; bLoadedProgress=false; RunCollectedItems.Reset(); RestoredMinerals.Reset(); }
    void RecordCollectible(int32 Id) { RunCollectedItems.Add(Id); TutorialCompleted|=8; }
    bool WasCollectedThisRun(int32 Id) const { return RunCollectedItems.Contains(Id); }
    UPROPERTY(BlueprintReadOnly) bool TutorialActive=false;
    UPROPERTY(BlueprintReadOnly) bool TutorialSoilPassed=false;
    UPROPERTY(BlueprintReadOnly) int32 TutorialCompleted=0;
    UPROPERTY(BlueprintReadOnly) int32 PrototypesFound=0;
    void RecordMineral(EMiningMineral Mineral);
private:
    void UpdateTutorial();
    bool bSessionStarted=false;
    bool bRestoring=false;
    bool bLoadedProgress=false;
    float SaveAge=0;
    float FallingSeconds=0;
    float ProbeAge=0;
    int32 PreviousArea=INDEX_NONE;
    int32 UpgradeHintLevel=INDEX_NONE;
    TWeakObjectPtr<UWorld> SessionWorld;
    TSet<int32> RestoredMinerals;
    TSet<int32> RunCollectedItems;
    void RestoreCollectedItems();
    void UpdateBombWallSigns();
};
