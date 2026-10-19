#include "MiningGamepadSubsystem.h"
#include "BrainMiningCharacter.h"
#include "BrainMiningPlayerController.h"
#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningBombToolComponent.h"
#include "MiningProgressionComponent.h"
#include "MiningAudioComponent.h"
#include "MiningCollection.h"
#include "MiningExperienceSubsystem.h"
#include "MiningEncounterPlayer.h"
#include "StartRoom/MiningOpening.h"
#include "EndingEmergence.h"
#include "Showcase/MiningExhibitHall.h"
#include "Showcase/MiningTreasure.h"
#include "StartRoom/MiningStartRoom.h"
#include "PopupClose/PopupCloseStage.h"
#include "WenSi/WenSiPuzzle.h"
#include "WenSi/WenSiMirrorPuzzle.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "EchoPassagePlayer.h"
#include "EchoPassageStage.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Widgets/SViewport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

namespace
{
    template<typename ActorType, typename Predicate>
    ActorType* FindGamepadContextActor(UWorld* World, Predicate Accept)
    {
        if (World) for (TActorIterator<ActorType> Actor(World); Actor; ++Actor)
            if (Accept(*Actor)) return *Actor;
        return nullptr;
    }

    class FMiningGamepadProcessor : public IInputProcessor
    {
        TWeakObjectPtr<UMiningGamepadSubsystem> Owner;
    public:
        explicit FMiningGamepadProcessor(UMiningGamepadSubsystem* InOwner) : Owner(InOwner) {}
        virtual void Tick(float DeltaSeconds, FSlateApplication&, TSharedRef<ICursor>) override
        {
            if (Owner.IsValid()) Owner->TickInput(DeltaSeconds);
        }
        virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
        {
            if (!Owner.IsValid() || Owner->IsSyntheticPointer() || !Owner->CanReceiveInput(Event.GetUserIndex())) return false;
            if (Event.GetKey() == EKeys::Escape && Owner->IsUpgradeOpen())
            {
                if (!Event.IsRepeat()) Owner->Dispatch(EMiningInputAction::Back, true);
                return true;
            }
            if (!Event.GetKey().IsGamepadKey()) { Owner->MarkKeyboardMouse(); return false; }
            return Owner->HandleButton(Event.GetKey(), true, Event.IsRepeat());
        }
        virtual bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& Event) override
        {
            if (!Owner.IsValid() || Owner->IsSyntheticPointer() || !Event.GetKey().IsGamepadKey()) return false;
            if (!Owner->CanReceiveInput(Event.GetUserIndex())) { Owner->ReleaseInputs(); return false; }
            return Owner->HandleButton(Event.GetKey(), false);
        }
        virtual bool HandleAnalogInputEvent(FSlateApplication&, const FAnalogInputEvent& Event) override
        {
            return Owner.IsValid() && Owner->CanReceiveInput(Event.GetUserIndex()) && Owner->HandleAxis(Event.GetKey(), Event.GetAnalogValue());
        }
        virtual bool HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent& Event) override
        {
            if (Owner.IsValid() && !Owner->IsSyntheticPointer() && !Owner->IsSyntheticCursorPosition(Event.GetScreenSpacePosition()) && Event.GetCursorDelta().SizeSquared() > 1.f
                && Owner->CanReceiveInput(Event.GetUserIndex())) Owner->MarkKeyboardMouse();
            return false;
        }
        virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent& Event) override
        {
            if (Owner.IsValid() && !Owner->IsSyntheticPointer() && Owner->CanReceiveInput(Event.GetUserIndex())) Owner->MarkKeyboardMouse();
            return false;
        }
    };
}

void UMiningGamepadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (!FSlateApplication::IsInitialized()) return;
    Processor = MakeShared<FMiningGamepadProcessor>(this);
    FSlateApplication::Get().RegisterInputPreProcessor(Processor, 0);
    ActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddWeakLambda(this, [this](bool bActive)
    {
        if (!bActive) ResetDevice();
    });
    ConnectionHandle = IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddWeakLambda(this,
        [this](EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId)
        {
            if (State == EInputDeviceConnectionState::Disconnected && GetLocalPlayer()->GetPlatformUserId() == User) ResetDevice();
        });
    CleanupHandle = FWorldDelegates::OnWorldCleanup.AddWeakLambda(this, [this](UWorld* World, bool, bool)
    {
        if (LastController.IsValid() && LastController->GetWorld() == World)
        {
            CloseUpgrade();
            ReleaseInputs();
            if (Hints.IsValid() && World->GetGameViewport()) World->GetGameViewport()->RemoveViewportWidgetContent(Hints.ToSharedRef());
            Hints.Reset();
            LastController.Reset();
            LastPawn.Reset();
        }
    });
}

