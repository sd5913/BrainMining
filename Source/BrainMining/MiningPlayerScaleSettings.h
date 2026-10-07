#pragma once

#include "CoreMinimal.h"
#include "MiningPlayerScaleSettings.generated.h"

USTRUCT(BlueprintType)
struct BRAINMINING_API FMiningPlayerScaleSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Scale", meta=(ToolTip="Apply this layout's player scale at spawn. Disable to use the original character blueprint settings. Changes take effect on the next Play, not during excavation."))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Scale", meta=(ClampMin="0.25", ClampMax="2.0", UIMin="0.5", UIMax="1.0", EditCondition="bEnabled", DisplayName="Player Scale", ToolTip="0.7 = 70% player size; 1 = original size. Scales the actual capsule, both meshes, camera offsets, movement and mining. Does not resize or rebuild terrain."))
	float Scale = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Scale", meta=(ClampMin="0.25", ClampMax="3.0", EditCondition="bEnabled", ToolTip="Extra speed multiplier AFTER Player Scale. 1 follows player size; 1.43 at scale 0.7 approximately restores the original walk speed."))
	float MovementSpeedMultiplier = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Scale", meta=(ClampMin="0.25", ClampMax="3.0", EditCondition="bEnabled", ToolTip="Extra jump HEIGHT multiplier after Player Scale. Gravity also follows size, preserving the original jump timing at multiplier 1."))
	float JumpHeightMultiplier = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Scale", meta=(ClampMin="0.25", ClampMax="3.0", EditCondition="bEnabled", ToolTip="Extra maximum step height multiplier after Player Scale. Does not change the walkable slope angle."))
	float StepHeightMultiplier = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Scale", meta=(ClampMin="0.25", ClampMax="3.0", EditCondition="bEnabled", ToolTip="Extra mining radius multiplier after Player Scale. Smaller cuts can require more cuts and more triangles within the same tunnel."))
	float MiningRadiusMultiplier = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Scale", meta=(ClampMin="0.25", ClampMax="3.0", EditCondition="bEnabled", ToolTip="Extra mining reach multiplier after Player Scale, independent of the cut radius."))
	float MiningReachMultiplier = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Player Scale", meta=(ClampMin="0.0", ClampMax="3.0", EditCondition="bEnabled", ToolTip="Extra downward cut-center offset multiplier after Player Scale. 0 removes the downward bias; 1 preserves the original proportions."))
	float MiningDownwardOffsetMultiplier = 1;
};
