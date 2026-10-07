#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningPlayerScaleSettings.h"
#include "MiningFlightSettings.h"
#include "MiningDiggingSettings.h"
#include "MiningDeveloperMapData.h"
#include "MiningTunnelGenerator.generated.h"

class ADynamicMeshActor;
class AGeneratedMiningRock;
class AMiningLayoutRoom;
class APlayerStart;
class UMaterialInterface;
class AMiningTunnelRoute;
class APawn;
class AMiningLayoutSpace;
class AMiningHollowRegion;

UENUM(BlueprintType)
enum class EMiningPreviewMode : uint8
{
	Layout, Section, Game
};

UENUM(BlueprintType)
enum class ECompletedMiningSegments : uint8
{
	KeepFrozen,
	SealAndRecycle
};

UCLASS(Blueprintable, HideCategories=(Transform), PrioritizeCategories=("Player Flight", "Player Scale", Generation, Materials))
class BRAINMINING_API AMiningTunnelGenerator : public AActor
{
	GENERATED_BODY()
public:
	AMiningTunnelGenerator();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
	void RequestRebuild();
	void ResetProgression();
	void UpdateProgression(APawn* Player, float DeltaSeconds);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hollow Region", meta=(ToolTip="Opt-in two-room prototype: hollow circular passage with diggable plugs inside ordinary rock. Original room-face generation is unchanged when disabled."))
	bool bHollowRegionPrototype = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hollow Region", meta=(EditCondition="bHollowRegionPrototype"))
	TObjectPtr<AMiningHollowRegion> HollowRegion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Scale", meta=(ShowOnlyInnerProperties))
	FMiningPlayerScaleSettings PlayerScaleSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Flight", meta=(ShowOnlyInnerProperties))
	FMiningFlightSettings FlightSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining Controls", meta=(ShowOnlyInnerProperties))
	FMiningDiggingSettings DiggingSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Developer Map", meta=(ShowOnlyInnerProperties, ToolTip="Debug overlay only. Disabled in Shipping and Test builds; never captures or rebuilds terrain."))
	FMiningDeveloperMapSettings DeveloperMapSettings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Developer Map", AdvancedDisplay)
	TArray<FMiningMapRegion> GeneratedMapRegions;

	UFUNCTION(BlueprintPure, Category="Progression")
	AGeneratedMiningRock* GetActiveSegment() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Progression", meta=(ClampMin="1000", ClampMax="30000", DisplayName="Chunk Triangle Budget", ToolTip="Independent budget per internal chunk when automatic chunks are enabled. Never a shared tunnel or level budget."))
	int32 SegmentTriangleBudget = 30000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining Performance", meta=(ToolTip="Automatically split tunnel interiors without changing rooms, full-face connections or bedrock. Editor rebuild only."))
	bool bAutomaticChunks = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining Performance", meta=(ClampMin="400", ClampMax="3000", Units="cm", EditCondition="bAutomaticChunks", ToolTip="Maximum chunk span along the longest tunnel axis. Smaller chunks reduce mesh/collision update cost but add components. Cross-section stays unchanged."))
	float ChunkLength = 1200;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining Performance", meta=(EditCondition="bAutomaticChunks", ToolTip="Periodic conservative simplification. Locks seams, original surfaces, sharp edges and thin-wall samples; invalid simplifications are discarded."))
	bool bSimplifyChunks = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining Performance", meta=(ClampMin="0.05", ClampMax="1", Units="cm", EditCondition="bAutomaticChunks && bSimplifyChunks", ToolTip="Per-pass geometric tolerance, with bidirectional surface sampling. Not a license to force geometry below budget."))
	float SimplificationTolerance = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Progression", meta=(ClampMin="0", ClampMax="200", Units="cm", ToolTip="Extra distance the whole player capsule must clear the arrival face. Automatically limited to fit small rooms."))
	float RoomEntryInset = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Progression", meta=(ClampMin="0", ClampMax="2", Units="s", ToolTip="Time fully inside the next room before committing one-way progression."))
	float RoomEntryHoldTime = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Progression", meta=(ToolTip="Keep Frozen preserves all old visuals and collision. Seal And Recycle adds permanent bedrock at older entrances before releasing excavated meshes; never deletes on camera rotation alone."))
	ECompletedMiningSegments CompletedSegmentPolicy = ECompletedMiningSegments::KeepFrozen;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Progression", meta=(ClampMin="1", ClampMax="15", EditCondition="CompletedSegmentPolicy == ECompletedMiningSegments::SealAndRecycle", ToolTip="Retain at least the immediately previous tunnel. Older tunnels are recycled only after a safe bedrock seal can be installed."))
	int32 FrozenSegmentsToKeep = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Progression", meta=(ClampMin="0.5", ClampMax="5", Units="s", EditCondition="CompletedSegmentPolicy == ECompletedMiningSegments::SealAndRecycle", ToolTip="An old entrance must stay fully behind the camera this long before sealing, to avoid a one-frame camera update race."))
	float RecycleHiddenTime = 0.75f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Progression|Status")
	int32 CurrentRoomIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Progression|Status")
	int32 ActiveSegmentIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Progression|Status")
	FString ProgressionStatus;

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Generation")
	bool RebuildTunnels();

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Spaces", meta=(DisplayName="Add Mining Space"))
	AMiningLayoutSpace* AddMiningSpace();

