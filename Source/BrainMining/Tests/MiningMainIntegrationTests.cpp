#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "MiningExperienceSubsystem.h"
#include "BrainMiningPlayerController.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningCollection.h"
#include "MiningComponent.h"
#include "MiningCellUniverse.h"
#include "PopupClose/PopupCloseStage.h"
#include "PopupClose/PopupOfficeDevice.h"
#include "WenSi/WenSiPuzzle.h"
#include "Showcase/MiningShowcase.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "InputKeyEventArgs.h"
#include "Input/Events.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"

namespace MiningMainTests
{
    template<typename ActorType> ActorType* First(UWorld* World)
    {
        for (TActorIterator<ActorType> Actor(World); Actor; ++Actor) return *Actor;
        return nullptr;
    }
    class FStart : public IAutomationLatentCommand
    {
        bool Update() override
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
    class FIntegration : public IAutomationLatentCommand
    {
    public:
        explicit FIntegration(FAutomationTestBase* InTest) : Test(InTest) {}
        bool Update() override
        {
            if (FPlatformTime::Seconds() - Started > 150)
            {
                Test->AddError(FString::Printf(TEXT("Main integration timed out at step %d"), Step));
                return true;
            }
            UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
            if (!World || FPlatformTime::Seconds() - Changed < 1) return false;
            auto* Controller = World->GetFirstPlayerController();
            if (!Controller || !Controller->GetPawn()) return false;
            auto* Menu = World->GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>();
            if (!Menu) { Test->AddError(TEXT("Main experience subsystem missing")); return true; }
            auto* Mining = Controller->GetPawn()->FindComponentByClass<UMiningComponent>();
            auto* MiningController = Cast<ABrainMiningPlayerController>(Controller);
            auto* Field = First<AMiningCollectionField>(World);
            if (Step == 0)
            {
                if (!Field || Field->Items.Num() != 5) return false;
                Controller->GetPawn()->SetActorLocation(FVector(-400,-4000,100));
            }
            else if (Step == 1)
            {
                if (!MiningController || !Mining) return false;
                Test->TestTrue(TEXT("Mining area owns upgrade UI"), MiningController->IsMiningGUIEnabled());
                MiningController->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Tab, IE_Pressed, 1));
                Test->TestTrue(TEXT("Held Tab opens upgrade UI"), MiningController->bUpgradeMenuHeld);
                Test->TestFalse(TEXT("Drill cannot start through upgrade UI"), Mining->ActivateDrill());
                Controller->GetPawn()->SetActorLocation(FVector(-500,-330,100));
            }
            else if (Step == 2)
            {
                Test->TestFalse(TEXT("Crossing into collection closes upgrades"), MiningController->bUpgradeMenuHeld);
                Test->TestFalse(TEXT("Collection reserves Tab"), MiningController->IsMiningGUIEnabled());
                Test->TestFalse(TEXT("Mining input suspension is released"), Mining->IsInputSuspended());
                Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Tab, IE_Released, 0));
                Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Tab, IE_Pressed, 1));
            }
            else if (Step == 3)
            {
                Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Tab, IE_Released, 0));
                Test->TestTrue(TEXT("Real Tab input opens collection catalogue"), Field->bCatalogueOpen);
                Test->TestFalse(TEXT("Main menu does not steal catalogue input"), Menu->ToggleMenu());
                if (Field->bCatalogueOpen) Field->ToggleCatalogue();
                Menu->ToggleDeveloperMenu();
                Test->TestTrue(TEXT("GM window opens in exploration"), Menu->IsMenuOpen());
                Test->TestTrue(TEXT("Menu pauses world and suspends mining"), Controller->IsPaused() && Mining->IsInputSuspended());
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("MainIntegrationPreview/AllExperiences.png"), true, false);
            }
            else if (Step == 4)
            {
                FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
                Test->TestFalse(TEXT("Escape closes main menu"), Menu->IsMenuOpen());
                Test->TestFalse(TEXT("Menu closing restores pause and input"), Controller->IsPaused() || Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored() || Mining->IsInputSuspended());
                Test->TestFalse(TEXT("Invalid destination is rejected"), Menu->OpenExperience(-1));
                Test->TestTrue(TEXT("Office destination opens"), Menu->OpenExperience(4));
            }
            else if (Step == 5)
            {
                if (!World->GetMapName().EndsWith(TEXT("Lvl_PopupClose"))) return false;
                auto* Office = First<APopupCloseStage>(World);
                if (!Office) return false;
                int32 Devices = 0;
                Office->StartOfficeRound();
                for (TActorIterator<APopupOfficeDevice> Device(World); Device; ++Device)
                {
                    ++Devices; Device->AddProblem();
                    Test->TestTrue(TEXT("Office device accumulates work"), Device->Pending() > 0);
                    if (Device->IsPrinter()) Device->ProcessPaper();
                }
                Test->TestEqual(TEXT("Latest office has four devices"), Devices, 4);
                Office->ResetCompletedSessions();
                for (TActorIterator<APopupOfficeDevice> Device(World); Device; ++Device)
                    Test->TestTrue(TEXT("Office reset clears counts and unlocks devices"), Device->Pending() == 0 && Device->Handled() == 0 && Device->AccountingValid() && !Device->IsLocked());
                Test->TestTrue(TEXT("Menu survives map travel"), Menu->ToggleMenu());
                Test->TestTrue(TEXT("Ending destination opens"), Menu->OpenExperience(5));
            }
            else if (Step == 6)
            {
                if (!World->GetMapName().EndsWith(TEXT("Lvl_CellUniverse_Demo"))) return false;
                Test->TestNotNull(TEXT("Latest ending present"), First<AMiningCellUniverse>(World));
                auto* Movement = Controller->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>();
                Test->TestTrue(TEXT("Ending keeps authored flight budget"), Movement && Movement->FlightSettings.bEnabled && !Movement->FlightSettings.bEnableHeat);
                Test->TestTrue(TEXT("Museum destination opens"), Menu->OpenExperience(3));
            }
            else if (Step == 7)
            {
                if (!World->GetMapName().EndsWith(TEXT("Lvl_WenSi_ScreenBridge"))) return false;
                auto* Puzzle = First<AWenSiPuzzle>(World);
                if (!Puzzle) return false;
                Test->TestTrue(TEXT("Museum has latest circuit puzzle"), Puzzle->bCircuitPuzzle);
                const FString Result = Puzzle->ValidateCircuitPuzzle();
                Test->TestTrue(*Result, Result.StartsWith(TEXT("PASS")));
                Test->TestTrue(TEXT("Shared menu works with WenSi pawn"), Menu->ToggleMenu());
                Test->TestTrue(TEXT("Return to main overview"), Menu->OpenExperience(0));
            }
            else if (Step == 8)
            {
                if (!World->GetMapName().EndsWith(TEXT("Lvl_ContinuousShowcase"))) return false;
                auto* Director = First<AMiningShowcaseDirector>(World);
                if (!Director || !Field || Field->Items.Num() != 5) return false;
                Test->TestEqual(TEXT("Overview retains seven areas"), Director->Exhibits.Num(), 7);
                Test->TestFalse(TEXT("Travel leaves no menu or input locks"), Menu->IsMenuOpen() || Controller->IsPaused() || Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored());
                return true;
            }
            ++Step;
            Changed = FPlatformTime::Seconds();
            return false;
        }
    private:
        FAutomationTestBase* Test;
        int32 Step = 0;
        double Started = FPlatformTime::Seconds();
        double Changed = FPlatformTime::Seconds();
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainMapCatalogTest, "BrainMining.Main.AllExperienceAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMainMapCatalogTest::RunTest(const FString&)
{
    for (const FString& Map : UMiningExperienceSubsystem::MapPaths()) TestTrue(*Map, FPackageName::DoesPackageExist(Map));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainIntegrationTest, "BrainMining.Main.InputAndLatestMapTravel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMainIntegrationTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Showcase/Maps/Lvl_ContinuousShowcase"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<MiningMainTests::FStart>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<MiningMainTests::FIntegration>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
