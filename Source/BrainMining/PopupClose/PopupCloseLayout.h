#pragma once
#include "CoreMinimal.h"

namespace PopupCloseLayout
{
 constexpr float RoomCompression=5.f/6.f;
 constexpr float HalfWidth=1000.f;
 constexpr float HalfDepth=750.f;
 inline FVector Compress(FVector P){P.X*=RoomCompression;P.Y*=RoomCompression;return P;}
 inline FVector DevicePosition(int32 I)
 {
  const FVector Positions[]={FVector(0,0,0),FVector(-450,300,0),FVector(650,330,0),FVector(-650,-330,0)};
  return Positions[FMath::Clamp(I,0,3)];
 }
 inline float DeviceYaw(int32 I){return I==1 || I==3?180.f:0.f;}
}