void UMiningGamepadSubsystem::Deinitialize()
{
    CloseUpgrade();
    ReleaseInputs();
    if (Hints.IsValid() && LastController.IsValid() && LastController->GetWorld()->GetGameViewport())
        LastController->GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Hints.ToSharedRef());
    Hints.Reset();
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
        FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ActivationHandle);
    }
    IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Remove(ConnectionHandle);
    FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
    Processor.Reset();
    Super::Deinitialize();
}

APlayerController* UMiningGamepadSubsystem::Controller() const
{
    return GetLocalPlayer() ? GetLocalPlayer()->GetPlayerController(GetWorld()) : nullptr;
}

UMiningGamepadSubsystem* UMiningGamepadSubsystem::Find(const APlayerController* Player)
{
    return Player && Player->GetLocalPlayer() ? Player->GetLocalPlayer()->GetSubsystem<UMiningGamepadSubsystem>() : nullptr;
}

bool UMiningGamepadSubsystem::UsesGamepad(const APlayerController* Player)
{
    const auto* Input = Find(Player);
    return Input && Input->bGamepad;
}

FVector2D UMiningGamepadSubsystem::ApplyDeadZone(FVector2D Axis, float DeadZone)
{
    if (!FMath::IsFinite(Axis.X) || !FMath::IsFinite(Axis.Y)) return FVector2D::ZeroVector;
    const double Length = Axis.Size();
    const float Threshold = FMath::Clamp(DeadZone, 0.f, .95f);
    return Length <= Threshold ? FVector2D::ZeroVector : Axis / Length * FMath::Clamp((Length - Threshold) / (1. - Threshold), 0., 1.);
}

FVector2D UMiningGamepadSubsystem::ReadMove(const APlayerController* Player)
{
    if (!Player) return FVector2D::ZeroVector;
    FVector2D Axis(float(Player->IsInputKeyDown(EKeys::D)) - float(Player->IsInputKeyDown(EKeys::A)),
        float(Player->IsInputKeyDown(EKeys::W)) - float(Player->IsInputKeyDown(EKeys::S)));
    if (const auto* Input = Find(Player); Input && Input->GetContext() == EMiningInputContext::Exploration)
        Axis += ApplyDeadZone(Input->MoveStick, Input->StickDeadZone);
    return FVector2D(FMath::Clamp(Axis.X, -1., 1.), FMath::Clamp(Axis.Y, -1., 1.));
}

bool UMiningGamepadSubsystem::ReadJumpHeld(const APlayerController* Player)
{
    const auto* Input = Find(Player);
    return Player && (Player->IsInputKeyDown(EKeys::SpaceBar) || (Input && Input->bJumpHeld));
}

bool UMiningGamepadSubsystem::ReadDescendHeld(const APlayerController* Player)
{
    const auto* Input = Find(Player);
    return Player && (Player->IsInputKeyDown(EKeys::LeftControl) || (Input && Input->GetContext() == EMiningInputContext::Exploration && Input->HeldButtons.Contains(EKeys::Gamepad_FaceButton_Right)));
}

bool UMiningGamepadSubsystem::ReadJumpPressed(const APlayerController* Player)
{
    const auto* Input = Find(Player);
    return Player && (Player->WasInputKeyJustPressed(EKeys::SpaceBar) || (Input && Input->JumpFrame == GFrameCounter));
}

