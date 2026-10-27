// Copyright Epic Games, Inc. All Rights Reserved.


#include "BrainMiningPlayerController.h"
#include "MiningLocalizationSubsystem.h"
#include "EndingCreditsInput.h"
#include "EndingEmergence.h"
#include "MobileUIWidgets.h"
#include "MiningExperienceSubsystem.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "MiningHapticsSubsystem.h"
#include "Widgets/Layout/SScrollBox.h"
#include "MainGameDoor.h"
#include "BrainMiningFonts.h"
#include "MiningMenuStyle.h"
#include "MiningAudioSettings.h"
#include "Camera/CameraComponent.h"
#include "MiningAudioComponent.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "BrainMiningCameraManager.h"
#include "BrainMiningCharacter.h"
#include "Blueprint/UserWidget.h"
#include "BrainMining.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "MiningGUIWidgets.h"
#include "MiningInteractionHUDWidget.h"
#include "MobileTouchControls.h"
#include "Engine/GameViewportClient.h"
#include "Components/InputComponent.h"
#include "MiningComponent.h"
#include "MiningProgressionComponent.h"
#include "MiningWalletSubsystem.h"
#include "StartRoom/MiningStartRoom.h"
#include "Engine/GameInstance.h"
#include "MiningBombToolComponent.h"
#include "PopupClose/PopupCloseStage.h"
#include "WenSi/WenSiMirrorPuzzle.h"
#include "WenSi/WenSiPuzzle.h"
#include "EchoPassageStage.h"
#include "EchoPassagePlayer.h"
#include "CardRoomStage.h"
#include "MiningCollection.h"
#include "MiningCellUniverse.h"
#include "EndingEmergence.h"
#include "Showcase/MiningExhibitHall.h"
#include "Showcase/MiningTreasure.h"
#include "CardRoomPlayer.h"
#include "InputKeyEventArgs.h"
#include "MiningGamepadSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"

ABrainMiningPlayerController::ABrainMiningPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = ABrainMiningCameraManager::StaticClass();
	MiningGUIClass = TSoftClassPtr<UMiningGUIHUDWidget>(FSoftObjectPath(TEXT("/Game/UI/MiningGUI/WBP_MiningHUD.WBP_MiningHUD_C")));
}

void ABrainMiningPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalPlayerController())
	{
		InteractionHUD = CreateWidget<UMiningInteractionHUDWidget>(this);
		if (InteractionHUD) InteractionHUD->AddToPlayerScreen(30);
		if (auto* WidgetClass = MiningGUIClass.LoadSynchronous())
		{
			MiningGUI = CreateWidget<UMiningGUIHUDWidget>(this, WidgetClass);
			if (MiningGUI)
			{
				MiningGUI->AddToPlayerScreen(10);
				if (ShouldUseTouchControls())
				{
					if (MiningGUI->AbilityG) MiningGUI->AbilityG->SetVisibility(ESlateVisibility::Collapsed);
					if (MiningGUI->AbilityV) MiningGUI->AbilityV->SetVisibility(ESlateVisibility::Collapsed);
				}
				if (!ShouldUseTouchControls())
				{
					if (auto* Viewport = GetWorld()->GetGameViewport())
					{
						PreviousMouseCaptureMode = Viewport->GetMouseCaptureMode();
						PreviousMouseLockMode = Viewport->GetMouseLockMode();
					}
					bOwnsDesktopGUIInput = true;
					FInputModeGameOnly Mode;
					Mode.SetConsumeCaptureMouseDown(false);
					SetInputMode(Mode);
					bShowMouseCursor = false;
					if (FSlateApplication::IsInitialized())
						ApplicationActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this, &ThisClass::OnApplicationActivationChanged);
				}
			}
		}
	}

	
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
        ActivateTouchInterface(nullptr);
		if (auto* Viewport = GetWorld()->GetGameViewport())
		{
			MobileTouchControls = SNew(SMobileTouchControls).OwnerController(this);
            MobileTouchLayer = SNew(SSafeZone).Visibility(EVisibility::SelfHitTestInvisible)[MobileTouchControls.ToSharedRef()];
			Viewport->AddViewportWidgetContent(MobileTouchLayer.ToSharedRef(), 0);
            MobileToolbar = MiningMobileUI::MakeToolbar(this);
            Viewport->AddViewportWidgetContent(MobileToolbar.ToSharedRef(), 2000);
		}
        if (!ApplicationActivationHandle.IsValid() && FSlateApplication::IsInitialized())
            ApplicationActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this, &ThisClass::OnApplicationActivationChanged);
	}
}

void ABrainMiningPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (IsLocalPlayerController())
	{
		InputComponent->BindKey(EKeys::G, IE_Pressed, this, &ThisClass::UseAbilityG);
		InputComponent->BindKey(EKeys::V, IE_Pressed, this, &ThisClass::UseAbilityV);
	}

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}

bool ABrainMiningPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls || FParse::Param(FCommandLine::Get(), TEXT("MobileUI"));
}

void ABrainMiningPlayerController::UseInputDefaultsFrom(const ABrainMiningPlayerController* Defaults)
{
    if (!Defaults) return;
    DefaultMappingContexts = Defaults->DefaultMappingContexts;
    MobileExcludedMappingContexts = Defaults->MobileExcludedMappingContexts;
    MobileControlsWidgetClass = Defaults->MobileControlsWidgetClass;
    PlayerCameraManagerClass = Defaults->PlayerCameraManagerClass;
}

