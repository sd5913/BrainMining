#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MiningMineralInventory.h"
#include "Tickable.h"
#include "Async/Future.h"
#include "MiningSaveSubsystem.generated.h"
class ADiggableRock;

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
    // 0: free progress, 1: office start, 2: bomb, 3: cards start, 4: drill, 5: ending.
    UPROPERTY() int32 Checkpoint=0;
    UPROPERTY() int32 OfficeState=0;
    UPROPERTY() int32 CardState=0;
    UPROPERTY() bool EchoCollapsed=false;
    UPROPERTY() bool EchoLanded=false;
    UPROPERTY() bool EchoCompleted=false;
    UPROPERTY() int32 EchoKnocks=0;
    UPROPERTY() TMap<int32,int32> MuseumWords;
    UPROPERTY() TSet<int32> MuseumLandedExhibits;
    UPROPERTY() int32 MuseumPlayerWord=0;
    UPROPERTY() bool MuseumRewardUnlocked=false;
    UPROPERTY() int32 UnlockedArea=0;
    UPROPERTY() TSet<int32> CompletedAreas;
    UPROPERTY() FTransform PlayerTransform;
    UPROPERTY() FRotator ViewRotation;
    UPROPERTY() int64 Balance=0;
    UPROPERTY() TMap<EMiningMineral,int32> Minerals;
    UPROPERTY() TMap<EMiningMineral,int32> CollectedMineralTotals;
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
    UPROPERTY() bool MuseumCircleTaskStarted=false;
    UPROPERTY() bool MuseumCircleTaskCompleted=false;
    UPROPERTY() bool CampaignComplete=false;
    UPROPERTY() bool AchievementEligible=true;
    UPROPERTY() FString AchievementOwner;
    UPROPERTY() bool HasVerifiedAchievementCollection=false;
    UPROPERTY() TArray<int32> AchievementCollectionIds;
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
    bool BeginEndingCheckpoint();
    void StartNewGame(bool bKeepCollection=false);
    TArray<int32> GetCollectionProgress() const;
    bool HasCollectionProgress() const;
    void EndSession();
    virtual void Deinitialize() override;
    void MarkCampaignComplete();
    bool IsSaveInFlight() const { return WriteTask.IsValid(); }
    int32 AutosavesCompleted=0;
    double LastSnapshotMilliseconds=0;
    void RecordCollectible(int32 Id);
    TArray<FText> CompletionNotices;
    bool WasCollectedThisRun(int32 Id) const { return RunCollectedItems.Contains(Id); }
    UPROPERTY(BlueprintReadOnly) bool TutorialActive=false;
    UPROPERTY(BlueprintReadOnly) bool TutorialSoilPassed=false;
    UPROPERTY(BlueprintReadOnly) int32 TutorialCompleted=0;
    UPROPERTY(BlueprintReadOnly) int32 PrototypesFound=0;
    UPROPERTY(BlueprintReadOnly) bool MuseumCircleTaskStarted=false;
    UPROPERTY(BlueprintReadOnly) bool MuseumCircleTaskCompleted=false;
    void StartMuseumCircleTask();
    void CompleteMuseumCircleTask();
    void RecordMineral(EMiningMineral Mineral);
private:
    int32 CheckpointMode() const;
    void CaptureRoomState(UMiningSaveGame* Data);
    void RestoreRoomState();
    UPROPERTY(Transient) TObjectPtr<UMiningSaveGame> FrozenCheckpoint;
    UPROPERTY(Transient) TObjectPtr<UMiningSaveGame> PendingRoomRestore;
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
    bool SaveProgressInternal(bool bAsync);
    bool PrepareMeshCache();
    void FinishPendingWrite();
    TMap<FString,FMiningSavedMesh> MeshCache;
    TFuture<FMiningSavedMesh> MeshTask;
    TFuture<bool> WriteTask;
    mutable TOptional<bool> CachedHasSave;
    TWeakObjectPtr<ADiggableRock> TutorialSoil;
};
