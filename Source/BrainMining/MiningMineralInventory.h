#pragma once

#include "CoreMinimal.h"
#include "MiningMineralInventory.generated.h"

// Values match the existing authored treasure IDs; old assets keep their identity.
UENUM(BlueprintType)
enum class EMiningMineral : uint8
{
    None = 0 UMETA(Hidden),
    Copper = 2 UMETA(DisplayName="铜矿"),
    Iron = 4 UMETA(DisplayName="铁矿"),
    Malachite = 6 UMETA(DisplayName="孔雀石"),
    Diamond = 9 UMETA(DisplayName="钻石")
};

USTRUCT(BlueprintType)
struct BRAINMINING_API FMiningMineralCost
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="矿石种类")) EMiningMineral Mineral = EMiningMineral::Copper;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="数量", ClampMin="1", ClampMax="1000000")) int32 Count = 1;
    static bool IsSupported(EMiningMineral Type)
    {
        return Type == EMiningMineral::Copper || Type == EMiningMineral::Iron || Type == EMiningMineral::Malachite || Type == EMiningMineral::Diamond;
    }
    static FString Name(EMiningMineral Type)
    {
        switch (Type)
        {
        case EMiningMineral::Copper: return TEXT("铜矿");
        case EMiningMineral::Iron: return TEXT("铁矿");
        case EMiningMineral::Malachite: return TEXT("孔雀石");
        case EMiningMineral::Diamond: return TEXT("钻石");
        default: return TEXT("未知矿石");
        }
    }
};