void ABrainMiningPlayerController::SetMiningGUIEnabled(bool bEnabled)
{
 if (!bEnabled) SetUpgradeMenuHeld(false);
 bMiningGUIEnabled = bEnabled;
 if (MiningGUI) MiningGUI->SetVisibility(bEnabled && !ShouldUseTouchControls() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void ABrainMiningPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if(GetPawn()) if(auto* Camera=GetPawn()->FindComponentByClass<UCameraComponent>())
        if(auto* Settings=GetGameInstance()->GetSubsystem<UMiningAudioSettings>())Camera->SetFieldOfView(Settings->GetFieldOfView());
    const auto* Cards = FindComponentByClass<UCardRoomInteractionComponent>();
    const bool bInCards = Cards && Cards->bRoomActive && Cards->Stage
        && Cards->Stage->Run.Phase != ECardRoomPhase::Exploring
        && Cards->Stage->Run.Phase != ECardRoomPhase::Collected;
    if (MiningGUI)
        MiningGUI->SetVisibility(bMiningGUIEnabled && !bInCards && !ShouldUseTouchControls() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (bInCards && bUpgradeMenuHeld) SetUpgradeMenuHeld(false);
    const auto* Echo = FindComponentByClass<UEchoPassageInteractionComponent>();
    if (Echo && Echo->bExplorationActive && Echo->Stage && Echo->Stage->bLanded && !Echo->Stage->bCompleted)
        if (auto* Bomb = GetPawn() ? GetPawn()->FindComponentByClass<UMiningBombToolComponent>() : nullptr)
            Bomb->Stow();
}

void ABrainMiningPlayerController::UseAbilityG()
{
	// Returning to the pickaxe must remain possible inside non-mining rooms.
	if (!IsPaused())
		if (auto* Equipped = GetPawn() ? GetPawn()->FindComponentByClass<UMiningBombToolComponent>() : nullptr; Equipped && Equipped->bEquipped)
		{ Equipped->Stow(); return; }
	if (!bMiningGUIEnabled || IsMoveInputIgnored() || IsLookInputIgnored() || IsPaused() || bUpgradeMenuHeld) return;
	if (MiningGUI && MiningGUI->AbilityG && !MiningGUI->AbilityG->bAvailable) return;
	auto* Bomb = GetPawn() ? GetPawn()->FindComponentByClass<UMiningBombToolComponent>() : nullptr;
	if (Bomb && Bomb->ToggleEquipped() && MiningGUI) MiningGUI->OnAbilityRequested.Broadcast(TEXT("G"));
}
void ABrainMiningPlayerController::UseAbilityV()
{
 if (IsMoveInputIgnored() || IsLookInputIgnored() || IsPaused() || bUpgradeMenuHeld) return;
	if (MiningGUI && MiningGUI->AbilityV && !MiningGUI->AbilityV->bAvailable) return;
	UMiningComponent* Mining = GetPawn() ? GetPawn()->FindComponentByClass<UMiningComponent>() : nullptr;
	if (Mining && Mining->ActivateDrill() && MiningGUI)
		MiningGUI->OnAbilityRequested.Broadcast(TEXT("V"));
}

bool ABrainMiningPlayerController::IsMobileMoveEnabled() const
{
    if(IsPaused()||bShowMouseCursor||!GetPawn())return false;
    if(const auto* Ending=AEndingEmergence::FindForPlayer(this))return Ending->GetTouchInput().bMove;
    return !IsMoveInputIgnored()&&!IsLookInputIgnored();
}

bool ABrainMiningPlayerController::IsMobileLookEnabled() const
{
    if(IsPaused()||bShowMouseCursor||IsLookInputIgnored()||!GetPawn())return false;
    if(const auto* Ending=AEndingEmergence::FindForPlayer(this))return Ending->GetTouchInput().bLook;
    return !IsMoveInputIgnored();
}

bool ABrainMiningPlayerController::IsMobileJumpEnabled() const
{
    if(IsPaused()||bShowMouseCursor||!GetPawn())return false;
    if(const auto* Ending=AEndingEmergence::FindForPlayer(this))return Ending->GetTouchInput().bJump;
    return !IsMoveInputIgnored()&&!IsLookInputIgnored();
}

void ABrainMiningPlayerController::HandleMobileLook(const FVector2D& Delta)
{
	if (!IsMobileLookEnabled()) return;
	if (!FMath::IsFinite(Delta.X) || !FMath::IsFinite(Delta.Y)) return;
	const float DegreesPerScreenHeight=FMath::Clamp(MobileLookSensitivity, .03f, .18f)*1440.f;
	RotationInput.Yaw+=Delta.X*DegreesPerScreenHeight;
	RotationInput.Pitch-=Delta.Y*DegreesPerScreenHeight;
}

void ABrainMiningPlayerController::HandleMobileMove(const FVector2D& Axis)
{
	if (!Axis.IsNearlyZero() && !IsMobileMoveEnabled()) { HandleMobileMove(FVector2D::ZeroVector); return; }
    MobileMoveAxis = FVector2D(FMath::Clamp(Axis.X, -1., 1.), FMath::Clamp(Axis.Y, -1., 1.));
	SetMobileVirtualKey(EKeys::W, Axis.Y > .2f);
	SetMobileVirtualKey(EKeys::S, Axis.Y < -.2f);
	SetMobileVirtualKey(EKeys::D, Axis.X > .2f);
	SetMobileVirtualKey(EKeys::A, Axis.X < -.2f);
	if (IsPaused() || IsMoveInputIgnored() || !GetPawn()) return;
	const FVector Forward=GetPawn()->GetActorForwardVector();
	const FVector Right=GetPawn()->GetActorRightVector();
	GetPawn()->AddMovementInput(Forward, FMath::Clamp(Axis.Y,-1.f,1.f));
	GetPawn()->AddMovementInput(Right, FMath::Clamp(Axis.X,-1.f,1.f));
}

void ABrainMiningPlayerController::HandleMobileMine(bool bPressed)
{
	if (auto* Mining = GetPawn() ? GetPawn()->FindComponentByClass<UMiningComponent>() : nullptr)
	{
		if (!bPressed)
		{
			Mining->EndMobileMiningInput();
			return;
		}
		if (IsPaused() || IsMoveInputIgnored() || IsLookInputIgnored()) return;
		Mining->BeginMobileMiningInput();
	}
}

void ABrainMiningPlayerController::HandleMobileBomb()
{
	if (!bMiningGUIEnabled || IsPaused() || IsMoveInputIgnored() || IsLookInputIgnored()) return;
	if (auto* Bomb=GetPawn()?GetPawn()->FindComponentByClass<UMiningBombToolComponent>():nullptr)
	{
		if (!Bomb->bEquipped && !Bomb->ToggleEquipped()) return;
		if (Bomb->ThrowBomb() && MiningGUI) MiningGUI->OnAbilityRequested.Broadcast(TEXT("G"));
	}
}

void ABrainMiningPlayerController::HandleMobileDrill()
{
	if (IsPaused() || IsMoveInputIgnored() || IsLookInputIgnored()) return;
	if (auto* Mining=GetPawn()?GetPawn()->FindComponentByClass<UMiningComponent>():nullptr)
		if (Mining->ActivateDrill() && MiningGUI) MiningGUI->OnAbilityRequested.Broadcast(TEXT("V"));
}

void ABrainMiningPlayerController::HandleMobileJump(bool bPressed)
{
	if (bPressed && !IsMobileJumpEnabled()) return;
	SetMobileVirtualKey(EKeys::SpaceBar, bPressed);
	auto* PawnCharacter = Cast<ABrainMiningCharacter>(GetPawn());
	if (!PawnCharacter) return;
	if (bPressed)
	{
		if (IsPaused() || IsMoveInputIgnored() || bUpgradeMenuHeld) return;
		PawnCharacter->DoJumpStart();
	}
	else
	{
		PawnCharacter->DoJumpEnd();
	}
}

void ABrainMiningPlayerController::HandleMobileInteract()
{
	for (TActorIterator<APopupCloseStage> It(GetWorld()); It; ++It)
	{
		if (It->IsSessionActive())
		{
			It->HandleInteractionInput(this);
			return;
		}
	}
    for (TActorIterator<AWenSiPuzzle> It(GetWorld()); It; ++It)
        if (It->HandleInteractionInput(this)) return;
    for (TActorIterator<AWenSiMirrorPuzzle> It(GetWorld()); It; ++It)
        if (It->HandleInteractionInput(this)) return;
    if (auto* Cards = FindComponentByClass<UCardRoomInteractionComponent>(); Cards && Cards->bRoomActive
        && Cards->Stage
        && (Cards->Stage->Run.Phase == ECardRoomPhase::Exploring || Cards->Stage->Run.Phase == ECardRoomPhase::Victory)
        && (Cards->Stage->Run.Phase == ECardRoomPhase::Victory
            || Cards->Stage->IsPlayerNear(GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector)))
    {
        Cards->Interact();
        return;
    }
    for (TActorIterator<AMiningCollectionField> It(GetWorld()); It; ++It)
        if (It->TryCollectTarget()) return;
    for (TActorIterator<AMiningTreasureField> It(GetWorld()); It; ++It)
        if (It->IsAreaActive() && It->CollectTarget(this)) return;
	if (IsPaused() || IsMoveInputIgnored() || IsLookInputIgnored() || bUpgradeMenuHeld) return;
	InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E, IE_Pressed, 1.f));
}

