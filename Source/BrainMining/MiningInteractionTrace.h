#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"

namespace MiningInteraction
{
// Preserve the central target's priority. Nearby rays give a small aim margin,
// but each ray must still hit the target before any opaque obstruction.
template<typename Predicate>
bool Trace(UWorld* World, FHitResult& Hit, const FVector& Eye, const FRotator& View,
           float Reach, const FCollisionQueryParams& Query, Predicate Accept)
{
    if (!World) return false;
    const FVector Forward=View.Vector();
    const FRotationMatrix Basis(View);
    const FVector Right=Basis.GetUnitAxis(EAxis::Y), Up=Basis.GetUnitAxis(EAxis::Z);
    auto Try=[&](const FVector& Direction)
    {
        return World->LineTraceSingleByChannel(Hit,Eye,Eye+Direction*Reach,ECC_Visibility,Query) && Accept(Hit);
    };
    if (Try(Forward)) return true;
    for (float Degrees : {2.5f,5.f})
    {
        const float Offset=FMath::Tan(FMath::DegreesToRadians(Degrees));
        for (int32 Ray=0; Ray<8; ++Ray)
        {
            const float Angle=Ray*PI/4.f;
            if (Try((Forward+(Right*FMath::Cos(Angle)+Up*FMath::Sin(Angle))*Offset).GetSafeNormal())) return true;
        }
    }
    return false;
}
}
