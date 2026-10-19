#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "MiningGamepadSubsystem.h"
#include "MiningGamepadHints.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "MiningComponent.h"
#include "MiningEncounterPlayer.h"
#include "MiningCollection.h"
#include "MiningProgressionComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningBombToolComponent.h"
#include "MiningWalletSubsystem.h"
#include "MiningExperienceSubsystem.h"
#include "StartRoom/MiningOpening.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SOverlay.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningGamepadDeadZoneTest, "BrainMining.Gamepad.DeadZone", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningGamepadDeadZoneTest::RunTest(const FString&)
{
    TestTrue(TEXT("Small diagonal drift is neutral"), UMiningGamepadSubsystem::ApplyDeadZone(FVector2D(.1, .1), .18f).IsNearlyZero());
    TestTrue(TEXT("Threshold is continuous"), UMiningGamepadSubsystem::ApplyDeadZone(FVector2D(.18001, 0), .18f).Size() < .001);
    TestTrue(TEXT("Diagonal input never accelerates movement"), FMath::IsNearlyEqual(UMiningGamepadSubsystem::ApplyDeadZone(FVector2D(1, 1), .18f).Size(), 1.));
    TestTrue(TEXT("Invalid device axis is neutral"), UMiningGamepadSubsystem::ApplyDeadZone(FVector2D(std::numeric_limits<float>::infinity(), 0), .18f).IsNearlyZero());
    EMiningInputAction Action;
    TestTrue(TEXT("RT has a primary action"), UMiningGamepadSubsystem::MapButton(EKeys::Gamepad_RightTrigger, Action));
    TestTrue(TEXT("RT is independent of the reset key"), Action == EMiningInputAction::Primary);
    TestTrue(TEXT("LT maps to upgrade"), UMiningGamepadSubsystem::MapButton(EKeys::Gamepad_LeftTrigger, Action) && Action == EMiningInputAction::Upgrade);
    TestFalse(TEXT("View no longer opens a map"), UMiningGamepadSubsystem::MapButton(EKeys::Gamepad_Special_Left, Action));
    TestTrue(TEXT("D-pad right only navigates"), UMiningGamepadSubsystem::MapButton(EKeys::Gamepad_DPad_Right, Action) && Action == EMiningInputAction::Next);
    for (int32 Index = 1; Index < static_cast<int32>(EMiningGamepadGlyph::Count); ++Index)
    {
        const FString Icon = FPaths::ProjectContentDir() / TEXT("UI/Gamepad") / (FString(MiningGamepadGlyphName(static_cast<EMiningGamepadGlyph>(Index))) + TEXT(".svg"));
        TestTrue(*FString::Printf(TEXT("Controller glyph exists: %s"), *Icon), IFileManager::Get().FileExists(*Icon));
    }
    TestFalse(TEXT("Keyboard keys are not treated as controller buttons"), UMiningGamepadSubsystem::MapButton(EKeys::R, Action));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningGamepadContextTest, "BrainMining.Gamepad.ContextAndRelease", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningGamepadContextTest::RunTest(const FString&)
{
    auto* Instance = NewObject<UGameInstance>(GEngine);
    Instance->InitializeStandalone();
    auto* World = Instance->GetWorld();
    auto& Context = *Instance->GetWorldContext();
    auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
    auto* Viewport = NewObject<UGameViewportClient>(GEngine);
    Context.GameViewport = Viewport;
    Viewport->Init(Context, Instance, false);
    Viewport->SetViewportOverlayWidget(nullptr, SNew(SOverlay));
    LocalPlayer->PlayerAdded(Viewport, 0);
    auto* Controller = World->SpawnActor<AMiningEncounterPlayerController>();
    World->AddController(Controller);
    Controller->SetPlayer(LocalPlayer);
    auto* Pawn = World->SpawnActor<ACharacter>();
    auto* Mining = NewObject<UMiningComponent>(Pawn);
    Pawn->AddInstanceComponent(Mining);
    Mining->RegisterComponent();
    Controller->Possess(Pawn);
    Controller->EncounterPolicy->SetMiningAllowed(true);
    auto* Input = LocalPlayer->GetSubsystem<UMiningGamepadSubsystem>();
    ON_SCOPE_EXIT
    {
        LocalPlayer->PlayerRemoved();
        Controller->UnPossess();
        LocalPlayer->PlayerController = nullptr;
        Instance->Shutdown();
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
    };
    if (!TestNotNull(TEXT("Local player owns the input router"), Input)) return false;
    if (!TestTrue(TEXT("Fixture local player resolves its registered controller"), LocalPlayer->GetPlayerController(World) == Controller)) return false;
    if (!TestTrue(TEXT("Fixture controller possesses its pawn"), Controller->GetPawn() == Pawn)) return false;
    AddInfo(FString::Printf(TEXT("Router context=%d, world=%s, local-world=%s, paused=%d, cursor=%d"), int32(Input->GetContext()), *GetNameSafe(Input->GetWorld()), *GetNameSafe(LocalPlayer->GetWorld()), Controller->IsPaused(), Controller->bShowMouseCursor));
    TestTrue(TEXT("Exploration context"), Input->GetContext() == EMiningInputContext::Exploration);
    Input->HandleAxis(EKeys::Gamepad_LeftX, 1);
    TestTrue(TEXT("Independent pawns can read controller movement"), UMiningGamepadSubsystem::ReadMove(Controller).X > .99);
    Input->HandleButton(EKeys::Gamepad_RightTrigger, true);
    TestTrue(TEXT("RT begins mining through the real component"), Mining->IsMiningActive());
    Input->HandleButton(EKeys::Gamepad_RightTrigger, false);
    TestFalse(TEXT("RT release ends mining"), Mining->IsMiningActive());
    Input->HandleButton(EKeys::Gamepad_FaceButton_Bottom, true);
    TestTrue(TEXT("Jump hold reaches standalone pawn readers"), UMiningGamepadSubsystem::ReadJumpHeld(Controller));
    TestTrue(TEXT("Jump edge reaches ending reader"), UMiningGamepadSubsystem::ReadJumpPressed(Controller));
    Input->ResetDevice();
    TestFalse(TEXT("Disconnect clears flight"), UMiningGamepadSubsystem::ReadJumpHeld(Controller));
    TestTrue(TEXT("Disconnect clears movement"), UMiningGamepadSubsystem::ReadMove(Controller).IsNearlyZero());
    Input->HandleButton(EKeys::Gamepad_FaceButton_Bottom, true);
    TestTrue(TEXT("Reconnect accepts a new press"), UMiningGamepadSubsystem::ReadJumpHeld(Controller));
    Input->HandleButton(EKeys::Gamepad_LeftTrigger, true);
    Input->HandleButton(EKeys::Gamepad_FaceButton_Bottom, false);
    TestFalse(TEXT("LT cannot keep flight active after A is released"), UMiningGamepadSubsystem::ReadJumpHeld(Controller));
    Input->HandleButton(EKeys::Gamepad_LeftTrigger, false);
    TestFalse(TEXT("Releasing LT does not restart flight"), UMiningGamepadSubsystem::ReadJumpHeld(Controller));
    Input->HandleButton(EKeys::Gamepad_RightTrigger, true);
    auto* Cards = Controller->FindComponentByClass<UCardRoomInteractionComponent>();
    Cards->Stage = World->SpawnActor<ACardRoomStage>();
    Cards->bRoomActive = true;
    auto& Run = Cards->Stage->Run;
    Run.Phase = ECardRoomPhase::Choosing;
    Run.Round = 1;
    Run.Hand = {ERoomCard::Laser};
    Input->HandleButton(EKeys::Gamepad_FaceButton_Left, true);
    TestFalse(TEXT("Entering cards releases the previous mining hold"), Mining->IsMiningActive());
    TestTrue(TEXT("X cannot select cards"), Run.Staged.IsEmpty());
    Input->HandleButton(EKeys::Gamepad_FaceButton_Left, false);
    TestTrue(TEXT("Card game has no controller hint strip"), Input->GetHint().IsEmpty());
    Cards->ToggleCardSelection(0);
    TestTrue(TEXT("Mouse selection action stages the chosen card"), Run.Staged.Contains(0));
    Input->HandleButton(EKeys::Gamepad_Special_Right, true);
    Input->HandleButton(EKeys::Gamepad_Special_Right, false);
    TestTrue(TEXT("Menu pauses over cards"), Instance->GetSubsystem<UMiningExperienceSubsystem>()->IsMenuOpen());
    TestTrue(TEXT("Pause owns routing over the seated card game"), Input->GetContext() == EMiningInputContext::Menu);
    Input->HandleButton(EKeys::Gamepad_FaceButton_Right, true);
    Input->HandleButton(EKeys::Gamepad_FaceButton_Right, false);
    TestTrue(TEXT("Back resumes the existing card selection"), Input->GetContext() == EMiningInputContext::Cards && Run.Staged.Contains(0));
    for (const FKey Key : {EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Top, EKeys::Gamepad_DPad_Right})
    {
        Input->HandleButton(Key, true);
        Input->HandleButton(Key, false);
        TestTrue(TEXT("Controller buttons preserve mouse-selected hand"), Run.Phase == ECardRoomPhase::Choosing && Run.Staged.Contains(0));
    }
    Cards->ConfirmCards();
    TestTrue(TEXT("Space confirmation resolves mouse-selected cards"), Run.Phase == ECardRoomPhase::Resolving);
    TestFalse(TEXT("Card confirmation does not start flight"), UMiningGamepadSubsystem::ReadJumpHeld(Controller));
    Cards->bRoomActive = false;
    auto* Catalogue = World->SpawnActor<AMiningCollectionField>();
    Catalogue->bCatalogueOpen = true;
    Input->HandleButton(EKeys::Gamepad_RightTrigger, false);
    Input->HandleButton(EKeys::Gamepad_RightTrigger, true);
    TestTrue(TEXT("Catalogue has its own context"), Input->GetContext() == EMiningInputContext::Catalogue);
    TestFalse(TEXT("RT in catalogue cannot mine"), Mining->IsMiningActive());
    Catalogue->bCatalogueOpen = false;
    Input->ResetDevice();
    auto* Wallet = Instance->GetSubsystem<UMiningWalletSubsystem>();
    Wallet->bUseMineralInventory = true;
    for (const TCHAR* Mineral : {TEXT("2"), TEXT("4"), TEXT("6"), TEXT("9")}) Wallet->AddMineral(Mineral, 999);
    auto* Flight = NewObject<UMiningCharacterMovementComponent>(Pawn);
    Pawn->AddInstanceComponent(Flight);
    auto* Bomb = NewObject<UMiningBombToolComponent>(Pawn);
    Pawn->AddInstanceComponent(Bomb);
    auto* Progression = NewObject<UMiningProgressionComponent>(Pawn);
    Pawn->AddInstanceComponent(Progression);
    TestTrue(TEXT("Fixture initializes the complete progression components"), Progression->InitializeProgression());
    TestTrue(TEXT("Fixture can afford the next upgrade"), Progression->CanUpgradePickaxe());
    TestTrue(TEXT("Exploration hints show LT upgrade"), Input->GetHint().Contains(TEXT("LT 升级")));
    TestFalse(TEXT("Exploration hints no longer offer a map"), Input->GetHint().Contains(TEXT("地图")));
    Input->HandleButton(EKeys::Gamepad_DPad_Right, true);
    Input->HandleButton(EKeys::Gamepad_DPad_Right, false);
    Input->HandleButton(EKeys::Gamepad_Special_Left, true);
    Input->HandleButton(EKeys::Gamepad_Special_Left, false);
    TestEqual(TEXT("Old upgrade and map buttons never purchase"), Progression->GetLevel(0), 0);
    Input->HandleButton(EKeys::Gamepad_LeftTrigger, true);
    TestEqual(TEXT("LT immediately purchases one level"), Progression->GetLevel(0), 1);
    TestTrue(TEXT("Upgrade leaves exploration active"), Input->GetContext() == EMiningInputContext::Exploration);
    TestFalse(TEXT("Upgrade never pauses the world"), Controller->IsPaused());
    TestFalse(TEXT("Upgrade never opens a cursor"), Controller->bShowMouseCursor);
    TestFalse(TEXT("Upgrade never suspends mining"), Mining->IsInputSuspended());
    TestFalse(TEXT("Upgrade never locks movement"), Controller->IsMoveInputIgnored());
    TestFalse(TEXT("Upgrade never locks the camera"), Controller->IsLookInputIgnored());
    TestFalse(TEXT("LT upgrade never starts flight"), UMiningGamepadSubsystem::ReadJumpHeld(Controller));
    Input->HandleButton(EKeys::Gamepad_LeftTrigger, true, true);
    Input->HandleButton(EKeys::Gamepad_LeftTrigger, true);
    Input->HandleAxis(EKeys::Gamepad_LeftTriggerAxis, 1.f);
    TestEqual(TEXT("Held LT and analog samples never buy again"), Progression->GetLevel(0), 1);
    Input->HandleButton(EKeys::Gamepad_LeftTrigger, false);
    Input->HandleButton(EKeys::Gamepad_FaceButton_Bottom, true);
    Input->HandleButton(EKeys::Gamepad_FaceButton_Bottom, false);
    TestEqual(TEXT("A remains jump, not purchase confirmation"), Progression->GetLevel(0), 1);
    Input->HandleButton(EKeys::Gamepad_LeftTrigger, true);
    Input->HandleButton(EKeys::Gamepad_LeftTrigger, false);
    TestEqual(TEXT("A fresh LT press purchases the next level"), Progression->GetLevel(0), 2);
    Input->HandleButton(EKeys::Gamepad_Special_Right, true);
    Input->HandleButton(EKeys::Gamepad_Special_Right, false);
    Input->HandleButton(EKeys::Gamepad_LeftTrigger, true);
    Input->HandleButton(EKeys::Gamepad_FaceButton_Right, true);
    Input->HandleButton(EKeys::Gamepad_FaceButton_Right, false);
    Input->HandleButton(EKeys::Gamepad_LeftTrigger, true, true);
    TestEqual(TEXT("LT held through pause cannot purchase on return"), Progression->GetLevel(0), 2);
    Input->HandleButton(EKeys::Gamepad_LeftTrigger, false);
    Catalogue->bCatalogueOpen = true;
    Input->HandleButton(EKeys::Gamepad_LeftTrigger, true);
    Input->HandleButton(EKeys::Gamepad_LeftTrigger, false);
    TestEqual(TEXT("Catalogue zoom cannot purchase upgrades"), Progression->GetLevel(0), 2);
    Catalogue->bCatalogueOpen = false;
    Wallet->Minerals.Empty();
    Input->HandleButton(EKeys::Gamepad_LeftTrigger, true);
    Input->HandleButton(EKeys::Gamepad_LeftTrigger, false);
    TestEqual(TEXT("Insufficient resources preserve the current level"), Progression->GetLevel(0), 2);
    TestTrue(TEXT("Failed purchase gives inline feedback"), Input->GetHint().Contains(TEXT("资源不足")));
    Input->ResetDevice();
    auto* ReplacementPawn = World->SpawnActor<ACharacter>();
    Controller->Possess(ReplacementPawn);
    Input->HandleButton(EKeys::Gamepad_LeftTrigger, true);
    TestEqual(TEXT("A replacement pawn never buys for the previous pawn"), Progression->GetLevel(0), 2);
    Input->HandleButton(EKeys::Gamepad_LeftTrigger, false);
    Controller->Possess(Pawn);
    auto* Opening = World->SpawnActor<AMiningOpening>();
    Controller->bShowMouseCursor = true;
    TestTrue(TEXT("Opening menu accepts UI controller input"), Input->GetContext() == EMiningInputContext::Menu);
    Input->HandleAxis(EKeys::Gamepad_LeftX, 1);
    TestTrue(TEXT("Opening menu cannot move standalone pawns"), UMiningGamepadSubsystem::ReadMove(Controller).IsNearlyZero());
    Input->ResetDevice();
    Opening->bStarted = true;
    Controller->bShowMouseCursor = false;
    TestTrue(TEXT("Opening animation still blocks gameplay"), Input->GetContext() == EMiningInputContext::Blocked);
    Input->HandleButton(EKeys::Gamepad_FaceButton_Bottom, true);
    TestFalse(TEXT("Opening animation cannot start flight"), UMiningGamepadSubsystem::ReadJumpHeld(Controller));
    Opening->bFinished = true;
    TestTrue(TEXT("Completed opening restores exploration"), Input->GetContext() == EMiningInputContext::Exploration);
    Input->ResetDevice();
    return true;
}
#endif
