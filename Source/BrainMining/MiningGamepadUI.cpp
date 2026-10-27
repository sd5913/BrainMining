#include "MiningGamepadSubsystem.h"
#include "MiningLocalizationSubsystem.h"
#include "MiningHapticsSubsystem.h"
#include "MiningGamepadHints.h"
#include "MiningGUIWidgets.h"
#include "BrainMiningCharacter.h"
#include "BrainMiningFonts.h"
#include "BrainMiningPlayerController.h"
#include "MiningComponent.h"
#include "MiningProgressionComponent.h"
#include "MiningExperienceSubsystem.h"
#include "MiningAudioComponent.h"
#include "MiningCollection.h"
#include "StartRoom/MiningStartRoom.h"
#include "PopupClose/PopupCloseStage.h"
#include "WenSi/WenSiPuzzle.h"
#include "WenSi/WenSiMirrorPuzzle.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

void UMiningGamepadSubsystem::TickInput(float DeltaSeconds)
{
    UpgradeFeedbackRemaining = FMath::Max(0.f, UpgradeFeedbackRemaining - FMath::Max(0.f, DeltaSeconds));
    UpdateContext();
    auto* Player = Controller();
    if (!Player) return;
    if (!CanReceiveInput(GetLocalPlayer()->GetControllerId())) { ResetDevice(); return; }
    const float Step = FMath::Clamp(DeltaSeconds, 0.f, .1f);
    if (!Hints.IsValid() && Player->GetWorld()->GetGameViewport())
    {
        Hints = SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(20, 0, 20, 24))
            .Visibility_Lambda([this]() { return bGamepad && !GetHintEntries().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                [MakeMiningGamepadHints(this)]];
        Player->GetWorld()->GetGameViewport()->AddViewportWidgetContent(Hints.ToSharedRef(), 2000);
    }
    const auto Context = GetContext();
    if (Hints.IsValid())
    {
        const auto HintBox = StaticCastSharedPtr<SBox>(Hints);
        HintBox->SetVAlign(Context == EMiningInputContext::Cards ? VAlign_Top : VAlign_Bottom);
        HintBox->SetPadding(FMargin(20, 24, 20, 24));
    }
    const FVector2D Move = ApplyDeadZone(MoveStick, StickDeadZone);
    const FVector2D Look = ApplyDeadZone(LookStick, StickDeadZone);
    if (Context == EMiningInputContext::Exploration)
    {
        if (auto* Character = Cast<ABrainMiningCharacter>(Player->GetPawn()); Character && !Player->IsMoveInputIgnored())
        {
            const FRotator Yaw(0, Player->GetControlRotation().Yaw, 0);
            Character->AddMovementInput(Yaw.Vector(), Move.Y);
            Character->AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Move.X);
        }
        if (!Player->IsLookInputIgnored())
        {
            Player->AddYawInput(Look.X * LookDegreesPerSecond * Step);
            Player->AddPitchInput(Look.Y * LookDegreesPerSecond * Step * (bInvertLookY ? 1.f : -1.f));
        }
        if (bResetHeld && !bResetFired)
        {
            ResetElapsed += Step;
            if (ResetElapsed >= FMath::Max(.4f, ResetHoldSeconds)) { bResetFired = true; ResetContext(); }
        }
    }
    else if (Context == EMiningInputContext::Catalogue)
    {
        for (TActorIterator<AMiningCollectionField> Field(Player->GetWorld()); Field; ++Field)
            if (Field->bCatalogueOpen)
            {
                Field->RotatePreview(Look.X * 120.f * Step, -Look.Y * 100.f * Step);
                Field->ZoomPreview((RightTrigger - LeftTrigger) * Step * 4.f);
                break;
            }
    }
    else if (Context == EMiningInputContext::Office || Context == EMiningInputContext::Thoughts || Context == EMiningInputContext::Menu)
    {
        if (!Look.IsNearlyZero()) { bMenuPointer = true; MovePointer(Look, Step); }
    }
    const bool bNavigate = Context == EMiningInputContext::Menu || Context == EMiningInputContext::Catalogue;
    int32 Direction = 0;
    if (Context == EMiningInputContext::Cards && FMath::Abs(Move.X) > .5f) Direction = Move.X > 0 ? 1 : 2;
    else if (bNavigate && Move.SizeSquared() > .25f)
        Direction = FMath::Abs(Move.X) > FMath::Abs(Move.Y) ? (Move.X > 0 ? 1 : 2) : (Move.Y > 0 ? 3 : 4);
    NavigationRemaining -= Step;
    if (Direction && (Direction != NavigationDirection || NavigationRemaining <= 0))
    {
        Dispatch(Direction == 1 ? EMiningInputAction::Next : Direction == 2 ? EMiningInputAction::Previous : Direction == 3 ? EMiningInputAction::Up : EMiningInputAction::Down, true);
        NavigationRemaining = Direction == NavigationDirection ? .14f : .35f;
    }
    NavigationDirection = Direction;
}

