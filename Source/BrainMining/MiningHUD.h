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
	/** Keep the original diagnostics available without covering the designed GUI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Developer Map")
	bool bShowDeveloperOverlay = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Developer Map")
	TObjectPtr<UMiningDeveloperMapComponent> DeveloperMap;
};
