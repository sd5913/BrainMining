#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MobileRuntimeSettings.generated.h"

UCLASS(config=Game, defaultconfig)
class BRAINMINING_API UMobileRuntimeSettings : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(config, EditAnywhere, Category="Vertical Slice")
    bool bEnableMobileVerticalSlice = false;

    UPROPERTY(config, EditAnywhere, Category="Mining", meta=(ClampMin="1.0", ClampMax="3.0"))
    float MobileMiningIntervalScale = 1.2f;

    UPROPERTY(config, EditAnywhere, Category="Mining", meta=(ClampMin="1000", ClampMax="100000"))
    int32 MobileMaxDynamicTriangles = 14000;

    UPROPERTY(config, EditAnywhere, Category="Mining", meta=(ClampMin="0.0", ClampMax="1.0"))
    float MobileCollisionUpdateInterval = 0.12f;

    UPROPERTY(config, EditAnywhere, Category="Mining")
    bool bUseSimplifiedMobileMining = true;

    UPROPERTY(config, EditAnywhere, Category="Input", meta=(ClampMin="0.01", ClampMax="0.5"))
    float MobileLookSensitivity = 0.09f;

    static bool IsMobileRuntime();
    static bool IsMobileVerticalSliceEnabled();
    static const UMobileRuntimeSettings* Get();
};
