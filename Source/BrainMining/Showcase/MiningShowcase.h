#pragma once

#include "CoreMinimal.h"
#include "BrainMiningPlayerController.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "Blueprint/UserWidget.h"
#include "MiningShowcase.generated.h"

class ULevelStreamingDynamic;
class UCameraComponent;
class UMiningEncounterPlayerPolicy;
class UEchoPassageInteractionComponent;
class UCardRoomInteractionComponent;
class IInputProcessor;
class AMiningShowcaseDirector;

UENUM(BlueprintType)
enum class EMiningShowcaseKind : uint8 { Mining, Collection, Echo, Cards, Popup, WenSi };

USTRUCT(BlueprintType)
struct FMiningShowcaseExhibit
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Title;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Instructions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UWorld> Level;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EMiningShowcaseKind Kind = EMiningShowcaseKind::Mining;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Color = FLinearColor(0.1f, 0.8f, 0.7f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SubtitleId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Center = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Extent = FVector(1000);
};

UCLASS()
class BRAINMINING_API UMiningShowcaseWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    TWeakObjectPtr<AMiningShowcaseDirector> Director;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};

UCLASS()
class BRAINMINING_API AMiningShowcaseController : public ABrainMiningPlayerController
{
    GENERATED_BODY()
public:
    AMiningShowcaseController();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void SetupInputComponent() override;
    AMiningShowcaseDirector* GetDirector() const;
    void ToggleShowcaseMenu();
    void ReturnToShowcase();
    void RestartExhibit();
private:
    void InteractExhibit();
    void KnockExhibit();
    void ResetEcho();
    void LeaveCards();
    void ConfirmCards();
    TSharedPtr<IInputProcessor> ShortcutProcessor;
};

UCLASS()
class BRAINMINING_API AMiningShowcaseGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AMiningShowcaseGameMode();
};

UCLASS()
class BRAINMINING_API AMiningShowcaseDirector : public AActor
{
    GENERATED_BODY()
public:
    AMiningShowcaseDirector();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Showcase") bool bContinuousLayout = false;
    bool bGuideVisible = true;
    int32 FindRegion(const FVector& Position) const;
    void CloseContinuousInteraction();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Showcase") TArray<FMiningShowcaseExhibit> Exhibits;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Showcase") int32 ActiveIndex = INDEX_NONE;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Showcase") bool bBusy = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Showcase") bool bMenuOpen = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Showcase") FText Status;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Showcase") TSet<FName> Visited;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Showcase") TSet<FName> Completed;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> LobbyCamera;
    UPROPERTY(Transient) TObjectPtr<UEchoPassageInteractionComponent> EchoInteraction;
    UPROPERTY(Transient) TObjectPtr<UCardRoomInteractionComponent> CardInteraction;
    UFUNCTION(BlueprintCallable, Category="Showcase") bool EnterExhibit(int32 Index);
    UFUNCTION(BlueprintCallable, Category="Showcase") void ReturnToLobby();
    UFUNCTION(BlueprintCallable, Category="Showcase") void RestartCurrent();
    UFUNCTION(BlueprintCallable, Category="Showcase") void ToggleMenu();
    UFUNCTION(BlueprintCallable, Category="Showcase") void InteractPortal();
    UFUNCTION(BlueprintPure, Category="Showcase") int32 GetNearbyExhibit() const;
    FVector GetPortalPosition(int32 Index) const;
    void SetMenuOpen(bool bOpen);
    void ReportCompletion(FName Id);
    EMiningShowcaseKind GetActiveKind() const;
    bool IsModalActive() const;
private:
    enum class ETransition : uint8 { None, Unloading, Loading };
    ETransition Transition = ETransition::None;
    int32 PendingIndex = INDEX_NONE;
    int32 InstanceSerial = 0;
    double TransitionStarted = 0;
    float ProgressPoll = 0;
    bool bMenuLocked = false;
    bool bEchoTransferPending = false;
    bool bModalWasActive = false;
    bool bContinuousReady = false;
    FVector LastContinuousSafePosition = FVector::ZeroVector;
    void UpdateContinuousRegion();
    void SwitchContinuousRegion(int32 Index);
    void RestartContinuousRegion();
    UPROPERTY(Transient) TObjectPtr<ULevelStreamingDynamic> Streaming;
    UPROPERTY(Transient) TObjectPtr<UMiningEncounterPlayerPolicy> Policy;
    UPROPERTY(Transient) TObjectPtr<UMiningShowcaseWidget> Widget;
    TArray<TWeakObjectPtr<AActor>> SessionActors;
    FDelegateHandle SpawnHandle;
    AMiningShowcaseController* Player() const;
    void BeginTransition(int32 Index);
    void BeginLoad();
    void ActivateLoadedExhibit();
    void ReleaseExhibit();
    void FinishLobby();
    bool SpawnPlayer(const FTransform& Transform);
    void ResetController();
    void CleanupSessionActors();
    void OnSessionActorSpawned(AActor* Actor);
    UFUNCTION() void OnEchoCompleted(FName Id);
    UFUNCTION() void OnCardCompleted(FName Id);
    UFUNCTION() void OnCardReward(FName Id);
};
