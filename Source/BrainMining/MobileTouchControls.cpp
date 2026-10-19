#include "MobileTouchControls.h"
#include "MobileUIWidgets.h"
#include "MiningComponent.h"
#include "MiningBombToolComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningExperienceSubsystem.h"
#include "MiningWalletSubsystem.h"
#include "MiningInteractionHUDWidget.h"
#include "StartRoom/MiningOpening.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "BrainMiningFonts.h"

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
    return MiningMobileUI::Scale(Size);
}

float SMobileTouchControls::GetJoystickRadius(const FVector2D& Size) const
{
    return 78.f * GetUiScale(Size);
}

float SMobileTouchControls::GetButtonRadius(EButton Button, const FVector2D& Size) const
{
    return (Button == EButton::Mine || Button == EButton::Primary ? 76.f : 52.f) * GetUiScale(Size);
}

float SMobileTouchControls::GetButtonHitRadius(EButton Button, const FVector2D& Size) const
{
    const float BaseRadius = Button == EButton::Mine || Button == EButton::Primary ? 84.f : 58.f;
    return BaseRadius * GetUiScale(Size);
}

FVector2D SMobileTouchControls::GetButtonCenter(EButton Button, const FVector2D& Size) const
{
    const float Scale = GetUiScale(Size);
    const float Right = Size.X - 152.f * Scale;
    const float Left = Size.X - 316.f * Scale;
    const float FarLeft = Size.X - 462.f * Scale;
    const float Extra = Size.X - 610.f * Scale;
    const float Lower = Size.Y - 140.f * Scale;
    const float Upper = Size.Y - 315.f * Scale;
    const float Upper2 = Size.Y - 440.f * Scale;
    const float Upper3 = Size.Y - 570.f * Scale;
    switch (Button)
    {
    case EButton::Mine: return FVector2D(Right, Lower);
    case EButton::Bomb: return FVector2D(Right, Upper);
    case EButton::Drill: return FVector2D(Left, Lower);
    case EButton::Jetpack: return FVector2D(Left, Upper);
    case EButton::Interact: return FVector2D(FarLeft, Lower);
    case EButton::Upgrade: return FVector2D(Extra, Upper);
    case EButton::Primary: return FVector2D(Right, Lower);
    case EButton::Confirm: return FVector2D(Right, Lower);
    case EButton::Reset: return FVector2D(Extra, Upper);
    case EButton::ThoughtLeft: return FVector2D(Extra, Upper);
    case EButton::ThoughtRight: return FVector2D(FarLeft, Upper);
    case EButton::Map: return FVector2D(FarLeft, Upper3);
    case EButton::Catalogue: return FVector2D(Extra, Upper3);
    case EButton::Back: return FVector2D(Right, Upper2);
    case EButton::Edit: return FVector2D(FarLeft, Upper2);
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
    case EButton::Upgrade: return EPointerAction::Upgrade;
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
    case EButton::Upgrade: return UpgradePointer;
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
    case EButton::Upgrade: return UpgradePointer;
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
    return FVector2D::DistSquared(Position, GetButtonCenter(Button, Size)) <= FMath::Square(GetButtonHitRadius(Button, Size));
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
    if (!OwnerController.IsValid() || !OwnerController->GetPawn()) return false;
    if (AMiningOpening::IsOpening(OwnerController->GetWorld())) return false;
    const auto* Menu = OwnerController->GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>();
    if ((Menu && Menu->IsMenuOpen()) || OwnerController->IsAudioSettingsOpen()) return false;
    const bool bExploring = !OwnerController->IsPaused() && !OwnerController->IsMoveInputIgnored()
        && !OwnerController->IsLookInputIgnored() && !OwnerController->bShowMouseCursor;
    switch (Button)
    {
    case EButton::Mine:
    case EButton::Bomb:
    case EButton::Drill:
        return bExploring && OwnerController->IsMiningGUIEnabled() && !OwnerController->IsMobileActionVisible() && !OwnerController->IsMobileConfirmVisible();
    case EButton::Jetpack: return bExploring;
    case EButton::Primary: return !OwnerController->IsPaused() && OwnerController->IsMobileActionVisible();
    case EButton::Confirm: return !OwnerController->IsPaused() && OwnerController->IsMobileConfirmVisible();
    case EButton::Interact: return !OwnerController->IsPaused();
    case EButton::ThoughtLeft:
    case EButton::ThoughtRight: return !OwnerController->IsPaused() && OwnerController->IsMobileThoughtVisible();
    case EButton::Edit: return !OwnerController->IsPaused() && OwnerController->IsMobileEditVisible();
    case EButton::Back: return OwnerController->IsMobileBackVisible();
    default: return false;
    }
}

FName SMobileTouchControls::GetButtonIcon(EButton Button) const
{
    switch (Button)
    {
    case EButton::Mine:
        if (const auto* Mining = OwnerController.IsValid() && OwnerController->GetPawn() ? OwnerController->GetPawn()->FindComponentByClass<UMiningComponent>() : nullptr)
            if (Mining->bDrillMode) return TEXT("drill");
        return TEXT("pick");
    case EButton::Bomb: return TEXT("bomb");
    case EButton::Drill: return TEXT("drill");
    case EButton::Jetpack: return TEXT("jet");
    case EButton::Interact: return OwnerController.IsValid() && OwnerController->GetMobileActionLabel() == TEXT("CLICK") ? TEXT("back") : TEXT("hand");
    case EButton::Primary: return OwnerController.IsValid() && OwnerController->GetMobileActionLabel() == TEXT("HIT") ? TEXT("knock") : TEXT("cursor");
    case EButton::Confirm: return TEXT("check");
    case EButton::ThoughtLeft: return TEXT("left");
    case EButton::ThoughtRight: return TEXT("right");
    case EButton::Back: return TEXT("back");
    case EButton::Edit: return TEXT("edit");
    default: return TEXT("hand");
    }
}

FReply SMobileTouchControls::HandlePointerStarted(const FGeometry& MyGeometry, int32 PointerIndex, const FVector2D& LocalPosition)
{
    if (!OwnerController.IsValid()) return FReply::Unhandled();
    const FVector2D Size = MyGeometry.GetLocalSize();
    const EButton Buttons[] = { EButton::Mine, EButton::Bomb, EButton::Drill, EButton::Jetpack, EButton::Interact, EButton::Upgrade,
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
            return FReply::Handled();
        }
    }

    if (OwnerController->IsPaused() || OwnerController->IsMoveInputIgnored() || OwnerController->IsLookInputIgnored() || OwnerController->bShowMouseCursor) return FReply::Unhandled();
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
    const EButton Buttons[] = { EButton::Mine, EButton::Bomb, EButton::Drill, EButton::Jetpack, EButton::Interact, EButton::Upgrade,
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
    const EButton Buttons[] = { EButton::Mine, EButton::Bomb, EButton::Drill, EButton::Jetpack, EButton::Interact, EButton::Upgrade,
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
    case EButton::Upgrade:
        if (bPressed) OwnerController->HandleMobileUpgrade();
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
    if (OwnerController.IsValid() && OwnerController->IsPaused()) ResetInput();
    if (OwnerController.IsValid() && (OwnerController->IsMoveInputIgnored() || OwnerController->IsLookInputIgnored() || OwnerController->bShowMouseCursor))
    {
        ReleaseJoystick();
        LookPointer = -1;
    }
    for (const EButton Button : {EButton::Mine, EButton::Jetpack})
    {
        int32& Pointer = GetButtonPointer(Button);
        if (Pointer != -1 && !IsButtonVisible(Button))
        {
            ButtonLastPositions.Remove(Pointer);
            Pointer = -1;
            SetButtonPressed(Button, false);
        }
    }
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

void SMobileTouchControls::ResetInput()
{
    if (JoystickPointer != -1) ReleaseJoystick();
    LookPointer = -1;
    TArray<int32> Pointers;
    ButtonLastPositions.GetKeys(Pointers);
    for (const int32 Pointer : Pointers) HandlePointerEnded(Pointer);
    ButtonLastPositions.Reset();
}

void SMobileTouchControls::DrawText(FSlateWindowElementList& OutDrawElements, const FGeometry& Geometry, int32 LayerId,
    const FString& Text, const FVector2D& Position, int32 FontSize, const FLinearColor& Color) const
{
    FSlateDrawElement::MakeText(OutDrawElements, LayerId,
        Geometry.ToPaintGeometry(FVector2D(300.f, 60.f), FSlateLayoutTransform(Position)),
        FText::FromString(Text), BrainMiningFonts::Style("Medium", FontSize), ESlateDrawEffect::None, Color);
}

void SMobileTouchControls::DrawButton(FSlateWindowElementList& OutDrawElements, const FGeometry& Geometry, int32 LayerId,
    EButton Button, bool bParentEnabled) const
{
    using namespace MiningMobileUI;
    const FVector2D Size = Geometry.GetLocalSize();
    const float Scale = GetUiScale(Size);
    const float Radius = GetButtonRadius(Button, Size) * (IsButtonPressed(Button) ? .94f : 1.f);
    const FVector2D Center = GetButtonCenter(Button, Size);
    const APawn* Pawn = OwnerController.IsValid() ? OwnerController->GetPawn() : nullptr;
    const auto* Mining = Pawn ? Pawn->FindComponentByClass<UMiningComponent>() : nullptr;
    const auto* Bomb = Pawn ? Pawn->FindComponentByClass<UMiningBombToolComponent>() : nullptr;
    const auto* Flight = Pawn ? Pawn->FindComponentByClass<UMiningCharacterMovementComponent>() : nullptr;
    bool bAvailable = bParentEnabled;
    if (Button == EButton::Drill) bAvailable &= Mining && Mining->IsDrillUnlocked() && (Mining->bInfiniteDrillEnergy || Mining->DrillEnergy > 0);
    if (Button == EButton::Bomb) bAvailable &= Bomb && Bomb->IsSkillUnlocked() && (!Bomb->bUseInventory || Bomb->BombCount > 0);
    const bool bHighlighted = Button == EButton::Mine || Button == EButton::Primary || Button == EButton::Confirm
        || (Button == EButton::Drill && Mining && Mining->bDrillMode)
        || (Button == EButton::Interact && OwnerController.IsValid() && OwnerController->InteractionHUD && !OwnerController->InteractionHUD->InteractionAction.IsEmpty());
    FLinearColor Surface = bHighlighted ? Gold() : Ink();
    if (IsButtonPressed(Button)) Surface = Gold() * .78f;
    Surface.A = bAvailable ? .92f : .55f;
    const FLinearColor Foreground = (bHighlighted || IsButtonPressed(Button) ? Ink() : Cream()).CopyWithNewOpacity(bAvailable ? 1.f : .4f);
    const auto Box = [&](int32 Layer, FName Name, FVector2D Position, FVector2D Extent, FLinearColor Color)
    {
        FSlateDrawElement::MakeBox(OutDrawElements, Layer, Geometry.ToPaintGeometry(Extent, FSlateLayoutTransform(Position)), Brush(Name), ESlateDrawEffect::None, Color);
    };
    Box(LayerId, TEXT("plate"), Center - FVector2D(Radius), FVector2D(Radius * 2), Surface);
    Box(LayerId + 1, TEXT("rim"), Center - FVector2D(Radius), FVector2D(Radius * 2), Cream().CopyWithNewOpacity(bAvailable ? .9f : .3f));
    const float IconExtent = Radius * 1.04f;
    Box(LayerId + 2, GetButtonIcon(Button), Center - FVector2D(IconExtent * .5f), FVector2D(IconExtent), Foreground);
    FString Status;
    if (Button == EButton::Bomb && Bomb && Bomb->bUseInventory) Status = FString::Printf(TEXT("%d / %d"), Bomb->BombCount, Bomb->MaxBombs);
    if (Button == EButton::Drill && Mining) Status = Mining->bInfiniteDrillEnergy ? TEXT("∞") : FString::Printf(TEXT("%d%%"), FMath::RoundToInt(100 * Mining->DrillEnergy / FMath::Max(1.f, Mining->MaxDrillEnergy)));
    if (!Status.IsEmpty())
    {
        const int32 FontSize = FMath::Max(10, FMath::RoundToInt(17 * Scale));
        const FVector2D Extent = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Status, BrainMiningFonts::Style("Medium", FontSize));
        DrawText(OutDrawElements, Geometry, LayerId + 3, Status, Center + FVector2D(-Extent.X * .5f, Radius + 8 * Scale), FontSize, Cream());
    }
    if (Button == EButton::Jetpack && Flight)
    {
        const FVector2D Position = Center + FVector2D(-35, 67) * Scale;
        FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 3, Geometry.ToPaintGeometry(FVector2D(70, 5) * Scale, FSlateLayoutTransform(Position)), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Ink());
        const float Remaining = FMath::Clamp(1.f - Flight->JetpackHeat, 0.f, 1.f);
        FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 4, Geometry.ToPaintGeometry(FVector2D(70 * Remaining, 5) * Scale, FSlateLayoutTransform(Position)), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Flight->IsJetpackOverheated() ? FLinearColor(.8f,.13f,.06f) : Teal());
    }
}

