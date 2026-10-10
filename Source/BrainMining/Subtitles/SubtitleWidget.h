#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SubtitleWidget.generated.h"

class STextBlock;

UCLASS()
class BRAINMINING_API USubtitleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetLine(const FText& Line);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	FText CurrentLine;
	TSharedPtr<STextBlock> TextBlock;
};
