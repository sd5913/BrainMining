#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningHollowChamber.generated.h"

class AMiningHollowRegion;
class UBoxComponent;
class USplineComponent;

UCLASS(Blueprintable, HideCategories=(Actor, Collision, Physics, Rendering, Replication, Input), PrioritizeCategories=(Chamber, Branch))
class BRAINMINING_API AMiningHollowChamber : public AActor
{
	GENERATED_BODY()
public:
	AMiningHollowChamber();
	virtual void OnConstruction(const FTransform& Transform) override;
	uint32 GetGeometryHash() const;
	bool IsInteractiveEdit() const { return bInteractiveEdit; }
	FBox GetChamberBounds() const;
	TArray<FVector> GetInteriorPoints() const;
	void ApplyResolvedPath(const TArray<FVector>& Points);
	void RefreshGuides();

	UFUNCTION(CallInEditor, BlueprintCallable, Category="Branch", meta=(ToolTip="Replace manual bends with a straight three-point editable branch. Undo supported."))
	void ResetBranchPath();
	UFUNCTION(CallInEditor, BlueprintCallable, Category="Chamber")
	void RebuildRegion();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chamber", meta=(ToolTip="Required owner. This cave never advances main-room progression."))
	TObjectPtr<AMiningHollowRegion> Region;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chamber")
	bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chamber", meta=(Units="cm", ToolTip="Full interior size. Actor location is the floor center. Positive actor scaling also resizes the chamber."))
	FVector ChamberSize = FVector(900, 900, 700);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Branch", meta=(ClampMin="0.05", ClampMax="0.95", ToolTip="Fraction of main passage length. Branch mouth follows the main passage."))
	float ConnectionPosition = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Branch", meta=(ClampMin="150", ClampMax="500", Units="cm"))
	float BranchRadius = 180;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Branch")
	bool bHiddenEntrance = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Branch", meta=(EditCondition="bHiddenEntrance", ClampMin="50", ClampMax="600", Units="cm", ToolTip="Minimum solid rock beyond the main passage clearance envelope. The entrance remains ordinary diggable rock."))
	float EntranceThickness = 150;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Branch", meta=(ToolTip="Off: straight automatic connection. On: drag interior points of EditableBranchPath; endpoints always follow the main passage and chamber."))
	bool bManualBranchPath = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chamber")
	TObjectPtr<UBoxComponent> Bounds;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Branch")
	TObjectPtr<USplineComponent> Path;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chamber")
	FString ChamberStatus = TEXT("Choose Region; move floor center and edit Chamber Size. No progression trigger.");
#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
	virtual void PostEditUndo() override;
#endif
private:
	void NotifyGenerators();
	bool bInteractiveEdit = false;
};