int32 SMobileTouchControls::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
    const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    using namespace MiningMobileUI;
    const FVector2D Size = AllottedGeometry.GetLocalSize();
    const float Scale = GetUiScale(Size);
    int32 CurrentLayer = LayerId;
    const auto* Player = OwnerController.Get();
    const auto* Menu = Player && Player->GetGameInstance() ? Player->GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>() : nullptr;
    if (!Player || (Menu && Menu->IsMenuOpen()) || Player->IsAudioSettingsOpen()) return CurrentLayer;
    if (AMiningOpening::IsOpening(Player->GetWorld())) return CurrentLayer;
    const bool bExploring = !Player->IsPaused() && !Player->IsMoveInputIgnored() && !Player->IsLookInputIgnored() && !Player->bShowMouseCursor;
    if (bExploring)
    {
        const FVector2D Origin = JoystickPointer == -1 ? FVector2D(164 * Scale, Size.Y - 150 * Scale) : JoystickOrigin;
        const float Radius = GetJoystickRadius(Size);
        const FSlateRoundedBoxBrush Ring(FLinearColor::White, Radius, Cream().CopyWithNewOpacity(.35f), 2 * Scale);
        const FSlateRoundedBoxBrush Disc(FLinearColor::White, 35 * Scale);
        FSlateDrawElement::MakeBox(OutDrawElements, ++CurrentLayer, AllottedGeometry.ToPaintGeometry(FVector2D(Radius * 2), FSlateLayoutTransform(Origin - FVector2D(Radius))), &Ring, ESlateDrawEffect::None, Ink().CopyWithNewOpacity(.28f));
        const FVector2D Thumb = JoystickPointer == -1 ? Origin : JoystickPosition;
        FSlateDrawElement::MakeBox(OutDrawElements, ++CurrentLayer, AllottedGeometry.ToPaintGeometry(FVector2D(70 * Scale), FSlateLayoutTransform(Thumb - FVector2D(35 * Scale))), &Disc, ESlateDrawEffect::None, Cream().CopyWithNewOpacity(JoystickPointer == -1 ? .45f : .82f));
    }
    if (bExploring && Player->IsMiningGUIEnabled())
    {
        const auto* Wallet = Player->GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
        if (Wallet)
        {
            const FVector2D Position(48 * Scale, 32 * Scale);
            FSlateDrawElement::MakeBox(OutDrawElements, ++CurrentLayer, AllottedGeometry.ToPaintGeometry(FVector2D(Wallet->UsesMineralInventory() ? 400 : 190, 64) * Scale, FSlateLayoutTransform(Position)), Brush(TEXT("panel")), ESlateDrawEffect::None, Ink().CopyWithNewOpacity(.8f));
            const EMiningMineral Types[] = {EMiningMineral::Copper, EMiningMineral::Iron, EMiningMineral::Malachite, EMiningMineral::Diamond};
            const FLinearColor Colors[] = {FLinearColor::FromSRGBColor(FColor(223,161,117)), FLinearColor::FromSRGBColor(FColor(185,183,173)), Teal(), FLinearColor::FromSRGBColor(FColor(167,212,222))};
            const int32 Count = Wallet->UsesMineralInventory() ? 4 : 1;
            for (int32 Index = 0; Index < Count; ++Index)
            {
                const FVector2D Offset = Position + FVector2D(12 + 96 * Index, 17) * Scale;
                FSlateDrawElement::MakeBox(OutDrawElements, ++CurrentLayer, AllottedGeometry.ToPaintGeometry(FVector2D(30 * Scale), FSlateLayoutTransform(Offset)), Brush(Wallet->UsesMineralInventory() ? TEXT("gem") : TEXT("coin")), ESlateDrawEffect::None, Colors[Index]);
                const FString Amount = FText::AsNumber(Wallet->UsesMineralInventory() ? int64(Wallet->GetMineralCount(Types[Index])) : Wallet->Balance).ToString();
                DrawText(OutDrawElements, AllottedGeometry, ++CurrentLayer, Amount, Offset + FVector2D(38, 2) * Scale, FMath::Max(10, FMath::RoundToInt(19 * Scale)), Cream());
            }
        }
    }
    const EButton Buttons[] = { EButton::Mine, EButton::Bomb, EButton::Drill, EButton::Jetpack, EButton::Interact,
        EButton::Primary, EButton::Confirm, EButton::ThoughtLeft, EButton::ThoughtRight, EButton::Back, EButton::Edit };
    for (const EButton Button : Buttons)
    {
        if (!IsButtonVisible(Button)) continue;
        DrawButton(OutDrawElements, AllottedGeometry, CurrentLayer + 1, Button, bParentEnabled);
        CurrentLayer += 6;
    }
    return CurrentLayer;
}
