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
	int32 GetManuallyClosedCount() const { return ManuallyClosedCount; }
	void ClearOldestPopup();
	void ClearPopups();
	FVector2D GetLatestClosePoint() const;
	bool HasVariedWindowSizes() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	void ClosePopup(int32 Id);

	TSharedPtr<SConstraintCanvas> Canvas;

	struct FWindow
	{
		int32 Id = 0;
		TSharedPtr<SWidget> Root;
		FVector2D Position;
		FVector2D Size;
	};

	TArray<FWindow> Windows;
	int32 NextId = 1;
	int32 ManuallyClosedCount = 0;
};
