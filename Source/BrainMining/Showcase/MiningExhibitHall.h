#pragma once
#include "CoreMinimal.h"
#include "MiningEncounterPlayer.h"
#include "GameFramework/Actor.h"
#include "MiningExhibitHall.generated.h"

class AMiningCollectionField;
class AMiningTreasureField;
class AMiningRoomEscape;
class AMiningCellUniverse;
class APopupCloseStage;
class AWenSiPuzzle;
class AMiningGiantRoomEntrance;
class UMiningGUIHUDWidget;
class UPostProcessComponent;
class SWidget;

USTRUCT(BlueprintType)
struct FMiningExperienceArea
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Title;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Instructions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Tag;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Origin = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform Entry;
};

UCLASS()
class BRAINMINING_API AMiningExhibitController : public AMiningEncounterPlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;
};

UCLASS()
class BRAINMINING_API AMiningExhibitGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AMiningExhibitGameMode();
};

UCLASS()
class BRAINMINING_API AMiningExhibitHall : public AActor
{
    GENERATED_BODY()
public:
    AMiningExhibitHall();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMiningExperienceArea> Areas;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform LobbyEntry;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<int32> UpgradePrices = {100, 200, 300};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ActiveArea = INDEX_NONE;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bReady = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<AMiningCollectionField> Collection;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<AMiningTreasureField> Treasure;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<AMiningRoomEscape> Journey;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<AMiningCellUniverse> Ending;
    UFUNCTION(BlueprintCallable) bool EnterArea(int32 Index);
    UFUNCTION(BlueprintCallable) void ReturnToLobby();
    UFUNCTION(BlueprintCallable) bool PurchaseUpgrade(int32 ItemIndex, int32 RequestedLevel);
    bool ToggleMenu();
    bool IsMenuOpen() const { return Menu.IsValid(); }
    void CloseMenu();
    void ToggleCatalogue();
    void InteractPortal();
    bool IsInteractionOpen() const;
    FString StatusText() const;
    static AMiningExhibitHall* Find(UWorld* World);
private:
    struct FAreaActor
    {
        TWeakObjectPtr<AActor> Actor;
        int32 Area = INDEX_NONE;
        bool bHidden = false;
        bool bTick = false;
        bool bSuspended = false;
        TArray<TPair<TWeakObjectPtr<UPostProcessComponent>, bool>> PostProcesses;
        bool bVolumeEnabled = false;
    };
    TArray<FAreaActor> ManagedActors;
    TArray<TWeakObjectPtr<AActor>> PendingActors;
    TSet<TWeakObjectPtr<AActor>> KnownActors;
    TArray<FTransform> ResumePoints;
    TSet<int32> Visited;
    TWeakObjectPtr<AMiningExhibitController> Controller;
    TWeakObjectPtr<UMiningGUIHUDWidget> BoundGUI;
    UPROPERTY() TObjectPtr<APopupCloseStage> Office;
    UPROPERTY() TObjectPtr<AWenSiPuzzle> Museum;
    UPROPERTY() TObjectPtr<AMiningGiantRoomEntrance> Entrance;
    TSharedPtr<SWidget> Menu;
    TSharedPtr<SWidget> Guide;
    FDelegateHandle SpawnHandle;
    float BaseRadius = 160;
    float BaseInterval = .3f;
    FString Notice;
    bool InitializeAreas();
    void RegisterActor(AActor* Actor);
    void UpdateActorVisibility();
    void CloseInteraction();
    void ApplyUpgrades();
    UFUNCTION() void OnUpgradeRequested(int32 ItemIndex, int32 RequestedLevel);
};
