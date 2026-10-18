#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SLeafWidget.h"

class ABrainMiningPlayerController;

class SMobileTouchControls : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SMobileTouchControls) {}
        SLATE_ARGUMENT(TWeakObjectPtr<ABrainMiningPlayerController>, OwnerController)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
        const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
    virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
    virtual FReply OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& Event) override;
    virtual FReply OnTouchFirstMove(const FGeometry& MyGeometry, const FPointerEvent& Event) override;
    virtual FReply OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& Event) override;
    virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& Event) override;
    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& Event) override;
    virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& Event) override;
    virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& Event) override;
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
    virtual bool SupportsKeyboardFocus() const override { return false; }

private:
    enum class EPointerAction : uint8
    {
        None,
        Joystick,
        Look,
        Mine,
        Bomb,
        Drill,
        Jetpack,
        Interact,
        Primary,
        Confirm,
        Reset,
        ThoughtLeft,
        ThoughtRight,
        Map,
        Catalogue,
        Back,
        Edit
    };

    enum class EButton : uint8
    {
        Mine,
        Bomb,
        Drill,
        Jetpack,
        Interact,
        Primary,
        Confirm,
        Reset,
        ThoughtLeft,
        ThoughtRight,
        Map,
        Catalogue,
        Back,
        Edit
    };

    TWeakObjectPtr<ABrainMiningPlayerController> OwnerController;
    int32 JoystickPointer = -1;
    int32 LookPointer = -1;
    int32 MinePointer = -1;
    int32 BombPointer = -1;
    int32 DrillPointer = -1;
    int32 JetpackPointer = -1;
    int32 InteractPointer = -1;
    int32 PrimaryPointer = -1;
    int32 ConfirmPointer = -1;
    int32 ResetPointer = -1;
    int32 ThoughtLeftPointer = -1;
    int32 ThoughtRightPointer = -1;
    int32 MapPointer = -1;
    int32 CataloguePointer = -1;
    int32 BackPointer = -1;
    int32 EditPointer = -1;
    FVector2D JoystickOrigin = FVector2D::ZeroVector;
    FVector2D JoystickPosition = FVector2D::ZeroVector;
    FVector2D JoystickAxis = FVector2D::ZeroVector;
    FVector2D LastLookPosition = FVector2D::ZeroVector;
    TMap<int32, FVector2D> ButtonLastPositions;
    float JoystickOpacity = 0.f;

    float GetUiScale(const FVector2D& Size) const;
    float GetJoystickRadius(const FVector2D& Size) const;
    float GetButtonRadius(const FVector2D& Size) const;
    FVector2D GetButtonCenter(EButton Button, const FVector2D& Size) const;
    EPointerAction GetButtonAction(EButton Button) const;
    int32& GetButtonPointer(EButton Button);
    const int32& GetButtonPointer(EButton Button) const;
    bool IsInsideButton(EButton Button, const FVector2D& Position, const FVector2D& Size) const;
    bool IsJoystickZone(const FVector2D& Position, const FVector2D& Size) const;
    bool IsButtonPressed(EButton Button) const;
    bool IsButtonVisible(EButton Button) const;
    FString GetButtonLabel(EButton Button) const;
    FLinearColor GetButtonColor(EButton Button) const;

    FReply HandlePointerStarted(const FGeometry& MyGeometry, int32 PointerIndex, const FVector2D& LocalPosition);
    FReply HandlePointerMoved(const FGeometry& MyGeometry, int32 PointerIndex, const FVector2D& LocalPosition);
    FReply HandlePointerEnded(int32 PointerIndex);
    void UpdateJoystick(const FVector2D& LocalPosition, const FVector2D& Size);
    void ReleaseJoystick();
    void SetButtonPressed(EButton Button, bool bPressed);
    void DrawButton(FSlateWindowElementList& OutDrawElements, const FGeometry& Geometry, int32 LayerId,
        EButton Button, const FString& Label, const FLinearColor& Color, bool bParentEnabled) const;
    void DrawText(FSlateWindowElementList& OutDrawElements, const FGeometry& Geometry, int32 LayerId,
        const FString& Text, const FVector2D& Position, int32 FontSize, const FLinearColor& Color) const;
};