void ABrainMiningPlayerController::HandleMobilePrimary()
{
	for (TActorIterator<APopupCloseStage> It(GetWorld()); It; ++It)
	{
		if (It->IsSessionActive())
		{
			It->HandleMobilePrimary(this);
			return;
		}
	}
	if (auto* EchoInteraction = FindComponentByClass<UEchoPassageInteractionComponent>())
	{
		if (EchoInteraction->bExplorationActive && EchoInteraction->Stage && EchoInteraction->Stage->bLanded && !EchoInteraction->Stage->bCompleted)
		{
			EchoInteraction->Knock();
			return;
		}
	}
	TriggerMobileKey(EKeys::LeftMouseButton);
}

void ABrainMiningPlayerController::HandleMobileConfirm()
{
    if (IsPaused()) return;
    if (auto* Cards = FindComponentByClass<UCardRoomInteractionComponent>(); Cards && Cards->bRoomActive)
        Cards->ConfirmCards();
}

void ABrainMiningPlayerController::HandleMobileReset()
{
	TriggerMobileKey(EKeys::R);
}

void ABrainMiningPlayerController::HandleMobileThoughtLeft()
{
	if (IsPaused()) return;
	for (TActorIterator<AWenSiMirrorPuzzle> It(GetWorld()); It; ++It)
		if (It->bAreaActive) { It->CloseThought(0); return; }
	for (TActorIterator<AWenSiPuzzle> It(GetWorld()); It; ++It)
		if (It->bAreaActive) { It->CloseThought(0); return; }
	TriggerMobileKey(EKeys::Q);
}