void UMiningGamepadSubsystem::MovePointer(FVector2D Axis, float DeltaSeconds)
{
    auto* Player = Controller();
    auto* Viewport = Player ? Player->GetWorld()->GetGameViewport() : nullptr;
    const auto Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
    if (!Widget.IsValid() || !FSlateApplication::IsInitialized()) return;
    auto& Slate = FSlateApplication::Get();
    const FGeometry Geometry = Widget->GetCachedGeometry();
    const FVector2D Size = Geometry.GetLocalSize();
    if (Size.X <= 1 || Size.Y <= 1) return;
    const FVector2D Previous = Slate.GetCursorPos();
    FVector2D Position = Geometry.AbsoluteToLocal(Previous);
    Position += FVector2D(Axis.X, -Axis.Y) * CursorPixelsPerSecond * DeltaSeconds * (LeftTrigger > .2f ? .25f : 1.f);
    Position.X = FMath::Clamp(Position.X, 1., Size.X - 1.);
    Position.Y = FMath::Clamp(Position.Y, 1., Size.Y - 1.);
    const FVector2D Absolute = Geometry.LocalToAbsolute(Position);
    TGuardValue<bool> Synthetic(bSyntheticPointer, true);
    SyntheticCursor = Absolute;
    bHasSyntheticCursor = true;
    Slate.SetCursorPos(Absolute);
    const FPointerEvent Event(GetLocalPlayer()->GetControllerId(), 0, Absolute, Previous, Slate.GetPressedMouseButtons(), EKeys::Invalid, 0, FModifierKeysState());
    Slate.ProcessMouseMoveEvent(Event, true);
}

void UMiningGamepadSubsystem::PointerButton(bool bPressed)
{
    if (!FSlateApplication::IsInitialized() || bPressed == bPointerHeld) return;
    bPointerHeld = bPressed;
    if (!bPressed && HeldOffice.IsValid())
    {
        HeldOffice->HandleGamepadPointer(false);
        HeldOffice.Reset();
        return;
    }
    auto* Player = Controller();
    if (bPressed && Player && GetContext() == EMiningInputContext::Office)
    {
        for (TActorIterator<APopupCloseStage> Office(Player->GetWorld()); Office; ++Office)
            if (Office->bAreaActive && Office->IsSessionActive())
            {
                if (Office->HandleGamepadPointer(true)) HeldOffice = *Office;
                else bPointerHeld = false;
                return;
            }
        bPointerHeld = false;
        return;
    }
    auto& Slate = FSlateApplication::Get();
    const FVector2D Position = Slate.GetCursorPos();
    TSet<FKey> Buttons = Slate.GetPressedMouseButtons();
    if (bPressed) Buttons.Add(EKeys::LeftMouseButton); else Buttons.Remove(EKeys::LeftMouseButton);
    const FPointerEvent Event(GetLocalPlayer()->GetControllerId(), 0, Position, Position, Buttons, EKeys::LeftMouseButton, 0, FModifierKeysState());
    TGuardValue<bool> Synthetic(bSyntheticPointer, true);
    if (bPressed) Slate.ProcessMouseButtonDownEvent(nullptr, Event); else Slate.ProcessMouseButtonUpEvent(Event);
}

