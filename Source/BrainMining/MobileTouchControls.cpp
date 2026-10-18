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
    const float FarLeft = Size.X - 324.f * Scale;
    const float Extra = Size.X - 442.f * Scale;
    const float Lower = Size.Y - 88.f * Scale;
    const float Upper = Size.Y - 204.f * Scale;
    const float Upper2 = Size.Y - 320.f * Scale;
    const float Upper3 = Size.Y - 436.f * Scale;
    switch (Button)
    {
    case EButton::Mine: return FVector2D(Right, Lower);
    case EButton::Bomb: return FVector2D(Right, Upper);
    case EButton::Drill: return FVector2D(Left, Lower);
    case EButton::Jetpack: return FVector2D(Left, Upper);
    case EButton::Interact: return FVector2D(FarLeft, Lower);
    case EButton::Primary: return FVector2D(Extra, Lower);
    case EButton::Confirm: return FVector2D(FarLeft, Upper);
    case EButton::Reset: return FVector2D(Extra, Upper);
    case EButton::ThoughtLeft: return FVector2D(FarLeft, Upper2);
    case EButton::ThoughtRight: return FVector2D(Extra, Upper2);
    case EButton::Map: return FVector2D(FarLeft, Upper3);
    case EButton::Catalogue: return FVector2D(Extra, Upper3);
    case EButton::Back: return FVector2D(Right, Upper2);
    case EButton::Edit: return FVector2D(Right, Upper3);
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
    case EButton::Jetpack: return EPointerAction::Jetpack;
    case EButton::Interact: return EPointerAction::Interact;
    case EButton::Primary: return EPointerAction::Primary;
    case EButton::Confirm: return EPointerAction::Confirm;
    case EButton::Reset: return EPointerAction::Reset;
    case EButton::ThoughtLeft: return EPointerAction::ThoughtLeft;
    case EButton::ThoughtRight: return EPointerAction::ThoughtRight;
    case EButton::Map: return EPointerAction::Map;
    case EButton::Catalogue: return EPointerAction::Catalogue;
    case EButton::Back: return EPointerAction::Back;
    case EButton::Edit: return EPointerAction::Edit;
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
    case EButton::Jetpack: return JetpackPointer;
    case EButton::Interact: return InteractPointer;
    case EButton::Primary: return PrimaryPointer;
    case EButton::Confirm: return ConfirmPointer;
    case EButton::Reset: return ResetPointer;
    case EButton::ThoughtLeft: return ThoughtLeftPointer;
    case EButton::ThoughtRight: return ThoughtRightPointer;
    case EButton::Map: return MapPointer;
    case EButton::Catalogue: return CataloguePointer;
    case EButton::Back: return BackPointer;
    case EButton::Edit: return EditPointer;
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
    case EButton::Jetpack: return JetpackPointer;
    case EButton::Interact: return InteractPointer;
    case EButton::Primary: return PrimaryPointer;
    case EButton::Confirm: return ConfirmPointer;
    case EButton::Reset: return ResetPointer;
    case EButton::ThoughtLeft: return ThoughtLeftPointer;
    case EButton::ThoughtRight: return ThoughtRightPointer;
    case EButton::Map: return MapPointer;
    case EButton::Catalogue: return CataloguePointer;
    case EButton::Back: return BackPointer;
    case EButton::Edit: return EditPointer;
    default: return MinePointer;
    }
}

bool SMobileTouchControls::IsInsideButton(EButton Button, const FVector2D& Position, const FVector2D& Size) const
{
    return FVector2D::DistSquared(Position, GetButtonCenter(Button, Size)) <= FMath::Square(GetButtonRadius(Size));
}

bool SMobileTouchControls::IsJoystickZone(const FVector2D& Position, const FVector2D& Size) const
{
    return JoystickPointer == -1 && Position.X < Size.X * .42f && Position.Y > Size.Y * .34f;
}

bool SMobileTouchControls::IsButtonPressed(EButton Button) const
{
    return GetButtonPointer(Button) != -1;
}

bool SMobileTouchControls::IsButtonVisible(EButton Button) const
{
    if (!OwnerController.IsValid()) return false;
    switch (Button)
    {
    case EButton::Primary: return OwnerController->IsMobileActionVisible();
    case EButton::Confirm: return OwnerController->IsMobileConfirmVisible();
    case EButton::Reset:
    case EButton::Map:
    case EButton::Catalogue: return false;
    case EButton::ThoughtLeft:
    case EButton::ThoughtRight: return OwnerController->IsMobileThoughtVisible();
    case EButton::Back: return OwnerController->IsMobileBackVisible();
    case EButton::Edit: return OwnerController->IsMobileEditVisible();
    default: return true;
    }
}

