#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "FogChamberComponent.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

UENUM(BlueprintType)
enum class EFogChamberState : uint8
{
	Sealed,
	Seeping,
	Venting,
	Open
};

UCLASS(ClassGroup=(Mining), meta=(BlueprintSpawnableComponent))
class BRAINMINING_API UFogChamberComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UFogChamberComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chamber")
	bool bEnabled = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chamber")
	FVector RoomSize = FVector(400, 480, 380);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chamber")
	FVector RoomOffset = FVector(0, 0, -90);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chamber", meta=(ClampMin="10", ClampMax="300"))
	float SeepDistance = 140.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chamber", meta=(ClampMin="0.5", ClampMax="20"))
	float BurstDuration = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chamber")
	TObjectPtr<UMaterialInterface> FogMaterial;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chamber|Runtime")
	EFogChamberState State = EFogChamberState::Sealed;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chamber|Runtime")
	bool bRoomReady = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chamber|Runtime")
	int32 BreachCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chamber|Runtime")
	int32 ActivePuffs = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chamber|Runtime")
	float RemainingWall = -1.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chamber|Runtime")
	float Pressure = 1.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chamber|Runtime")
	FVector OutletLocation = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chamber|Runtime")
	FVector OutletDirection = FVector::ForwardVector;

	bool InitializeRoom(UE::Geometry::FDynamicMesh3& Mesh, FVector RockSize);
	void ObserveCut(const UE::Geometry::FDynamicMesh3& Mesh, FVector LocalCenter, double LocalRadius);
	UFUNCTION(BlueprintCallable, Category="Chamber")
	FString GetStateLabel() const;

private:
	struct FAirNode
	{
		FVector Center;
		double Radius = 0;
		bool bExterior = false;
		bool bRoom = false;
		TArray<int32> Neighbors;
	};
	struct FSeep
	{
		FVector Position;
		FVector Direction;
		float Strength = 0;
		float Life = 0;
	};
	struct FPuff
	{
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Age = 0;
		float Life = 0;
		float Size = 0;
		float Opacity = 0;
		bool bAmbient = false;
	};
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> FogMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> PuffMeshAsset;
	TArray<FAirNode> AirNodes;
	TArray<FSeep> Seeps;
	TArray<FPuff> Puffs;
	TArray<FTransform> PuffTransforms;
	FBox RoomBounds;
	FVector RockExtent;
	FRandomStream Random{1042026};
	FVector OutletLocal = FVector::ZeroVector;
	FVector OutletLocalDirection = FVector::ForwardVector;
	float VentAge = 0;
	float SpawnRemainder = 0;
	float AmbientRemainder = 0;
	void ResetEffects();
	void SpawnPuff(FVector Position, FVector Velocity, float Size, float Opacity, float Life, bool bAmbient);
};
