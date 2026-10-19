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
    bool ToggleMenu();
    bool ToggleDeveloperMenu();
    UFUNCTION(BlueprintCallable) bool TogglePauseMenu();
    UFUNCTION(BlueprintCallable) bool JumpToCollectible(int32 ItemId);
    UFUNCTION(BlueprintCallable) void OpenPauseCollection();
    UFUNCTION(BlueprintCallable) void RequestQuitConfirmation();
    UFUNCTION(BlueprintCallable) void CancelQuitConfirmation();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bQuitConfirmation=false;
    bool JumpToDeveloperArea(int32 Index);
    bool OpenDeveloperExperience(int32 Index);
    static TArray<FString> DeveloperMapPaths();
    void CloseMenu();
    bool IsMenuOpen() const;
    const FString& GetMenuError() const { return MenuError; }
    bool OpenExperience(int32 Index);
    static TArray<FString> MapPaths();
private:
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
