#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningHollowRegion.generated.h"

class AMiningLayoutRoom;
class USplineComponent;
class UBoxComponent;
class AMiningHollowChamber;

USTRUCT(BlueprintType)
struct FMiningHollowPlug
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plug", meta=(ClampMin="0.01", ClampMax="0.99", ToolTip="Fraction of actual path length, not X distance."))
	float Position = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plug", meta=(ClampMin="50", ClampMax="1000", Units="cm"))
	float Thickness = 240;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plug")
	bool bEnabled = true;
};

USTRUCT(BlueprintType)
struct FMiningGuideVolume
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Guide")
	FVector Start = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Guide")
	FVector End = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Guide")
	float Radius = 350;
};

UCLASS(Blueprintable, HideCategories=(Actor, Collision, Physics, Rendering, Replication, Input), PrioritizeCategories=(Region, Passage, Plugs))
class BRAINMINING_API AMiningHollowRegion : public AActor
{
	GENERATED_BODY()
public:
	AMiningHollowRegion();
	virtual void OnConstruction(const FTransform& Transform) override;
	uint32 GetGeometryHash() const;
	void RefreshBounds(const FBox& WorldBounds);
	TArray<FVector> GetPathPoints() const;
	TArray<AMiningHollowChamber*> GetChambers() const;
	bool IsInteractiveEdit() const { return bInteractiveEdit; }

	UFUNCTION(CallInEditor, BlueprintCallable, Category="Region", meta=(ToolTip="Explicitly replaces the authored path with five points between the room portals. Undo supported."))
	void FitPathBetweenRooms();
	UFUNCTION(CallInEditor, BlueprintCallable, Category="Region")
	void RebuildRegion();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Region")
	TObjectPtr<AMiningLayoutRoom> FromRoom;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Region")
	TObjectPtr<AMiningLayoutRoom> ToRoom;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Region", meta=(ClampMin="1000", ClampMax="8000", Units="cm", ToolTip="Rock width on Y. Length follows the gap between the rooms' facing X walls."))
	float RegionWidth = 2600;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Region", meta=(ClampMin="100", ClampMax="3000", Units="cm"))
	float RockAbove = 500;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Region", meta=(ClampMin="100", ClampMax="3000", Units="cm"))
	float RockBelow = 400;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(ClampMin="150", ClampMax="1000", Units="cm", ToolTip="Circular YZ sections; first and last centers follow room floors. Internal spline points keep their authored world positions."))
	float PassageRadius = 350;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(ClampMin="5", ClampMax="35", Units="deg"))
	float MaximumSlope = 25;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugs", meta=(TitleProperty="Position"))
	TArray<FMiningHollowPlug> Plugs;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Passage")
	TObjectPtr<USplineComponent> Path;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Region")
	TObjectPtr<UBoxComponent> Bounds;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Region")
	FString RegionStatus = TEXT("Choose two rooms, Fit Path Between Rooms, then assign this region on the generator.");
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Region")
	FVector ResolvedSize = FVector::ZeroVector;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
	virtual void PostEditUndo() override;
#endif
private:
	void NotifyGenerators();
	bool bInteractiveEdit = false;
};
