#include "MobileTouchControls.h"

#include "BrainMiningPlayerController.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SWidget.h"

void SMobileTouchControls::Construct(const FArguments& InArgs)
{
    OwnerController = InArgs._OwnerController;
    JoystickOpacity = 0.f;
}

FVector2D SMobileTouchControls::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
    return FVector2D(100.f, 100.f);
}

float SMobileTouchControls::GetUiScale(const FVector2D& Size) const
{
    return FMath::Clamp(FMath::Min(Size.X, Size.Y) / 720.f, .8f, 1.6f);
}

float SMobileTouchControls::GetJoystickRadius(const FVector2D& Size) const
{
    return 78.f * GetUiScale(Size);
}

float SMobileTouchControls::GetButtonRadius(const FVector2D& Size) const
{
    return 44.f * GetUiScale(Size);
}

FVector2D SMobileTouchControls::GetButtonCenter(EButton Button, const FVector2D& Size) const
{
    const float Scale = GetUiScale(Size);
    const float Right = Size.X - 88.f * Scale;
    const float Left = Size.X - 206.f * Scale;
    const float Lower = Size.Y - 88.f * Scale;
    const float Upper = Size.Y - 204.f * Scale;
    switch (Button)
    {
    case EButton::Mine: return FVector2D(Right, Lower);
    case EButton::Bomb: return FVector2D(Right, Upper);
    case EButton::Drill: return FVector2D(Left, Lower);
    case EButton::Upgrade: return FVector2D(Left, Upper);
    default: return FVector2D::ZeroVector;
    }
}

SMobileTouchControls::EPointerAction SMobileTouchControls::GetButtonAction(EButton Button) const
{
    switch (Button)
    {
    case EButton::Mine: return EPointerAction::Mine;
    case EButton::Bomb: return EPointerAction::Bomb;
    case EButton::Drill: return EPointerAction::Drill;
    case EButton::Upgrade: return EPointerAction::Upgrade;
    default: return EPointerAction::None;
    }
}

int32& SMobileTouchControls::GetButtonPointer(EButton Button)
{
    switch (Button)
    {
    case EButton::Mine: return MinePointer;
    case EButton::Bomb: return BombPointer;
    case EButton::Drill: return DrillPointer;
    case EButton::Upgrade: return UpgradePointer;
    default: return MinePointer;
    }
}

const int32& SMobileTouchControls::GetButtonPointer(EButton Button) const
{
    switch (Button)
    {
    case EButton::Mine: return MinePointer;
    case EButton::Bomb: return BombPointer;
    case EButton::Drill: return DrillPointer;
    case EButton::Upgrade: return UpgradePointer;
    default: return MinePointer;
    }
}

bool SMobileTouchControls::IsInsideButton(EButton Button, const FVector2D& Position, const FVector2D& Size) const
{
    return FVector2D::DistSquared(Position, GetButtonCenter(Button, Size)) <= FMath::Square(GetButtonRadius(Size));
}

bool SMobileTouchControls::IsJoystickZone(const FVector2D& Position, const FVector2D& Size) const
{
    return JoystickPointer == -1 && Position.X < Size.X * .55f && Position.Y > Size.Y * .34f;
}

bool SMobileTouchControls::IsButtonPressed(EButton Button) const
{
    return GetButtonPointer(Button) != -1;
}

FReply SMobileTouchControls::HandlePointerStarted(const FGeometry& MyGeometry, int32 PointerIndex, const FVector2D& LocalPosition)
{
    if (!OwnerController.IsValid()) return FReply::Unhandled();
    const FVector2D Size = MyGeometry.GetLocalSize();
    const EButton Buttons[] = { EButton::Mine, EButton::Bomb, EButton::Drill, EButton::Upgrade };
    for (const EButton Button : Buttons)
    {
        int32& ButtonPointer = GetButtonPointer(Button);
        if (ButtonPointer == -1 && IsInsideButton(Button, LocalPosition, Size))
        {
            ButtonPointer = PointerIndex;
            SetButtonPressed(Button, true);
            return FReply::Handled().CaptureMouse(SharedThis(this));
        }
    }

    if (IsJoystickZone(LocalPosition, Size))
    {
        const float Radius = GetJoystickRadius(Size);
        JoystickPointer = PointerIndex;
        JoystickOrigin = FVector2D(
            FMath::Clamp(LocalPosition.X, Radius, Size.X * .55f - Radius),
            FMath::Clamp(LocalPosition.Y, Size.Y * .45f, Size.Y - Radius));
        JoystickPosition = JoystickOrigin;
        JoystickAxis = FVector2D::ZeroVector;
        JoystickOpacity = .15f;
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }

    if (LookPointer == -1)
    {
        LookPointer = PointerIndex;
        LastLookPosition = LocalPosition;
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }
    return FReply::Unhandled();
}

