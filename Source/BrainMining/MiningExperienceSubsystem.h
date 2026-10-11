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
    void CloseMenu();
    bool IsMenuOpen() const;
    bool OpenExperience(int32 Index);
    static TArray<FString> MapPaths();
private:
    TSharedPtr<IInputProcessor> Shortcuts;
    TSharedPtr<SWidget> Menu;
    TWeakObjectPtr<APlayerController> MenuController;
    TWeakObjectPtr<UMiningComponent> MenuMining;
    bool bPreviousMiningSuspended = false;
    bool bPausedByMenu = false;
};
