#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "GameFramework/Actor.h"
#include "DiggableRock.generated.h"

class UDynamicMeshComponent;
class UMaterialInterface;
class UFogChamberComponent;
class FMiningTerrainQuery;

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
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining")
	TObjectPtr<UDynamicMeshComponent> RockMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining")
	TObjectPtr<UFogChamberComponent> FogChamber;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining", meta=(ClampMin="50.0"))
	FVector RockSize = FVector(1000.0, 1000.0, 600.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining")
	TObjectPtr<UMaterialInterface> RockMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Hardness", meta=(DisplayName="岩层硬度等级", ClampMin="0", ClampMax="4", ToolTip="仅影响镐子的每击深度。破坏等级减硬度等级小于等于 -4 时镐子无法破坏；炸弹和钻机沿用各自规则。"))
	int32 HardnessLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining", meta=(ClampMin="1000", ClampMax="100000"))
	int32 MaxTriangles = 30000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining|Mobile")
	bool bAllowMobileSimplification = true;

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

protected:
	virtual void InitializeForPlay();
	void CommitMesh(UE::Geometry::FDynamicMesh3&& Mesh);
	void UpdateRockCollision();
	bool MineCutter(FVector WorldCenter, float WorldRadius, const UE::Geometry::FDynamicMesh3* WorldCutter);
	bool bUseProceduralCutFallback = false;
	bool bMobileCollisionPending = false;
	double NextMobileCollisionUpdate = 0.0;

private:
	UE::Geometry::FDynamicMesh3 UnitCutter;
};
