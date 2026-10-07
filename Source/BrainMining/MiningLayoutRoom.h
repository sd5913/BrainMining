#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningLayoutRoom.generated.h"

class UBoxComponent;
class UPointLightComponent;
class UTextRenderComponent;
class USceneComponent;

UCLASS(Blueprintable, PrioritizeCategories=(Room, Transform))
class BRAINMINING_API AMiningLayoutRoom : public AActor
{
	GENERATED_BODY()
public:
	AMiningLayoutRoom();
	virtual void OnConstruction(const FTransform& Transform) override;
	FBox GetRoomBounds() const;
	void UpgradeFloorAnchor();

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Room")
	void SetAsStartRoom();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room")
	bool bStartRoom = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room", AdvancedDisplay)
	bool bFloorAnchored = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room")
	TObjectPtr<USceneComponent> RoomOrigin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Room", meta=(ClampMin="0", ToolTip="Connection order: 0 = START, then 1, 2, 3. Each room needs a unique number."))
	int32 ConnectionOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Room", meta=(ClampMin="250.0", ToolTip="Interior dimensions in cm. After migration the actor is the FLOOR center; changing height keeps the floor fixed. Rotation is not supported."))
	FVector RoomSize = FVector(1000, 1100, 540);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Room", AdvancedDisplay)
	FName LayoutId = TEXT("Main");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room")
	TObjectPtr<UBoxComponent> Bounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room")
	TObjectPtr<UPointLightComponent> RoomLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room")
	TObjectPtr<UTextRenderComponent> Label;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
	virtual void PostEditUndo() override;
#endif

private:
	void RefreshGuides();
	void NotifyGenerators(bool bImmediate);
};
