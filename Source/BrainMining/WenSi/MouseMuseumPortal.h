#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MouseMuseumPortal.generated.h"

class AWenSiMirrorPuzzle;
class ACameraActor;
class UMaterialInstanceDynamic;
class ACharacter;

/** The tiny gallery and the painted cell share one coordinate system. */
UCLASS()
class BRAINMINING_API AMouseMuseumPortal : public AActor
{
 GENERATED_BODY()
public:
 AMouseMuseumPortal();
 void Initialize(AWenSiMirrorPuzzle* OwnerPuzzle);
 void RefreshFocus();
 bool TryEnter();
 bool Advance(float Dt);
 void Cancel();
 bool IsFocused() const { return bFocused; }
 bool IsEntering() const { return EntryAge>=0; }
 bool HasShownGalleryHint() const { return bGalleryHintShown; }
 FVector CellCenter() const;
 static constexpr float CanvasWidth=126.f;
 static constexpr float CanvasHeight=168.f;
 // A small red textile motif; aim tolerance extends just beyond its white outline.
 static constexpr float CellRadius=8.f;
 static constexpr float CellU=.6374f;
 static constexpr float CellV=.5656f;
protected:
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 UPROPERTY(Transient) TObjectPtr<AWenSiMirrorPuzzle> Puzzle;
 UPROPERTY(Transient) TObjectPtr<ACameraActor> EntryCamera;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Painting;
 FVector EntryEye;
 FRotator EntryRotation;
 TWeakObjectPtr<ACharacter> EntryPawn;
 uint8 PreviousMovementMode=0,PreviousCustomMode=0;
 bool bPawnWasHidden=false;
 float EntryFOV=90,EntryAge=-1;
 double GalleryEnteredAt=-1;
 bool bGalleryHintShown=false;
 bool bFocused=false,bTravelRequested=false,bInputLocked=false;
};
