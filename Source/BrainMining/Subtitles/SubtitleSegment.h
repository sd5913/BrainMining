#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SubtitleSegment.generated.h"

class USoundBase;

USTRUCT(BlueprintType)
struct FSubtitleLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="字幕", meta=(DisplayName="字幕文本"))
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="字幕", meta=(DisplayName="音频"))
	TObjectPtr<USoundBase> Audio = nullptr;
};

UCLASS(BlueprintType)
class BRAINMINING_API USubtitleSegment : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AssetRegistrySearchable, Category="字幕", meta=(DisplayName="字幕段ID"))
	FName SegmentId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="字幕", meta=(DisplayName="字幕文本"))
	TArray<FSubtitleLine> Lines;
};
