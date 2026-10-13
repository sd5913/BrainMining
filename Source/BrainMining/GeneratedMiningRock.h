#pragma once

#include "CoreMinimal.h"
#include "DiggableRock.h"
#include "Async/Future.h"
#include "MiningHollowRegion.h"
#include "GeneratedMiningRock.generated.h"

class UDynamicMesh;
class UMaterialInstanceDynamic;
class UMiningTerrainSource;
struct FMiningSimplificationResult;
struct FMiningDeferredCut;
struct FMiningDeferredRequest;
namespace MiningChunks { struct FSpatialGrid; }

USTRUCT(BlueprintType)
struct FMiningRockChunk
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chunk")
	TObjectPtr<UDynamicMesh> InitialMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chunk")
	TObjectPtr<UDynamicMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chunk")
	FBox Bounds = FBox(ForceInit);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Chunk")
	int32 Cuts = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Chunk")
	int32 Triangles = 0;
	UPROPERTY()
	int32 SpatialCell = INDEX_NONE;
	UPROPERTY(Transient)
	bool bRuntimeDeferred = false;
};

UENUM(BlueprintType)
enum class EMiningSegmentState : uint8
{
	Waiting, Active, Frozen, Recycled
};

UCLASS(NotBlueprintable)
class BRAINMINING_API AGeneratedMiningRock : public ADiggableRock
{
	GENERATED_BODY()
public:
	AGeneratedMiningRock();
	virtual void PostLoad() override;
	virtual void PreRegisterAllComponents() override;
	UPROPERTY(VisibleAnywhere, Category="Mining|Source")
	TObjectPtr<UMiningTerrainSource> TerrainSource;
#if WITH_EDITOR
	bool ExtractTerrainSource(UMiningTerrainSource* Destination);
#endif
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void ResetRock() override;
	virtual bool MineSphere(FVector WorldCenter, float WorldRadius) override;
	virtual bool MineBlast(FVector WorldCenter, float WorldRadius, float Roughness = .32f, int32 Seed = 1) override;
	void SetGeneratedMesh(UE::Geometry::FDynamicMesh3&& Mesh);
	// Large external regions explicitly opt in; existing mining grids keep their original limit.
	bool SetChunkedMesh(const UE::Geometry::FDynamicMesh3& Mesh, double Length, bool bSpatial = false, bool bAllowLargeSpatialGrid = false);
	void ApplySpatialGrid(const UE::Geometry::FDynamicMesh3& Mesh, TSharedPtr<const MiningChunks::FSpatialGrid, ESPMode::ThreadSafe> Grid);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Chunks")
	int32 LastRebuiltEditorChunks = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Chunks")
	int32 LastReusedEditorChunks = 0;
	void CopyMiningMesh(UE::Geometry::FDynamicMesh3& Mesh) const;
	void CopyEditorPreviewMesh(UE::Geometry::FDynamicMesh3& Mesh) const;
	UMaterialInterface* GetMiningMaterial();
	UDynamicMesh* GetChunkQueryMesh(const FMiningRockChunk& Chunk) const;
	void PrepareRuntimeGeometry(const FVector& WorldPosition, float WorldRadius);
	static void PrepareTerrainAt(UWorld* World, const FVector& WorldPosition, float WorldRadius = 5000.f);
	static int32 GetBackgroundTaskCount();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Chunks")
	int32 DeferredRuntimeChunks = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hollow Region")
	TArray<FMiningGuideVolume> GuideMaterialVolumes;

	UFUNCTION(BlueprintCallable, Category="Mining|Chunks")
	UDynamicMesh* GetCombinedMesh() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Chunks")
	TArray<FMiningRockChunk> Chunks;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Chunks")
	bool bChunked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Chunks")
	bool bSimplifyChunks = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Chunks", meta=(ClampMin="0.05", ClampMax="1", Units="cm"))
	float SimplificationTolerance = 0.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Chunks")
	int32 LargestChunkTriangles = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Chunks")
	int32 LastTouchedChunks = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Chunks")
	int32 SimplifiedTriangles = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Chunks")
	float LastSimplificationMilliseconds = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Chunks")
	float LastTerrainNotificationMilliseconds = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Chunks")
	int32 CompletedSimplifications = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Chunks")
	int32 DiscardedSimplifications = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Chunks")
	bool bSimplificationPending = false;
	// A time slice ending defers the accepted hit; it must not silently cancel it.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Chunks")
	bool bCutPending = false;
	UPROPERTY(EditAnywhere, Category="Mining|Chunks", meta=(ClampMin="0.01", ClampMax="40"))
	float InteractiveCutBudgetMilliseconds = 12;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Mining|Chunks")
	int32 DeferredCutsCompleted = 0;
	void SetSegmentState(EMiningSegmentState State);
	void RecycleGeometry();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Segment")
	int32 SegmentIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Segment")
	EMiningSegmentState SegmentState = EMiningSegmentState::Active;

protected:
	virtual void InitializeForPlay() override;

private:
	bool bRuntimeInitializationEnabled = false;
	FBox RuntimeChunkBounds = FBox(ForceInit);
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GuideMaterial;
	void ClearChunks();
	void EnsureChunkComponent(FMiningRockChunk& Chunk);
	void MakeSourceEditable();
	void UpdateChunkStats();
	bool MineChunks(FVector WorldCenter, float WorldRadius, const UE::Geometry::FDynamicMesh3* WorldCutter = nullptr);
	void ScheduleSimplification(int32 Index);
	bool QueueDeferredCut(FVector WorldCenter, float WorldRadius, const UE::Geometry::FDynamicMesh3& LocalCutter, bool bPreserveEdges);
	void StartDeferredCut();
	void PollDeferredCut();
	void PollBackgroundWork();
	void EnsureBackgroundPolling();
	FTimerHandle BackgroundWorkTimer;
	void CommitChunks(TArray<int32>& Indices, TArray<UE::Geometry::FDynamicMesh3>& Meshes, const FBox& Dirty);
	TArray<TSharedPtr<FMiningDeferredRequest, ESPMode::ThreadSafe>> CutQueue;
	TFuture<TSharedPtr<FMiningDeferredCut, ESPMode::ThreadSafe>> CutTask;
	TFuture<TSharedPtr<FMiningSimplificationResult, ESPMode::ThreadSafe>> SimplificationTask;
	uint64 MeshGeneration = 0;
	TSharedPtr<const MiningChunks::FSpatialGrid, ESPMode::ThreadSafe> EditorGrid;
	UE::Geometry::FDynamicMesh3 ChunkCutter;
	UPROPERTY(VisibleAnywhere, Category="Generated")
	TObjectPtr<UDynamicMesh> InitialMesh;

	UPROPERTY()
	bool bHasGeneratedMesh = false;
};
