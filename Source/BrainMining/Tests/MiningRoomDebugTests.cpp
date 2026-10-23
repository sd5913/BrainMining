#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "MiningExperienceSubsystem.h"
#include "Showcase/MiningExhibitHall.h"
#include "StartRoom/MiningStartRoom.h"
#include "StartRoom/MiningOpening.h"
#include "MiningComponent.h"
#include "BrainMiningCharacter.h"
#include "MiningProgressionComponent.h"
#include "MiningBombToolComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "MiningCollection.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningWalletSubsystem.h"
#include "MiningRoomEscape.h"
#include "WenSi/WenSiMirrorPuzzle.h"
#include "PopupClose/PopupCloseStage.h"
#include "CardRoomStage.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

namespace MiningRoomDebugTests
{
    class FStart : public IAutomationLatentCommand
    {
        virtual bool Update() override
        {
            FRequestPlaySessionParams Parameters;
            auto* Settings = DuplicateObject<ULevelEditorPlaySettings>(GetMutableDefault<ULevelEditorPlaySettings>(), GetTransientPackage());
            Settings->NewWindowWidth = 1440;
            Settings->NewWindowHeight = 900;
            Parameters.EditorPlaySettings = Settings;
            GEditor->RequestPlaySession(Parameters);
            return true;
        }
    };