void UMiningGamepadSubsystem::PurchaseUpgrade()
{
    auto* Player = Cast<ABrainMiningPlayerController>(Controller());
    if (!Player || !Player->GetPawn() || GetContext() != EMiningInputContext::Exploration || !Player->IsMiningGUIEnabled()
        || Player->IsMoveInputIgnored() || Player->IsLookInputIgnored() || Player->IsPaused()) return;
    if (auto* Room = AMiningStartRoom::Find(Player->GetWorld()); Room && (!Room->bPickaxeUnlocked || !Room->bJetpackUnlocked)) return;
    auto* Mining = Player->GetPawn()->FindComponentByClass<UMiningComponent>();
    if (!Mining || Mining->IsInputSuspended() || Mining->IsEncounterBlocked()) return;
    auto* Progression = Player->GetPawn()->FindComponentByClass<UMiningProgressionComponent>();
    if (!Progression || (!Progression->bInitialized && !Progression->InitializeProgression())) return;
    const bool bPurchased = Progression->Purchase(0, Progression->GetLevel(0) + 1);
    UpgradeFeedback = MiningText::Get(bPurchased?TEXT("UI.Pad.Purchased"):TEXT("UI.Pad.Rejected"));
    UpgradeFeedbackRemaining = 2.5f;
    UMiningHapticsSubsystem::Emit(Player, bPurchased ? EMiningHapticCue::UpgradeSuccess : EMiningHapticCue::UpgradeRejected);
    if (auto* Audio = Player->GetPawn()->FindComponentByClass<UMiningAudioComponent>()) Audio->PlayPurchase(bPurchased);
    if (bPurchased && Player->MiningGUI) Player->MiningGUI->RefreshMineralProgression();
}

