#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PopupCloseWidgets.generated.h"

class SConstraintCanvas;

UCLASS()
class BRAINMINING_API UPopupClosePromptWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};

UCLASS()
class BRAINMINING_API UPopupCloseDesktopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SpawnPopup();
	int32 GetOpenCount() const { return Windows.Num(); }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void ClosePopup(int32 Id);

	TSharedPtr<SConstraintCanvas> Canvas;

	struct FWindow
	{
		int32 Id = 0;
		TSharedPtr<SWidget> Root;
	};

	TArray<FWindow> Windows;
	int32 NextId = 1;
};