	UPROPERTY(EditAnywhere, Category="Spaces", meta=(ToolTip="Optional start room for Add Mining Space. Defaults to the first room; existing spaces keep their own assignment."))
	TObjectPtr<AMiningLayoutRoom> NewSpaceFromRoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spaces")
	int32 SpaceCount = 0;

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Preview")
	void ShowLayout();

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Preview")
	void ShowSection();

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Preview")
	void ShowGameGeometry();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation", AdvancedDisplay, meta=(ToolTip="Connect complete room faces directly. Disable only to use the legacy detour planner."))
	bool bDirectRoomFaces = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Routing", meta=(ClampMin="5", ClampMax="35", EditCondition="!bDirectRoomFaces", EditConditionHides))
	float MaxSlopeDegrees = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Routing", meta=(ClampMin="1000", ClampMax="50000", Units="cm", EditCondition="!bDirectRoomFaces", EditConditionHides))
	float SearchDistance = 12000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Routing", meta=(ClampMin="30", ClampMax="200", Units="cm", EditCondition="!bDirectRoomFaces", EditConditionHides))
	float DoorMargin = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Routing")
	bool bRequireDescending = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Preview")
	EMiningPreviewMode PreviewMode = EMiningPreviewMode::Layout;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Preview", meta=(ClampMin="0", ClampMax="2", ToolTip="0=X, 1=Y, 2=Z. Only the editor preview is cut."))
	int32 SectionAxis = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Preview", meta=(Units="cm"))
	float SectionPosition = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Preview")
	bool bSectionKeepPositive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Preview")
	TObjectPtr<UMaterialInterface> PreviewBedrockMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Preview")
	TObjectPtr<UMaterialInterface> PreviewFillMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Generation|Status")
	bool bLayoutApplied = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Generation|Status")
	FString LayoutWarnings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Routing")
	TArray<TObjectPtr<AMiningTunnelRoute>> Routes;

	UPROPERTY()
	TObjectPtr<ADynamicMeshActor> PreviewBedrock;

	UPROPERTY()
	TObjectPtr<ADynamicMeshActor> PreviewFill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation")
	bool bAutoRebuild = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation", meta=(ClampMin="200", ClampMax="1200", EditCondition="!bDirectRoomFaces", EditConditionHides))
	float TunnelWidth = 500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation", meta=(ClampMin="240", ClampMax="1000", EditCondition="!bDirectRoomFaces", EditConditionHides))
	float TunnelHeight = 400;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation", meta=(ClampMin="30", ClampMax="500"))
	float BedrockThickness = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Play Visibility")
	bool bImproveMiningVisibility = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Play Visibility", meta=(ClampMin="0", ClampMax="20", ToolTip="Unitless brightness with bounded falloff so nearby walls do not become overexposed."))
	float HeadlampIntensity = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Play Visibility", meta=(ClampMin="0", ClampMax="10", ToolTip="Soft near-player fill, independent of the direction the camera faces."))
	float FillLightIntensity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation", meta=(ToolTip="Editor display only. Hide the bedrock to see room guides and diggable connectors. In play it is always visible and collidable."))
	bool bShowBedrockInEditor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation", AdvancedDisplay)
	FName LayoutId = TEXT("Main");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Materials")
	TObjectPtr<UMaterialInterface> BedrockMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Materials")
	TObjectPtr<UMaterialInterface> TunnelMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Generation|Status")
	FString LastBuildStatus = TEXT("Add MiningLayoutRoom actors, then Rebuild Tunnels");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Generation|Status")
	int32 BuildRevision = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Generation|Status")
	int32 RoomCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Generation|Status")
	int32 TunnelCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Generation|Status")
	float LastBuildMilliseconds = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Generated", AdvancedDisplay)
	TObjectPtr<ADynamicMeshActor> GeneratedBedrock;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Generated", AdvancedDisplay)
	TObjectPtr<AGeneratedMiningRock> GeneratedFill;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Generated", AdvancedDisplay)
	TArray<TObjectPtr<AGeneratedMiningRock>> GeneratedSegments;

	UPROPERTY()
	TArray<FBox> GeneratedRoomBounds;

	UPROPERTY()
	TArray<FBox> GeneratedSegmentBounds;

	UPROPERTY()
	TArray<int32> GeneratedArrivalWalls;

	UPROPERTY()
	bool bGeneratedSegmented = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Generated", AdvancedDisplay)
	TObjectPtr<APlayerStart> GeneratedPlayerStart;

private:
	TArray<AMiningLayoutRoom*> GetOrderedRooms() const;
	TArray<AMiningLayoutSpace*> GetLayoutSpaces() const;
	uint32 GetInputHash() const;
	void RefreshEditorVisibility();
	void UpdatePreview();
	void DrawLayoutGuides();
	void RemoveStaleSegments();
	bool TryRecycleSegment(int32 Index, APawn* Player);
	float RoomEntryElapsed = 0;
	bool bRuntimeConfigured = false;
	TMap<int32, double> RecycleHiddenSince;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ADynamicMeshActor>> RuntimeSeals;
	bool bBuilding = false;
	bool bPendingRebuild = true;
	uint32 ObservedInputHash = 0;
	double LastInputChangeTime = 0;
	uint32 ObservedPreviewHash = 0;

	UPROPERTY()
	float RecommendedFallResetDistance = 2000;
};