bool UMiningGamepadSubsystem::MapButton(FKey Key, EMiningInputAction& Action)
{
    if (Key == EKeys::Gamepad_RightTrigger) Action = EMiningInputAction::Primary;
    else if (Key == EKeys::Gamepad_FaceButton_Bottom || Key == EKeys::Gamepad_LeftTrigger) Action = EMiningInputAction::Jump;
    else if (Key == EKeys::Gamepad_FaceButton_Right) Action = EMiningInputAction::Back;
    else if (Key == EKeys::Gamepad_FaceButton_Left) Action = EMiningInputAction::Interact;
    else if (Key == EKeys::Gamepad_FaceButton_Top) Action = EMiningInputAction::Tool;
    else if (Key == EKeys::Gamepad_RightShoulder) Action = EMiningInputAction::Bomb;
    else if (Key == EKeys::Gamepad_LeftShoulder) Action = EMiningInputAction::ClearThought;
    else if (Key == EKeys::Gamepad_Special_Left) Action = EMiningInputAction::Map;
    else if (Key == EKeys::Gamepad_Special_Right) Action = EMiningInputAction::Pause;
    else if (Key == EKeys::Gamepad_DPad_Up) Action = EMiningInputAction::Catalogue;
    else if (Key == EKeys::Gamepad_DPad_Down) Action = EMiningInputAction::Reset;
    else if (Key == EKeys::Gamepad_DPad_Left) Action = EMiningInputAction::Previous;
    else if (Key == EKeys::Gamepad_DPad_Right) Action = EMiningInputAction::Upgrade;
    else return false;
    return true;
}

bool UMiningGamepadSubsystem::CanReceiveInput(int32 UserIndex) const
{
    const auto* Player = Controller();
    if (!Player || !Player->IsLocalController() || !Player->GetWorld()->IsGameWorld() || !FSlateApplication::IsInitialized()) return false;
    if (UserIndex != GetLocalPlayer()->GetControllerId()) return false;
    const auto* Viewport = Player->GetWorld()->GetGameViewport();
    const auto Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
    return FSlateApplication::Get().IsActive() && Widget.IsValid()
        && (Widget->HasUserFocus(UserIndex).IsSet() || Widget->HasUserFocusedDescendants(UserIndex));
}

EMiningInputContext UMiningGamepadSubsystem::GetContext() const
{
    auto* Player = Controller();
    if (!Player || !Player->GetPawn()) return EMiningInputContext::Blocked;
    if (AMiningOpening::IsOpening(Player->GetWorld()))
        return Player->bShowMouseCursor ? EMiningInputContext::Menu : EMiningInputContext::Blocked;
    UWorld* World = Player->GetWorld();
    auto* Menu = World->GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>();
    if (Menu && Menu->IsMenuOpen()) return EMiningInputContext::Menu;
    if (IsUpgradeOpen()) return EMiningInputContext::Upgrade;
    if (FindGamepadContextActor<AMiningCollectionField>(World, [](auto* Field) { return Field->bCatalogueOpen; })) return EMiningInputContext::Catalogue;
    const auto* MiningPlayer = Cast<ABrainMiningPlayerController>(Player);
    const auto* Hall = AMiningExhibitHall::Find(World);
    if ((MiningPlayer && (MiningPlayer->IsAudioSettingsOpen() || MiningPlayer->bUpgradeMenuHeld)) || (Hall && Hall->IsMapOpen())) return EMiningInputContext::Menu;
    if (Player->IsPaused()) return EMiningInputContext::Blocked;
    if (FindGamepadContextActor<APopupCloseStage>(World, [](auto* Office) { return Office->bAreaActive && Office->IsSessionActive(); })) return EMiningInputContext::Office;
    if (auto* Cards = Player->FindComponentByClass<UCardRoomInteractionComponent>(); Cards && Cards->bRoomActive && Cards->Stage && Cards->Stage->Run.Phase != ECardRoomPhase::Exploring) return EMiningInputContext::Cards;
    if (FindGamepadContextActor<AWenSiPuzzle>(World, [](auto* Puzzle) { return Puzzle->bAreaActive && Puzzle->bEditing; })
        || FindGamepadContextActor<AWenSiMirrorPuzzle>(World, [](auto* Puzzle) { return Puzzle->bAreaActive && Puzzle->bEditing; })) return EMiningInputContext::Thoughts;
    if (Player->bShowMouseCursor) return EMiningInputContext::Menu;
    return EMiningInputContext::Exploration;
}

void UMiningGamepadSubsystem::UpdateContext()
{
    auto* Player = Controller();
    const auto Context = GetContext();
    const auto* Hall = Player ? AMiningExhibitHall::Find(Player->GetWorld()) : nullptr;
    const int32 Area = Hall ? Hall->ActiveArea : INDEX_NONE;
    if (LastController.Get() != Player || LastPawn.Get() != (Player ? Player->GetPawn() : nullptr) || Context != LastContext || Area != LastArea)
    {
        ReleaseInputs();
        LastController = Player;
        LastPawn = Player ? Player->GetPawn() : nullptr;
        LastContext = Context;
        bMenuPointer = false;
        NavigationDirection = 0;
        LastArea = Area;
    }
}

