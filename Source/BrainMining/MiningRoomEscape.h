#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningRoomEscape.generated.h"
class ACardRoomStage;
class AMiningEncounterPlayerController;
class AMiningGiantRoomEntrance;
class ADiggableRock;
class SMiningEscapeView;
UENUM(BlueprintType)
enum class EMiningEscapePhase : uint8 { Idle, Warning, Flooding, Failed, Escaped };
// Host-side orchestration: plugins keep their own card/echo logic; the original miner does all cuts.
UCLASS()
class BRAINMINING_API AMiningRoomEscape : public AActor
{
    GENERATED_BODY()
public:
    AMiningRoomEscape();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditAnywhere,Category="Escape") float WarningSeconds=5;
    UPROPERTY(EditAnywhere,Category="Escape") float FloodSeconds=70;
    UPROPERTY(EditAnywhere,Category="Escape") float RetrySeconds=2;
    UPROPERTY(EditAnywhere,Category="Escape") float WaterStartZ=80;
    UPROPERTY(EditAnywhere,Category="Escape") float WaterEndZ=2690;
    UPROPERTY(EditAnywhere,Category="Escape") FVector Checkpoint=FVector(10000,3600,795);
    UPROPERTY(EditAnywhere,Category="Escape") FVector ComputerEntry=FVector(11700,5000,1730);
    UPROPERTY(EditAnywhere,Category="Escape") FVector Exit=FVector(13300,5000,2630);
    UPROPERTY(EditInstanceOnly,Category="Escape|Actors") TObjectPtr<ACardRoomStage> CardStage;
    UPROPERTY(EditInstanceOnly,Category="Escape|Actors") TObjectPtr<AMiningGiantRoomEntrance> Entrance;
    UPROPERTY(EditInstanceOnly,Category="Escape|Actors") TObjectPtr<ADiggableRock> BreachWall;
    UPROPERTY(EditInstanceOnly,Category="Escape|Actors") TArray<TObjectPtr<ADiggableRock>> ComputerRocks;
    UPROPERTY(EditInstanceOnly,Category="Escape|Actors") TObjectPtr<AActor> Water;
    UPROPERTY(EditInstanceOnly,Category="Escape|Actors") TArray<TObjectPtr<AActor>> Streams;
    UPROPERTY(EditInstanceOnly,Category="Escape|Actors") TArray<TObjectPtr<AActor>> Foam;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") EMiningEscapePhase Phase=EMiningEscapePhase::Idle;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") float Elapsed=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") float WaterZ=80;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") int32 Attempts=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") bool bWallBroken=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") bool bEnteredComputer=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") int32 ClearedNodes=0;
    FString FailureReason;
    float WarningLeft() const {return FMath::Max(0.f,WarningSeconds-PhaseTime);}
    float ElectricalSecondsLeft() const {return FMath::Max(0.f,FloodSeconds*(1690-WaterStartZ)/FMath::Max(1.f,WaterEndZ-WaterStartZ)-Elapsed); }
    FVector GuideTarget() const;
    float SecondsLeft() const {return FMath::Max(0.f,FloodSeconds-Elapsed);}
    UFUNCTION(BlueprintCallable,Category="Escape") void RetryEscape();
private:
    UFUNCTION() void OnWallImpact(FName RoomId);
    UFUNCTION() void OnRewardClaimed(FName RoomId);
    bool HandleRestart();
    void ResetJourney();
    void StartFlood();
    void Fail(const FString& Reason);
    void ApplyExploration();
    void UpdateWater();
    float PhaseTime=0;
    bool bFailureLock=false;
    TWeakObjectPtr<AMiningEncounterPlayerController> Player;
    TSharedPtr<SMiningEscapeView> View;
};
