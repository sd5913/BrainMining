#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/GameModeBase.h"
#include "CardRoomPlayer.generated.h"
class APlayerController;
class UInputComponent;
class ACardRoomStage;
class SCardRoomView;
UCLASS(ClassGroup=(Encounters), meta=(BlueprintSpawnableComponent))
class CARDROOM_API UCardRoomInteractionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCardRoomInteractionComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaSeconds,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
    void BindInput(UInputComponent* Input);
    void SetRoomActive(bool bActive);
    void ReleaseForExploration();
    bool bRoomActive=true;
    bool bUseSharedInteractionHUD=false;
    FString ExplorationHint;
    UFUNCTION(BlueprintCallable) void Interact();
    UFUNCTION(BlueprintCallable) void LeaveSeat();
    UFUNCTION(BlueprintCallable) void ConfirmCards();
    UFUNCTION(BlueprintCallable) void ToggleCardSelection(int32 Index);
    UPROPERTY(BlueprintReadOnly) TObjectPtr<ACardRoomStage> Stage;
    APawn* GetPawn() const;
private:
    APlayerController* Player() const;
    void RestoreSeat();
    TSharedPtr<SCardRoomView> View;
    TWeakObjectPtr<APawn> SeatedPawn;
    FVector EntryLocation;
    FRotator EntryRotation;
    uint8 EntryMovementMode=0,EntryCustomMode=0;
    bool bSeated=false,bEntryCollision=true;
};
UCLASS()
class CARDROOM_API ACardRoomGameMode : public AGameModeBase { GENERATED_BODY() };