    class FRun : public IAutomationLatentCommand
    {
    public:
        explicit FRun(FAutomationTestBase* InTest) : Test(InTest) {}
        virtual bool Update() override
        {
            if (FPlatformTime::Seconds() - Started > 240)
            {
                Test->AddError(FString::Printf(TEXT("Room debug timeout at step %d"), Step));
                return true;
            }
            UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
            if (!World || !World->HasBegunPlay() || World->GetTimeSeconds() < 2) return false;
            auto* Controller = World->GetFirstPlayerController();
            auto* Menu = World->GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>();
            if (!Controller || !Controller->GetPawn() || !Menu) return false;
            auto* Hall = AMiningExhibitHall::Find(World);
            // Entering or reloading MainGame shows the real opening menu first.
            for(TActorIterator<AMiningOpening> Opening(World);Opening;++Opening)
                if(!Opening->bFinished){Opening->StartGame();return false;}
            if (Step == 0)
            {
                if (!Hall || !Hall->bReady) return false;
                auto* Mining = Controller->GetPawn()->FindComponentByClass<UMiningComponent>();
                auto* Movement = Controller->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>();
                auto* Wallet = World->GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
                if (!Mining || !Movement || !Wallet) { Test->AddError(TEXT("Player test dependencies missing")); return true; }
                const int32 InitialArea = Hall->ActiveArea;
                const int32 InitialUnlocked = Hall->UnlockedArea;
                Wallet->AddMineral(TEXT("2"), 3);
                const int32 Copper = Wallet->GetMineralCount(EMiningMineral::Copper);
                FSlateApplication::Get().SetAllUserFocusToGameViewport();
                FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::F10,
                    FModifierKeysState(false, false, true, false, false, false, false, false, false), 0, false, 0, 0));
                if (!Test->TestTrue(TEXT("Ctrl F10 opens developer menu in main game"), Menu->IsMenuOpen())) return true;
                Test->TestTrue(TEXT("Menu pauses game"), Controller->IsPaused());
                Test->TestTrue(TEXT("Menu suspends mining"), Mining->IsInputSuspended());
                auto* Progress=Controller->GetPawn()->FindComponentByClass<UMiningProgressionComponent>();
                auto* Character=Cast<ABrainMiningCharacter>(Controller->GetPawn());
                if(!Test->TestNotNull(TEXT("GM upgrades target the actual player"),Character) || !Progress)return true;
                const int32 InitialPickaxeLevel=Progress->GetLevel(0);
                for(int32 Level=InitialPickaxeLevel;Level<4;++Level)
                    Test->TestTrue(TEXT("GM grants one free pickaxe level"),Menu->DeveloperGrantTool(0));
                Test->TestTrue(TEXT("Repeated max-level grant is safe"),Menu->DeveloperGrantTool(0));
                Test->TestEqual(TEXT("GM caps the pickaxe at level four"),Progress->GetLevel(0),4);
                Test->TestEqual(TEXT("GM preserves collected upgrade materials"),Wallet->GetMineralCount(EMiningMineral::Copper),Copper);
                auto* MaxMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Pickaxe/SM_Pickaxe_max.SM_Pickaxe_max"));
                auto* Model=Character->GetPickaxeModel();
                Test->TestTrue(TEXT("Max-level player holds the authored diamond mesh"),MaxMesh && Model->GetStaticMesh()==MaxMesh);
                for(int32 Slot=0;MaxMesh && Slot<MaxMesh->GetStaticMaterials().Num();++Slot)
                    Test->TestTrue(TEXT("Diamond materials replace earlier tier overrides"),Model->GetMaterial(Slot)==MaxMesh->GetMaterial(Slot));
                Test->TestTrue(TEXT("GM unlocks bombs"),Menu->DeveloperGrantTool(1));
                Test->TestTrue(TEXT("GM unlocks drilling"),Menu->DeveloperGrantTool(2));
                Test->TestTrue(TEXT("Equipment grants leave the GM menu paused and open"),Menu->IsMenuOpen() && Controller->IsPaused());
                Test->TestFalse(TEXT("Invalid GM tool is rejected"),Menu->DeveloperGrantTool(3));
                Test->TestFalse(TEXT("Invalid room rejected"), Menu->JumpToDeveloperArea(-1));
                Test->TestFalse(TEXT("Invalid map rejected"), Menu->OpenDeveloperExperience(-1));
                Test->TestTrue(TEXT("Failure leaves menu open"), Menu->IsMenuOpen());
                Test->TestEqual(TEXT("Failure preserves active area"), Hall->ActiveArea, InitialArea);
                Test->TestEqual(TEXT("Failure preserves unlocks"), Hall->UnlockedArea, InitialUnlocked);
                FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
                Test->TestFalse(TEXT("Escape closes debug menu"), Menu->IsMenuOpen());
                Test->TestFalse(TEXT("Closing unpauses game"), Controller->IsPaused());
                Test->TestFalse(TEXT("Closing restores mining"), Mining->IsInputSuspended());
                for (int32 Index = Hall->Areas.Num() - 1; Index >= 0; --Index)
                {
                    const FString Label = Hall->Areas[Index].Title.ToString();
                    Test->TestTrue(*FString::Printf(TEXT("Open menu for %s"), *Label), Menu->ToggleDeveloperMenu());
                    Movement->Velocity = FVector(1200, 400, -1800);
                    const bool bJumped = Menu->JumpToDeveloperArea(Index);
                    Test->TestTrue(*FString::Printf(TEXT("Jump to %s"), *Label), bJumped);
                    if (!bJumped) { Test->AddError(Menu->GetMenuError()); Menu->CloseMenu(); return true; }
                    Test->AddInfo(FString::Printf(TEXT("Room jump verified: %s, entry offset %.1f cm"), *Label,
                        FVector::Dist(Controller->GetPawn()->GetActorLocation(), Hall->Areas[Index].Entry.GetLocation())));
                    Test->TestEqual(TEXT("Requested room activated"), Hall->ActiveArea, Index);
                    Test->TestTrue(TEXT("Arrive near authored entry"), FVector::Dist(Controller->GetPawn()->GetActorLocation(), Hall->Areas[Index].Entry.GetLocation()) < 350);
                    Test->TestTrue(TEXT("Momentum cleared"), Movement->Velocity.IsNearlyZero());
                    Test->TestFalse(TEXT("Jump closes menu"), Menu->IsMenuOpen());
                    Test->TestFalse(TEXT("Jump resumes world"), Controller->IsPaused());
                    Test->TestFalse(TEXT("Jump restores look"), Controller->IsLookInputIgnored());
                    Test->TestFalse(TEXT("Jump restores movement"), Controller->IsMoveInputIgnored());
                    Test->TestEqual(TEXT("Jump retains minerals"), Wallet->GetMineralCount(EMiningMineral::Copper), Copper);
                    Test->TestFalse(TEXT("Jump does not finish campaign"), Hall->bCampaignComplete);
                    if (Hall->AreaKind(Index) == EMiningExperienceKind::Journey)
                    {
                        Test->TestTrue(TEXT("Journey activated"), Hall->Journey && Hall->Journey->bAreaActive);
                        Test->TestTrue(TEXT("Journey retains its mining restriction"), Mining->IsEncounterBlocked());
                    }
                    if (Hall->AreaKind(Index) == EMiningExperienceKind::Museum)
                        for (TActorIterator<AWenSiMirrorPuzzle> Puzzle(World); Puzzle; ++Puzzle)
                        {
                            Test->TestTrue(TEXT("Mirror puzzle activated"), Puzzle->bAreaActive);
                            Test->TestFalse(TEXT("Mirror puzzle not auto-completed"), Puzzle->bComplete);
                        }
                    if (Hall->AreaKind(Index) == EMiningExperienceKind::Office)
                        for (TActorIterator<APopupCloseStage> Office(World); Office; ++Office)
                            Test->TestTrue(TEXT("Office activated"), Office->bAreaActive);
                }
                if (auto* StartRoom = AMiningStartRoom::Find(World))
                {
                    Test->TestTrue(TEXT("Pickaxe tutorial lock cleared"), StartRoom->bPickaxeUnlocked);
                    Test->TestTrue(TEXT("Jetpack tutorial lock cleared"), StartRoom->bJetpackUnlocked);
                }
                Test->TestFalse(TEXT("Mining usable after returning to mine"), Mining->IsEncounterBlocked());
                Test->TestTrue(TEXT("Jetpack usable after returning to mine"), Movement->IsFlightAvailable());
                Test->TestTrue(TEXT("Escape pause opens"),Menu->TogglePauseMenu());
                Test->TestTrue(TEXT("Pause owns input and time"),Controller->IsPaused()&&Mining->IsInputSuspended());
                Menu->RequestQuitConfirmation();
                Test->TestTrue(TEXT("Desktop pause has no quit confirmation; exit belongs to opening menu"),!Menu->bQuitConfirmation&&Menu->IsMenuOpen());
                Menu->OpenPauseCollection();
                AMiningCollectionField* Collection=nullptr;
                for(TActorIterator<AMiningCollectionField> It(World);It;++It)if(It->bUseAuthoredPlacements){Collection=*It;break;}
                if(!Test->TestNotNull(TEXT("Shared collection exists"),Collection))return true;
                Test->TestTrue(TEXT("Pause opens the real catalogue"),Collection->bCatalogueOpen&&!Menu->IsMenuOpen());
                Collection->ToggleCatalogue();
                Test->TestFalse(TEXT("Closing catalogue restores world"),Controller->IsPaused());
                const TArray<int32> CollectedBefore=Collection->Inventory;
                for(int32 Item=0;Item<12;++Item)
                {
                    Test->TestTrue(TEXT("Open collectible GM"),Menu->ToggleDeveloperMenu());
                    const bool Jumped=Menu->JumpToCollectible(Item);
                    Test->TestTrue(*FString::Printf(TEXT("GM reaches collectible %d"),Item),Jumped);
                    if(!Jumped){Test->AddError(Menu->GetMenuError());Menu->CloseMenu();return true;}
                    Test->TestFalse(TEXT("GM resumes controls"),Controller->IsPaused()||Controller->IsMoveInputIgnored()||Controller->IsLookInputIgnored());
                    Test->TestTrue(TEXT("GM does not award collectibles"),Collection->Inventory==CollectedBefore);
                }
                Test->TestFalse(TEXT("Travel without debug menu rejected"), Menu->OpenDeveloperExperience(3));
                Test->TestTrue(TEXT("Open before independent room travel"), Menu->ToggleDeveloperMenu());
                Test->TestTrue(TEXT("Start independent card room travel"), Menu->OpenDeveloperExperience(3));
                Test->TestFalse(TEXT("Travel closes menu"), Menu->IsMenuOpen());
                ++Step;
                return false;
            }
            if (Step == 1)
            {
                if (UGameplayStatics::GetCurrentLevelName(World) != TEXT("Lvl_CardRoom_P1")) return false;
                Test->TestTrue(TEXT("Independent card room exists"), TActorIterator<ACardRoomStage>(World) ? true : false);
                Test->TestFalse(TEXT("Travel did not carry pause"), Controller->IsPaused());
                Test->TestTrue(TEXT("Menu available in independent room"), Menu->ToggleDeveloperMenu());
                Test->TestTrue(TEXT("Return to main game"), Menu->OpenDeveloperExperience(0));
                ++Step;
                return false;
            }
            if (!Hall || !Hall->bReady || UGameplayStatics::GetCurrentLevelName(World) != TEXT("Lvl_MainGame")) return false;
            Test->TestEqual(TEXT("Returning main game starts at first area"), Hall->ActiveArea, 0);
            Test->TestTrue(TEXT("Menu reusable after two map changes"), Menu->ToggleDeveloperMenu());
            Test->TestTrue(TEXT("Toggle closes menu"), Menu->ToggleDeveloperMenu());
            Test->TestFalse(TEXT("No stale menu after travel"), Menu->IsMenuOpen());
            Test->TestFalse(TEXT("No stacked move lock"), Controller->IsMoveInputIgnored());
            Test->TestFalse(TEXT("No stacked look lock"), Controller->IsLookInputIgnored());
            return true;
        }
    private:
        FAutomationTestBase* Test;
        int32 Step = 0;
        double Started = FPlatformTime::Seconds();
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDeveloperRoomAssets, "BrainMining.RoomDebug.Assets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDeveloperRoomAssets::RunTest(const FString&)
{
    const auto Paths = UMiningExperienceSubsystem::DeveloperMapPaths();
    TestEqual(TEXT("All developer destinations registered"), Paths.Num(), 7);
    for (const auto& Path : Paths) TestTrue(*Path, FPackageName::DoesPackageExist(Path));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDeveloperRoomTravel, "BrainMining.RoomDebug.MainGameTravel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDeveloperRoomTravel::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/MainGame/Maps/Lvl_MainGame"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<MiningRoomDebugTests::FStart>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<MiningRoomDebugTests::FRun>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
