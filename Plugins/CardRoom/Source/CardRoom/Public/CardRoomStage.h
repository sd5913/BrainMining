#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CardRoomRun.h"
#include "CardRoomStage.generated.h"
class UStaticMeshComponent;
class USceneComponent;
class UStaticMesh;
class UPointLightComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCardRoomCompleted, FName, RoomId);

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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Card Room") FName RoomId = TEXT("CardDuel_P1");
    // Integration hooks only: listeners decide what a win or collectible means in the main game.
    UPROPERTY(BlueprintAssignable, Category="Card Room") FCardRoomCompleted OnRoomCompleted;
    UPROPERTY(BlueprintAssignable, Category="Card Room") FCardRoomCompleted OnOpponentWallImpact;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Card Room|Presentation") FVector OpponentExitLocal=FVector(620,200,472);
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
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Card Room|Presentation") FVector RewardLocal=FVector(-30,0,143);
    FVector GetCardLanding(int32 Index,int32 Count) const;
    FVector GetSeatEyeLocation() const {return GetActorTransform().TransformPosition(FVector(-270,0,188));}
    FVector GetSeatLocation() const { return GetActorTransform().TransformPosition(FVector(-270,0,100)); }
    FVector GetApproachLocation() const { return GetActorTransform().TransformPosition(bTabletopEntry?FVector(-270,0,115):FVector(-295,0,0)); }
    bool IsPlayerNear(const FVector& Position) const;
private:
    UPROPERTY() TObjectPtr<USceneComponent> ToyRoot;
    UPROPERTY() TObjectPtr<USceneComponent> HeadPivot;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> ToyParts;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> GhostDots;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Puffs;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Trails;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Beam;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Ball;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Reward;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> PlayedCards;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> CardMarks;
    UPROPERTY() TObjectPtr<UPointLightComponent> SpellLight;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> FxShards;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> SymbolMeshes;
    void InitializeEffectMeshes();
    void TickEffects();
    ECardRoomPhase LastPhase = ECardRoomPhase::Exploring;
    bool bReplySound = false;
    bool bWinSent = false;
    bool bImpactSent = false;
    UStaticMeshComponent* Shape(FName Name, USceneComponent* Parent, const TCHAR* MeshPath, FVector Position, FVector Scale);
    void Tint(UStaticMeshComponent* Part, FLinearColor Color, bool bGlow=false);
    void Sound(const TCHAR* Name, float Volume=0.65f);
};