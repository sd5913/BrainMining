#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MiningExperienceSubsystem.generated.h"

class IInputProcessor;
class SWidget;
class APlayerController;
class APawn;
class UMiningComponent;

UCLASS()
class BRAINMINING_API UMiningExperienceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly) TArray<int32> CampaignCollection;
    UPROPERTY(Transient) FVector CampaignToiletScale=FVector(.491739f);
    UPROPERTY(Transient) TObjectPtr<class UStaticMesh> CampaignToiletBody;
    UPROPERTY(Transient) TObjectPtr<class UStaticMesh> CampaignToiletLid;
    UPROPERTY(Transient) TArray<TObjectPtr<class UMaterialInterface>> CampaignToiletBodyMaterials;
    UPROPERTY(Transient) TArray<TObjectPtr<class UMaterialInterface>> CampaignToiletLidMaterials;
    bool ToggleMenu();
    UFUNCTION(BlueprintCallable) bool ToggleDeveloperMenu();
    UFUNCTION(BlueprintCallable) void ToggleAllUI();
    UFUNCTION(BlueprintCallable) void ReturnToMainMenu();
    bool bAllUIHidden=false;
    UFUNCTION(BlueprintCallable) bool DeveloperGrantTool(int32 ToolIndex);
    UFUNCTION(BlueprintCallable) bool DeveloperUnlockMineral(int32 ItemId);
    UFUNCTION(BlueprintCallable) bool TogglePauseMenu(bool bFromCredits=false);
    bool ToggleMobileMenu(bool bUpgrade = false);
    UFUNCTION(BlueprintCallable) bool JumpToCollectible(int32 ItemId);
    UFUNCTION(BlueprintCallable) void OpenPauseCollection();
    UFUNCTION(BlueprintCallable) void RequestQuitConfirmation();
    UFUNCTION(BlueprintCallable) void CancelQuitConfirmation();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bQuitConfirmation=false;
    bool JumpToDeveloperArea(int32 Index);
    int32 PendingDeveloperArea=INDEX_NONE;
    void FinishDeveloperTravel();
    bool OpenDeveloperExperience(int32 Index);
    static TArray<FString> DeveloperMapPaths();
    void CloseMenu();
    bool IsMenuOpen() const;
    const FString& GetMenuError() const { return MenuError; }
    bool OpenExperience(int32 Index);
    static TArray<FString> MapPaths();
private:
    friend class FMiningMobileLifecycleTest;
    bool BuildMenu(bool bDeveloper);
    void CaptureMenuInput(APlayerController* Controller);
    void RestoreMenuInput(APlayerController* Controller);
    void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
    FDelegateHandle WorldCleanupHandle;
    TWeakObjectPtr<UWorld> MenuWorld;
    FString MenuError;
    bool bDeveloperMenu = false;
    bool bPauseMenu = false;
    TSharedPtr<IInputProcessor> Shortcuts;
    TSharedPtr<SWidget> Menu;
    TWeakPtr<SWidget> PreviousMenuFocus;
    TSharedPtr<SWidget> ResumeButton;
    TSharedPtr<SWidget> CancelQuitButton;
    TWeakObjectPtr<APawn> MenuPawn;
    EMouseCaptureMode PreviousMenuCapture = EMouseCaptureMode::CapturePermanently;
    EMouseLockMode PreviousMenuLock = EMouseLockMode::LockOnCapture;
    bool bPreviousMenuCursor = false;
    bool bPreviousMenuUIOnly = false;
    bool bPreviousMenuGameAndUI = false;
    bool bPreviousMenuHideCursor = true;
    TWeakObjectPtr<APlayerController> MenuController;
    TWeakObjectPtr<UMiningComponent> MenuMining;
    bool bPreviousMiningSuspended = false;
    bool bPausedByMenu = false;
};