void ABrainMiningPlayerController::HandleMobileThoughtRight()
{
	if (IsPaused()) return;
	for (TActorIterator<AWenSiMirrorPuzzle> It(GetWorld()); It; ++It)
		if (It->bAreaActive) { It->CloseThought(1); return; }
	for (TActorIterator<AWenSiPuzzle> It(GetWorld()); It; ++It)
		if (It->bAreaActive) { It->CloseThought(1); return; }
	TriggerMobileKey(EKeys::R);
}

void ABrainMiningPlayerController::HandleMobileMap()
{
	TriggerMobileKey(EKeys::M);
}

void ABrainMiningPlayerController::HandleMobileCatalogue()
{
	if (AMiningExhibitHall::Find(GetWorld())) TriggerMobileKey(EKeys::C);
	else TriggerMobileKey(EKeys::Tab);
}

void ABrainMiningPlayerController::HandleMobileBack()
{
	for (TActorIterator<AMiningCollectionField> Field(GetWorld()); Field; ++Field)
	{
		if (Field->bCatalogueOpen)
		{
			Field->ToggleCatalogue();
			return;
		}
	}
	for (TActorIterator<AWenSiMirrorPuzzle> It(GetWorld()); It; ++It)
		if (It->bAreaActive && It->bEditing) { It->ToggleEdit(); return; }
	for (TActorIterator<AWenSiPuzzle> It(GetWorld()); It; ++It)
		if (It->bAreaActive && It->bEditing) { It->ToggleEdit(); return; }
	TriggerMobileKey(EKeys::Escape);
}

void ABrainMiningPlayerController::HandleMobileEdit()
{
	if (IsPaused()) return;
	for (TActorIterator<AWenSiMirrorPuzzle> It(GetWorld()); It; ++It)
		if (It->bAreaActive)
		{
			if (It->bEditing) It->ToggleEdit();
			else It->SwapThoughts();
			return;
		}
	for (TActorIterator<AWenSiPuzzle> It(GetWorld()); It; ++It)
		if (It->bAreaActive) { It->ToggleEdit(); return; }
	TriggerMobileKey(EKeys::G);
}

void ABrainMiningPlayerController::HandleMobileUpgrade()
{
	if (!MiningGUI || !bMiningGUIEnabled || IsPaused() || IsMoveInputIgnored() || IsLookInputIgnored()) return;
    if (ShouldUseTouchControls())
    {
        if (auto* Menu = GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()) Menu->ToggleMobileMenu(true);
        return;
    }
	if (auto* Progression = GetPawn() ? GetPawn()->FindComponentByClass<UMiningProgressionComponent>() : nullptr)
	{
		if (!Progression->bInitialized) Progression->InitializeProgression();
		if (Progression->UsesMineralInventory())
		{
			const bool bUpgraded = Progression->Purchase(0, Progression->GetLevel(0) + 1);
			if (auto* Audio = GetPawn()->FindComponentByClass<UMiningAudioComponent>()) Audio->PlayPurchase(bUpgraded);
			if (bUpgraded) MiningGUI->RefreshMineralProgression();
			return;
		}
	}
	SetUpgradeMenuHeld(!bUpgradeMenuHeld);
}

void ABrainMiningPlayerController::SetMobileVirtualKey(const FKey& Key, bool bPressed)
{
	const bool bWasPressed = MobileVirtualKeys.Contains(Key);
	if (bWasPressed == bPressed) return;
	const FInputKeyEventArgs Args = FInputKeyEventArgs::CreateSimulated(Key, bPressed ? IE_Pressed : IE_Released, bPressed ? 1.f : 0.f);
	Super::InputKey(Args);
	if (bPressed) MobileVirtualKeys.Add(Key);
	else MobileVirtualKeys.Remove(Key);
}

void ABrainMiningPlayerController::TriggerMobileKey(const FKey& Key)
{
	InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.f));
	InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.f));
}

bool ABrainMiningPlayerController::IsMobileActionVisible() const
{
	const auto* Echo = FindComponentByClass<UEchoPassageInteractionComponent>();
	if (Echo && Echo->bExplorationActive && Echo->Stage && Echo->Stage->bLanded && !Echo->Stage->bCompleted) return true;
	for (TActorIterator<APopupCloseStage> It(GetWorld()); It; ++It) if (It->IsSessionActive()) return true;
	return false;
}

bool ABrainMiningPlayerController::IsMobileInteractVisible() const
{
    const auto* Cards = FindComponentByClass<UCardRoomInteractionComponent>();
    return !Cards || !Cards->bRoomActive || !Cards->Stage
        || Cards->Stage->Run.Phase == ECardRoomPhase::Exploring
        || Cards->Stage->Run.Phase == ECardRoomPhase::Collected;
}