FReply SMobileTouchControls::HandlePointerMoved(const FGeometry& MyGeometry, int32 PointerIndex, const FVector2D& LocalPosition)
{
    if (!OwnerController.IsValid()) return FReply::Unhandled();
    if (PointerIndex == JoystickPointer)
    {
        UpdateJoystick(LocalPosition, MyGeometry.GetLocalSize());
        return FReply::Handled();
    }
    if (PointerIndex == LookPointer)
    {
        const FVector2D Delta = LocalPosition - LastLookPosition;
        LastLookPosition = LocalPosition;
        OwnerController->HandleMobileLook(Delta);
        return FReply::Handled();
    }
    if (PointerIndex == MinePointer || PointerIndex == BombPointer || PointerIndex == DrillPointer || PointerIndex == UpgradePointer)
    {
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

FReply SMobileTouchControls::HandlePointerEnded(int32 PointerIndex)
{
    if (PointerIndex == JoystickPointer)
    {
        ReleaseJoystick();
        return FReply::Handled().ReleaseMouseCapture();
    }
    if (PointerIndex == LookPointer)
    {
        LookPointer = -1;
        return FReply::Handled().ReleaseMouseCapture();
    }
    const EButton Buttons[] = { EButton::Mine, EButton::Bomb, EButton::Drill, EButton::Upgrade };
    for (const EButton Button : Buttons)
    {
        if (GetButtonPointer(Button) == PointerIndex)
        {
            GetButtonPointer(Button) = -1;
            SetButtonPressed(Button, false);
            return FReply::Handled().ReleaseMouseCapture();
        }
    }
    return FReply::Unhandled();
}

void SMobileTouchControls::UpdateJoystick(const FVector2D& LocalPosition, const FVector2D& Size)
{
    if (JoystickPointer == -1) return;
    const float Radius = GetJoystickRadius(Size);
    const FVector2D Offset = LocalPosition - JoystickOrigin;
    const float Distance = Offset.Size();
    JoystickPosition = Distance > Radius ? JoystickOrigin + Offset / Distance * Radius : LocalPosition;
    JoystickAxis = Radius > UE_SMALL_NUMBER ? (JoystickPosition - JoystickOrigin) / Radius : FVector2D::ZeroVector;
}

void SMobileTouchControls::ReleaseJoystick()
{
    JoystickPointer = -1;
    JoystickPosition = JoystickOrigin;
    JoystickAxis = FVector2D::ZeroVector;
    if (OwnerController.IsValid()) OwnerController->HandleMobileMove(FVector2D::ZeroVector);
}

void SMobileTouchControls::SetButtonPressed(EButton Button, bool bPressed)
{
    if (!OwnerController.IsValid()) return;
    switch (Button)
    {
    case EButton::Mine:
        OwnerController->HandleMobileMine(bPressed);
        break;
    case EButton::Bomb:
        if (bPressed) OwnerController->HandleMobileBomb();
        break;
    case EButton::Drill:
        if (bPressed) OwnerController->HandleMobileDrill();
        break;
    case EButton::Upgrade:
        if (bPressed) OwnerController->HandleMobileUpgrade();
        break;
    default:
        break;
    }
}

FReply SMobileTouchControls::OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& Event)
{
    return HandlePointerStarted(MyGeometry, static_cast<int32>(Event.GetPointerIndex()), MyGeometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
}

FReply SMobileTouchControls::OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& Event)
{
    return HandlePointerMoved(MyGeometry, static_cast<int32>(Event.GetPointerIndex()), MyGeometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
}

FReply SMobileTouchControls::OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& Event)
{
    return HandlePointerEnded(static_cast<int32>(Event.GetPointerIndex()));
}

FReply SMobileTouchControls::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
    return HandlePointerStarted(MyGeometry, 0, MyGeometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
}

FReply SMobileTouchControls::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& Event)
{
    return HandlePointerMoved(MyGeometry, 0, MyGeometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
}

FReply SMobileTouchControls::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
    return HandlePointerEnded(0);
}

