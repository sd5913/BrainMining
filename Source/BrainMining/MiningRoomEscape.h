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
    virtual void OnConstruction(const FTransform& Transform) override;
    // The same CRT shell is visible while authoring and used by the live escape.
    UPROPERTY(EditAnywhere,Category="Escape|Preview") bool bPreviewTelevision=true;
    UPROPERTY(EditInstanceOnly,Category="Escape|Actors") TObjectPtr<ADiggableRock> AuthoredTelevisionScreen;
    // P2 moves directly from the victory presentation to the shared P1 escape.
    UPROPERTY(EditAnywhere,Category="Escape") bool bAutoClaimVictoryReward=false;
    UPROPERTY(EditAnywhere, Category="Integration") bool bAreaManaged = false;
    UPROPERTY(EditAnywhere, Category="Integration") bool bWalkThroughConnections = false;
    bool bAreaActive = true;
    // Authored escape coordinates belong to the source map, before streaming translation.
    FVector SourceLevelOffset = FVector::ZeroVector;
    void SetAreaActive(bool bActive);
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditAnywhere,Category="Escape") float WarningSeconds=5;
    // Keep the 4-second surge; halve the previous 116-second steady rise.
    UPROPERTY(EditAnywhere,Category="Escape") float FloodSeconds=62;
    UPROPERTY(EditAnywhere,Category="Escape") float ComputerSeconds=80;
    UPROPERTY(EditAnywhere,Category="Escape") float TelevisionMineRadius=220;
    UPROPERTY(EditAnywhere,Category="Escape",meta=(ClampMin="0")) float TelevisionGuideRadius=900;
    UPROPERTY(EditAnywhere,Category="Escape") float RetrySeconds=2;
    UPROPERTY(EditAnywhere,Category="Escape|Floating",meta=(ClampMin="0")) float FloatingSinkDelay=1.2f;
    UPROPERTY(EditAnywhere,Category="Escape|Floating",meta=(ClampMin="0.1")) float FloatingSinkSeconds=2.6f;
    UPROPERTY(EditAnywhere,Category="Escape") float WaterStartZ=80;
    UPROPERTY(EditAnywhere,Category="Escape") float WaterEndZ=9000;
    UPROPERTY(EditAnywhere,Category="Escape") float RapidFillSeconds=4;
    UPROPERTY(EditAnywhere,Category="Escape") float TableWaterZ=2800;
    UPROPERTY(EditAnywhere,Category="Escape") FVector Checkpoint=FVector(12060,3600,3100);
    UPROPERTY(EditAnywhere,Category="Escape") FVector ComputerEntry=FVector(23520,11750,6700);
    UPROPERTY(EditAnywhere,Category="Escape") FVector ComputerCheckpointPosition=FVector(22840,11750,6100);
    UPROPERTY(EditAnywhere,Category="Escape") FVector TunnelEnd=FVector(30140,11750,10230);
    UPROPERTY(EditAnywhere,Category="Escape") FVector Exit=FVector(31300,11750,9996);
    UPROPERTY(EditAnywhere,Category="Escape") float TunnelHalfWidth=350;
    UPROPERTY(EditAnywhere,Category="Escape") float TunnelHalfHeight=400;
    UPROPERTY(EditAnywhere,Category="Escape") float ExitRadius=220;
    UPROPERTY(EditAnywhere,Category="Escape") bool bBuildStudyLayout=true;
    UPROPERTY(EditInstanceOnly,Category="Escape|Actors") TObjectPtr<ACardRoomStage> CardStage;
    UPROPERTY(EditInstanceOnly,Category="Escape|Actors") TObjectPtr<AMiningGiantRoomEntrance> Entrance;
    UPROPERTY(EditInstanceOnly,Category="Escape|Actors") TObjectPtr<ADiggableRock> BreachWall;
    UPROPERTY(EditInstanceOnly,Category="Escape|Actors") TArray<TObjectPtr<ADiggableRock>> ComputerRocks;
    UPROPERTY(EditInstanceOnly,Category="Escape|Actors") TObjectPtr<AActor> Water;
    UPROPERTY(VisibleAnywhere,Category="Escape|Actors") TObjectPtr<class UDynamicMeshComponent> JetStream;
    UPROPERTY(EditInstanceOnly,Category="Escape|Actors") TArray<TObjectPtr<AActor>> Foam;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") EMiningEscapePhase Phase=EMiningEscapePhase::Idle;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") float Elapsed=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") float WaterZ=80;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") int32 Attempts=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") bool bWallBroken=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") bool bEnteredComputer=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") bool bExitGuidance=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") bool bComputerCheckpoint=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") float ComputerElapsed=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") int32 ClearedNodes=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") float CheckpointWaterZ=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") float CheckpointElapsed=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") TArray<TObjectPtr<class UStaticMeshComponent>> FloatingProps;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") TArray<float> FloatingSinkElapsed;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") TObjectPtr<class AMiningCollectionEffects> StageCelebration;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Escape|State") TObjectPtr<class AMiningCollectible> StageRibbon;
    UPROPERTY(VisibleAnywhere,Category="Escape|Actors") TObjectPtr<ADiggableRock> TelevisionScreen;
    UPROPERTY(VisibleAnywhere,Category="Escape|Actors") TObjectPtr<class UStaticMeshComponent> ExitRamp;
    FString FailureReason;
    float WarningLeft() const {return FMath::Max(0.f,WarningSeconds-PhaseTime);}
    float ElectricalSecondsLeft() const {return FMath::Max(0.f,ComputerSeconds-ComputerElapsed);}
    FVector GuideTarget() const;
    float ExitDistance() const;
    bool IsInsideComputer(const FVector& Position) const;
    float SecondsLeft() const {return FMath::Max(0.f,FloodSeconds-Elapsed);}
    UFUNCTION(BlueprintCallable,Category="Escape") void RetryEscape();
    void RestoreSaveState(int32 State);
