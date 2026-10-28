#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningWallSign.generated.h"
class UWidgetComponent;

// World-space signs use the same embedded CJK font as the interaction HUD.
UCLASS()
class BRAINMINING_API AMiningWallSign : public AActor
{
    GENERATED_BODY()
public:
    AMiningWallSign();
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    UPROPERTY(EditAnywhere) FText Words;
    UPROPERTY(EditAnywhere) FName LocalizationKey;
    UFUNCTION(BlueprintPure) FText GetDisplayText() const;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetComponent> Surface;
private:
    void Refresh();
};
