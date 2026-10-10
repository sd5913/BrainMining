#pragma once
#include "CoreMinimal.h"
#include "DiggableRock.h"
#include "MiningGiantRoomEntrance.generated.h"
class AMiningEncounterPlayerController;
class UBoxComponent;
class UPostProcessComponent;

// A real DiggableRock wall; same-level input coordination is confined to this encounter.
UCLASS()
class BRAINMINING_API AMiningGiantRoomEntrance : public ADiggableRock
{
    GENERATED_BODY()
public:
    AMiningGiantRoomEntrance();
    bool bExternalEncounterActive=false;
    virtual void ResetRock() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Giant Room") float MiningZoneDepth=650;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> RoomBounds;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPostProcessComponent> RoomExposure;
private:
    void ResetJourney();
    TWeakObjectPtr<AMiningEncounterPlayerController> Player;
};
