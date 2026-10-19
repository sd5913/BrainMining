#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "StartRoom/MiningOpening.h"
#include "PopupClose/PopupOfficeDevice.h"
#include "PopupClose/PopupCloseWidgets.h"
#include "Components/WidgetComponent.h"
#include "Components/DecalComponent.h"
#include "Components/Button.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpeningTitleProjectionTest,"BrainMining.Opening.TitleProjection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOpeningTitleProjectionTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    for(const FRotator Rotation:{FRotator(0,180,0),FRotator(12,47,9)})
    {
        auto* Opening=World->SpawnActor<AMiningOpening>();
        Opening->DoorTitleLocation=FVector(466,200,168);
        Opening->DoorTitleRotation=Rotation;Opening->EnsureTitleOnWall();
        auto* Decal=Opening->FindComponentByClass<UDecalComponent>();
        if(TestNotNull(TEXT("Wall title created"),Decal))
        {
            // Compare all four texture corners with the widget's rendered UV convention.
            for(int U=0;U<=1;++U)for(int V=0;V<=1;++V)
            {
                const FVector WidgetCorner=Rotation.RotateVector(FVector(0,(.5-U)*209.92,(.5-V)*123.));
                const FVector DecalCorner=Decal->GetComponentQuat().RotateVector(FVector(0,(V*2-1)*Decal->DecalSize.Y,(U*2-1)*Decal->DecalSize.Z));
                TestTrue(TEXT("Projected bitmap preserves orientation and aspect"),WidgetCorner.Equals(DecalCorner,.01));
            }
        }
        Opening->Destroy();
    }
    World->DestroyWorld(false);return true;
}

namespace
{
class FOpeningChecks : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Deadline=FPlatformTime::Seconds()+90;bool Started=false;
public:
    explicit FOpeningChecks(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()>Deadline){Test->AddError(TEXT("Opening timed out"));return true;}
        UWorld* W=GEditor?GEditor->PlayWorld:nullptr;if(!W)return false;
        auto* PC=W->GetFirstPlayerController();if(!PC || !PC->GetPawn())return false;
        AMiningOpening* Opening=nullptr;for(TActorIterator<AMiningOpening> It(W);It;++It){Opening=*It;break;}
        if(!Opening)return false;
        if(!Started)
        {
            if(!PC->bShowMouseCursor || !Opening->WorldTitle->IsVisible())return false;
            Test->TestTrue(TEXT("Seated menu locks movement"),PC->IsMoveInputIgnored());
            Test->TestTrue(TEXT("Title uses supplied image"),Opening->TitleTexture!=nullptr);
            Test->TestTrue(TEXT("Camera starts on the bed"),PC->GetViewTarget()!=PC->GetPawn());
            TArray<UUserWidget*> Widgets;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UMiningOpeningMenu::StaticClass(),true);
            if(!Test->TestEqual(TEXT("Exactly one visible opening menu"),Widgets.Num(),1))return true;
            UButton* StartButton=nullptr;int32 Buttons=0;
            Widgets[0]->WidgetTree->ForEachWidget([&](UWidget* Widget){if(auto* B=Cast<UButton>(Widget)){if(!StartButton)StartButton=B;++Buttons;}});
            Test->TestEqual(TEXT("Start and Quit are built and visible"),Buttons,2);
            if(!Test->TestNotNull(TEXT("Actual Start button exists"),StartButton))return true;
            StartButton->OnClicked.Broadcast();Started=true;return false;
        }
        if(!Opening->bFinished)return false;
        Test->TestTrue(TEXT("Started exactly once"),Opening->bStarted);
        Test->TestFalse(TEXT("Title has not flown to the wall before opening the door"),Opening->HasTitleLanded());
        Test->TestNull(TEXT("No wall decal until the player opens the door"),Opening->FindComponentByClass<UDecalComponent>());
        Test->TestTrue(TEXT("Standing pawn returned to player"),PC->GetViewTarget()==PC->GetPawn());
        Test->TestFalse(TEXT("Movement unlocked after standing"),PC->IsMoveInputIgnored());
        Test->TestFalse(TEXT("Look unlocked after standing"),PC->IsLookInputIgnored());
        Test->TestFalse(TEXT("Mouse released to gameplay"),PC->bShowMouseCursor);
        Test->TestTrue(TEXT("Pawn collision restored"),PC->GetPawn()->GetActorEnableCollision());
        Opening->StartGame();Test->TestTrue(TEXT("Repeated start does not replay intro"),Opening->bFinished);
        return true;
    }
};
class FPlacedOfficeChecks : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Deadline=FPlatformTime::Seconds()+30;TWeakObjectPtr<APopupOfficeDevice> Device;double ClickAt=0;
public:
    explicit FPlacedOfficeChecks(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()>Deadline){Test->AddError(TEXT("Office interaction timed out"));return true;}
        UWorld* W=GEditor?GEditor->PlayWorld:nullptr;if(!W || !W->HasBegunPlay())return false;
        if(!Device.IsValid())
        {
            int32 Central=0,Count=0;
            for(TActorIterator<APopupOfficeDevice> It(W);It;++It){++Count;if(It->DeviceIndex==0){++Central;Device=*It;}}
            Test->TestEqual(TEXT("One interactive computer at central desk"),Central,1);
            Test->TestEqual(TEXT("Four authored devices, no duplicate or fallback"),Count,4);
            if(!Device.IsValid())return true;
            Test->TestTrue(TEXT("Central placement preserved on BeginPlay"),Device->GetActorLocation().Equals(FVector(-57.282797,0,0),.1));
            Device->SetScreenInteractive(true);Device->GetSurface()->SetTickWhenOffscreen(true);
            Device->AddProblem();ClickAt=FPlatformTime::Seconds()+1;return false;
        }
        if(FPlatformTime::Seconds()<ClickAt)return false;
        auto* Desktop=Device->GetDesktop();if(!Test->TestNotNull(TEXT("Live desktop exists"),Desktop))return true;
        const int32 Before=Desktop->GetOpenCount();Test->TestTrue(TEXT("Popup created on retained computer"),Before>0);
        Test->TestTrue(TEXT("Retained computer accepts real widget click"),Device->ClickReplyForValidation());
        Test->TestEqual(TEXT("Click closes exactly one popup"),Desktop->GetOpenCount(),Before-1);
        return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainOpeningTest,"BrainMining.Opening.BedToDoor",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMainOpeningTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/MainGame/Maps/Lvl_MainGame"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FOpeningChecks>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacedOfficeTest,"BrainMining.Office.PlacedCentralComputer",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlacedOfficeTest::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/PopupClose/Maps/Lvl_PopupClose1"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FPlacedOfficeChecks>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
