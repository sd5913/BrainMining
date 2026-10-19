#include "MiningGamepadSubsystem.h"
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
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

void UMiningGamepadSubsystem::TickInput(float DeltaSeconds)
{
    if (IsUpgradeOpen() && (UpgradeController.Get() != Controller() || !UpgradePawn.IsValid() || UpgradeController->GetPawn() != UpgradePawn.Get())) CloseUpgrade();
    UpdateContext();
    auto* Player = Controller();
    if (!Player) return;
    if (!CanReceiveInput(GetLocalPlayer()->GetControllerId())) { ResetDevice(); return; }
    const float Step = FMath::Clamp(DeltaSeconds, 0.f, .1f);
    if (!Hints.IsValid() && Player->GetWorld()->GetGameViewport())
    {
        Hints = SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(20, 0, 20, 24))
            .Visibility_Lambda([this]() { return bGamepad && !GetHint().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
            [SNew(SBorder).BorderBackgroundColor(FLinearColor(0, 0, 0, .8f)).Padding(FMargin(18, 10))
                [SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(GetHint()); })
                    .ColorAndOpacity(FLinearColor::White).Font(BrainMiningFonts::Style("Regular", 14))]];
        Player->GetWorld()->GetGameViewport()->AddViewportWidgetContent(Hints.ToSharedRef(), 2000);
    }
    const auto Context = GetContext();
    if (auto* Cards = Player->FindComponentByClass<UCardRoomInteractionComponent>())
    {
        Cards->bGamepadNavigation = bGamepad;
        if (Cards->Stage) Cards->FocusedCard = FMath::Clamp(Cards->FocusedCard, 0, FMath::Max(0, Cards->Stage->Run.Hand.Num() - 1));
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
    else if (Context == EMiningInputContext::Office || Context == EMiningInputContext::Thoughts || Context == EMiningInputContext::Menu || Context == EMiningInputContext::Upgrade)
    {
        if (!Look.IsNearlyZero()) { bMenuPointer = true; MovePointer(Look, Step); }
    }
    const bool bNavigate = Context == EMiningInputContext::Cards || Context == EMiningInputContext::Menu || Context == EMiningInputContext::Catalogue || Context == EMiningInputContext::Upgrade;
    int32 Direction = 0;
    if (bNavigate && Move.SizeSquared() > .25f)
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

void UMiningGamepadSubsystem::ToggleUpgrade()
{
    if (IsUpgradeOpen()) { CloseUpgrade(); return; }
    auto* Player = Cast<ABrainMiningPlayerController>(Controller());
    if (!Player || GetContext() != EMiningInputContext::Exploration || !Player->IsMiningGUIEnabled() || Player->IsMoveInputIgnored() || Player->IsLookInputIgnored()) return;
    if (auto* Room = AMiningStartRoom::Find(Player->GetWorld()); Room && (!Room->bPickaxeUnlocked || !Room->bJetpackUnlocked)) return;
    auto* Progression = Player->GetPawn()->FindComponentByClass<UMiningProgressionComponent>();
    auto* Viewport = Player->GetWorld()->GetGameViewport();
    if (!Progression || !Viewport) return;
    if (!Progression->bInitialized && !Progression->InitializeProgression()) return;
    ReleaseInputs();
    UpgradeController = Player;
    UpgradePawn = Player->GetPawn();
    UpgradeMining = Player->GetPawn()->FindComponentByClass<UMiningComponent>();
    bUpgradeMiningSuspended = UpgradeMining.IsValid() && UpgradeMining->IsInputSuspended();
    if (UpgradeMining.IsValid()) UpgradeMining->SetInputSuspended(true);
    UpgradeTrack = 0;
    UpgradeFeedback = FText::GetEmpty();
    TSharedPtr<SButton> Confirm;
    UpgradePanel = SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SBorder).Padding(28).BorderBackgroundColor(FLinearColor(.025f, .035f, .05f, .98f))
            [SNew(SBox).WidthOverride(680)
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 20)
                    [SNew(STextBlock).Text(FText::FromString(TEXT("装备升级"))).Font(BrainMiningFonts::Style("Bold", 28))]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 20)
                    [SNew(STextBlock).Text_Lambda([this]() { return UpgradeDescription(); }).WrapTextAt(640).Font(BrainMiningFonts::Style("Regular", 18))]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
                    [SNew(STextBlock).Text_Lambda([this]() { return UpgradeFeedback; }).WrapTextAt(640)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
                    [SAssignNew(Confirm, SButton).Text(FText::FromString(TEXT("A / 确认购买")))
                        .OnClicked_Lambda([this]() { PurchaseUpgrade(); return FReply::Handled(); })]
                    + SVerticalBox::Slot().AutoHeight()
                    [SNew(SButton).Text(FText::FromString(TEXT("B / 返回")))
                        .OnClicked_Lambda([this]() { CloseUpgrade(); return FReply::Handled(); })]
                ]]];
    Viewport->AddViewportWidgetContent(UpgradePanel.ToSharedRef(), 500);
    Player->SetIgnoreMoveInput(true);
    Player->SetIgnoreLookInput(true);
    bUpgradePaused = Player->SetPause(true);
    Player->bShowMouseCursor = true;
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(Confirm);
    Player->SetInputMode(Mode);
}

