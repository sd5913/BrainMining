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
    FTransform GetResumeTransform() const;
    UCardRoomInteractionComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaSeconds,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
    void BindInput(UInputComponent* Input);
    void SetRoomActive(bool bActive);
    void ReleaseForExploration();
    bool bRoomActive=true;
    bool IsSeated() const { return bSeated; }
    bool bUseSharedInteractionHUD=false;
    FString ExplorationHint;
    UFUNCTION(BlueprintCallable) void Interact();
    UFUNCTION(BlueprintCallable) void LeaveSeat();
    UFUNCTION(BlueprintCallable) void ConfirmCards();
    UFUNCTION(BlueprintCallable) void ToggleCardSelection(int32 Index);
    void NavigateCard(int32 Direction);
    int32 GetFocusedCardIndex() const;
    bool HasCardInputFocus(int32 UserIndex) const;
    void SelectAllCards();
    void ClearCardSelection();
    bool bGamepadInput = false;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<ACardRoomStage> Stage;
    APawn* GetPawn() const;
private:
    friend class FCardRoomTouchTest;
    APlayerController* Player() const;
    void RestoreSeat();
    TSharedPtr<SCardRoomView> View;
    TWeakObjectPtr<APawn> SeatedPawn;
    FVector EntryLocation;
    FRotator EntryRotation;
    uint8 EntryMovementMode=0,EntryCustomMode=0;
    bool bSeated=false,bEntryCollision=true;
    int32 FocusedCard = 0, FocusedRound = INDEX_NONE;
};
UCLASS()
class CARDROOM_API ACardRoomGameMode : public AGameModeBase { GENERATED_BODY() };