void SMobileTouchControls::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SLeafWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    if (JoystickPointer != -1)
    {
        JoystickOpacity = FMath::FInterpTo(JoystickOpacity, 1.f, InDeltaTime, 12.f);
        if (OwnerController.IsValid()) OwnerController->HandleMobileMove(FVector2D(JoystickAxis.X, -JoystickAxis.Y));
    }
    else
    {
        JoystickOpacity = FMath::FInterpTo(JoystickOpacity, 0.f, InDeltaTime, 8.f);
    }
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SMobileTouchControls::DrawText(FSlateWindowElementList& OutDrawElements, const FGeometry& Geometry, int32 LayerId,
    const FString& Text, const FVector2D& Position, int32 FontSize, const FLinearColor& Color) const
{
    FSlateDrawElement::MakeText(OutDrawElements, LayerId,
        Geometry.ToPaintGeometry(FVector2D(300.f, 60.f), FSlateLayoutTransform(Position)),
        FText::FromString(Text), FCoreStyle::GetDefaultFontStyle("Bold", FontSize), ESlateDrawEffect::None, Color);
}

void SMobileTouchControls::DrawButton(FSlateWindowElementList& OutDrawElements, const FGeometry& Geometry, int32 LayerId,
    EButton Button, const FString& Label, const FLinearColor& Color, bool bParentEnabled) const
{
    const FVector2D Size = Geometry.GetLocalSize();
    const float Radius = GetButtonRadius(Size);
    const FVector2D Center = GetButtonCenter(Button, Size);
    const float PressScale = IsButtonPressed(Button) ? .9f : 1.f;
    const float DrawRadius = Radius * PressScale;
    FSlateRoundedBoxBrush Brush(Color.CopyWithNewOpacity(Color.A * (bParentEnabled ? .88f : .35f)), DrawRadius);
    FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(
        FVector2D(DrawRadius * 2.f), FSlateLayoutTransform(Center - FVector2D(DrawRadius))), &Brush);
    const float TextSize = 15.f * GetUiScale(Size);
    DrawText(OutDrawElements, Geometry, LayerId + 1, Label,
        Center - FVector2D(Label.Len() * TextSize * .28f, TextSize * .55f), FMath::RoundToInt(TextSize), FLinearColor::White);
}

int32 SMobileTouchControls::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
    const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    const FVector2D Size = AllottedGeometry.GetLocalSize();
    int32 CurrentLayer = LayerId;
    if (JoystickOpacity > .01f)
    {
        const float Radius = GetJoystickRadius(Size) * (.86f + .14f * JoystickOpacity);
        const FLinearColor OuterColor = FLinearColor(.12f, .82f, .92f, .28f * JoystickOpacity);
        const FLinearColor InnerColor = FLinearColor(.35f, .95f, 1.f, .75f * JoystickOpacity);
        FSlateRoundedBoxBrush OuterBrush(OuterColor, Radius);
        FSlateRoundedBoxBrush InnerBrush(InnerColor, Radius * .42f);
        FSlateDrawElement::MakeBox(OutDrawElements, ++CurrentLayer, AllottedGeometry.ToPaintGeometry(
            FVector2D(Radius * 2.f), FSlateLayoutTransform(JoystickOrigin - FVector2D(Radius))), &OuterBrush);
        const float ThumbRadius = Radius * .36f;
        FSlateDrawElement::MakeBox(OutDrawElements, ++CurrentLayer, AllottedGeometry.ToPaintGeometry(
            FVector2D(ThumbRadius * 2.f), FSlateLayoutTransform(JoystickPosition - FVector2D(ThumbRadius))), &InnerBrush);
    }
    DrawButton(OutDrawElements, AllottedGeometry, ++CurrentLayer, EButton::Mine, TEXT("MINE"), FLinearColor(.08f, .72f, .88f, 1.f), bParentEnabled);
    DrawButton(OutDrawElements, AllottedGeometry, ++CurrentLayer, EButton::Bomb, TEXT("BOMB"), FLinearColor(.92f, .45f, .12f, 1.f), bParentEnabled);
    DrawButton(OutDrawElements, AllottedGeometry, ++CurrentLayer, EButton::Drill, TEXT("DRILL"), FLinearColor(.12f, .78f, .56f, 1.f), bParentEnabled);
    DrawButton(OutDrawElements, AllottedGeometry, ++CurrentLayer, EButton::Upgrade, TEXT("UP"), FLinearColor(.64f, .36f, .92f, 1.f), bParentEnabled);
    return CurrentLayer;
}