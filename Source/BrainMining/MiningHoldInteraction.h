#pragma once
#include "CoreMinimal.h"

// One activation per hold. Losing focus, opening a menu or releasing cancels it.
struct FMiningHoldInteraction
{
    float Seconds=0;
    bool Fired=false;
    void Reset(){Seconds=0;Fired=false;}
    bool Update(bool Focused,bool Held,float Dt)
    {
        if(!Focused || !Held){Reset();return false;}
        if(Fired)return false;
        Seconds=FMath::Min(3.f,Seconds+FMath::Max(0.f,Dt));
        if(Seconds>=3.f){Fired=true;return true;}
        return false;
    }
    float Progress() const {return Seconds/3.f;}
};
