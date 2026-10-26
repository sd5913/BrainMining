#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PopupCloseWidgets.generated.h"

class SConstraintCanvas;
class UTexture2D;
struct FSlateBrush;

TSharedRef<SWidget> MakePopupCloseComputerCursor();

UCLASS()
class BRAINMINING_API UPopupClosePromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMessage(const FString& Value) { Message=Value; }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	FString Message=TEXT("按 E 使用电脑");
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
	void AdvanceIntroduction(float DeltaSeconds);
	void SetWorkElapsed(float Seconds) { WorkElapsed=Seconds; }
	FVector2D GetLatestClosePoint() const;
	bool HasVariedWindowSizes() const;
	UFUNCTION(BlueprintPure, Category="Office") FString GetFrontPopupTitle() const;
	UFUNCTION(BlueprintPure, Category="Office") FString GetFrontPopupBody() const;
	UFUNCTION(BlueprintPure, Category="Office") int32 GetFrontPopupPhase() const;
	void SetRepliesEnabled(bool bEnabled) { bRepliesEnabled=bEnabled; }
	TFunction<void()> OnReply;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	bool ClosePopup(int32 Id);
	int32 FrontWindowIndex() const;
 TSharedRef<SWidget> MakeArtPopup(int32 Id,int32 Variant,FVector2D Size,int32 ContentIndex);
 UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> ArtTextures;
 TArray<TSharedPtr<FSlateBrush>> ArtBrushes;

	TSharedPtr<SConstraintCanvas> Canvas;

	struct FWindow
	{
		int32 Id = 0;
		int32 ContentIndex = 0;
		TSharedPtr<SWidget> Root;
		FVector2D Position;
		FVector2D Size;
  FVector2D ClosePoint;
	};

	TArray<FWindow> Windows;
	int32 NextId = 1;
	int32 ManuallyClosedCount = 0;
	bool bRepliesEnabled = true;
	int32 IntroductionPriority=1;
	float IntroductionAge=0.f;
	float WorkElapsed=-1.f;
	bool bReconnectShown=false;
};

UCLASS()
class BRAINMINING_API UPopupCloseWarningWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	int32 PendingCount=0;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
 UPROPERTY(Transient) TObjectPtr<UTexture2D> WarningTexture;
 TSharedPtr<FSlateBrush> WarningBrush;
};