bool ABrainMiningPlayerController::IsMobileConfirmVisible() const
{
    const auto* Cards = FindComponentByClass<UCardRoomInteractionComponent>();
    return Cards && Cards->bRoomActive && Cards->Stage && Cards->Stage->Run.Phase == ECardRoomPhase::Choosing;
}

bool ABrainMiningPlayerController::IsMobileResetVisible() const
{
	if (IsMobileActionVisible() || IsMobileThoughtVisible() || IsMobileCatalogueVisible()) return true;
	if (AMiningExhibitHall::Find(GetWorld())) return true;
	TActorIterator<AMiningCellUniverse> CellIt(GetWorld());
	if (CellIt) return true;
	TActorIterator<AEndingEmergence> EndingIt(GetWorld());
	if (EndingIt) return true;
	return false;
}

bool ABrainMiningPlayerController::IsMobileThoughtVisible() const
{
	for (TActorIterator<AWenSiPuzzle> It(GetWorld()); It; ++It) if (It->bAreaActive) return true;
	for (TActorIterator<AWenSiMirrorPuzzle> It(GetWorld()); It; ++It) if (It->bAreaActive) return true;
	return false;
}

bool ABrainMiningPlayerController::IsMobileMapVisible() const
{
	const AMiningExhibitHall* Hall = AMiningExhibitHall::Find(GetWorld());
	return Hall && Hall->bReady;
}

bool ABrainMiningPlayerController::IsMobileCatalogueVisible() const
{
	for (TActorIterator<AMiningCollectionField> It(GetWorld()); It; ++It) if (It->IsAreaActive()) return true;
	return false;
}

bool ABrainMiningPlayerController::IsMobileBackVisible() const
{
	for (TActorIterator<AWenSiMirrorPuzzle> It(GetWorld()); It; ++It) if (It->bAreaActive && It->bEditing) return true;
	for (TActorIterator<AWenSiPuzzle> It(GetWorld()); It; ++It) if (It->bAreaActive && It->bEditing) return true;
	const AMiningExhibitHall* Hall = AMiningExhibitHall::Find(GetWorld());
	if (Hall && Hall->IsMapOpen()) return true;
	for (TActorIterator<AMiningCollectionField> It(GetWorld()); It; ++It) if (It->bCatalogueOpen) return true;
	return false;
}

bool ABrainMiningPlayerController::IsMobileEditVisible() const
{
	for (TActorIterator<AWenSiMirrorPuzzle> It(GetWorld()); It; ++It)
		if (It->bAreaActive) return It->bEditing || (!It->bComplete && !It->IsSwapping() && It->Thoughts[0].IsValid() && It->Thoughts[1].IsValid());
	for (TActorIterator<AWenSiPuzzle> It(GetWorld()); It; ++It)
		if (It->bAreaActive) return It->bEditing || (!It->bComplete && (It->Thoughts[0].IsValid() || It->Thoughts[1].IsValid()));
	return false;
}

bool ABrainMiningPlayerController::IsMobileUpgradeVisible() const
{
	return MiningGUI && bMiningGUIEnabled;
}

FString ABrainMiningPlayerController::GetMobileActionLabel() const
{
	const auto* Echo = FindComponentByClass<UEchoPassageInteractionComponent>();
	if (Echo && Echo->bExplorationActive && Echo->Stage && Echo->Stage->bLanded && !Echo->Stage->bCompleted) return TEXT("HIT");
	for (TActorIterator<APopupCloseStage> It(GetWorld()); It; ++It) if (It->IsSessionActive()) return TEXT("CLICK");
	return TEXT("TAP");
}

