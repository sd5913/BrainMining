#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Showcase/MiningShowcase.h"
#include "MiningCollection.h"
#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "DiggableRock.h"
#include "PopupClose/PopupCloseStage.h"
#include "WenSi/WenSiPuzzle.h"
#include "EchoPassageStage.h"
#include "EchoPassagePlayer.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EchoPassageLayout.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "GameFramework/Character.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "InputKeyEventArgs.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"

namespace MiningContinuousTests
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
            Settings->NewWindowWidth = 1600; Settings->NewWindowHeight = 900;
            Parameters.EditorPlaySettings = Settings;
            GEditor->RequestPlaySession(Parameters);
            return true;
        }
    };
    struct FStop
    {
        FVector Position;
        int32 Region = -2;
        int32 Action = 0;
        FString Label;
    };
    class FWalk : public IAutomationLatentCommand
    {
    public:
        explicit FWalk(FAutomationTestBase* InTest) : Test(InTest)
        {
            Add(-2200,0); Add(-1100,0,88,1); Add(-500,-330,88,1,1,TEXT("02-Collection"));
            Add(-500,-330,88,1,2); Add(-1000,0); Add(-2200,0); Add(-2200,3500);
            Add(-210,3500,88,4,3); Add(-210,3500,88,4,4,TEXT("05-Popup")); Add(-210,3500,88,4,2);
            Add(-2200,3500); Add(-2200,6500); Add(-350,6500,88,5,5,TEXT("06-WenSi")); Add(-350,6500,88,5,2);
            Add(600,6500,88,5); Add(1300,6500); Add(1300,7600); Add(-2200,7600); Add(-2200,9300); Add(-310,9300,88,3,6,TEXT("04-Cards")); Add(-310,9300,88,3,2);
            Add(-2200,9300); Add(-2200,0); Add(-1000,0,88,1,7); Add(-2200,0);
            Add(-2200,-4000); Add(-400,-4000,88,0,8,TEXT("01-Mining"));
            Add(-400,-4700); Add(1500,-4700); Add(1600,-3500); Add(4200,-3500,988); Add(4520,-3500,988,2,0,TEXT("03-EchoEntry"));
            for (const FIntPoint Cell : EchoPassageLayout::MainRoute(true)) Add(5000+Cell.X*400,-3500+Cell.Y*400,88,2);
            Add(11400,100,88,2,9,TEXT("03-EchoExit")); Add(11400,-100); Add(12100,-100); Add(12600,100);
            Add(13000,100); Add(13000,1800); Add(-2200,1800,88,INDEX_NONE,10,TEXT("07-Promenade"));
        }
        bool Update() override
        {
            UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
            if (!World) return FPlatformTime::Seconds()-Started>60;
            auto* Director = First<AMiningShowcaseDirector>(World);
            auto* Controller = Cast<AMiningShowcaseController>(World->GetFirstPlayerController());
            auto* Field = First<AMiningCollectionField>(World);
            if (!Director || !Controller || !Controller->GetPawn() || !Field || Field->Items.Num()!=5) return false;
            if (!Pawn.IsValid())
            {
                Pawn = Controller->GetPawn();
                Test->TestTrue(TEXT("Continuous map enabled"), Director->bContinuousLayout);
                Test->TestEqual(TEXT("Six regions only"), Director->Exhibits.Num(),6);
                Test->TestEqual(TEXT("All content in one persistent level"),World->GetLevels().Num(),1);
                Test->TestEqual(TEXT("No streaming levels"),World->GetStreamingLevels().Num(),0);
                Test->TestFalse(TEXT("No selection directory"),Director->bMenuOpen);
                Test->TestFalse(TEXT("Remote entry is disabled"),Director->EnterExhibit(3));
                Test->TestNotNull(TEXT("Echo already exists"),First<AEchoPassageStage>(World));
                Test->TestNotNull(TEXT("Cards already exist"),First<ACardRoomStage>(World));
                Test->TestNotNull(TEXT("Popup already exists"),First<APopupCloseStage>(World));
                Test->TestNotNull(TEXT("WenSi already exists"),First<AWenSiPuzzle>(World));
                Started = FPlatformTime::Seconds();
                Screenshot(TEXT("00-Arrival"));
            }
            if (Index >= Stops.Num())
            {
                if (!Overview.IsValid())
                {
                    Overview=World->SpawnActor<ACameraActor>(FVector(5350,2400,16000),FRotator(-90,0,0));
                    Overview->GetCameraComponent()->SetFieldOfView(100);
                    Director->bGuideVisible=false;
                    Controller->SetViewTarget(Overview.Get());
                    Started=FPlatformTime::Seconds();
                    return false;
                }
                if (FPlatformTime::Seconds()-Started>1 && !bOverviewCaptured) { Screenshot(TEXT("99-WholeMap")); bOverviewCaptured=true; }
                return FPlatformTime::Seconds()-Started>2;
            }
            const FStop& Stop = Stops[Index];
            if (FPlatformTime::Seconds()-Started>45)
            {
                Test->AddError(FString::Printf(TEXT("Walk blocked at waypoint %d (%s); pawn=%s target=%s"), Index,*Stop.Label,*Pawn->GetActorLocation().ToString(),*Stop.Position.ToString()));
                Screenshot(TEXT("FAIL-Walk"));
                return true;
            }
            if (!bArrived)
            {
                FVector Direction = Stop.Position-Pawn->GetActorLocation(); Direction.Z=0;
                if (Stop.Action!=2 && (Direction.Size2D()>38 || FMath::Abs(Pawn->GetActorLocation().Z-Stop.Position.Z)>85))
                {
                    Controller->SetControlRotation(Direction.Rotation());
                    Pawn->AddMovementInput(Direction.GetSafeNormal());
                    return false;
                }
                if (auto* Character=Cast<ACharacter>(Pawn.Get())) Character->GetCharacterMovement()->StopMovementImmediately();
                bArrived=true; ArrivedAt=FPlatformTime::Seconds();
                if (Stop.Action==1)
                    for (AMiningCollectible* Item:Field->Items) if(Item->bReleased && Item->ItemId==2) Aim(Controller,Item->GetActorLocation());
                if (Stop.Action==3) Aim(Controller,FVector(42,3500,124));
            }
            if (FPlatformTime::Seconds()-ArrivedAt<.7) return false;
            Test->TestTrue(TEXT("Pawn identity persists while walking"),Controller->GetPawn()==Pawn.Get());
            Test->TestEqual(TEXT("Still zero streamed levels"),World->GetStreamingLevels().Num(),0);
            if (Stop.Region!=-2) Test->TestEqual(FString::Printf(TEXT("Waypoint %d region"),Index),Director->ActiveIndex,Stop.Region);
            Act(Stop.Action,World,Director,Controller,Field);
            if (!Stop.Label.IsEmpty()) Screenshot(Stop.Label);
            UE_LOG(LogTemp,Display,TEXT("CONTINUOUS_WALK %d %s %s"),Index,*Stop.Label,*Pawn->GetActorLocation().ToString());
            ++Index; bArrived=false; Started=FPlatformTime::Seconds();
            return false;
        }
    private:
        FAutomationTestBase* Test;
        TWeakObjectPtr<APawn> Pawn;
        TWeakObjectPtr<ACameraActor> Overview;
        bool bOverviewCaptured=false;
        TArray<FStop> Stops;
        int32 Index=0;
        bool bArrived=false;
        double Started=FPlatformTime::Seconds(),ArrivedAt=0;
        int32 CollectionCuts=0;
        void Add(float Horizontal,float Vertical,float Height=88,int32 Region=-2,int32 Action=0,FString Label=FString()) { Stops.Add({FVector(Horizontal,Vertical,Height),Region,Action,Label}); }
        void Aim(APlayerController* Controller,FVector Target)
        {
            FVector Eye; FRotator Rotation; Controller->GetPlayerViewPoint(Eye,Rotation);
            Controller->SetControlRotation((Target-Eye).Rotation());
        }
        void Screenshot(const FString& Name) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("ContinuousPreview")/(Name+TEXT(".png")),true,false); }
        void Shortcut(FKey Key) { FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(Key,FModifierKeysState(),0,false,0,0)); }
        void Act(int32 Action,UWorld* World,AMiningShowcaseDirector* Director,AMiningShowcaseController* Controller,AMiningCollectionField* Field);
    };
    void FWalk::Act(int32 Action,UWorld* World,AMiningShowcaseDirector* Director,AMiningShowcaseController* Controller,AMiningCollectionField* Field)
    {
        if(Director->EchoInteraction) Director->EchoInteraction->Knock();
        if(Action==1)
        {
            Test->TestNotNull(TEXT("Tutorial collectible aimed at"),Field->Target.Get());
            Field->CollectTarget();
            Test->TestEqual(TEXT("Collected tutorial item"),Field->UniqueCount(),1);
            ADiggableRock* Host=Field->Items[0]->HostRock;
            if(Host)
            {
                Test->TestTrue(TEXT("Collection rock can be excavated"),Host->MineSphere(Host->GetActorTransform().TransformPosition(FVector(-Host->RockSize.X*.5f+30,0,0)),100));
                CollectionCuts=Host->SuccessfulCuts;
            }
            Field->ToggleCatalogue();
            Test->TestTrue(TEXT("Catalogue pauses continuous world"),UGameplayStatics::IsGamePaused(World));
        }
        if(Action==2)
        {
            const FVector Before=Pawn->GetActorLocation();
            Shortcut(EKeys::F8);
            Test->TestFalse(TEXT("F8 releases pause"),UGameplayStatics::IsGamePaused(World));
            Test->TestFalse(TEXT("F8 releases look"),Controller->IsLookInputIgnored());
            Test->TestFalse(TEXT("F8 releases movement"),Controller->IsMoveInputIgnored());
            Test->TestTrue(TEXT("F8 does not teleport to lobby"),FVector::Dist2D(Before,Pawn->GetActorLocation())<100);
        }
        if(Action==3) Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1));
        if(Action==4)
        {
            Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0));
            Test->TestTrue(TEXT("Popup reached from promenade"),First<APopupCloseStage>(World)->IsSessionActive());
            Test->TestTrue(TEXT("Popup locks look only here"),Controller->IsLookInputIgnored());
        }
        if(Action==5)
        {
            auto* Puzzle=First<AWenSiPuzzle>(World);
            Test->TestFalse(TEXT("Translated puzzle not auto-completed"),Puzzle->bComplete);
            AWenSiObject* ByKind[3]={};
            for(AWenSiObject* Object:Puzzle->Objects) if(Object->ObjectKind>=0 && Object->ObjectKind<3) ByKind[Object->ObjectKind]=Object;
            if(!ByKind[0] || !ByKind[1] || !ByKind[2]) {Test->AddError(TEXT("Missing puzzle objects"));return;}
            Puzzle->Thoughts[0]=ByKind[1]; Puzzle->Thoughts[1]=ByKind[2]; Puzzle->ToggleEdit(); Puzzle->SwapRules(0,1);
            Puzzle->Thoughts[0]=ByKind[0]; Puzzle->SwapRules(0,1);
            Test->TestTrue(TEXT("Translated puzzle editing works"),Puzzle->bEditing);
        }
        if(Action==6)
        {
            Test->TestTrue(TEXT("WenSi exit completed by walking"),First<AWenSiPuzzle>(World)->bComplete);
            Test->TestNotNull(TEXT("Cards input only active in cards region"),Director->CardInteraction.Get());
            if(Director->CardInteraction) Director->CardInteraction->Interact();
            Test->TestTrue(TEXT("Walking to card chair permits seating"),Controller->IsMoveInputIgnored());
        }
        if(Action==7) Test->TestEqual(TEXT("Inventory survives visits to other areas"),Field->UniqueCount(),1);
        if(Action==8)
        {
            const FVector Before=Pawn->GetActorLocation();
            Shortcut(EKeys::F5);
            Test->TestTrue(TEXT("Mining reset stays in place"),Before.Equals(Pawn->GetActorLocation(),1));
            Test->TestEqual(TEXT("Mining reset preserves collected item"),Field->UniqueCount(),1);
            Test->TestEqual(TEXT("Mining reset preserves collection geometry"),Field->Items.Num(),5);
            if(Field->Items[0]->HostRock) Test->TestEqual(TEXT("Mining reset does not repair another area's rock"),Field->Items[0]->HostRock->SuccessfulCuts,CollectionCuts);
        }
        if(Action==9)
        {
            Test->TestTrue(TEXT("Maze completed entirely on foot"),First<AEchoPassageStage>(World)->bCompleted);
            Test->TestTrue(TEXT("Echo strikes work at translated coordinates"),First<AEchoPassageStage>(World)->KnockCount>0);
            Test->TestTrue(TEXT("No automatic map destination"),First<AEchoPassageStage>(World)->DestinationLevel.IsNull());
        }
        if(Action==10)
        {
            Test->TestEqual(TEXT("All six regions visited on foot"),Director->Visited.Num(),6);
            Test->TestNull(TEXT("Echo input removed outside maze"),Director->EchoInteraction.Get());
            Test->TestNull(TEXT("Card input removed outside room"),Director->CardInteraction.Get());
            Test->TestFalse(TEXT("Collection input removed outside field"),Field->IsAreaActive());
            Test->TestEqual(TEXT("Inventory persists after full loop"),Field->UniqueCount(),1);
        }
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContinuousWalkTest,"BrainMining.Showcase.Continuous.WalkAllSixRegions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FContinuousWalkTest::RunTest(const FString& Parameters)
{
    AutomationOpenMap(TEXT("/Game/Showcase/Maps/Lvl_ContinuousShowcase"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<MiningContinuousTests::FStart>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<MiningContinuousTests::FWalk>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
namespace MiningContinuousTests
{
    class FAppearance : public IAutomationLatentCommand
    {
    public:
        explicit FAppearance(FAutomationTestBase* InTest) : Test(InTest) {}
        bool Update() override
        {
            if(FPlatformTime::Seconds()-Started>30) {Test->AddError(TEXT("Appearance test timed out"));return true;}
            UWorld* World=GEditor?GEditor->PlayWorld:nullptr;
            if(!World) return false;
            auto* Director=First<AMiningShowcaseDirector>(World);
            auto* Controller=Cast<AMiningShowcaseController>(World->GetFirstPlayerController());
            if(!Director || !Controller || !Controller->GetPawn()) return false;
            auto* Camera=Controller->GetPawn()->FindComponentByClass<UCameraComponent>();
            if(!Camera || FPlatformTime::Seconds()-Changed<1.5) return false;
            if(Step==0)
            {
                Controller->GetPawn()->SetActorLocation(FVector(-310,9300,100));
                Controller->SetControlRotation(FRotator(-9,0,0));
            }
            else if(Step==1)
            {
                Test->TestEqual(TEXT("Card region selected"),Director->GetActiveKind(),EMiningShowcaseKind::Cards);
                Test->TestEqual(TEXT("Original card exposure restored locally"),Camera->PostProcessSettings.AutoExposureBias,-6.f);
                if(Director->CardInteraction) Director->CardInteraction->Interact();
            }
            else if(Step==2) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("ContinuousPreview/04-Cards.png"),true,false);
            else if(Step==3)
            {
                Director->CloseContinuousInteraction();
                Controller->GetPawn()->SetActorLocation(FVector(-2200,9000,100));
            }
            else if(Step==4)
            {
                Test->TestEqual(TEXT("Card exposure released outside room"),Camera->PostProcessSettings.AutoExposureBias,0.f);
                Test->TestEqual(TEXT("Appearance check retains one level"),World->GetLevels().Num(),1);
                return true;
            }
            ++Step; Changed=FPlatformTime::Seconds();
            return false;
        }
    private:
        FAutomationTestBase* Test;
        int32 Step=0;
        double Started=FPlatformTime::Seconds(),Changed=FPlatformTime::Seconds();
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContinuousAppearanceTest,"BrainMining.Showcase.Continuous.Appearance",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FContinuousAppearanceTest::RunTest(const FString& Parameters)
{
    AutomationOpenMap(TEXT("/Game/Showcase/Maps/Lvl_ContinuousShowcase"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<MiningContinuousTests::FStart>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<MiningContinuousTests::FAppearance>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
