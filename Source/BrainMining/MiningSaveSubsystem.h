#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "MiningSaveSubsystem.generated.h"

UCLASS(BlueprintType)
class BRAINMINING_API UMiningSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 Balance=0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<uint8> Cuts;
};

UCLASS()
class BRAINMINING_API UMiningSaveSubsystem : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UMiningSaveGame> SaveData;
};
