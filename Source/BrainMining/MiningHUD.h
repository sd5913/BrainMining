#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MiningHUD.generated.h"

class UMiningDeveloperMapComponent;

UCLASS()
class BRAINMINING_API AMiningHUD : public AHUD
{
	GENERATED_BODY()
public:
	AMiningHUD();
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Developer Map")
	TObjectPtr<UMiningDeveloperMapComponent> DeveloperMap;
};
