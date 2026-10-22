#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Async/Future.h"
#include "GameFramework/Actor.h"
#include "DiggableRock.generated.h"

class UDynamicMeshComponent;
class UMaterialInterface;
class UFogChamberComponent;
class FMiningTerrainQuery;
struct FMiningRockCutResult;

// Emitted before returning a committed terrain edit, so inclusions and rock are
// submitted to the renderer in the same frame. Invalid bounds mean a full reset.
DECLARE_MULTICAST_DELEGATE_OneParam(FMiningTerrainCommitted, const FBox&);

UCLASS()
class BRAINMINING_API ADiggableRock : public AActor
{
	GENERATED_BODY()
public:
	ADiggableRock();
	// Shared by nearby deposits, released with this rock.
	TSharedPtr<FMiningTerrainQuery> TerrainQuery;
	FMiningTerrainCommitted OnTerrainCommitted;
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual bool IsTerrainCutPending() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining")
	TObjectPtr<UDynamicMeshComponent> RockMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining")
	TObjectPtr<UFogChamberComponent> FogChamber;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining", meta=(ClampMin="50.0"))
	FVector RockSize = FVector(1000.0, 1000.0, 600.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining")
	TObjectPtr<UMaterialInterface> RockMaterial;

	// Optional rounded starting volume used by the ending's circular doorway.
	// Ordinary mining rocks remain box-shaped unless this is explicitly enabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Ending")
	bool bUseCircularRockMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Hardness", meta=(DisplayName="岩层硬度等级", ClampMin="0", ClampMax="4", ToolTip="仅影响镐子的每击深度。破坏等级减硬度等级小于等于 -4 时镐子无法破坏；炸弹和钻机沿用各自规则。"))
	int32 HardnessLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining", meta=(ClampMin="1000", ClampMax="100000"))
	int32 MaxTriangles = 30000;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Stats")
	int32 TriangleCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Stats")
	int32 SuccessfulCuts = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Stats")
	float LastBooleanMilliseconds = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Stats")
	float LastCollisionMilliseconds = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Stats")
	float LastTotalMilliseconds = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining|Stats")
	FString LastStatus = TEXT("Ready");

	UFUNCTION(BlueprintCallable, Category="Mining")
	virtual bool MineSphere(FVector WorldCenter, float WorldRadius);

	UFUNCTION(BlueprintCallable, Category="Mining")
	virtual bool MinePickaxe(FTransform WorldFrame, FVector HalfSize, int32 Seed = 0, float Roughness = 1);

	UFUNCTION(BlueprintCallable, Category="Mining")
	virtual bool MineBlast(FVector WorldCenter, float WorldRadius, float Roughness = .32f, int32 Seed = 1);

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Mining")
	virtual void ResetRock();

	// Optional authored geometry (for example the rounded television glass).
	// Stored with the level so packaged games never need editor mesh conversion.
	UPROPERTY(EditAnywhere, Category="Mining|Authored")
	TObjectPtr<class UDynamicMesh> AuthoredRestMesh;
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Mining|Authored")
	void BakeAuthoredMesh(UDynamicMesh* Mesh);

protected:
    friend class UMiningSaveSubsystem;
	virtual void InitializeForPlay();
	void CommitMesh(UE::Geometry::FDynamicMesh3&& Mesh);
	bool MineCutter(FVector WorldCenter, float WorldRadius, const UE::Geometry::FDynamicMesh3* WorldCutter);
	bool bUseProceduralCutFallback = false;

private:
	bool DeferCut(UE::Geometry::FDynamicMesh3 Source, UE::Geometry::FDynamicMesh3 Cutter, FVector WorldCenter, float WorldRadius, bool bPreserveEdges);
	void PollRockCut();
	TFuture<TSharedPtr<FMiningRockCutResult, ESPMode::ThreadSafe>> PendingRockCut;
	FTimerHandle RockCutTimer;
	uint64 TerrainRevision = 0;
	UE::Geometry::FDynamicMesh3 UnitCutter;
};
