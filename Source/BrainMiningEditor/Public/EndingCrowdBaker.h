#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EndingCrowd.h"
#include "EndingCrowdBaker.generated.h"

UCLASS()
class BRAINMININGEDITOR_API UEndingCrowdBaker : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Ending")
    static UStaticMesh* BakeWalker(const FEndingPedestrian& Person, const FString& PackagePath, int32 LOD, bool bIdle, FLinearColor ShirtColor);
};
