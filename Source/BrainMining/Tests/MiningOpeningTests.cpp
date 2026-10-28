#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "StartRoom/MiningOpening.h"
#include "BrainMiningPlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "MainGameDoor.h"
#include "MiningGamepadSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Camera/PlayerCameraManager.h"
#include "InputKeyEventArgs.h"
#include "MainGameStartMenuWidget.h"
#include "StartRoom/MiningStartRoom.h"
#include "Components/WidgetComponent.h"
#include "Components/DecalComponent.h"
#include "Components/Button.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpeningBootstrapTest,"BrainMining.Opening.Bootstrap",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOpeningBootstrapTest::RunTest(const FString&)
{
    // No binary map rewrite is needed: a transformed source room supplies the anchors.
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("Lvl_MainGame"),CreatePackage(TEXT("/Temp/OpeningBootstrap/Lvl_MainGame")));
    auto* Room=World->SpawnActor<AMiningStartRoom>();
    const FTransform Transform(FRotator(0,45,0),FVector(-10400,300,5200));
    Room->SetActorTransform(Transform);
    auto* Hall=World->SpawnActor<AMiningExhibitHall>();
    Hall->Areas.AddDefaulted();Hall->Areas[0].Tag=TEXT("BM_Campaign_MainGame01");
    TestNull(TEXT("Wait for hall initialization"),AMiningOpening::EnsureMainGameOpening(Hall));
    Hall->bReady=true;
    auto* Opening=AMiningOpening::EnsureMainGameOpening(Hall);
    if(TestNotNull(TEXT("Latest MainGame can bootstrap without an authored opening actor"),Opening))
    {
        TestNotNull(TEXT("Published title asset loads"),Opening->TitleTexture.Get());
        TestTrue(TEXT("Seat follows source-room transform"),Opening->SeatEye.Equals(Transform.TransformPosition(FVector(-390,-160,210)),.01));
        TestTrue(TEXT("Door follows source-room transform"),Opening->DoorTitleLocation.Equals(Transform.TransformPosition(FVector(466,0,168)),.01));
        TestTrue(TEXT("Opening belongs to the active start area"),Opening->ActorHasTag(Hall->Areas[0].Tag));
        Opening->Duration=4.25f;
        TestTrue(TEXT("Existing authored opening is retained without duplication"),AMiningOpening::EnsureMainGameOpening(Hall)==Opening);
        TestEqual(TEXT("Existing customization remains intact"),Opening->Duration,4.25f);
        int32 Count=0;for(TActorIterator<AMiningOpening> It(World);It;++It)++Count;
        TestEqual(TEXT("Only one opening"),Count,1);
    }
    World->DestroyWorld(false);
    UWorld* Other=UWorld::CreateWorld(EWorldType::Game,false,TEXT("Lvl_StartRoom"),CreatePackage(TEXT("/Temp/OpeningBootstrap/Lvl_StartRoom")));
    Other->SpawnActor<AMiningStartRoom>();
    auto* OtherHall=Other->SpawnActor<AMiningExhibitHall>();OtherHall->bReady=true;OtherHall->Areas.AddDefaulted();
    TestNull(TEXT("Standalone StartRoom is unaffected"),AMiningOpening::EnsureMainGameOpening(OtherHall));
    Other->DestroyWorld(false);
    return true;
}

