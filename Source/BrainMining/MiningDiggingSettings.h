#pragma once

#include "CoreMinimal.h"
#include "MiningDiggingSettings.generated.h"

class UMiningComponent;

USTRUCT(BlueprintType)
struct BRAINMINING_API FMiningDiggingSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining Controls", meta=(ToolTip="Opt in to per-level base mining values. Off preserves the character blueprint. Applied before Player Scale, on next Play."))
	bool bOverrideCharacter = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining Controls", meta=(ClampMin="50", ClampMax="1000", Units="cm", EditCondition="bOverrideCharacter"))
	float Reach = 400;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining Controls", meta=(ClampMin="25", ClampMax="300", Units="cm", EditCondition="bOverrideCharacter"))
	float Radius = 160;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining Controls", meta=(ClampMin="0.1", ClampMax="2", Units="s", EditCondition="bOverrideCharacter"))
	float Interval = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining Controls", meta=(ClampMin="0", ClampMax="0.9", EditCondition="bOverrideCharacter"))
	float InsetFraction = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining Controls", meta=(ClampMin="0", ClampMax="150", Units="cm", EditCondition="bOverrideCharacter"))
	float DownwardOffset = 40;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mining Controls", meta=(ClampMin="0", ClampMax="100000", Units="cm", EditCondition="bOverrideCharacter", ToolTip="Zero disables the safety reset. Positive values are raised to the minimum needed for the generated layout. Not multiplied by Player Scale."))
	float FallResetDistance = 2000;

	void ApplyTo(UMiningComponent& Mining) const;
};
