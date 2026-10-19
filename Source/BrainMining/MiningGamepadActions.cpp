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
#include "WenSi/MouseMuseumPortal.h"
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
    ActorType* FindGamepadActionActor(UWorld* World, Predicate Accept)
    {
        if (World) for (TActorIterator<ActorType> Actor(World); Actor; ++Actor)
            if (Accept(*Actor)) return *Actor;
        return nullptr;
    }
}

void UMiningGamepadSubsystem::Primary(bool bPressed)
{
    if (!bPressed)
    {
        if (HeldMining.IsValid()) HeldMining->EndMiningInput();
        HeldMining.Reset();
        if (bPointerHeld) PointerButton(false);
        return;
    }
    auto* Player = Controller();
    if (!Player || !Player->GetPawn()) return;
    const auto Context = GetContext();
    if (Context == EMiningInputContext::Cards) return;
    if (Context == EMiningInputContext::Office || Context == EMiningInputContext::Menu || Context == EMiningInputContext::Thoughts)
    {
        PointerButton(true);
        return;
    }
    if (Context != EMiningInputContext::Exploration || Player->IsMoveInputIgnored() || Player->IsLookInputIgnored()) return;
    auto* Mining = Player->GetPawn()->FindComponentByClass<UMiningComponent>();
    if (Mining && !Mining->IsInputSuspended() && !Mining->IsEncounterBlocked())
    {
        if (auto* Bomb = Player->GetPawn()->FindComponentByClass<UMiningBombToolComponent>(); Bomb && Bomb->bEquipped)
        {
            Mining->CancelMining();
            Bomb->ThrowBomb();
            return;
        }
        Mining->BeginMiningInput();
        HeldMining = Mining;
    }
    else if (auto* Echo = Player->FindComponentByClass<UEchoPassageInteractionComponent>()) Echo->Knock();
}

void UMiningGamepadSubsystem::Interact()
{
    auto* Player = Controller();
    if (!Player || !Player->GetPawn() || Player->IsPaused()) return;
    UWorld* World = Player->GetWorld();
    if (auto* Room = AMiningStartRoom::Find(World); Room && Room->Interact(Player)) return;
    for (TActorIterator<APopupCloseStage> Office(World); Office; ++Office) if (Office->HandleInteractionInput(Player)) return;
    for (TActorIterator<AWenSiPuzzle> Puzzle(World); Puzzle; ++Puzzle) if (Puzzle->HandleInteractionInput(Player)) return;
    for (TActorIterator<AWenSiMirrorPuzzle> Puzzle(World); Puzzle; ++Puzzle)
    {
        if (Puzzle->bAreaActive && Puzzle->MousePortal)
        {
            Puzzle->MousePortal->RefreshFocus();
            if (Puzzle->MousePortal->IsEntering() || (Puzzle->MousePortal->IsFocused() && Puzzle->MousePortal->TryEnter())) return;
        }
        if (Puzzle->HandleInteractionInput(Player)) return;
    }
    if (auto* Cards = Player->FindComponentByClass<UCardRoomInteractionComponent>(); Cards && Cards->bRoomActive && Cards->Stage)
    {
        if (Cards->Stage->Run.Phase == ECardRoomPhase::Choosing) return;
        if (Cards->Stage->Run.Phase == ECardRoomPhase::Victory || Cards->Stage->IsPlayerNear(Player->GetPawn()->GetActorLocation())) { Cards->Interact(); return; }
    }
    for (TActorIterator<AMiningCollectionField> Field(World); Field; ++Field) if (Field->TryCollectTarget()) return;
    for (TActorIterator<AMiningTreasureField> Field(World); Field; ++Field) if (Field->IsAreaActive() && Field->CollectTarget(Player)) return;
    if (auto* Hall = AMiningExhibitHall::Find(World); Hall && Hall->ActiveArea == INDEX_NONE && !Hall->IsInteractionOpen()) Hall->InteractPortal();
}