bool ABrainMiningPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    if((Params.Key==EKeys::F10 || Params.Key==EKeys::F9) && (IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl) ||
       (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsControlDown())))
    {
        if(Params.Event==IE_Pressed)
        {
            auto* Experience=GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>();
            if(Params.Key==EKeys::F9){if(Experience->IsMenuOpen())Experience->CloseMenu();Experience->ToggleAllUI();}
            else Experience->ToggleDeveloperMenu();
        }
        return true;
    }
    if(Params.Event==IE_Pressed && EndingCreditsInput::Handle(this,Params.Key))return true;
	if (Params.Key == EKeys::F8 || (IsAudioSettingsOpen() && Params.Key == EKeys::Escape))
	{
		if (Params.Event == IE_Pressed) ToggleAudioSettings();
		return true;
	}
	if (IsAudioSettingsOpen()) return true;
    if(Params.Key==EKeys::C && !IsPaused() && !bShowMouseCursor && !IsLookInputIgnored() && !IsMoveInputIgnored())
    {
        if(Params.Event==IE_Pressed)
        {
            AMiningCollectionField* Field=nullptr;
            for(TActorIterator<AMiningCollectionField> It(GetWorld());It;++It){Field=*It;if(It->bUseAuthoredPlacements)break;}
            if(!Field)
            {
                Field=GetWorld()->SpawnActorDeferred<AMiningCollectionField>(AMiningCollectionField::StaticClass(),FTransform::Identity);
                if(Field){Field->bCatalogueOnly=true;Field->bSharedAbilities=true;Field->bUseAuthoredPlacements=true;Field->bShowExplorationHUD=false;Field->FinishSpawning(FTransform::Identity);}
            }
            if(Field)Field->ToggleCatalogue();
        }
        return true;
    }
	if (Params.Key == EKeys::E && Params.Event == IE_Pressed && !bUpgradeMenuHeld)
	{
		if(auto* Door=AMainGameDoor::FindInteractionTarget(this)){Door->OpenDoor();return true;}
		for (TActorIterator<APopupCloseStage> It(GetWorld()); It; ++It)
			if (It->HandleInteractionInput(this)) return true;
		for (TActorIterator<AWenSiPuzzle> It(GetWorld()); It; ++It)
			if (It->HandleInteractionInput(this)) { if(It->bAimReset)Super::InputKey(Params);return true; }
		for (TActorIterator<AWenSiMirrorPuzzle> It(GetWorld()); It; ++It)
			if (It->HandleInteractionInput(this)) { if(It->bAimReset)Super::InputKey(Params);return true; }
        if (auto* Cards = FindComponentByClass<UCardRoomInteractionComponent>(); Cards && Cards->bRoomActive
            && Cards->Stage
            && (Cards->Stage->Run.Phase == ECardRoomPhase::Exploring || Cards->Stage->Run.Phase == ECardRoomPhase::Victory)
            && (Cards->Stage->Run.Phase == ECardRoomPhase::Victory
                || Cards->Stage->IsPlayerNear(GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector)))
		{
			Cards->Interact();
			return true;
		}
		for (TActorIterator<AMiningCollectionField> It(GetWorld()); It; ++It)
			if (It->TryCollectTarget()) return true;
		for (TActorIterator<AMiningTreasureField> It(GetWorld()); It; ++It)
			if (It->IsAreaActive() && It->CollectTarget(this)) return true;
	}
    auto* Progression = GetPawn() ? GetPawn()->FindComponentByClass<UMiningProgressionComponent>() : nullptr;
    if (Params.Key == EKeys::Tab && Progression && Progression->UsesMineralInventory())
    {
        if (Params.Event == IE_Pressed && bMiningGUIEnabled && !IsMoveInputIgnored() && !IsLookInputIgnored() && !IsPaused() && !bShowMouseCursor)
        {
            const bool bUpgraded = Progression->Purchase(0, Progression->GetLevel(0) + 1);
            if (auto* Audio = GetPawn()->FindComponentByClass<UMiningAudioComponent>()) Audio->PlayPurchase(bUpgraded);
        }
        return true; // Repeat/release never purchase, show a cursor, or suspend mining.
    }
	// Consume the throw before Enhanced Input sees a mining press. A held throw
	// therefore cannot turn into a pickaxe swing after the bomb leaves the hand.
	if (Params.Key == EKeys::LeftMouseButton && Params.Event == IE_Pressed)
	{
		if (auto* Bomb = GetPawn() ? GetPawn()->FindComponentByClass<UMiningBombToolComponent>() : nullptr; Bomb && Bomb->bEquipped)
		{
			Bomb->ThrowBomb();
			return true;
		}
	}
	if (bOwnsDesktopGUIInput && bMiningGUIEnabled)
	{
		if (Params.Key == EKeys::Tab && (bUpgradeMenuHeld || (!IsMoveInputIgnored() && !IsLookInputIgnored() && !IsPaused())))
		{
			if (Params.Event == IE_Pressed || Params.Event == IE_Repeat) SetUpgradeMenuHeld(true);
			else if (Params.Event == IE_Released) SetUpgradeMenuHeld(false);
			return true;
		}
		// UI handles its buttons first. Unhandled pointer events must not reach mining or recapture RMB.
		if (bUpgradeMenuHeld && (Params.Key.IsMouseButton() || Params.Key == EKeys::MouseX || Params.Key == EKeys::MouseY))
			return true;
	}
	return Super::InputKey(Params);
}

void ABrainMiningPlayerController::SetUpgradeMenuHeld(bool bHeld)
{
    if (GetPawn()) if (auto* Progression = GetPawn()->FindComponentByClass<UMiningProgressionComponent>(); Progression && Progression->UsesMineralInventory()) return;
    if ((!bOwnsDesktopGUIInput && !ShouldUseTouchControls()) || !MiningGUI || bUpgradeMenuHeld == bHeld) return;
 if (bHeld && (!bMiningGUIEnabled || IsMoveInputIgnored() || IsLookInputIgnored() || IsPaused())) return;
	bUpgradeMenuHeld = bHeld;
	SetIgnoreLookInput(bHeld);
	RotationInput = FRotator::ZeroRotator;
	if (APawn* ControlledPawn = GetPawn())
		if (auto* Mining = ControlledPawn->FindComponentByClass<UMiningComponent>()) Mining->SetInputSuspended(bHeld);
	MiningGUI->SetUpgradePanelOpen(bHeld);
	bShowMouseCursor = bHeld;
	if (bHeld)
	{
		// Clear any held mining click before handing the pointer to Slate.
		Super::InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 0.f));
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(MiningGUI->TakeWidget());
		Mode.SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		if (auto* Viewport = GetWorld()->GetGameViewport()) Viewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
		int32 Width = 0, Height = 0;
		GetViewportSize(Width, Height);
		if (Width > 0 && Height > 0) SetMouseLocation(Width / 2, Height / 2);
	}
	else
	{
		FInputModeGameOnly Mode;
		Mode.SetConsumeCaptureMouseDown(false);
		SetInputMode(Mode);
	}
}

