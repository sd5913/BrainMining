#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MiningExperienceSubsystem.generated.h"

class IInputProcessor;
class SWidget;
class APlayerController;
class UMiningComponent;

UCLASS()
class BRAINMINING_API UMiningExperienceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    bool ToggleMenu();
    bool ToggleDeveloperMenu();
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
    void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
    FDelegateHandle WorldCleanupHandle;
    TWeakObjectPtr<UWorld> MenuWorld;
    FString MenuError;
    bool bDeveloperMenu = false;
    TSharedPtr<IInputProcessor> Shortcuts;
    TSharedPtr<SWidget> Menu;
    TWeakObjectPtr<APlayerController> MenuController;
    TWeakObjectPtr<UMiningComponent> MenuMining;
    bool bPreviousMiningSuspended = false;
    bool bPausedByMenu = false;
};
