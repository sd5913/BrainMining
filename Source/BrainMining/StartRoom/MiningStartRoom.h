#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Showcase/MiningExhibitHall.h"
#include "MiningStartRoom.generated.h"

class UBoxComponent;
class UMiningComponent;
class UMiningCharacterMovementComponent;
class UMiningToolEffectsComponent;

UENUM(BlueprintType)
enum class EMiningStarterEquipment : uint8 { Pickaxe, Jetpack };

/** An editable pickup; visual children can be replaced without changing interaction. */
UCLASS()
class BRAINMINING_API AMiningStarterPickup : public AActor
{
    GENERATED_BODY()
public:
    AMiningStarterPickup();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> InteractionBox;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Equipment") EMiningStarterEquipment Equipment = EMiningStarterEquipment::Pickaxe;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment") bool bCollected = false;
    void Collect();
};

/** Owns only this level's introductory locks. Uses the existing nested encounter blocks. */
UCLASS()
class BRAINMINING_API AMiningStartRoom : public AActor
{
    GENERATED_BODY()
public:
    AMiningStartRoom();
    static AMiningStartRoom* Find(UWorld* World);
    void InitializePlayer(APawn* Pawn);
    UFUNCTION(BlueprintCallable, Category="Start Room") bool Interact(APlayerController* Controller);
    UFUNCTION(BlueprintPure, Category="Start Room") AMiningStarterPickup* GetTarget(APlayerController* Controller) const;
    FString GetInteractionAction(APlayerController* Controller) const;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Start Room", meta=(ClampMin="50",ClampMax="500")) float InteractionReach = 260.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Start Room") bool bPickaxeUnlocked = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Start Room") bool bJetpackUnlocked = false;
private:
    void ReleasePlayer();
    TWeakObjectPtr<APawn> Player;
    TWeakObjectPtr<UMiningComponent> Mining;
    TWeakObjectPtr<UMiningCharacterMovementComponent> Flight;
    TWeakObjectPtr<UMiningToolEffectsComponent> Effects;
    bool bOwnMiningBlock = false;
    bool bOwnFlightBlock = false;
    bool bPreviousEffectsEnabled = true;
};

UCLASS()
class BRAINMINING_API AMiningStartRoomController : public AMiningExhibitController
{
    GENERATED_BODY()
public:
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;
protected:
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
};

UCLASS()
class BRAINMINING_API AMiningStartRoomGameMode : public AMiningExhibitGameMode
{
    GENERATED_BODY()
public:
    AMiningStartRoomGameMode();
};
