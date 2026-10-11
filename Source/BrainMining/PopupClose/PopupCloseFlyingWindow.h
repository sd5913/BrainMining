#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PopupCloseFlyingWindow.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class BRAINMINING_API APopupCloseFlyingWindow : public AActor
{
	GENERATED_BODY()

public:
	APopupCloseFlyingWindow();

	void Launch(const FLinearColor& Color, const FVector& Velocity);
	void PrepareForPool(int32 Variant);

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> TitleBar;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> CloseButton;
	UPROPERTY() TObjectPtr<UTextRenderComponent> TitleText;
	UPROPERTY() TObjectPtr<UTextRenderComponent> BodyText;
};
