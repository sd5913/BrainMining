#pragma once
#include "CoreMinimal.h"
#include "BrainMiningPlayerController.h"
#include "Components/ActorComponent.h"
#include "GameFramework/GameModeBase.h"
#include "MiningEncounterPlayer.generated.h"
class UMiningComponent;
class UMiningCharacterMovementComponent;
class ULightComponent;
class UEchoPassageInteractionComponent;
class UCardRoomInteractionComponent;

// Runtime-only scope. No Blueprint defaults or mining-generator settings are changed.
UCLASS(ClassGroup=(Encounters),meta=(BlueprintSpawnableComponent))
class BRAINMINING_API UMiningEncounterPlayerPolicy : public UActorComponent
{
    GENERATED_BODY()
public:
    void Apply(APawn* Pawn);
    void Release();
    void SetMiningAllowed(bool bAllowed);
    bool IsMiningAllowed() const {return !bMiningBlocked;}
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool bMiningBlocked=false,bOldInputContextEnabled=true;
    bool bFallResetOverridden=false;
    float OldFallResetDistance=0.f;
    TWeakObjectPtr<APawn> AppliedPawn;
    TWeakObjectPtr<UMiningComponent> Mining;
    TWeakObjectPtr<UMiningCharacterMovementComponent> Movement;
    TArray<TPair<TWeakObjectPtr<ULightComponent>,bool>> Lights;
};

UCLASS()
class BRAINMINING_API AMiningEncounterPlayerController : public ABrainMiningPlayerController
{
    GENERATED_BODY()
public:
    AMiningEncounterPlayerController();
    virtual void BeginPlay() override;
    bool IsCardRoomContext() const;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;
    void MinePressed();
    void MineReleased();
    virtual void SetupInputComponent() override;
    virtual void OnPossess(APawn* Pawn) override;
    virtual void OnUnPossess() override;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UEchoPassageInteractionComponent> EchoInteraction;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCardRoomInteractionComponent> CardInteraction;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UMiningEncounterPlayerPolicy> EncounterPolicy;
};

// Both independent encounters use the exact same project Blueprint pawn.
UCLASS()
class BRAINMINING_API AMiningEncounterGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AMiningEncounterGameMode();
    virtual void StartPlay() override;
};