void UMiningGamepadSubsystem::Dispatch(EMiningInputAction Action, bool bPressed)
{
    auto* Player = Controller();
    if (!Player) return;
    if (Action == EMiningInputAction::Primary) { Primary(bPressed); return; }
    if (!bPressed)
    {
        if (Action == EMiningInputAction::Jump)
        {
            if (GetContext() == EMiningInputContext::Exploration && HeldButtons.Contains(EKeys::Gamepad_FaceButton_Bottom)) return;
            if (auto* Character = Cast<ABrainMiningCharacter>(JumpPawn.Get())) Character->DoJumpEnd();
            JumpPawn.Reset();
            bJumpHeld = false;

        }
        if ((Action == EMiningInputAction::Jump || Action == EMiningInputAction::Interact) && bPointerHeld) PointerButton(false);
        if (Action == EMiningInputAction::Reset) { bResetHeld = false; ResetElapsed = 0; bResetFired = false; }
        return;
    }
    const auto Context = GetContext();
    if (Context == EMiningInputContext::Blocked) return;
    if (Action == EMiningInputAction::Back)
    {
        if (Context == EMiningInputContext::Exploration)
            if (auto* Flight = Player->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>(); Flight && Flight->bCreativeFlight) return;
        Back();
        return;
    }
    if (Action == EMiningInputAction::Pause)
    {
        ReleaseInputs();
        if (auto* Menu = Player->GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()) Menu->TogglePauseMenu();
        return;
    }
    // The seated card encounter uses mouse selection and Space only. Pause remains available.
    if (Context == EMiningInputContext::Cards) return;
    if (Action == EMiningInputAction::Previous || Action == EMiningInputAction::Next || Action == EMiningInputAction::Up || Action == EMiningInputAction::Down)
    {
        if (Context == EMiningInputContext::Menu)
        {
            bMenuPointer = false;
            const FKey Key = Action == EMiningInputAction::Previous ? EKeys::Left : Action == EMiningInputAction::Next ? EKeys::Right : Action == EMiningInputAction::Up ? EKeys::Up : EKeys::Down;
            SendUIKey(Key, true);
            SendUIKey(Key, false);
        }
        else Navigate(Action == EMiningInputAction::Previous || Action == EMiningInputAction::Up ? -1 : 1);
        return;
    }
    if (Context == EMiningInputContext::Catalogue)
    {
        if (Action == EMiningInputAction::Tool)
            if (auto* Field = FindGamepadActionActor<AMiningCollectionField>(Player->GetWorld(), [](auto* Candidate) { return Candidate->bCatalogueOpen; })) Field->ResetPreview();
        if (Action == EMiningInputAction::Bomb) Navigate(1);
        if (Action == EMiningInputAction::ClearThought) Navigate(-1);
        return;
    }
    if (Context == EMiningInputContext::Menu)
    {
        if (Action == EMiningInputAction::Jump)
        {
            if (bMenuPointer) PointerButton(true);
            else { SendUIKey(EKeys::Enter, true); SendUIKey(EKeys::Enter, false); }
        }
        return;
    }
    if (Action == EMiningInputAction::Interact) { Interact(); return; }
    if (Context == EMiningInputContext::Office)
    {
        if (Action == EMiningInputAction::Jump) PointerButton(true);
        return;
    }
    auto* Mirror = FindGamepadActionActor<AWenSiMirrorPuzzle>(Player->GetWorld(), [](auto* Puzzle) { return Puzzle->bAreaActive; });
    auto* Puzzle = FindGamepadActionActor<AWenSiPuzzle>(Player->GetWorld(), [](auto* Candidate) { return Candidate->bAreaActive; });
    if (Mirror || Puzzle)
    {
        if (Action == EMiningInputAction::Tool) { if (Mirror) Mirror->ToggleEdit(); else Puzzle->ToggleEdit(); return; }
        if (Action == EMiningInputAction::ClearThought) { if (Mirror) Mirror->CloseThoughts(); else Puzzle->CloseThought(0); return; }
        if (Action == EMiningInputAction::Bomb) { if (Mirror) Mirror->SwapThoughts(); else Puzzle->CloseThought(1); return; }
        if (Context == EMiningInputContext::Thoughts && Action == EMiningInputAction::Jump)
        {
            if (Mirror) Mirror->SwapThoughts(); else Puzzle->SwapRules(0, 1);
            return;
        }
    }
    if (Context != EMiningInputContext::Exploration) return;
    if (auto* Room = AMiningStartRoom::Find(Player->GetWorld()); Room && (!Room->bPickaxeUnlocked || !Room->bJetpackUnlocked))
        if (Action == EMiningInputAction::Tool || Action == EMiningInputAction::Bomb || Action == EMiningInputAction::Upgrade) return;
    if (Action == EMiningInputAction::Jump)
    {
        if (bJumpHeld) return;
        bJumpHeld = true;
        JumpFrame = GFrameCounter;
        JumpPawn = Player->GetPawn();
        if (auto* Character = Cast<ABrainMiningCharacter>(JumpPawn.Get())) Character->DoJumpStart();
    }
    else if (Action == EMiningInputAction::Tool)
    {
        if (auto* MiningPlayer = Cast<ABrainMiningPlayerController>(Player)) MiningPlayer->HandleMobileDrill();
    }
    else if (Action == EMiningInputAction::Bomb)
    {
        if (auto* MiningPlayer = Cast<ABrainMiningPlayerController>(Player); MiningPlayer && MiningPlayer->IsMiningGUIEnabled()
            && !Player->IsMoveInputIgnored() && !Player->IsLookInputIgnored())
            if (auto* Bomb = Player->GetPawn()->FindComponentByClass<UMiningBombToolComponent>()) Bomb->ToggleEquipped();
    }
    else if (Action == EMiningInputAction::Upgrade) PurchaseUpgrade();
    else if (Action == EMiningInputAction::Reset) { bResetHeld = true; bResetFired = false; ResetElapsed = 0; }
    else if (Action == EMiningInputAction::Catalogue)
    {
        if (auto* Hall = AMiningExhibitHall::Find(Player->GetWorld())) Hall->ToggleCatalogue();
        else if (auto* Field = FindGamepadActionActor<AMiningCollectionField>(Player->GetWorld(), [](auto* Candidate) { return Candidate->IsAreaActive(); })) Field->ToggleCatalogue();
    }
}