void ABrainMiningPlayerController::OnApplicationActivationChanged(bool bActive)
{
	if (!bActive) { SetUpgradeMenuHeld(false); ResetMobileInput(); }
}

void ABrainMiningPlayerController::ResetMobileInput()
{
    if (MobileTouchControls.IsValid()) MobileTouchControls->ResetInput();
    HandleMobileMine(false);
    HandleMobileJump(false);
    HandleMobileMove(FVector2D::ZeroVector);
    for (const FKey& Key : MobileVirtualKeys)
        Super::InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.f));
    MobileVirtualKeys.Reset();
}

void ABrainMiningPlayerController::HandleMobilePause()
{
    if (auto* Menu = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>() : nullptr)
        Menu->TogglePauseMenu();
}

void ABrainMiningPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    ResetMobileInput();
    if (MobileToolbar.IsValid())
    {
        if (auto* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr) Viewport->RemoveViewportWidgetContent(MobileToolbar.ToSharedRef());
        MobileToolbar.Reset();
    }
	for (const FKey& Key : MobileVirtualKeys)
		Super::InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.f));
	MobileVirtualKeys.Reset();
	if (IsAudioSettingsOpen()) ToggleAudioSettings();
	if (ApplicationActivationHandle.IsValid() && FSlateApplication::IsInitialized())
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ApplicationActivationHandle);
	SetUpgradeMenuHeld(false);
	if (MiningGUI) MiningGUI->RemoveFromParent();
	if (InteractionHUD) InteractionHUD->RemoveFromParent();
	if (MobileTouchControls.IsValid())
	{
		if (auto* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
			if (MobileTouchLayer.IsValid()) Viewport->RemoveViewportWidgetContent(MobileTouchLayer.ToSharedRef());
		MobileTouchLayer.Reset();
		MobileTouchControls.Reset();
	}
	if (bOwnsDesktopGUIInput && IsLocalPlayerController())
	{
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
		if (auto* Viewport = GetWorld()->GetGameViewport())
		{
			Viewport->SetMouseCaptureMode(PreviousMouseCaptureMode);
			Viewport->SetMouseLockMode(PreviousMouseLockMode);
		}
	}
	Super::EndPlay(Reason);
}

