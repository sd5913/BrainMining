#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CardRoomRun.h"
#include "CardRoomStage.generated.h"
class UStaticMeshComponent;
class USkeletalMeshComponent;
class USceneComponent;
class UStaticMesh;
class USkeletalMesh;
class UCardRoomUISettings;
class UPointLightComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCardRoomCompleted, FName, RoomId);
DECLARE_DELEGATE_TwoParams(FCardRoomAudioOverride, FName, float);
DECLARE_DELEGATE_TwoParams(FCardRoomSubtitleOverride, const FString&, float);

UCLASS()
class CARDROOM_API ACardRoomStage : public AActor
{
    GENERATED_BODY()
public:
    ACardRoomStage();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void BeginPlay() override;
    FCardRoomRun Run;
    FCardRoomAudioOverride AudioOverride;
    FCardRoomSubtitleOverride SubtitleOverride;
    void Sound(const TCHAR* Name, float Volume=0.65f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Card Room") FName RoomId = TEXT("CardDuel_P1");
    // Integration hooks only: listeners decide what a win or collectible means in the main game.
    UPROPERTY(BlueprintAssignable, Category="Card Room") FCardRoomCompleted OnRoomCompleted;
    UPROPERTY(BlueprintAssignable, Category="Card Room") FCardRoomCompleted OnOpponentWallImpact;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Card Room|Presentation") FVector OpponentExitLocal=FVector(620,200,472);
    // Optional model-space visual anchor. Zero preserves standalone room presentation.
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Card Room|Presentation") FVector OpponentExitCenterOffset=FVector::ZeroVector;
    UPROPERTY(BlueprintAssignable, Category="Card Room") FCardRoomCompleted OnCollectibleClaimed;
    UFUNCTION(BlueprintCallable, Category="Card Room") bool BeginEncounter();
    UFUNCTION(BlueprintCallable, Category="Card Room") bool CollectReward();
    UFUNCTION(BlueprintCallable, Category="Card Room") void RestartEncounter();
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Card Room") bool bTabletopEntry=false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Card Room|Presentation") float OpponentScale=1.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Card Room|Presentation") float SeatPitch=-9.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Card Room|Presentation") FVector OpponentBaseLocal=FVector(180,0,112);
    // Local presentation anchors: the giant encounter configures these without changing card rules.
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Card Room|Presentation") FVector CardLandingLocal=FVector(-10,0,116);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Card Room|Presentation") float CardLandingPitch=0.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Card Room|Presentation") float CardThrowHeight=48.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Card Room|Presentation") float CardVisualScale=1.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Card Room|Presentation") float SpellPresentationScale=1.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Card Room|UI") TObjectPtr<UCardRoomUISettings> UISettings;
    // Art-only override. Both study maps continue to use the same FCardRoomRun.
    UPROPERTY(EditAnywhere, Category="Card Room|Presentation") bool bStyledEffects=false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Card Room|Presentation") bool bUseRobotOpponent=false;
    UPROPERTY(EditAnywhere, Category="Card Room|Presentation") TArray<TObjectPtr<class UNiagaraSystem>> StyledSystems;
    UPROPERTY(EditAnywhere, Category="Card Room|Presentation") TObjectPtr<class UMaterialInterface> StyledEnergyMaterial;
    UPROPERTY(EditAnywhere, Category="Card Room|Presentation") TObjectPtr<class UMaterialInterface> StyledRingMaterial;
    UPROPERTY(EditAnywhere, Category="Card Room|Presentation") TMap<FName,TObjectPtr<UStaticMesh>> StyledToyMeshes;
    UPROPERTY(EditAnywhere, Category="Card Room|Presentation") TMap<FName,TObjectPtr<class UMaterialInterface>> StyledToyMaterials;
    // Optional P2 character replacement. When assigned, the authored static toy is hidden
    // and this skeletal mesh drives the encounter presentation and final drop.
    UPROPERTY(EditAnywhere, Category="Card Room|Presentation|Rabbit") TObjectPtr<USkeletalMesh> RabbitSkeletalMesh;
    UPROPERTY(EditAnywhere, Category="Card Room|Presentation|Rabbit", meta=(ClampMin="0.01")) float RabbitVisualScale=1.f;
    UPROPERTY(EditAnywhere, Category="Card Room|Presentation|Rabbit") FVector RabbitVisualOffset=FVector::ZeroVector;
    UPROPERTY(EditAnywhere, Category="Card Room|Presentation|Rabbit") FVector RabbitLandingLocal=FVector(1700.f,0.f,-420.f);
    UPROPERTY(EditAnywhere, Category="Card Room|Presentation|Rabbit") bool bRabbitPhysicsDrop=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Card Room|Presentation") FVector RewardLocal=FVector(-30,0,143);
    FVector GetCardLanding(int32 Index,int32 Count) const;
    FVector GetSeatEyeLocation() const {return GetActorTransform().TransformPosition(FVector(-270,0,188));}
    FVector GetSeatLocation() const { return GetActorTransform().TransformPosition(FVector(-270,0,100)); }
    FVector GetApproachLocation() const { return GetActorTransform().TransformPosition(bTabletopEntry?FVector(-75,0,115):FVector(-295,0,0)); }
    float GetApproachRadius() const { return bTabletopEntry?800.f:180.f; }
    bool IsPlayerNear(const FVector& Position) const;
private:
    UPROPERTY() TObjectPtr<USceneComponent> ToyRoot;
    UPROPERTY() TObjectPtr<class UChildActorComponent> RobotOpponent;
    UPROPERTY() TObjectPtr<USceneComponent> HeadPivot;
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> RabbitVisual;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> ToyParts;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> GhostDots;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Puffs;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Trails;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Beam;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Ball;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Reward;
    UPROPERTY(Transient) TObjectPtr<class UPostProcessComponent> RewardOutline;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> PlayedCards;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> CardMarks;
    UPROPERTY() TObjectPtr<UPointLightComponent> SpellLight;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> FxShards;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> SymbolMeshes;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> CardFaceMaterials;
    void InitializeEffectMeshes();
    void TickEffects();
    void InitializeStyledEffects();
    void TickStyledEffects();
    void TriggerRabbitPhysicsDrop();
    void ResetRabbitPhysicsDrop();
    void TickRabbitExpression(float Time, float DeltaSeconds);
    UPROPERTY(Transient) TArray<TObjectPtr<class UNiagaraComponent>> StyledFX;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> StyledRings;
    uint32 StyledFired=0;
    int32 StyledRound=0;
    float StyledLastTime=0;
    bool bStyledPoisonLive=false;
    bool bRabbitPhysicsDropped=false;
    bool bRabbitPhysicsSettled=false;
    FTransform RabbitInitialRelativeTransform;
    ECardRoomPhase LastPhase = ECardRoomPhase::Exploring;
    bool bReplySound = false;
    float PreviousAudioTime = 0;
    bool bWinSent = false;
    bool bImpactSent = false;
    UStaticMeshComponent* Shape(FName Name, USceneComponent* Parent, const TCHAR* MeshPath, FVector Position, FVector Scale);
    void Tint(UStaticMeshComponent* Part, FLinearColor Color, bool bGlow=false);
};
