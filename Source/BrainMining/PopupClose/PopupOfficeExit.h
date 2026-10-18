#pragma once
#include "CoreMinimal.h"

class APopupCloseStage;
class UBoxComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMiningBombToolComponent;
class APlayerController;
class AMiningBombExplosion;
class USceneComponent;
class UPostProcessComponent;

// A world timer keeps the evacuation finale alive after the campaign disables
// the office actor's tick on entry to the next area.
class FPopupOfficeExit
{
public:
 explicit FPopupOfficeExit(APopupCloseStage& Stage);
 ~FPopupOfficeExit();
 void OfferBomb();
 void HandleExplosion(const FVector& Position);
 void Reset();
 bool DoorIsOpen() const { return Phase>=4; }
 bool HasEscaped() const { return Phase>=5; }
 bool IsRuined() const { return Phase==6 && bDestructionSignaled; }
 bool IsWaitingForBomb() const { return Phase==2 || Phase==3; }
 bool HasOfferedBomb() const { return Phase>0; }
 bool NeedsDoorBreach() const { return Phase==3; }
 FString Describe() const;
 FVector PickupPosition() const;
 bool CanCollect(APlayerController* PC) const;
 bool Collect(APlayerController* PC);
private:
 void Tick();
 void BindPlayer();
 void StartTimer();
 void SealRoom();
 void FinishExitTurn();
 void TickExitTurn(float Dt);
 void EmitBomb(int32 Source);
 void Collapse();
 void SetInteriorVisible(bool bVisible);
 void SpawnExplosion(FVector Position,float Scale);
 void SetBox(UStaticMeshComponent* Mesh,FVector Position,FVector Size);

 struct FBombVisual {FVector Origin,Velocity;float Born=0;FRotator Rotation;};
 struct FRubble {FVector Position,Size;FRotator Rotation;float DropDelay=0.f;float DropDuration=1.2f;};
 APopupCloseStage& Owner;
 TWeakObjectPtr<UStaticMeshComponent> Door,Pickup,Shutter;
 TWeakObjectPtr<UPostProcessComponent> PickupOutline;
 TWeakObjectPtr<UBoxComponent> RuinBlocker;
 TWeakObjectPtr<UInstancedStaticMeshComponent> Bombs,Rubble,DoorFragments,ReturnGrille;
 TWeakObjectPtr<UMiningBombToolComponent> BoundTool;
 TArray<TWeakObjectPtr<AMiningBombExplosion>> Effects;
 TArray<FBombVisual> BombVisuals;
 TArray<FRubble> RubblePieces;
 TArray<FVector> Sources;
 FTimerHandle Timer;
 TWeakObjectPtr<APlayerController> TurnController;
 float ExitTurnElapsed=0.f;
 FRotator ExitTurnStart=FRotator::ZeroRotator,ExitTurnTarget=FRotator::ZeroRotator;
 double LastTime=0;
 float Age=0,EmissionDebt=0;
 float PreviousX=0;
 bool bInsideSeen=false,bHavePosition=false,bDestructionSignaled=false;
 int32 Phase=0,BombsEmitted=0,BombCursor=0,ExplosionWaves=0;
 FVector BombMeshOrigin=FVector::ZeroVector;
 TArray<TWeakObjectPtr<USceneComponent>> CorridorParts;
};