void ABrainMiningPlayerController::AddYawInput(float Value)
{
    const auto* Settings=GetGameInstance()?GetGameInstance()->GetSubsystem<UMiningAudioSettings>():nullptr;
    Super::AddYawInput(Value*(Settings?Settings->GetLookSensitivity():1.f));
}
void ABrainMiningPlayerController::AddPitchInput(float Value)
{
    const auto* Settings=GetGameInstance()?GetGameInstance()->GetSubsystem<UMiningAudioSettings>():nullptr;
    Super::AddPitchInput(Value*(Settings?Settings->GetLookSensitivity():1.f));
}
void ABrainMiningPlayerController::ToggleAudioSettings()
{
	auto* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	auto* Settings = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMiningAudioSettings>() : nullptr;
	if (!Viewport || !Settings) return;
	auto* Mining = GetPawn() ? GetPawn()->FindComponentByClass<UMiningComponent>() : nullptr;
	if (AudioSettingsPanel.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(AudioSettingsPanel.ToSharedRef());
		AudioSettingsPanel.Reset();
		Settings->Save();
		if (bAudioPausedWorld) SetPause(false);
		bAudioPausedWorld = false;
		SetIgnoreMoveInput(false);
		SetIgnoreLookInput(false);
		if (Mining) Mining->SetInputSuspended(bAudioPreviousMiningSuspended);
		bShowMouseCursor = false;
		FInputModeGameOnly Mode;
		Mode.SetConsumeCaptureMouseDown(false);
		SetInputMode(Mode);
		return;
	}
	if (bShowMouseCursor || IsPaused()) return;
	bAudioPreviousMiningSuspended = Mining && Mining->IsInputSuspended();
	if (Mining) Mining->SetInputSuspended(true);
	if (auto* Audio = GetPawn() ? GetPawn()->FindComponentByClass<UMiningAudioComponent>() : nullptr) Audio->StopTools();
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	bAudioPausedWorld = SetPause(true);
    TSharedRef<SVerticalBox> Rows=SNew(SVerticalBox);
    Rows->AddSlot().AutoHeight().Padding(0,0,0,18)[SNew(STextBlock).Text(MiningText::Get(TEXT("UI.Settings.Title"))).Font(BrainMiningFonts::Title(32)).ColorAndOpacity(MiningMenuStyle::Gold)];
    Rows->AddSlot().AutoHeight()[GetGameInstance()->GetSubsystem<UMiningLocalizationSubsystem>()->MakeLanguageSelector()];
    Rows->AddSlot().AutoHeight().Padding(0,0,0,10)[MiningMenuStyle::Section(MiningText::Get(TEXT("UI.Settings.Audio")))];
    TSharedPtr<SSlider> FirstSlider;
    const TCHAR* Labels[]={TEXT("UI.Settings.Master"),TEXT("UI.Settings.Music"),TEXT("UI.Settings.Effects"),TEXT("UI.Settings.Ambience"),TEXT("UI.Settings.Voice")};
    TWeakObjectPtr<UMiningAudioSettings> WeakSettings(Settings);
    auto SliderRow=[&](const TCHAR* Label,TAttribute<float> Value,TAttribute<FText> Readout,FOnFloatValueChanged Changed)
    {
        TSharedPtr<SSlider> Slider;
        Rows->AddSlot().AutoHeight().Padding(0,3)[SNew(SBorder).BorderImage(MiningMenuStyle::Brush(1)).Padding(FMargin(18,12))
            [SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(.42f).VAlign(VAlign_Center).Padding(0,0,22,0)
                    [SNew(STextBlock).Text(MiningText::Get(Label)).Justification(ETextJustify::Right).Font(BrainMiningFonts::Body(18)).ColorAndOpacity(MiningMenuStyle::Cream)]
                +SHorizontalBox::Slot().FillWidth(.46f).VAlign(VAlign_Center).Padding(20,0)
                    [SAssignNew(Slider,SSlider).Style(MiningMenuStyle::Slider()).Value(Value).OnValueChanged(Changed)]
                +SHorizontalBox::Slot().FillWidth(.12f).VAlign(VAlign_Center)
                    [SNew(STextBlock).Text(Readout).Font(BrainMiningFonts::Body(15)).ColorAndOpacity(MiningMenuStyle::Gold)]]];
        if(!FirstSlider)FirstSlider=Slider;
    };
    for(int32 Channel=0;Channel<5;++Channel)
        SliderRow(Labels[Channel],TAttribute<float>::CreateLambda([WeakSettings,Channel](){return WeakSettings.IsValid()?WeakSettings->GetVolume(Channel):0.f;}),
            TAttribute<FText>::CreateLambda([WeakSettings,Channel](){return FText::AsPercent(WeakSettings.IsValid()?WeakSettings->GetVolume(Channel):0.f);}),
            FOnFloatValueChanged::CreateLambda([WeakSettings,Channel](float Value){if(WeakSettings.IsValid())WeakSettings->SetVolume(Channel,Value);}));
    Rows->AddSlot().AutoHeight().Padding(0,18,0,8)[MiningMenuStyle::Section(MiningText::Get(TEXT("UI.Settings.Sensitivity")))];
    SliderRow(TEXT("UI.Settings.Look"),TAttribute<float>::CreateLambda([WeakSettings](){return WeakSettings.IsValid()?(WeakSettings->GetLookSensitivity()-.2f)/2.8f:2.f/7.f;}),
        TAttribute<FText>::CreateLambda([WeakSettings](){return FText::FromString(FString::Printf(TEXT("%.2f ×"),WeakSettings.IsValid()?WeakSettings->GetLookSensitivity():1.f));}),
        FOnFloatValueChanged::CreateLambda([WeakSettings](float Value){if(WeakSettings.IsValid())WeakSettings->SetLookSensitivity(.2f+Value*2.8f);}));
    SliderRow(TEXT("UI.Settings.FOV"),TAttribute<float>::CreateLambda([WeakSettings](){return WeakSettings.IsValid()?(WeakSettings->GetFieldOfView()-80.f)/35.f:20.f/35.f;}),
        TAttribute<FText>::CreateLambda([WeakSettings](){return FText::AsNumber(FMath::RoundToInt(WeakSettings.IsValid()?WeakSettings->GetFieldOfView():100.f));}),
        FOnFloatValueChanged::CreateLambda([WeakSettings](float Value){if(WeakSettings.IsValid())WeakSettings->SetFieldOfView(80.f+Value*35.f);}));
    if(auto* Haptics=UMiningHapticsSubsystem::Find(this))Rows->AddSlot().AutoHeight()[Haptics->MakeSettingsWidget()];
    auto Page=SNew(SVerticalBox)
        +SVerticalBox::Slot().FillHeight(1.f)[SNew(SScrollBox).ScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll)+SScrollBox::Slot()[Rows]]
        +SVerticalBox::Slot().AutoHeight().Padding(0,20,0,0)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,8,0)[SNew(SButton).ButtonStyle(MiningMenuStyle::Button()).HAlign(HAlign_Center).OnClicked_Lambda([WeakSettings]()
                {if(WeakSettings.IsValid())WeakSettings->RestoreDefaults();return FReply::Handled();})
                [SNew(STextBlock).Font(BrainMiningFonts::Body(18)).ColorAndOpacity(FSlateColor::UseForeground()).Text(MiningText::Get(TEXT("UI.Settings.Reset")))]]
            +SHorizontalBox::Slot().FillWidth(1).Padding(8,0,0,0)[SNew(SButton).ButtonStyle(MiningMenuStyle::Button()).HAlign(HAlign_Center).OnClicked_Lambda([this](){ToggleAudioSettings();return FReply::Handled();})
                [SNew(STextBlock).Font(BrainMiningFonts::Body(18)).ColorAndOpacity(FSlateColor::UseForeground()).Text_Lambda([this](){return FText::Format(FText::AsCultureInvariant(TEXT("[ {0} ] {1}")),FText::AsCultureInvariant(UMiningGamepadSubsystem::UsesGamepad(this)?TEXT("B"):TEXT("ESC")),MiningText::Get(TEXT("UI.Settings.Back")));})]]];
    AudioSettingsPanel=MiningMenuStyle::Frame(Page,1060,820);
    Viewport->AddViewportWidgetContent(AudioSettingsPanel.ToSharedRef(),1000);
    bShowMouseCursor=true;
    FInputModeGameAndUI Mode;
    Mode.SetWidgetToFocus(FirstSlider).SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    Viewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
}