TArray<FMiningGamepadHint> UMiningGamepadSubsystem::GetHintEntries() const
{
    using Glyph = EMiningGamepadGlyph;
    const auto Context = GetContext();
    if (Context == EMiningInputContext::Blocked) return {};
    if (Context == EMiningInputContext::Cards)
    {
        const auto* Cards = Controller()->FindComponentByClass<UCardRoomInteractionComponent>();
        if (Cards && Cards->Stage && Cards->Stage->Run.Phase == ECardRoomPhase::Choosing)
            return {{Glyph::DPad, MiningText::Get(TEXT("UI.Pad.Choose")).ToString()}, {Glyph::A, MiningText::Get(TEXT("UI.Pad.Toggle")).ToString()}, {Glyph::Y, MiningText::Get(TEXT("UI.Pad.ToggleAll")).ToString()}, {Glyph::B, MiningText::Get(TEXT("UI.Pad.Clear")).ToString()}, {Glyph::X, Cards->Stage->Run.Round == 4 && !Cards->Stage->Run.CanConfirm() ? MiningText::Get(TEXT("UI.Pad.SelectAll")).ToString() : FText::Format(MiningText::Get(TEXT("UI.Pad.Play")),Cards->Stage->Run.Staged.Num(),Cards->Stage->Run.Hand.Num()).ToString()}, {Glyph::Menu, MiningText::Get(TEXT("UI.Pad.Pause")).ToString()}};
        if (Cards && Cards->Stage && Cards->Stage->Run.Phase == ECardRoomPhase::Victory)
            return {{Glyph::X, MiningText::Get(TEXT("UI.Pad.Reward")).ToString(), Glyph::A}, {Glyph::Menu, MiningText::Get(TEXT("UI.Pad.Pause")).ToString()}};
        return {{Glyph::Menu, MiningText::Get(TEXT("UI.Pad.Waiting")).ToString()}};
    }
    if (Context == EMiningInputContext::Office)
        return {{Glyph::RightStick, MiningText::Get(TEXT("UI.Pad.Cursor")).ToString()}, {Glyph::LT, MiningText::Get(TEXT("UI.Pad.Fine")).ToString()}, {Glyph::A, MiningText::Get(TEXT("UI.Pad.Click")).ToString(), Glyph::RT}, {Glyph::B, MiningText::Get(TEXT("UI.Pad.LeaveComputer")).ToString()}};
    if (Context == EMiningInputContext::Catalogue)
        return {{Glyph::LB, MiningText::Get(TEXT("UI.Pad.Collection")).ToString(), Glyph::RB}, {Glyph::RightStick, MiningText::Get(TEXT("UI.Pad.Rotate")).ToString()}, {Glyph::LT, MiningText::Get(TEXT("UI.Pad.Zoom")).ToString(), Glyph::RT}, {Glyph::Y, MiningText::Get(TEXT("UI.Pad.Reset")).ToString()}, {Glyph::B, MiningText::Get(TEXT("UI.Pad.Back")).ToString()}};
    if (Context == EMiningInputContext::Menu)
        return {{Glyph::DPad, MiningText::Get(TEXT("UI.Pad.Navigate")).ToString()}, {Glyph::A, MiningText::Get(TEXT("UI.Pad.Confirm")).ToString()}, {Glyph::RightStick, MiningText::Get(TEXT("UI.Pad.Cursor")).ToString()}, {Glyph::RT, MiningText::Get(TEXT("UI.Pad.Click")).ToString()}, {Glyph::B, MiningText::Get(TEXT("UI.Pad.Back")).ToString()}};
    if (Context == EMiningInputContext::Thoughts)
        return {{Glyph::A, MiningText::Get(TEXT("UI.Pad.Swap")).ToString()}, {Glyph::RightStick, MiningText::Get(TEXT("UI.Pad.Cursor")).ToString()}, {Glyph::RT, MiningText::Get(TEXT("UI.Pad.Drag")).ToString()}, {Glyph::LB, MiningText::Get(TEXT("UI.Pad.Thoughts")).ToString(), Glyph::RB}, {Glyph::B, MiningText::Get(TEXT("UI.Pad.Back")).ToString()}};
    if (UpgradeFeedbackRemaining > 0) return {{Glyph::LT, UpgradeFeedback.ToString()}};
    if (const auto* Player = Controller())
    {
        for (TActorIterator<AWenSiMirrorPuzzle> Puzzle(Player->GetWorld()); Puzzle; ++Puzzle)
            if (Puzzle->bAreaActive)
                return {{Glyph::X, MiningText::Get(TEXT("UI.Pad.Think")).ToString()}, {Glyph::RB, MiningText::Get(TEXT("UI.Pad.SwapProperties")).ToString()}, {Glyph::LB, MiningText::Get(TEXT("UI.Pad.ClearThoughts")).ToString()}, {Glyph::Y, MiningText::Get(TEXT("UI.Pad.Edit")).ToString()}, {Glyph::A, MiningText::Get(TEXT("UI.Pad.Fly")).ToString()}};
        for (TActorIterator<AWenSiPuzzle> Puzzle(Player->GetWorld()); Puzzle; ++Puzzle)
            if (Puzzle->bAreaActive)
                return {{Glyph::X, MiningText::Get(TEXT("UI.Pad.Think")).ToString()}, {Glyph::Y, MiningText::Get(TEXT("UI.Pad.Edit")).ToString()}, {Glyph::LB, MiningText::Get(TEXT("UI.Pad.CloseThoughts")).ToString(), Glyph::RB}, {Glyph::A, MiningText::Get(TEXT("UI.Pad.Fly")).ToString()}};
    }
    if (bResetHeld)
        return {{Glyph::DPad, FText::Format(MiningText::Get(TEXT("UI.Pad.ResetProgress")),FMath::Clamp(FMath::RoundToInt(ResetElapsed / FMath::Max(.4f, ResetHoldSeconds) * 100),0,100)).ToString()}};
    return {{Glyph::A, MiningText::Get(TEXT("UI.Pad.HoldFly")).ToString()}, {Glyph::RT, MiningText::Get(TEXT("UI.Pad.Use")).ToString()}, {Glyph::X, MiningText::Get(TEXT("UI.Pad.Interact")).ToString()}, {Glyph::Y, MiningText::Get(TEXT("UI.Pad.Drill")).ToString()},
        {Glyph::RB, MiningText::Get(TEXT("UI.Pad.Bomb")).ToString()}, {Glyph::DPadUp, MiningText::Get(TEXT("UI.Pad.Catalogue")).ToString()}, {Glyph::LT, MiningText::Get(TEXT("UI.Pad.Upgrade")).ToString()}, {Glyph::Menu, MiningText::Get(TEXT("UI.Pad.Pause")).ToString()}};
}

FString UMiningGamepadSubsystem::GetHint() const
{
    FString Result;
    for (const auto& Entry : GetHintEntries())
    {
        if (!Result.IsEmpty()) Result += TEXT("   ");
        Result += MiningGamepadGlyphName(Entry.Key);
        if (Entry.AlternateKey != EMiningGamepadGlyph::None) Result += FString(TEXT("/")) + MiningGamepadGlyphName(Entry.AlternateKey);
        Result += TEXT(" ") + Entry.Label;
    }
    return Result;
}
