#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "GameFramework/Actor.h"
#include "DiggableRock.generated.h"

class UDynamicMeshComponent;
class UMaterialInterface;
class UFogChamberComponent;

UCLASS()
class BRAINMINING_API ADiggableRock : public AActor
{
	GENERATED_BODY()
public:
	ADiggableRock();
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining")
	TObjectPtr<UDynamicMeshComponent> RockMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mining")
	TObjectPtr<UFogChamberComponent> FogChamber;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining", meta=(ClampMin="50.0"))
	FVector RockSize = FVector(1000.0, 1000.0, 600.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining")
	TObjectPtr<UMaterialInterface> RockMaterial;

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

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Mining")
	virtual void ResetRock();

protected:
	void CommitMesh(UE::Geometry::FDynamicMesh3&& Mesh);
	bool bUseProceduralCutFallback = false;

private:
	UE::Geometry::FDynamicMesh3 UnitCutter;
};
