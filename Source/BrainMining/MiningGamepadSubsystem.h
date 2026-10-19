#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "InputCoreTypes.h"
#include "MiningGamepadSubsystem.generated.h"

class IInputProcessor;
class SWidget;
class UMiningComponent;
class APlayerController;
class APawn;
class APopupCloseStage;
struct FMiningGamepadHint;

enum class EMiningInputAction : uint8
{
    Primary, Jump, Interact, Back, Tool, Bomb, ClearThought,
    Catalogue, Upgrade, Pause, Reset, Previous, Next, Up, Down
};

enum class EMiningInputContext : uint8
{
    Exploration, Cards, Office, Thoughts, Catalogue, Menu, Blocked
};

UCLASS(Config=Game)
class BRAINMINING_API UMiningGamepadSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    static UMiningGamepadSubsystem* Find(const APlayerController* Controller);
    static FVector2D ReadMove(const APlayerController* Controller);
    static bool ReadJumpHeld(const APlayerController* Controller);
    static bool ReadDescendHeld(const APlayerController* Controller);
    static bool ReadJumpPressed(const APlayerController* Controller);
    static bool UsesGamepad(const APlayerController* Controller);
    static bool MapButton(FKey Key, EMiningInputAction& Action);
    static FVector2D ApplyDeadZone(FVector2D Axis, float DeadZone);
    bool HandleButton(FKey Key, bool bPressed, bool bRepeat = false);
    bool HandleAxis(FKey Key, float Value);
    void Dispatch(EMiningInputAction Action, bool bPressed);
    void TickInput(float DeltaSeconds);
    void ReleaseInputs();
    void MarkKeyboardMouse();
    void ResetDevice();
    void SendUIKey(FKey Key, bool bPressed);
    bool CanReceiveInput(int32 UserIndex) const;
    EMiningInputContext GetContext() const;
    FString GetHint() const;
    TArray<FMiningGamepadHint> GetHintEntries() const;
    bool IsSyntheticPointer() const { return bSyntheticPointer; }
    bool IsSyntheticCursorPosition(FVector2D Position) const { return bHasSyntheticCursor && FVector2D::Distance(Position, SyntheticCursor) < 1.; }

    UPROPERTY(Config, EditAnywhere, Category="Gamepad") float StickDeadZone = .18f;
    UPROPERTY(Config, EditAnywhere, Category="Gamepad") float LookDegreesPerSecond = 135.f;
    UPROPERTY(Config, EditAnywhere, Category="Gamepad") float CursorPixelsPerSecond = 900.f;
    UPROPERTY(Config, EditAnywhere, Category="Gamepad") bool bInvertLookY = false;
    UPROPERTY(Config, EditAnywhere, Category="Gamepad") float ResetHoldSeconds = .8f;

private:
    APlayerController* Controller() const;
    void Primary(bool bPressed);
    void Interact();
    void Back();
    void ResetContext();
    void Navigate(int32 Direction);
    void PointerButton(bool bPressed);
    void MovePointer(FVector2D Axis, float DeltaSeconds);
    void PurchaseUpgrade();
    void UpdateContext();
    TSharedPtr<IInputProcessor> Processor;
    TSharedPtr<SWidget> Hints;
    TWeakObjectPtr<APlayerController> LastController;
    TWeakObjectPtr<APawn> LastPawn;
    TWeakObjectPtr<UMiningComponent> HeldMining;
    TWeakObjectPtr<APawn> JumpPawn;
    TWeakObjectPtr<APopupCloseStage> HeldOffice;
    FText UpgradeFeedback;
    float UpgradeFeedbackRemaining = 0;
    bool bMenuPointer = false;

    FVector2D MoveStick = FVector2D::ZeroVector;
    FVector2D LookStick = FVector2D::ZeroVector;
    float LeftTrigger = 0;
    float RightTrigger = 0;
    float ResetElapsed = 0;
    float NavigationRemaining = 0;
    int32 NavigationDirection = 0;
    uint64 JumpFrame = MAX_uint64;
    TSet<FKey> HeldButtons;
    EMiningInputContext LastContext = EMiningInputContext::Blocked;
    int32 LastArea = INDEX_NONE;
    bool bGamepad = false;
    bool bJumpHeld = false;
    bool bPointerHeld = false;
    bool bSyntheticPointer = false;
    bool bHasSyntheticCursor = false;
    FVector2D SyntheticCursor = FVector2D::ZeroVector;
    bool bResetHeld = false;
    bool bResetFired = false;
    FDelegateHandle ActivationHandle;
    FDelegateHandle ConnectionHandle;
    FDelegateHandle CleanupHandle;
};
