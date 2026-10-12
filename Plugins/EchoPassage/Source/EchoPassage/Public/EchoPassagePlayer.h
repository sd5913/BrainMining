#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/GameModeBase.h"
#include "EchoPassagePlayer.generated.h"
class ACharacter;
class APlayerController;
class UInputComponent;
class USceneComponent;
class AEchoPassageStage;
class SEchoPassageView;

DECLARE_DELEGATE_RetVal(bool, FEchoRestartOverride);

// Attach to the host player controller; movement, camera and character remain host-owned.
UCLASS(ClassGroup=(Encounters), meta=(BlueprintSpawnableComponent))
class ECHOPASSAGE_API UEchoPassageInteractionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UEchoPassageInteractionComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaSeconds,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
    void BindInput(UInputComponent* Input);
    void SetExplorationActive(bool bActive);
    FSimpleMulticastDelegate OnRestarted;
    FSimpleMulticastDelegate OnKnock;
    bool bUseSharedPickaxe = false;
    FEchoRestartOverride RestartOverride;
    bool bExplorationActive=true;
    bool bUseSharedInteractionHUD=false;
    UFUNCTION(BlueprintCallable) void Knock();
    UFUNCTION(BlueprintCallable) void Restart();
    UFUNCTION() void OnLanded(const FHitResult& Hit);
    void TravelToDestination();
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AEchoPassageStage> Stage;
    bool bChangingLevel=false;
    FString Feedback;
    float FeedbackUntil=0;
    bool bPendingStrike=false;
    float StrikeAt=0;
    FVector PendingPoint;
    APawn* GetPawn() const;
private:
    APlayerController* Player() const;
    void RefreshCharacter();
    void ReleaseCharacter();
    void LockMovement(bool bLock);
    TWeakObjectPtr<ACharacter> Character;
    UPROPERTY(Transient) TObjectPtr<USceneComponent> Pickaxe;
    TSharedPtr<SEchoPassageView> View;
    FTimerHandle TravelTimer;
    float SwingTime=10;
    bool bWallStrike=false,bMovementLocked=false,bLookLocked=false;
};
// Kept so older maps can load before Scripts/integrate_encounter_player.py migrates them.
UCLASS()
class ECHOPASSAGE_API AEchoPassageGameMode : public AGameModeBase { GENERATED_BODY() };