FString SMobileTouchControls::GetButtonLabel(EButton Button) const
{
    switch (Button)
    {
    case EButton::Primary: return OwnerController.IsValid() ? OwnerController->GetMobileActionLabel() : TEXT("TAP");
    case EButton::Confirm: return TEXT("OK");
    case EButton::Reset: return TEXT("RST");
    case EButton::ThoughtLeft: return TEXT("Q");
    case EButton::ThoughtRight: return TEXT("R");
    case EButton::Map: return TEXT("MAP");
    case EButton::Catalogue: return TEXT("BOOK");
    case EButton::Back: return TEXT("BACK");
    case EButton::Edit: return TEXT("EDIT");
    case EButton::Mine: return TEXT("MINE");
    case EButton::Bomb: return TEXT("BOMB");
    case EButton::Drill: return TEXT("DRILL");
    case EButton::Jetpack: return TEXT("UP");
    case EButton::Interact: return TEXT("E");
    default: return TEXT("");
    }
}

FLinearColor SMobileTouchControls::GetButtonColor(EButton Button) const
{
    switch (Button)
    {
    case EButton::Mine: return FLinearColor(.08f, .72f, .88f, 1.f);
    case EButton::Bomb: return FLinearColor(.92f, .45f, .12f, 1.f);
    case EButton::Drill: return FLinearColor(.12f, .78f, .56f, 1.f);
    case EButton::Jetpack: return FLinearColor(.64f, .36f, .92f, 1.f);
    case EButton::Interact: return FLinearColor(.96f, .78f, .24f, 1.f);
    case EButton::Primary: return FLinearColor(.95f, .55f, .22f, 1.f);
    case EButton::Confirm: return FLinearColor(.34f, .88f, .48f, 1.f);
    case EButton::Reset: return FLinearColor(.95f, .32f, .28f, 1.f);
    case EButton::ThoughtLeft:
    case EButton::ThoughtRight: return FLinearColor(.92f, .28f, .46f, 1.f);
    case EButton::Map:
    case EButton::Catalogue: return FLinearColor(.32f, .68f, .95f, 1.f);
    case EButton::Back:
    case EButton::Edit: return FLinearColor(.74f, .72f, .62f, 1.f);
    default: return FLinearColor::White;
    }
}

