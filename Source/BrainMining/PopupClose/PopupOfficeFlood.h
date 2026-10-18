#pragma once
#include "CoreMinimal.h"

class AActor;
class APopupCloseFlyingWindow;
class UInstancedStaticMeshComponent;

// Runtime effects are owned by the stage. Only airborne objects simulate physics;
// the bounded, persistent floor layer uses collision-free instanced meshes.
class FPopupOfficeFlood
{
public:
 explicit FPopupOfficeFlood(AActor& InOwner);
 ~FPopupOfficeFlood();
 void Warm(float Dt);
 void Start();
 void Tick(float Dt);
 void Pause();
 void Stop();
 void ClearVisuals();
 void Reset();
 bool IsFloorCovered() const;
 bool IsHealthy() const;
 int32 Emitted(int32 Source) const;
 FString Describe() const;
private:
 struct FSource {FVector Origin;FVector Direction;float Debt=0;int32 Count=0;};
 struct FSlot
 {
  TWeakObjectPtr<APopupCloseFlyingWindow> Actor;
  FVector Target=FVector::ZeroVector;
  float RetireTime=0;
  float Born=0;
  FVector Origin=FVector::ZeroVector, Velocity=FVector::ZeroVector;
  // Additional control-point lift used by generated (non-physics) emissions.
  // It creates a visible upward arc while preserving the authored landing cell.
  FRotator Rotation=FRotator::ZeroRotator;
  int32 Cell=INDEX_NONE;
  int32 Variant=0;
  bool bPaper=false;
  bool bActive=false;
  bool bPhysical=false;
 };
 void Emit(int32 Source, bool bPhysical);
 void Retire(FSlot& Slot,bool bDeposit);
 void Deposit(const FSlot& Slot);
 int32 SelectCell(const FSource& Source,bool bPaper);
 void AddGround(int32 Group,FVector P,FVector Size,FRotator Rotation);

 AActor& Owner;
 TArray<TWeakObjectPtr<UInstancedStaticMeshComponent>> Ground;
 TArray<FSlot> Slots;
 TArray<FSource> Sources;
 TArray<FVector> Cells;
 TArray<uint8> CellState;
 TArray<int32> CellOrder;
 int32 CellCursor=0;
 int32 WindowCursor=0;
 int32 PaperCursor=0;
 int32 FloorWindows=0;
 int32 FloorPapers=0;
 int32 Recycled=0;
 int32 BelowFloorPapers=0;
 float Time=0;
 float WarmDebt=0;
 float PeakFrame=0;
 bool bStarted=false;
 bool bAllSources=false;
};
