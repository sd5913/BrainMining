#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Showcase/MiningShowcase.h"
#include "MiningPrototypeGameMode.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningComponent.h"
#include "MiningCollection.h"
#include "DiggableRock.h"
#include "PopupClose/PopupCloseStage.h"
#include "WenSi/WenSiPuzzle.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "EchoPassagePlayer.h"
#include "EchoPassageStage.h"
#include "EchoPassageLayout.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"

namespace MiningShowcaseTests
{
    class FStartShowcasePIE : public IAutomationLatentCommand
    {
    public:
        virtual bool Update() override
        {
            FRequestPlaySessionParams Parameters;
            auto* Settings = DuplicateObject<ULevelEditorPlaySettings>(GetMutableDefault<ULevelEditorPlaySettings>(), GetTransientPackage());
            Settings->NewWindowWidth = 1600;
            Settings->NewWindowHeight = 900;
            Parameters.EditorPlaySettings = Settings;
            GEditor->RequestPlaySession(Parameters);
            return true;
        }
    };
    template<typename ActorType> ActorType* First(UWorld* World)
    {
        for (TActorIterator<ActorType> Actor(World); Actor; ++Actor) return *Actor;
        return nullptr;
    }
    class FVisitAllExhibits : public IAutomationLatentCommand
    {
    public:
        explicit FVisitAllExhibits(FAutomationTestBase* InTest) : Test(InTest) {}
        virtual bool Update() override
        {
            if (FPlatformTime::Seconds() - Started > 120)
            {
                Test->AddError(FString::Printf(TEXT("Showcase timeout: exhibit %d, step %d"), Visit, Step));
                return true;
            }
            UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
            if (!World) return false;
            auto* Director = First<AMiningShowcaseDirector>(World);
            auto* Controller = Cast<AMiningShowcaseController>(World->GetFirstPlayerController());
            if (!Director || !Controller) return false;
            const double Age = FPlatformTime::Seconds() - Started;
            if (Step == 0)
            {
                if (Age < 2) return false;
                Test->TestEqual(TEXT("Eleven source exhibits"), Director->Exhibits.Num(), 11);
                Test->TestTrue(TEXT("Directory opens in lobby"), Director->bMenuOpen);
                Test->TestFalse(TEXT("Invalid exhibit rejected"), Director->EnterExhibit(-2));
                Screenshot(TEXT("00-Directory"));
                Advance();
            }
            else if (Step == 1)
            {
                if (Age < 0.5) return false;
                const int32 Index = Visit < 11 ? Visit : 6;
                Test->TestTrue(TEXT("Exhibit request accepted"), Director->EnterExhibit(Index));
                Test->TestFalse(TEXT("Concurrent transition rejected"), Director->EnterExhibit(Index));
                Advance();
            }
            else if (Step == 2)
            {
                if (Director->bBusy || !Controller->GetPawn() || Age < 2) return false;
                Test->TestEqual(TEXT("Expected active exhibit"), Director->ActiveIndex, Visit < 11 ? Visit : 6);
                Test->TestEqual(TEXT("Original shared pawn"), Controller->GetPawn()->GetClass()->GetName(), FString(TEXT("BP_FirstPersonCharacter_C")));
                Test->TestFalse(TEXT("Move unlocked on entry"), Controller->IsMoveInputIgnored());
                Test->TestFalse(TEXT("Look unlocked on entry"), Controller->IsLookInputIgnored());
                auto* Mining = Controller->GetPawn()->FindComponentByClass<UMiningComponent>();
                auto* Movement = Controller->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>();
                if (!Mining || !Movement) { Test->AddError(TEXT("Shared mining/movement missing")); return true; }
                const auto Kind = Director->GetActiveKind();
                const bool bEncounter = Kind != EMiningShowcaseKind::Mining && Kind != EMiningShowcaseKind::Collection;
                Test->TestEqual(TEXT("Scoped mining policy"), Mining->IsEncounterBlocked(), bEncounter);
                if (bEncounter) Test->TestFalse(TEXT("Encounter cannot fly"), Movement->IsFlightAvailable());
                if (Kind == EMiningShowcaseKind::Mining) Test->TestNotNull(TEXT("Mining geometry loaded"), First<ADiggableRock>(World));
                if (Kind == EMiningShowcaseKind::Collection)
                {
                    auto* Field = First<AMiningCollectionField>(World);
                    if (!Field || Field->Items.Num() != 5) { Test->AddError(TEXT("Collection field failed to initialize")); return true; }
                    Test->TestEqual(TEXT("Reentry inventory starts empty"), Field->UniqueCount(), 0);
                    for (auto Item : Field->Items) if (Item && Item->bReleased && Item->ItemId == 2)
                    {
                        Controller->GetPawn()->SetActorLocation(Item->GetActorLocation() + FVector(-220, 0, 25));
                        Controller->PlayerCameraManager->UpdateCamera(0.01f);
                        FVector Eye;
                        FRotator Rotation;
                        Controller->GetPlayerViewPoint(Eye, Rotation);
                        Controller->SetControlRotation((Item->GetActorLocation() - Eye).Rotation());
                        Controller->PlayerCameraManager->UpdateCamera(0.01f);
                        Field->CollectTarget();
                        break;
                    }
                    Test->TestEqual(TEXT("Tutorial collectible enters inventory"), Field->UniqueCount(), 1);
                    Field->ToggleCatalogue();
                    Test->TestTrue(TEXT("Catalogue pauses world"), UGameplayStatics::IsGamePaused(World));
                    Test->TestTrue(TEXT("Catalogue open"), Field->bCatalogueOpen);
                }
                if (Kind == EMiningShowcaseKind::Cards)
                {
                    auto* Cards = Director->CardInteraction.Get();
                    if (!Cards || !Cards->Stage) { Test->AddError(TEXT("Card interaction missing")); return true; }
                    auto* Character = Cast<ACharacter>(Controller->GetPawn());
                    Character->SetActorLocation(Cards->Stage->GetApproachLocation() + FVector(0, 0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
                    Cards->Interact();
                    Test->TestTrue(TEXT("Seat locks movement"), Controller->IsMoveInputIgnored());
                    Test->TestFalse(TEXT("Seat disables collision"), Character->GetActorEnableCollision());
                }
                if (Kind == EMiningShowcaseKind::Echo)
                {
                    Test->TestNotNull(TEXT("Echo interaction"), Director->EchoInteraction.Get());
                    if (Director->EchoInteraction && Director->EchoInteraction->Stage)
                        Test->TestTrue(TEXT("OpenLevel removed only in runtime instance"), Director->EchoInteraction->Stage->DestinationLevel.IsNull());
                }
                if (Kind == EMiningShowcaseKind::WenSi)
                {
                    auto* Puzzle = First<AWenSiPuzzle>(World);
                    if (!Puzzle) { Test->AddError(TEXT("WenSi puzzle missing")); return true; }
                    Test->TestTrue(TEXT("WenSi solutions work with shared pawn"), Puzzle->ValidatePuzzle().StartsWith(TEXT("PASS")));
                    if (!Puzzle->Objects.IsEmpty()) Puzzle->Thoughts[0] = Puzzle->Objects[0];
                    Puzzle->ToggleEdit();
                    Test->TestTrue(TEXT("WenSi editing entered"), Puzzle->bEditing);
                }
                if (Kind == EMiningShowcaseKind::Popup)
                {
                    Controller->GetPawn()->SetActorLocation(FVector(-170, 0, 110));
                    FVector Eye;
                    FRotator Rotation;
                    Controller->GetPlayerViewPoint(Eye, Rotation);
                    Controller->SetControlRotation((FVector(42, 0, 124) - Eye).Rotation());
                }
                if (Visit == 0)
                {
                    WalkStart = Controller->GetPawn()->GetActorLocation();
                    Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W, IE_Pressed, 1));
                }
                Advance();
            }
            else if (Step == 3)
            {
                if (Age < 0.7) return false;
                if (Visit == 0)
                {
                    Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W, IE_Released, 0));
                    Test->TestTrue(TEXT("Original W input moves the shared pawn"), FVector::Dist2D(WalkStart, Controller->GetPawn()->GetActorLocation()) > 20);
                }
                if (Director->GetActiveKind() == EMiningShowcaseKind::Popup)
                    Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E, IE_Pressed, 1));
                Screenshot(FString::Printf(TEXT("%02d-%s"), Visit + 1, *Director->Exhibits[Director->ActiveIndex].Id.ToString()));
                Advance();
            }
            else if (Step == 4)
            {
                if (Age < 1.2) return false;
                if (Visit == 11 && !bRestarted)
                {
                    bRestarted = true;
                    Shortcut(EKeys::F5);
                    Step = 2;
                    Started = FPlatformTime::Seconds();
                    return false;
                }
                if (Director->GetActiveKind() == EMiningShowcaseKind::Popup)
                {
                    Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E, IE_Released, 0));
                    Test->TestTrue(TEXT("Popup screen locks look"), Controller->IsLookInputIgnored());
                    Test->TestTrue(TEXT("Popup desktop exposes cursor"), Controller->bShowMouseCursor);
                    Screenshot(TEXT("10-PopupDesktop"));
                    Step = 40;
                    Started = FPlatformTime::Seconds();
                    return false;
                }
                Shortcut(EKeys::F8);
                Advance();
            }
            else if (Step == 40)
            {
                if (Age < 0.4) return false;
                Shortcut(EKeys::F8);
                Step = 5;
                Started = FPlatformTime::Seconds();
            }
            else if (Step == 5)
            {
                if (Director->bBusy || Age < 1) return false;
                Test->TestEqual(TEXT("Back in lobby"), Director->ActiveIndex, INDEX_NONE);
                Test->TestFalse(TEXT("Pause released after exit"), UGameplayStatics::IsGamePaused(World));
                Director->SetMenuOpen(false);
                Test->TestFalse(TEXT("Move lock cleared after exit"), Controller->IsMoveInputIgnored());
                Test->TestFalse(TEXT("Look lock cleared after exit"), Controller->IsLookInputIgnored());
                Test->TestTrue(TEXT("Collision restored"), Controller->GetPawn() && Controller->GetPawn()->GetActorEnableCollision());
                Test->TestNull(TEXT("No collectible leaks"), First<AMiningCollectible>(World));
                Test->TestNull(TEXT("No rock leaks"), First<ADiggableRock>(World));
                Test->TestNull(TEXT("No echo stage leaks"), First<AEchoPassageStage>(World));
                Test->TestNull(TEXT("No card stage leaks"), First<ACardRoomStage>(World));
                Test->TestNull(TEXT("No WenSi leaks"), First<AWenSiPuzzle>(World));
                Test->TestNull(TEXT("No popup stage leaks"), First<APopupCloseStage>(World));
                ++Visit;
                if (Visit == 12)
                {
                    Test->TestEqual(TEXT("All exhibits visited"), Director->Visited.Num(), 11);
                    Screenshot(TEXT("99-Lobby"));
                    Director->EnterExhibit(7);
                    Advance();
                    return false;
                }
                Step = 1;
                Started = FPlatformTime::Seconds();
            }
            else if (Step == 6)
            {
                if (Director->bBusy || !Director->EchoInteraction || Age < 1) return false;
                auto* Stage = Director->EchoInteraction->Stage.Get();
                if (!Stage) { Test->AddError(TEXT("Missing stage on echo reentry")); return true; }
                Stage->bLanded = true;
                Controller->GetPawn()->SetActorLocation(Stage->GetActorTransform().TransformPosition(EchoPassageLayout::ExitOffset(Stage->bUseMaze) + FVector(4200, 1600, 100)));
                Advance();
            }
            else if (Step == 7)
            {
                if (Director->bBusy || Director->ActiveIndex != 8 || Age < 1) return false;
                Test->TestNotNull(TEXT("Echo exit activates cards in same world"), Director->CardInteraction.Get());
                Test->TestTrue(TEXT("Echo completion recorded"), Director->Completed.Contains(TEXT("Echo")));
                Test->TestNull(TEXT("Echo was unloaded on journey"), First<AEchoPassageStage>(World));
                Director->ReturnToLobby();
                Advance();
            }
            else if (Step == 8)
            {
                if (Director->bBusy || Age < 1) return false;
                Test->TestEqual(TEXT("Journey returns to persistent lobby"), Director->ActiveIndex, INDEX_NONE);
                return true;
            }
            return false;
        }
    private:
        FAutomationTestBase* Test;
        int32 Visit = 0;
        int32 Step = 0;
        bool bRestarted = false;
        FVector WalkStart;
        double Started = FPlatformTime::Seconds();
        void Advance() { ++Step; Started = FPlatformTime::Seconds(); }
        void Shortcut(FKey Key)
        {
            FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0));
        }
        void Screenshot(const FString& Name)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("ShowcasePreview") / (Name + TEXT(".png")), true, false);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShowcasePawnTest, "BrainMining.Showcase.SharedPawn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShowcasePawnTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Showcase reuses prototype pawn"), GetDefault<AMiningShowcaseGameMode>()->DefaultPawnClass, GetDefault<AMiningPrototypeGameMode>()->DefaultPawnClass);
    TestEqual(TEXT("Showcase input router"), GetDefault<AMiningShowcaseGameMode>()->PlayerControllerClass.Get(), AMiningShowcaseController::StaticClass());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShowcasePIETest, "BrainMining.Showcase.PIE.AllExhibitsAndCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShowcasePIETest::RunTest(const FString& Parameters)
{
    AutomationOpenMap(TEXT("/Game/Showcase/Maps/Lvl_FeatureShowcase"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<MiningShowcaseTests::FStartShowcasePIE>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<MiningShowcaseTests::FVisitAllExhibits>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