private:
    UFUNCTION() void OnWallImpact(FName RoomId);
    UFUNCTION() void OnRewardClaimed(FName RoomId);
    bool HandleRestart();
    void ResetJourney();
    void StartFlood();
    void Fail(const FString& Reason);
    void ApplyExploration();
    void SetTelevisionMining(bool Enabled);
    void UpdateWater();
    void BuildStudyLayout();
    void BuildExitStage();
    void TickExitStage();
    void InitializeRuntime();
    void TickFloatingProps(float DeltaSeconds);
    void BuildTelevisionModel(bool bEditorPreview);
    void ClearTelevisionPreview();
    void TickPresentation(float DeltaSeconds);
    TArray<float> FloatingOffsets;
    TArray<int32> FloatingPadIndices;
    TArray<float> FloatingTopOffsets;
    TWeakObjectPtr<class AMiningCollectionField> StageCollection;
    bool bOwnsStageCollection=false;
    UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> ScreenMaterial;
    UPROPERTY(Transient) TObjectPtr<class UPointLightComponent> Beacon;
    UPROPERTY(Transient) TArray<TObjectPtr<class UPointLightComponent>> InteriorLights;
    UPROPERTY(Transient) TArray<TObjectPtr<AActor>> GeneratedActors;
    TWeakObjectPtr<AActor> ReplacedWall;
    UPROPERTY(Transient) TObjectPtr<class UStaticMeshComponent> ComputerWater;
    float BreachBurstTime=0;
    float JetUpdateTime=0;
    float PhaseTime=0;
    float ComputerStartWaterZ=0;
    float VictoryHoldTime=0;
    bool bFailureLock=false;
    bool bRuntimeInitialized=false;
    TWeakObjectPtr<class UMiningComponent> ScopedMiner;
    float SavedMineRadius=0;
    TWeakObjectPtr<AMiningEncounterPlayerController> Player;
    TSharedPtr<SMiningEscapeView> View;
};