FReply SMobileTouchControls::HandlePointerStarted(const FGeometry& MyGeometry, int32 PointerIndex, const FVector2D& LocalPosition)
{
    if (!OwnerController.IsValid()) return FReply::Unhandled();
    const FVector2D Size = MyGeometry.GetLocalSize();
    const EButton Buttons[] = { EButton::Mine, EButton::Bomb, EButton::Drill, EButton::Jetpack, EButton::Interact,
        EButton::Primary, EButton::Confirm, EButton::Reset, EButton::ThoughtLeft, EButton::ThoughtRight,
        EButton::Map, EButton::Catalogue, EButton::Back, EButton::Edit };
    for (const EButton Button : Buttons)
    {
        int32& ButtonPointer = GetButtonPointer(Button);
        if (ButtonPointer == -1 && IsButtonVisible(Button) && IsInsideButton(Button, LocalPosition, Size))
        {
            ButtonPointer = PointerIndex;
            ButtonLastPositions.Add(PointerIndex, LocalPosition);
            SetButtonPressed(Button, true);
            // Do not capture the whole touch stream for a button. A second finger
            // must still be able to enter the look zone while this button is held.
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

    if (LookPointer == -1 && LocalPosition.X >= Size.X * .58f)
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
    const EButton Buttons[] = { EButton::Mine, EButton::Bomb, EButton::Drill, EButton::Jetpack, EButton::Interact,
        EButton::Primary, EButton::Confirm, EButton::Reset, EButton::ThoughtLeft, EButton::ThoughtRight,
        EButton::Map, EButton::Catalogue, EButton::Back, EButton::Edit };
    for (const EButton Button : Buttons)
        if (GetButtonPointer(Button) == PointerIndex)
        {
            if (const FVector2D* LastPosition = ButtonLastPositions.Find(PointerIndex))
            {
                const FVector2D Delta = LocalPosition - *LastPosition;
                *ButtonLastPositions.Find(PointerIndex) = LocalPosition;
                OwnerController->HandleMobileLook(Delta);
            }
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
    const EButton Buttons[] = { EButton::Mine, EButton::Bomb, EButton::Drill, EButton::Jetpack, EButton::Interact,
        EButton::Primary, EButton::Confirm, EButton::Reset, EButton::ThoughtLeft, EButton::ThoughtRight,
        EButton::Map, EButton::Catalogue, EButton::Back, EButton::Edit };
    for (const EButton Button : Buttons)
    {
        if (GetButtonPointer(Button) == PointerIndex)
        {
            GetButtonPointer(Button) = -1;
            ButtonLastPositions.Remove(PointerIndex);
            SetButtonPressed(Button, false);
            return FReply::Handled();
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
    case EButton::Jetpack:
        OwnerController->HandleMobileJump(bPressed);
        break;
    case EButton::Interact:
        if (bPressed) OwnerController->HandleMobileInteract();
        break;
    case EButton::Primary:
        if (bPressed) OwnerController->HandleMobilePrimary();
        break;
    case EButton::Confirm:
        if (bPressed) OwnerController->HandleMobileConfirm();
        break;
    case EButton::Reset:
        if (bPressed) OwnerController->HandleMobileReset();
        break;
    case EButton::ThoughtLeft:
        if (bPressed) OwnerController->HandleMobileThoughtLeft();
        break;
    case EButton::ThoughtRight:
        if (bPressed) OwnerController->HandleMobileThoughtRight();
        break;
    case EButton::Map:
        if (bPressed) OwnerController->HandleMobileMap();
        break;
    case EButton::Catalogue:
        if (bPressed) OwnerController->HandleMobileCatalogue();
        break;
    case EButton::Back:
        if (bPressed) OwnerController->HandleMobileBack();
        break;
    case EButton::Edit:
        if (bPressed) OwnerController->HandleMobileEdit();
        break;
    default:
        break;
    }
}

FReply SMobileTouchControls::OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& Event)
{
    return HandlePointerStarted(MyGeometry, static_cast<int32>(Event.GetPointerIndex()), MyGeometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
}

FReply SMobileTouchControls::OnTouchFirstMove(const FGeometry& MyGeometry, const FPointerEvent& Event)
{
    return HandlePointerMoved(MyGeometry, static_cast<int32>(Event.GetPointerIndex()), MyGeometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
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
    const FLinearColor OuterColor = Color.CopyWithNewOpacity(Color.A * (bParentEnabled ? .30f : .12f));
    const FLinearColor InnerColor = Color.CopyWithNewOpacity(Color.A * (bParentEnabled ? .78f : .25f));
    FSlateRoundedBoxBrush Brush(OuterColor, DrawRadius);
    FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(
        FVector2D(DrawRadius * 2.f), FSlateLayoutTransform(Center - FVector2D(DrawRadius))), &Brush);
    FSlateRoundedBoxBrush InnerBrush(InnerColor, DrawRadius * .42f);
    const float InnerRadius = DrawRadius * .42f;
    FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(
        FVector2D(InnerRadius * 2.f), FSlateLayoutTransform(Center - FVector2D(InnerRadius))), &InnerBrush);
    const float TextSize = 13.f * GetUiScale(Size);
    DrawText(OutDrawElements, Geometry, LayerId + 1, Label,
        Center - FVector2D(Label.Len() * TextSize * .28f, TextSize * .55f), FMath::RoundToInt(TextSize), FLinearColor(.02f, .02f, .02f, 1.f));
}

int32 SMobileTouchControls::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
    const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    const FVector2D Size = AllottedGeometry.GetLocalSize();
    int32 CurrentLayer = LayerId;
    const EButton Buttons[] = { EButton::Mine, EButton::Bomb, EButton::Drill, EButton::Jetpack, EButton::Interact,
        EButton::Primary, EButton::Confirm, EButton::Reset, EButton::ThoughtLeft, EButton::ThoughtRight,
        EButton::Map, EButton::Catalogue, EButton::Back, EButton::Edit };
    for (const EButton Button : Buttons)
        if (IsButtonVisible(Button)) DrawButton(OutDrawElements, AllottedGeometry, ++CurrentLayer, Button, GetButtonLabel(Button), GetButtonColor(Button), bParentEnabled);
    return CurrentLayer;
}