void UMiningGamepadSubsystem::Back()
{
    auto* Player = Controller();
    if (!Player) return;
    ReleaseInputs();
    if (auto* Menu = Player->GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>(); Menu && Menu->IsMenuOpen())
    {
        if (Menu->bQuitConfirmation) Menu->CancelQuitConfirmation(); else Menu->CloseMenu();
        return;
    }
    UWorld* World = Player->GetWorld();
    if (auto* Field = FindGamepadActionActor<AMiningCollectionField>(World, [](auto* Candidate) { return Candidate->bCatalogueOpen; })) { Field->ToggleCatalogue(); return; }
    if (auto* MiningPlayer = Cast<ABrainMiningPlayerController>(Player))
    {
        if (MiningPlayer->IsAudioSettingsOpen()) { MiningPlayer->ToggleAudioSettings(); return; }
        if (MiningPlayer->bUpgradeMenuHeld) { MiningPlayer->SetUpgradeMenuHeld(false); return; }
    }
    if (auto* Hall = AMiningExhibitHall::Find(World); Hall && Hall->IsMapOpen()) { Hall->CloseMap(); return; }

    if (auto* Office = FindGamepadActionActor<APopupCloseStage>(World, [](auto* Candidate) { return Candidate->bAreaActive && Candidate->IsSessionActive(); })) { Office->HandleInteractionInput(Player); return; }
    if (auto* Mirror = FindGamepadActionActor<AWenSiMirrorPuzzle>(World, [](auto* Puzzle) { return Puzzle->bAreaActive; }))
    {
        if (Mirror->bEditing) Mirror->ToggleEdit(); else Mirror->CloseThoughts();
        return;
    }
    if (auto* Puzzle = FindGamepadActionActor<AWenSiPuzzle>(World, [](auto* Candidate) { return Candidate->bAreaActive && Candidate->bEditing; })) { Puzzle->ToggleEdit(); return; }
}

void UMiningGamepadSubsystem::ResetContext()
{
    auto* Player = Controller();
    if (!Player || GetContext() != EMiningInputContext::Exploration) return;
    if (auto* Hall = AMiningExhibitHall::Find(Player->GetWorld()))
    {
        if (Hall->AreaKind(Hall->ActiveArea) == EMiningExperienceKind::Ending) return;
    }
    else if (TActorIterator<AEndingEmergence>(Player->GetWorld())) return;
    UWorld* World = Player->GetWorld();
    if (auto* Mirror = FindGamepadActionActor<AWenSiMirrorPuzzle>(World, [](auto* Puzzle) { return Puzzle->bAreaActive; })) { Mirror->ResetPuzzle(); return; }
    if (auto* Puzzle = FindGamepadActionActor<AWenSiPuzzle>(World, [](auto* Candidate) { return Candidate->bAreaActive; })) { Puzzle->ResetPuzzle(); return; }
    if (auto* Encounter = Cast<AMiningEncounterPlayerController>(Player); Encounter && Encounter->IsCardRoomContext()) return;
    if (auto* Field = FindGamepadActionActor<AMiningCollectionField>(World, [](auto* Candidate) { return Candidate->IsAreaActive() && !Candidate->bUseAuthoredPlacements; })) { Field->ResetCollection(); return; }
    if (auto* Echo = Player->FindComponentByClass<UEchoPassageInteractionComponent>(); Echo && Echo->bExplorationActive) { Echo->Restart(); return; }
    if (auto* Mining = Player->GetPawn() ? Player->GetPawn()->FindComponentByClass<UMiningComponent>() : nullptr;
        Mining && Mining->IsInputContextEnabled() && !Mining->IsInputSuspended() && !Mining->IsEncounterBlocked()) Mining->ResetField();
}

void UMiningGamepadSubsystem::Navigate(int32 Direction)
{
    auto* Player = Controller();
    if (!Player) return;
    if (auto* Field = FindGamepadActionActor<AMiningCollectionField>(Player->GetWorld(), [](auto* Candidate) { return Candidate->bCatalogueOpen; }))
    {
        const TArray<int32> Ids = Field->CatalogueIds();
        if (!Ids.IsEmpty()) Field->SelectItem(Ids[(FMath::Max(0, Ids.IndexOfByKey(Field->SelectedId)) + Direction + Ids.Num()) % Ids.Num()]);
        return;
    }
    if (GetContext() == EMiningInputContext::Menu && FSlateApplication::IsInitialized())
    {
        TGuardValue<bool> Synthetic(bSyntheticPointer, true);
        const FKey Key = Direction < 0 ? EKeys::Up : EKeys::Down;
        FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(Key, FModifierKeysState(), GetLocalPlayer()->GetControllerId(), false, 0, 0));
        FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(Key, FModifierKeysState(), GetLocalPlayer()->GetControllerId(), false, 0, 0));
    }
}
