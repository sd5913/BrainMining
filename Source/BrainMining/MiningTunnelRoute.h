#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningTunnelRoute.generated.h"

class AMiningLayoutRoom;
class USplineComponent;
class UTextRenderComponent;
class AMiningTunnelShapeSection;

UENUM(BlueprintType)
enum class EMiningDoorWall : uint8
{
	Auto, PositiveX, NegativeX, PositiveY, NegativeY, PositiveZ, NegativeZ
};

UCLASS(Blueprintable, PrioritizeCategories=(Route))
class BRAINMINING_API AMiningTunnelRoute : public AActor
{
	GENERATED_BODY()
public:
	AMiningTunnelRoute();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Route")
	TObjectPtr<USplineComponent> Path;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Route")
	TObjectPtr<UTextRenderComponent> StatusLabel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Route")
	TObjectPtr<AMiningLayoutRoom> FromRoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Route")
	TObjectPtr<AMiningLayoutRoom> ToRoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Route")
	bool bDirectConnection = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Route", meta=(EditCondition="!bDirectConnection", EditConditionHides))
	bool bAutomatic = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Route")
	EMiningDoorWall StartWall = EMiningDoorWall::Auto;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Route")
	EMiningDoorWall EndWall = EMiningDoorWall::Auto;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Route")
	FString RouteStatus;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Route")
	float MaximumSlope = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Route")
	float RouteLength = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Route", AdvancedDisplay)
	int32 ResolvedStartWall = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Route", AdvancedDisplay)
	int32 ResolvedEndWall = 1;

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Route")
	void LockRouteForEditing();

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Route")
	void RestoreAutomaticRoute();

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Tunnel Shape", meta=(DisplayName="Add Shape Section"))
	void AddShapeSection();

	UFUNCTION(BlueprintCallable, Category="Tunnel Shape")
	AMiningTunnelShapeSection* CreateShapeSection();

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Tunnel Shape", meta=(DisplayName="Restore Straight Tunnel", ToolTip="Disable all shape sections without deleting them. Re-enable individual sections to recover their edits."))
	void RestoreStraightTunnel();

	TArray<AMiningTunnelShapeSection*> GetShapeSections() const;

	TArray<FVector> GetPoints() const;
	void SetPoints(const TArray<FVector>& Points);
};