void UMiningGamepadSubsystem::CloseUpgrade()
{
    if (!IsUpgradeOpen()) return;
    ReleaseInputs();
    auto* Player = UpgradeController.Get();
    if (Player && Player->GetGameInstance())
        if (auto* Menu = Player->GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>(); Menu && Menu->IsMenuOpen()) Menu->CloseMenu();
    if (Player && Player->GetWorld()->GetGameViewport()) Player->GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(UpgradePanel.ToSharedRef());
    UpgradePanel.Reset();
    if (UpgradeMining.IsValid()) UpgradeMining->SetInputSuspended(bUpgradeMiningSuspended);
    if (Player)
    {
        if (bUpgradePaused) Player->SetPause(false);
        Player->SetIgnoreMoveInput(false);
        Player->SetIgnoreLookInput(false);
        Player->bShowMouseCursor = false;
        Player->SetInputMode(FInputModeGameOnly());
    }
    bUpgradePaused = false;
    UpgradeController.Reset();
    UpgradePawn.Reset();
    UpgradeMining.Reset();
}

void UMiningGamepadSubsystem::PurchaseUpgrade()
{
    auto* Player = UpgradeController.Get();
    auto* Progression = Player && Player->GetPawn() ? Player->GetPawn()->FindComponentByClass<UMiningProgressionComponent>() : nullptr;
    if (!IsUpgradeOpen() || !Player || Player->GetPawn() != UpgradePawn.Get() || !Progression || !Progression->Tracks.IsValidIndex(UpgradeTrack)) return;
    const bool bPurchased = Progression->Purchase(UpgradeTrack, Progression->GetLevel(UpgradeTrack) + 1);
    UpgradeFeedback = FText::FromString(bPurchased ? TEXT("升级成功") : TEXT("无法升级：资源不足或已达到最高等级"));
    if (auto* Audio = Player->GetPawn()->FindComponentByClass<UMiningAudioComponent>()) Audio->PlayPurchase(bPurchased);
}

FText UMiningGamepadSubsystem::UpgradeDescription() const
{
    const auto* Player = UpgradeController.Get();
    const auto* Progression = Player && Player->GetPawn() ? Player->GetPawn()->FindComponentByClass<UMiningProgressionComponent>() : nullptr;
    if (!Progression || !Progression->Tracks.IsValidIndex(UpgradeTrack)) return FText::FromString(TEXT("暂无可用升级"));
    const FString Cost = Progression->UsesMineralInventory() ? Progression->GetNextMaterialRequirements().ToString() : FString::Printf(TEXT("需要金币：%lld"), Progression->GetNextPrice(UpgradeTrack));
    return FText::FromString(FString::Printf(TEXT("%s · 等级 %d\n\n%s\n\n%s\n\n方向键切换项目 · A 购买 · B 返回"),
        *Progression->Tracks[UpgradeTrack].Name.ToString(), Progression->GetLevel(UpgradeTrack), *Progression->GetNextDescription(UpgradeTrack).ToString(), *Cost));
}

FString UMiningGamepadSubsystem::GetHint() const
{
    const auto Context = GetContext();
    if (Context == EMiningInputContext::Blocked || Context == EMiningInputContext::Upgrade) return FString();
    if (Context == EMiningInputContext::Cards) return TEXT("方向键 选牌   X 选择/取消   A 确认   Y 末轮全选   B 离座");
    if (Context == EMiningInputContext::Office) return TEXT("右摇杆 光标   LT 精调   A / RT 点击   B 离开电脑");
    if (Context == EMiningInputContext::Catalogue) return TEXT("LB / RB 切换藏品   右摇杆 旋转   LT / RT 缩放   Y 复位   B 返回");
    if (Context == EMiningInputContext::Menu) return TEXT("方向键 导航   A 确认   右摇杆 光标   RT 点击   B 返回");
    if (Context == EMiningInputContext::Thoughts) return TEXT("A 交换   右摇杆 + RT 拖拽   LB / RB 思维操作   B 返回");
    if (const auto* Player = Controller())
    {
        for (TActorIterator<AWenSiMirrorPuzzle> Puzzle(Player->GetWorld()); Puzzle; ++Puzzle)
            if (Puzzle->bAreaActive) return TEXT("X 思考/交互   RB 交换属性   LB 清空思维   Y 编辑   A 跳跃/飞行");
        for (TActorIterator<AWenSiPuzzle> Puzzle(Player->GetWorld()); Puzzle; ++Puzzle)
            if (Puzzle->bAreaActive) return TEXT("X 思考/交互   Y 编辑   LB / RB 关闭左右思维   A 跳跃/飞行");
    }
    if (bResetHeld) return FString::Printf(TEXT("重置：%d%% · 松开取消"), FMath::Clamp(FMath::RoundToInt(ResetElapsed / FMath::Max(.4f, ResetHoldSeconds) * 100), 0, 100));
    return TEXT("A 跳跃/按住飞行   RT 操作   X 交互   Y 钻机   RB 炸弹   ↑ 图鉴   → 升级   View 地图   Menu 暂停");
}
