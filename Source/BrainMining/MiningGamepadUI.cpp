#include "MiningGamepadSubsystem.h"
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
    UpgradeFeedback = FText::FromString(bPurchased ? TEXT("升级成功") : TEXT("暂时无法升级：资源不足或已满级"));
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
            return {{Glyph::DPad, TEXT("左右选牌")}, {Glyph::A, TEXT("选中/取消")}, {Glyph::Y, TEXT("全选/清空")}, {Glyph::B, TEXT("清空")}, {Glyph::X, Cards->Stage->Run.Round == 4 && !Cards->Stage->Run.CanConfirm() ? TEXT("全选") : FString::Printf(TEXT("出牌 %d/%d"), Cards->Stage->Run.Staged.Num(), Cards->Stage->Run.Hand.Num())}, {Glyph::Menu, TEXT("暂停")}};
        if (Cards && Cards->Stage && Cards->Stage->Run.Phase == ECardRoomPhase::Victory)
            return {{Glyph::X, TEXT("领取钻机"), Glyph::A}, {Glyph::Menu, TEXT("暂停")}};
        return {{Glyph::Menu, TEXT("暂停 · 等待牌局演出")}};
    }
    if (Context == EMiningInputContext::Office)
        return {{Glyph::RightStick, TEXT("光标")}, {Glyph::LT, TEXT("精调")}, {Glyph::A, TEXT("点击"), Glyph::RT}, {Glyph::B, TEXT("离开电脑")}};
    if (Context == EMiningInputContext::Catalogue)
        return {{Glyph::LB, TEXT("切换藏品"), Glyph::RB}, {Glyph::RightStick, TEXT("旋转")}, {Glyph::LT, TEXT("缩放"), Glyph::RT}, {Glyph::Y, TEXT("复位")}, {Glyph::B, TEXT("返回")}};
    if (Context == EMiningInputContext::Menu)
        return {{Glyph::DPad, TEXT("导航")}, {Glyph::A, TEXT("确认")}, {Glyph::RightStick, TEXT("光标")}, {Glyph::RT, TEXT("点击")}, {Glyph::B, TEXT("返回")}};
    if (Context == EMiningInputContext::Thoughts)
        return {{Glyph::A, TEXT("交换")}, {Glyph::RightStick, TEXT("光标")}, {Glyph::RT, TEXT("按住拖拽")}, {Glyph::LB, TEXT("思维操作"), Glyph::RB}, {Glyph::B, TEXT("返回")}};
    if (UpgradeFeedbackRemaining > 0) return {{Glyph::LT, UpgradeFeedback.ToString()}};
    if (const auto* Player = Controller())
    {
        for (TActorIterator<AWenSiMirrorPuzzle> Puzzle(Player->GetWorld()); Puzzle; ++Puzzle)
            if (Puzzle->bAreaActive)
                return {{Glyph::X, TEXT("思考/交互")}, {Glyph::RB, TEXT("交换属性")}, {Glyph::LB, TEXT("清空思维")}, {Glyph::Y, TEXT("编辑")}, {Glyph::A, TEXT("跳跃/飞行")}};
        for (TActorIterator<AWenSiPuzzle> Puzzle(Player->GetWorld()); Puzzle; ++Puzzle)
            if (Puzzle->bAreaActive)
                return {{Glyph::X, TEXT("思考/交互")}, {Glyph::Y, TEXT("编辑")}, {Glyph::LB, TEXT("关闭左右思维"), Glyph::RB}, {Glyph::A, TEXT("跳跃/飞行")}};
    }
    if (bResetHeld)
        return {{Glyph::DPad, FString::Printf(TEXT("重置：%d%% · 松开取消"), FMath::Clamp(FMath::RoundToInt(ResetElapsed / FMath::Max(.4f, ResetHoldSeconds) * 100), 0, 100))}};
    return {{Glyph::A, TEXT("跳跃/按住飞行")}, {Glyph::RT, TEXT("操作")}, {Glyph::X, TEXT("交互")}, {Glyph::Y, TEXT("钻机")},
        {Glyph::RB, TEXT("炸弹")}, {Glyph::DPadUp, TEXT("图鉴")}, {Glyph::LT, TEXT("升级")}, {Glyph::Menu, TEXT("暂停")}};
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