namespace
{
class FPublishedOpeningChecks : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Deadline=FPlatformTime::Seconds()+90;bool Started=false;bool Opened=false;TWeakObjectPtr<AMainGameDoor> Door;
public:
    bool bUseAlternateMenu = false;
    bool bUseGamepad = false;
    explicit FPublishedOpeningChecks(FAutomationTestBase* T, bool bAlternateMenu = false, bool bGamepad = false):Test(T),bUseAlternateMenu(bAlternateMenu),bUseGamepad(bGamepad){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()>Deadline){Test->AddError(TEXT("Opening timed out"));return true;}
        UWorld* W=GEditor?GEditor->PlayWorld:nullptr;if(!W)return false;
        auto* PC=W->GetFirstPlayerController();if(!PC || !PC->GetPawn())return false;
        AMiningOpening* Opening=nullptr;for(TActorIterator<AMiningOpening> It(W);It;++It){Opening=*It;break;}
        if(!Opening)return false;
        if(!Started)
        {
            if(!PC->bShowMouseCursor)return false;
            Test->TestTrue(TEXT("Seated menu locks movement"),PC->IsMoveInputIgnored());
            Test->TestTrue(TEXT("Title uses supplied image"),Opening->TitleTexture!=nullptr);
            Test->TestTrue(TEXT("Camera starts on the bed"),PC->GetViewTarget()!=PC->GetPawn());
            TArray<UUserWidget*> Widgets;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UMiningOpeningMenu::StaticClass(),true);
            if(!Test->TestEqual(TEXT("Exactly one visible opening menu"),Widgets.Num(),1))return true;
            auto* OpeningMenu=Cast<UMiningOpeningMenu>(Widgets[0]);
            UButton* StartButton=OpeningMenu->StartButton;int32 Buttons=0;
            Widgets[0]->WidgetTree->ForEachWidget([&](UWidget* Widget){if(Cast<UButton>(Widget))++Buttons;});
            Test->TestEqual(TEXT("Start, Settings and Quit share the opening menu"),Buttons,OpeningMenu->ContinueButton?4:3);
            if(!Test->TestNotNull(TEXT("Actual Start button exists"),StartButton))return true;
            if(!Test->TestNotNull(TEXT("Actual Settings button exists"),OpeningMenu->SettingsButton.Get()))return true;
            auto* SettingsController=Cast<ABrainMiningPlayerController>(PC);
            if(!Test->TestNotNull(TEXT("Opening uses the gameplay settings controller"),SettingsController))return true;
            const auto* OpeningCamera=PC->GetViewTarget();
            for(int32 SettingsVisit=0;SettingsVisit<2;++SettingsVisit)
            {
                OpeningMenu->SettingsButton->OnClicked.Broadcast();
                Test->TestTrue(TEXT("Main menu opens the shared settings panel"),SettingsController->IsAudioSettingsOpen());
                Test->TestTrue(TEXT("Settings pauses the opening"),PC->IsPaused());
                Test->TestFalse(TEXT("Opening settings does not start the game"),Opening->bStarted);
                if(SettingsVisit==0)
                    PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Escape,IE_Pressed,1.f));
                else
                    SettingsController->ToggleAudioSettings();
                Test->TestFalse(TEXT("Closing settings removes the shared panel"),SettingsController->IsAudioSettingsOpen());
                Test->TestFalse(TEXT("Closing settings releases its pause"),PC->IsPaused());
                Test->TestTrue(TEXT("Closing settings preserves opening movement lock"),PC->IsMoveInputIgnored());
                Test->TestTrue(TEXT("Closing settings preserves opening look lock"),PC->IsLookInputIgnored());
                Test->TestTrue(TEXT("Closing settings preserves opening camera"),PC->GetViewTarget()==OpeningCamera);
                Test->TestTrue(TEXT("Closing settings restores the visible cursor"),PC->bShowMouseCursor);
                Test->TestTrue(TEXT("Closing settings restores focus to Settings"),FSlateApplication::Get().GetUserFocusedWidget(PC->GetLocalPlayer()->GetControllerId())==OpeningMenu->SettingsButton->GetCachedWidget());
            }
            if (bUseAlternateMenu)
            {
                auto* AlternateMenu = CreateWidget<UMainGameStartMenuWidget>(PC);
                AlternateMenu->AddToViewport(600);
                auto* AlternateStart = Cast<UButton>(AlternateMenu->WidgetTree->FindWidget(TEXT("MainGameStartButton")));
                if (!Test->TestNotNull(TEXT("Alternate Start button exists"), AlternateStart)) return true;
                AlternateStart->OnClicked.Broadcast();
                Test->TestTrue(TEXT("Alternate Start launches the existing wall-title animation"), Opening->bStarted);
                Test->TestTrue(TEXT("Alternate menu records successful start"), AlternateMenu->bGameStarted);
                Test->TestFalse(TEXT("Alternate overlay leaves the world title visible"), AlternateMenu->IsInViewport());
            }
            else
                StartButton->OnClicked.Broadcast();
            Started=true;return false;
        }
        if(!Opening->bFinished)return false;
        Test->TestTrue(TEXT("Started exactly once"),Opening->bStarted);
        if(!Opened)
        {
            Test->TestFalse(TEXT("Title stays absent until the pickaxe is collected"),Opening->HasTitleLanded());
            for(TActorIterator<AMainGameDoor> It(W);It;++It){Door=*It;break;}
            if (bUseGamepad && !Test->TestTrue(TEXT("Published map supplies the starting bedroom door"), Door.IsValid())) return true;
            if(!Door.IsValid())
            {
                Door=W->SpawnActor<AMainGameDoor>(FVector(440,0,125),FRotator::ZeroRotator);
                Door->DoorMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
                Door->DoorMesh->SetWorldScale3D(FVector(.2f,2.3f,2.46f));
            }
            const FVector Center=Door->DoorMesh->Bounds.Origin;
            const FVector Eye=Center-FVector(220,0,0);
            PC->GetPawn()->SetActorLocation(Eye-(PC->GetPawn()->GetPawnViewLocation()-PC->GetPawn()->GetActorLocation()));
            PC->SetControlRotation(FRotator::ZeroRotator);PC->PlayerCameraManager->UpdateCamera(.05f);
            Test->TestTrue(TEXT("Authored door is targeted at the actual streamed location"),AMainGameDoor::FindInteractionTarget(PC)==Door.Get());
            if (bUseGamepad)
            {
                auto* Input = PC->GetLocalPlayer()->GetSubsystem<UMiningGamepadSubsystem>();
                if (!Test->TestNotNull(TEXT("Gamepad input subsystem exists"), Input)) return true;
                const auto PressInteract = [&]()
                {
                    Input->HandleButton(EKeys::Gamepad_FaceButton_Left, true);
                    Input->HandleButton(EKeys::Gamepad_FaceButton_Left, false);
                };
                PC->SetControlRotation(FRotator(0, 180, 0));
                PC->PlayerCameraManager->UpdateCamera(.05f);
                PressInteract();
                Test->TestFalse(TEXT("X cannot open the door while looking away"), Door->bIsOpen);
                PC->SetControlRotation(FRotator::ZeroRotator);
                PC->PlayerCameraManager->UpdateCamera(.05f);
                PC->SetIgnoreLookInput(true);
                PressInteract();
                Test->TestFalse(TEXT("X cannot open the door while interaction is locked"), Door->bIsOpen);
                PC->SetIgnoreLookInput(false);
                const FVector NearLocation = PC->GetPawn()->GetActorLocation();
                PC->GetPawn()->SetActorLocation(NearLocation - FVector(1000, 0, 0));
                PC->PlayerCameraManager->UpdateCamera(.05f);
                PressInteract();
                Test->TestFalse(TEXT("X cannot open the door from outside interaction range"), Door->bIsOpen);
                PC->GetPawn()->SetActorLocation(NearLocation);
                PC->PlayerCameraManager->UpdateCamera(.05f);
                PressInteract();
                if (!Test->TestTrue(TEXT("Gamepad X opens the starting bedroom door"), Door->bIsOpen)) return true;
                PressInteract();
                Test->TestTrue(TEXT("Repeated X leaves the door open"), Door->bIsOpen);
            }
            else
            {
                PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1.f));
                if(!Test->TestTrue(TEXT("Real E input opens the door"),Door->bIsOpen))return true;
            }
            Test->TestFalse(TEXT("Opening the door does not reveal the title"),Opening->HasTitleLanded());
            auto* Room=AMiningStartRoom::Find(W);
            if(Room)
            {
                AMiningStarterPickup* Pickaxe=nullptr;
                for(TActorIterator<AMiningStarterPickup> It(W);It;++It)if(It->Equipment==EMiningStarterEquipment::Pickaxe){Pickaxe=*It;break;}
                if(!Test->TestNotNull(TEXT("Starter pickaxe exists"),Pickaxe))return true;
                const FVector Target=Pickaxe->GetActorLocation();
                const FVector PickupEye=Target+FVector(-100,0,70);
                PC->GetPawn()->SetActorLocation(PickupEye-(PC->GetPawn()->GetPawnViewLocation()-PC->GetPawn()->GetActorLocation()));
                PC->SetControlRotation((Target-PickupEye).Rotation());PC->PlayerCameraManager->UpdateCamera(.05f);
                Test->TestTrue(TEXT("Real E interaction collects pickaxe"),Room->Interact(PC));
                Test->TestTrue(TEXT("Pickaxe unlock state changed"),Room->bPickaxeUnlocked);
                Test->TestFalse(TEXT("Picking up the tool alone does not reveal the title"),Opening->HasTitleLanded());
                const FVector WallEye=Opening->DoorTitleLocation-FVector(180,0,0);
                PC->GetPawn()->SetActorLocation(WallEye-(PC->GetPawn()->GetPawnViewLocation()-PC->GetPawn()->GetActorLocation()));
                PC->SetControlRotation((Opening->DoorTitleLocation-WallEye).Rotation());
                PC->PlayerCameraManager->UpdateCamera(.05f);

            }
            else Opening->BeginTitleToWall(nullptr);
            Opened=true;return false;
        }
        if(!Opening->HasTitleLanded())return false;
        Test->TestNotNull(TEXT("Aiming at dirt after pickup projects the title onto the mineable wall"),Opening->FindComponentByClass<UDecalComponent>());
        Test->TestFalse(TEXT("Floating title becomes a wall decal"),Opening->WorldTitle->IsVisible());
        Test->TestTrue(TEXT("Standing pawn returned to player"),PC->GetViewTarget()==PC->GetPawn());
        Test->TestFalse(TEXT("Movement unlocked after standing"),PC->IsMoveInputIgnored());
        Test->TestFalse(TEXT("Look unlocked after standing"),PC->IsLookInputIgnored());
        Test->TestFalse(TEXT("Mouse released to gameplay"),PC->bShowMouseCursor);
        Test->TestTrue(TEXT("Pawn collision restored"),PC->GetPawn()->GetActorEnableCollision());
        Opening->StartGame();Test->TestTrue(TEXT("Repeated start does not replay intro"),Opening->bFinished);
        return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPublishedMainOpeningTest,"BrainMining.Opening.PublishedBedToDoor",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPublishedMainOpeningTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/MainGame/Maps/Lvl_MainGame"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FPublishedOpeningChecks>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamepadStartRoomDoorTest,"BrainMining.Gamepad.StartRoomDoor",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGamepadStartRoomDoorTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/MainGame/Maps/Lvl_MainGame"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FPublishedOpeningChecks>(this, false, true));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpeningMenuTest,"BrainMining.Opening.MenuLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOpeningMenuTest::RunTest(const FString&)
{
    UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
    World->GetWorldSettings()->DefaultGameMode=AMiningStartRoomGameMode::StaticClass();
    auto* Opening=World->SpawnActor<AMiningOpening>();
    Opening->TitleTexture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/StartRoom/UI/T_OpeningTitle.T_OpeningTitle"));
    Opening->SeatEye=FVector(0,0,127);Opening->StandingLocation=FVector(0,90,98);
    Opening->DoorTitleLocation=FVector(466,0,168);
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FPublishedOpeningChecks>(this, true));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
