#pragma once

#include "CoreMinimal.h"
#include "MiningHollowRegion.h"
#include "MiningManualRegion.generated.h"

class AMiningManualShape;

UCLASS(Blueprintable, HideCategories=(Passage, Plugs))
class BRAINMINING_API AMiningManualRegion : public AMiningHollowRegion
{
	GENERATED_BODY()
public:
	AMiningManualRegion();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual uint32 GetGeometryHash() const override;
	TArray<AMiningManualShape*> GetShapes() const;
	virtual FBox GetRockBounds() const;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="手绘矿区", meta=(DisplayName="岩石范围尺寸", Units="cm"))
	FVector RockSize = FVector(6000, 2200, 2600);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="手绘矿区", meta=(DisplayName="曲线采样误差", ClampMin="2", ClampMax="50", Units="cm"))
	float CurveTolerance = 8;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="手绘矿区", meta=(DisplayName="主线连接通道"))
	TObjectPtr<AMiningManualShape> MainPassage;
};