bool UMiningGamepadSubsystem::HandleButton(FKey Key, bool bPressed, bool bRepeat)
{
    if (!Key.IsGamepadKey()) return false;
    UpdateContext();
    bGamepad = true;
    if (Key == EKeys::Gamepad_LeftTrigger && (!bLeftTriggerFlight || GetContext() != EMiningInputContext::Exploration)) return true;
    EMiningInputAction Action;
    if (!MapButton(Key, Action)) return true;
    if (bPressed)
    {
        if (bRepeat || HeldButtons.Contains(Key)) return true;
        HeldButtons.Add(Key);
    }
    else HeldButtons.Remove(Key);
    const auto Context = GetContext();
    if (Context != EMiningInputContext::Exploration && Context != EMiningInputContext::Blocked)
    {
        if (Key == EKeys::Gamepad_DPad_Right) Action = EMiningInputAction::Next;
        if (Key == EKeys::Gamepad_DPad_Down) Action = EMiningInputAction::Down;
        if (Key == EKeys::Gamepad_DPad_Up) Action = EMiningInputAction::Up;
    }
    Dispatch(Action, bPressed);
    return true;
}

bool UMiningGamepadSubsystem::HandleAxis(FKey Key, float Value)
{
    if (!Key.IsGamepadKey()) return false;
    UpdateContext();
    Value = FMath::IsFinite(Value) ? FMath::Clamp(Value, -1.f, 1.f) : 0.f;
    if (FMath::Abs(Value) > FMath::Max(.2f, StickDeadZone)) bGamepad = true;
    if (Key == EKeys::Gamepad_LeftX) MoveStick.X = Value;
    else if (Key == EKeys::Gamepad_LeftY) MoveStick.Y = Value;
    else if (Key == EKeys::Gamepad_RightX) LookStick.X = Value;
    else if (Key == EKeys::Gamepad_RightY) LookStick.Y = Value;
    else if (Key == EKeys::Gamepad_LeftTriggerAxis) LeftTrigger = FMath::Max(0.f, Value);
    else if (Key == EKeys::Gamepad_RightTriggerAxis) RightTrigger = FMath::Max(0.f, Value);
    return true;
}

void UMiningGamepadSubsystem::ReleaseInputs()
{
    if (HeldMining.IsValid()) HeldMining->CancelMining();
    HeldMining.Reset();
    if (auto* Character = Cast<ABrainMiningCharacter>(JumpPawn.Get())) Character->DoJumpEnd();
    else if (auto* OtherCharacter = Cast<ACharacter>(JumpPawn.Get())) OtherCharacter->StopJumping();
    JumpPawn.Reset();
    bJumpHeld = false;
    JumpFrame = MAX_uint64;
    bResetHeld = bResetFired = false;
    ResetElapsed = 0;
    if (bPointerHeld)
    {
        if (!HeldOffice.IsValid() && FSlateApplication::IsInitialized())
        {
            FSlateApplication::Get().CancelDragDrop();
            FSlateApplication::Get().ReleaseAllPointerCapture(GetLocalPlayer()->GetControllerId());
        }
        PointerButton(false);
    }

    MoveStick = LookStick = FVector2D::ZeroVector;
    LeftTrigger = RightTrigger = 0;
}

void UMiningGamepadSubsystem::MarkKeyboardMouse()
{
    if (bGamepad) ReleaseInputs();
    bGamepad = false;
}

void UMiningGamepadSubsystem::ResetDevice()
{
    ReleaseInputs();
    HeldButtons.Reset();
    bGamepad = false;
}

void UMiningGamepadSubsystem::SendUIKey(FKey Key, bool bPressed)
{
    if (!FSlateApplication::IsInitialized()) return;
    TGuardValue<bool> Synthetic(bSyntheticPointer, true);
    const FKeyEvent Event(Key, FModifierKeysState(), GetLocalPlayer()->GetControllerId(), false, 0, 0);
    if (bPressed) FSlateApplication::Get().ProcessKeyDownEvent(Event);
    else FSlateApplication::Get().ProcessKeyUpEvent(Event);
}
