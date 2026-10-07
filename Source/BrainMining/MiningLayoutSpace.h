#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningLayoutSpace.generated.h"

class AMiningLayoutRoom;
class UDynamicMeshComponent;
class UMaterialInterface;
class USceneComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EMiningSpaceShape : uint8 { Box, Ellipsoid };

UENUM(BlueprintType)
enum class EMiningSpaceContents : uint8 { Empty, DiggableRock };

UENUM(BlueprintType)
enum class EMiningSpaceConnection : uint8 { Overlap, StraightBranch };

UENUM(BlueprintType)
enum class EMiningSpaceEntrance : uint8 { Open, RockFilled, HiddenWall };

UCLASS(Blueprintable, HideCategories=(Actor, AssetUserData, Collision, Cooking, DataLayers, HLOD, Input, LOD, Networking, Physics, Replication, Rendering, WorldPartition, Materials), AutoExpandCategories=(Space, Connection), PrioritizeCategories=(Space, Connection, Transform))
class BRAINMINING_API AMiningLayoutSpace : public AActor
{
	GENERATED_BODY()
public:
	AMiningLayoutSpace();
	virtual void OnConstruction(const FTransform& Transform) override;
	FBox GetSpaceBounds() const;
	uint32 GetGeometryHash() const;
	void SetGuideVisibility(bool bVisible);
	bool IsInteractiveEdit() const { return bInteractiveEdit; }

	UFUNCTION(CallInEditor, Category="Space", meta=(DisplayName="Rebuild Layout"))
	void RebuildLayout();

	UFUNCTION(CallInEditor, Category="Space", meta=(DisplayName="Assign Nearest Tunnel"))
	void AssignNearestTunnel();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space")
	FString SpaceName = TEXT("Cave");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space", meta=(ToolTip="The room at the START of the owning tunnel. Moving a space never changes its tunnel automatically."))
	TObjectPtr<AMiningLayoutRoom> FromRoom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space")
	EMiningSpaceShape Shape = EMiningSpaceShape::Box;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space", meta=(ClampMin="250", ClampMax="10000", Units="cm", ToolTip="Interior size; origin is floor center. Changing dimensions does not scale attached content. Axis-aligned only."))
	FVector SpaceSize = FVector(1000, 900, 700);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space")
	EMiningSpaceContents Contents = EMiningSpaceContents::Empty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connection")
	EMiningSpaceConnection Connection = EMiningSpaceConnection::Overlap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connection", meta=(EditCondition="Connection == EMiningSpaceConnection::StraightBranch", EditConditionHides))
	EMiningSpaceEntrance Entrance = EMiningSpaceEntrance::Open;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connection", meta=(ClampMin="200", ClampMax="1200", Units="cm", EditCondition="Connection == EMiningSpaceConnection::StraightBranch", EditConditionHides))
	float BranchWidth = 350;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connection", meta=(ClampMin="250", ClampMax="1200", Units="cm", EditCondition="Connection == EMiningSpaceConnection::StraightBranch", EditConditionHides))
	float BranchHeight = 350;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connection", meta=(EditCondition="Connection == EMiningSpaceConnection::StraightBranch", EditConditionHides, ToolTip="Connect near the tunnel/space floors instead of their centers. Keeps branch cross-sections upright. Curved cave floors and steep slopes still need playtesting."))
	bool bAlignBranchFloors = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Connection", meta=(ClampMin="30", ClampMax="500", Units="cm", EditCondition="Connection == EMiningSpaceConnection::StraightBranch && Entrance == EMiningSpaceEntrance::HiddenWall", EditConditionHides, ToolTip="Uncarved rock before the cave entrance, measured along the branch centerline. Requires a detached empty cave."))
	float EntranceRockThickness = 120;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space", AdvancedDisplay)
	FName LayoutId = TEXT("Main");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Space|Status")
	FString SpaceStatus = TEXT("Choose From Room or Assign Nearest Tunnel");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Space|Content")
	TObjectPtr<USceneComponent> ContentRoot;

	UPROPERTY(VisibleAnywhere, Category="Space|Preview", AdvancedDisplay)
	TObjectPtr<UDynamicMeshComponent> Preview;

	UPROPERTY(VisibleAnywhere, Category="Space|Preview", AdvancedDisplay)
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(EditAnywhere, Category="Space|Preview", AdvancedDisplay)
	TObjectPtr<UMaterialInterface> PreviewMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Space|Status", AdvancedDisplay)
	FVector ResolvedBranchAnchor = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Space|Status", AdvancedDisplay)
	FVector ResolvedWallCenter = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Space|Status", AdvancedDisplay)
	FVector ResolvedWallSize = FVector::ZeroVector;

	UPROPERTY(Transient)
	FQuat ResolvedWallRotation = FQuat::Identity;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
	virtual void PostEditUndo() override;
#endif

private:
	void RefreshGuides();
	void NotifyGenerators();
	bool bInteractiveEdit = false;
};
