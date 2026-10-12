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
class ULevelStreaming;

UENUM(BlueprintType)
enum class EMiningExperienceKind : uint8 { Legacy, Mining, Collection, Journey, Office, Museum, Ending };

USTRUCT(BlueprintType)
struct FMiningExperienceArea
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Title;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Instructions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Tag;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Origin = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform Entry;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EMiningExperienceKind Kind = EMiningExperienceKind::Legacy;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Exit = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector ExitExtent = FVector(180,300,300);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation") TArray<FVector> MapRoute;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AActor> ProgressGate;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FBox MiningBounds = FBox(ForceInit);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FBox ActivationBounds = FBox(ForceInit);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Source Room") TObjectPtr<ULevelStreaming> SourceLevel;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Source Room") TArray<FName> SuppressedSourceActors;
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
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign") bool bLinearCampaign = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Campaign") bool bContinuousCampaign = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Campaign") int32 UnlockedArea = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Campaign") bool bCampaignComplete = false;
    EMiningExperienceKind AreaKind(int32 Index) const;
    bool CanAdvanceCampaign() const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ActiveArea = INDEX_NONE;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bReady = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<AMiningCollectionField> Collection;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<AMiningTreasureField> Treasure;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<AMiningRoomEscape> Journey;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<AMiningCellUniverse> Ending;
    UFUNCTION(BlueprintCallable) bool EnterArea(int32 Index, bool bMovePlayer = true);
    UFUNCTION(BlueprintCallable) void ReturnToLobby();
    UFUNCTION(BlueprintCallable) bool SkipCurrentRoom();
    UFUNCTION(BlueprintCallable) bool PurchaseUpgrade(int32 ItemIndex, int32 RequestedLevel);
    bool ToggleMenu();
    bool ToggleMap();
    void CloseMap();
    bool IsMapOpen() const { return Map.IsValid(); }
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
    TSet<int32> CompletedAreas;
    TWeakObjectPtr<AMiningExhibitController> Controller;
    TWeakObjectPtr<UMiningGUIHUDWidget> BoundGUI;
    UPROPERTY() TObjectPtr<APopupCloseStage> Office;
    UPROPERTY() TObjectPtr<AWenSiPuzzle> Museum;
    UPROPERTY() TObjectPtr<AMiningGiantRoomEntrance> Entrance;
    TSharedPtr<SWidget> Menu;
    TSharedPtr<SWidget> Map;
    TSharedPtr<SWidget> Guide;
    FDelegateHandle SpawnHandle;
    FString Notice;
    float CompletionDelay = 0;
    bool InitializeAreas();
    void RegisterActor(AActor* Actor);
    void UpdateActorVisibility();
    void CloseInteraction();
    void ApplyUpgrades();
    UFUNCTION() void OnUpgradeRequested(int32 ItemIndex, int32 RequestedLevel);
};
