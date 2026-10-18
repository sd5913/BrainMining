#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SubtitleWidget.generated.h"

class STextBlock;
class SSubtitleVisual;

UCLASS()
class BRAINMINING_API USubtitleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetLine(const FText& Line);
	static TSharedRef<SWidget> CreateStylePreview(const FText& Text);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	FText CurrentLine;
	TSharedPtr<SSubtitleVisual> SubtitleVisual;
};
