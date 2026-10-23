#pragma once
#include "MiningMineralInventory.h"

inline EMiningMineral MineralRewardType(int32 Id)
{
    switch(Id)
    {
    case 12:return EMiningMineral::Copper;
    case 13:return EMiningMineral::Iron;
    case 14:return EMiningMineral::Malachite;
    case 15:return EMiningMineral::Diamond;
    default:return EMiningMineral::None;
    }
}
