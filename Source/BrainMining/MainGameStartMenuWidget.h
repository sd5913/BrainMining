#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainGameStartMenuWidget.generated.h"

class UCanvasPanel;
class UImage;
class UButton;
class UTexture2D;
class ADiggableRock;
class AMainGameDoor;

/** MainGame start overlay. It is deliberately built from normal UMG controls so its layout stays editable. */
UCLASS()
class BRAINMINING_API UMainGameStartMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category="MainGame|Start Menu")
	void BeginGame();

	UFUNCTION(BlueprintCallable, Category="MainGame|Start Menu")
	void BeginTitleToEntrance(AMainGameDoor* Door);

	UFUNCTION(BlueprintCallable, Category="MainGame|Start Menu")
	void SetEntranceTarget(ADiggableRock* Rock);

	UPROPERTY(BlueprintReadOnly, Category="MainGame|Start Menu")
	bool bGameStarted = false;

	UPROPERTY(BlueprintReadOnly, Category="MainGame|Start Menu")
	bool bTitleArrivedAtEntrance = false;

private:
	void BuildWidgetTree();
	UFUNCTION()
	void HandleStartClicked();
	void UpdateLayout(const FVector2D& ViewportSize);
	void MoveTitleTo(const FVector2D& Target, float DeltaSeconds);
	void HideTitle();
	UFUNCTION()
	void ExitGame();

	UPROPERTY()
	TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY()
	TObjectPtr<UImage> TitleImage;
	UPROPERTY()
	TObjectPtr<UButton> StartButton;
	UPROPERTY()
	TObjectPtr<UButton> ExitButton;
	UPROPERTY()
	TObjectPtr<UTexture2D> TitleTexture;
	UPROPERTY()
	TObjectPtr<UTexture2D> StartTexture;
	UPROPERTY()
	TObjectPtr<UTexture2D> ExitTexture;

	TWeakObjectPtr<ADiggableRock> EntranceTarget;
	FVector2D TitlePosition = FVector2D::ZeroVector;
	FVector2D TitleTarget = FVector2D::ZeroVector;
	FVector2D TitleSize = FVector2D(520.0f, 360.0f);
	float TitleElapsed = 0.0f;
	float TitleDuration = 1.15f;
	bool bMovingToEntrance = false;
	bool bTargetWasObserved = false;
	bool bLayoutInitialized = false;
};
