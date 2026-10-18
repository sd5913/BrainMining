#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "MiningComponent.h"
#include "MiningEncounterPlayer.h"
#include "DiggableRock.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"

namespace
{
class FPickaxeShortClickCommand : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    UWorld* World = nullptr;
    AMiningEncounterPlayerController* Controller = nullptr;
    UMiningComponent* Mining = nullptr;
    ADiggableRock* Rock = nullptr;
    AActor* Obstruction = nullptr;
    int32 Phase = 0;
    int32 Strike = 0;
    int32 ReleasedCuts = 0;
    float Elapsed = 0;

    void Key(EInputEvent Event)
    {
        for (const FInputKeyBinding& Binding : Controller->InputComponent->KeyBindings)
            if (Binding.Chord.Key == EKeys::LeftMouseButton && Binding.KeyEvent == Event)
                Binding.KeyDelegate.Execute(EKeys::LeftMouseButton);
    }

    void Next(int32 NewPhase)
    {
        Phase = NewPhase;
        Elapsed = 0;
    }

public:
    explicit FPickaxeShortClickCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    virtual ~FPickaxeShortClickCommand()
    {
        if (World)
        {
            Controller->UnPossess();
            GEngine->DestroyWorldContext(World);
            World->DestroyWorld(false);
        }
    }

    virtual bool Update() override
    {
        if (!World)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false);
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            Rock = World->SpawnActor<ADiggableRock>();
            Rock->RockSize = FVector(1000, 1000, 1000);
            Rock->ResetRock();
            auto* Pawn = World->SpawnActor<ACharacter>(FVector(-750, 0, 0), FRotator::ZeroRotator);
            Mining = NewObject<UMiningComponent>(Pawn);
            Pawn->AddInstanceComponent(Mining);
            Mining->RegisterComponent();
            Mining->Radius = 80;
            Mining->Reach = 1200;
            Mining->DownwardOffset = 0;
            Mining->Interval = .3f;
            Controller = World->SpawnActor<AMiningEncounterPlayerController>();
            Controller->SetAsLocalPlayerController();
            Controller->Possess(Pawn);
            Controller->EncounterPolicy->SetMiningAllowed(true);
            Mining->SetInputContextEnabled(true);
            Controller->SetupInputComponent();
            Controller->SpawnPlayerCameraManager();
            if (!Test->TestNotNull(TEXT("Controller input fixture has a camera"), Controller->PlayerCameraManager.Get())) return true;
            // There is no ULocalPlayer in this isolated world. Otherwise UpdateCamera
            // skips its work and GetPlayerViewPoint uses the pawn's unrotated root.
            Controller->PlayerCameraManager->bUseClientSideCameraUpdates = false;
            Controller->SetControlRotation(FRotator::ZeroRotator);
            Controller->SetViewTarget(Pawn);
            Controller->PlayerCameraManager->UpdateCamera(.05f);
            Mining->TickComponent(0, LEVELTICK_All, nullptr);
            if (!Test->TestTrue(TEXT("Controller input fixture aims at rock"), Mining->TargetRock == Rock)) return true;
        }
        // This synthetic world has no normal player-camera tick; update it
        // before timers resolve a strike after a test changes control rotation.
        Controller->PlayerCameraManager->UpdateCamera(.05f);
        World->Tick(LEVELTICK_All, .05f);
        Mining->TickComponent(.05f, LEVELTICK_All, nullptr);
        Elapsed += .05f;
        switch (Phase)
        {
        case 0:
            Key(IE_Pressed);
            Test->TestEqual(TEXT("Press starts swing before terrain impact"), Rock->SuccessfulCuts, Strike);
            Next(1);
            break;
        case 1:
            Key(IE_Released);
            Test->TestEqual(TEXT("Short click releases before impact"), Rock->SuccessfulCuts, Strike);
            Next(2);
            break;
        case 2:
            if (Elapsed < .8f) break;
            ++Strike;
            Test->TestEqual(TEXT("Short click commits exactly one cut after release"), Rock->SuccessfulCuts, Strike);
            if (Strike < 3)
            {
                Test->TestEqual(TEXT("Separate short clicks advance local damage"), Mining->TargetPickaxeHits, Strike);
                if (Strike == 1) Mining->SetInputContextEnabled(false);
                Next(0);
            }
            else
            {
                Test->TestTrue(TEXT("Third short click completes excavation"), Mining->bLastPickaxeBreak);
                Key(IE_Pressed);
                Mining->SetInputSuspended(true);
                Next(3);
            }
            break;
        case 3:
            if (Elapsed < .8f) break;
            Test->TestEqual(TEXT("UI suspension still cancels a pending swing"), Rock->SuccessfulCuts, 3);
            Mining->SetInputSuspended(false);
            Rock->ResetRock();
            Key(IE_Pressed);
            Next(4);
            break;
        case 4:
            if (Elapsed < .75f) break;
            Test->TestTrue(TEXT("Holding input still repeats swings"), Rock->SuccessfulCuts >= 2);
            ReleasedCuts = Rock->SuccessfulCuts;
            Key(IE_Released);
            Next(5);
            break;
        case 5:
            if (Elapsed < .8f) break;
            Test->TestTrue(TEXT("Release allows at most the already-started swing"), Rock->SuccessfulCuts <= ReleasedCuts + 1);
            ReleasedCuts = Rock->SuccessfulCuts;
            Next(6);
            break;
        case 6:
            if (Elapsed < .8f) break;
            Test->TestEqual(TEXT("Released input never resumes repeated mining"), Rock->SuccessfulCuts, ReleasedCuts);
            Rock->ResetRock();
            Controller->GetPawn()->SetActorLocation(FVector(-750, 499, 0));
            Controller->SetControlRotation(FRotator::ZeroRotator);
            Controller->PlayerCameraManager->UpdateCamera(.05f);
            Mining->TickComponent(0, LEVELTICK_All, nullptr);
            Test->TestTrue(TEXT("Narrow edge is aimed at when the swing starts"), Mining->TargetRock == Rock);
            Key(IE_Pressed); Key(IE_Released);
            Controller->SetControlRotation(FRotator(0, 1, 0));
            Controller->PlayerCameraManager->UpdateCamera(.05f);
            Next(7);
            break;
        case 7:
            if (Elapsed < .8f) break;
            Test->TestEqual(TEXT("Small aim drift during windup retains the verified edge contact"), Rock->SuccessfulCuts, 1);
            Rock->ResetRock();
            Controller->SetControlRotation(FRotator::ZeroRotator);
            Controller->PlayerCameraManager->UpdateCamera(.05f);
            Key(IE_Pressed); Key(IE_Released);
            Controller->SetControlRotation(FRotator(0, 15, 0));
            Controller->PlayerCameraManager->UpdateCamera(.05f);
            Next(8);
            break;
        case 8:
            if (Elapsed < .8f) break;
            {
                FVector Eye; FRotator View;
                Controller->GetPlayerViewPoint(Eye, View);
                Test->TestTrue(TEXT("Fixture actually turns its rendered view away"), FMath::IsNearlyEqual(View.Yaw, 15.0, .01));
            }
            Test->TestEqual(TEXT("Deliberately looking away still misses the original rock"), Rock->SuccessfulCuts, 0);
            Controller->SetControlRotation(FRotator::ZeroRotator);
            Controller->PlayerCameraManager->UpdateCamera(.05f);
            Key(IE_Pressed); Key(IE_Released);
            Obstruction = World->SpawnActor<AActor>();
            {
                auto* Box = NewObject<UBoxComponent>(Obstruction);
                Obstruction->SetRootComponent(Box); Obstruction->AddInstanceComponent(Box);
                Box->SetBoxExtent(FVector(10, 100, 150));
                Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
                Obstruction->SetActorLocation(FVector(-650, 499, 64));
            }
            Controller->SetControlRotation(FRotator(0, 1, 0));
            Next(9);
            break;
        case 9:
            if (Elapsed < .8f) break;
            Test->TestEqual(TEXT("New obstruction blocks both direct and retained contact"), Rock->SuccessfulCuts, 0);
            Obstruction->Destroy();
            Controller->SetControlRotation(FRotator::ZeroRotator);
            Controller->PlayerCameraManager->UpdateCamera(.05f);
            Key(IE_Pressed); Key(IE_Released);
            Controller->GetPawn()->SetActorLocation(FVector(-1900, 499, 0));
            Next(10);
            break;
        case 10:
            if (Elapsed < .8f) break;
            Test->TestEqual(TEXT("Retained contact cannot extend mining reach"), Rock->SuccessfulCuts, 0);
            return true;
        }
        return false;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningPickaxeShortClickTest, "BrainMining.Pickaxe.ControllerShortClick", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningPickaxeShortClickTest::RunTest(const FString&)
{
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FPickaxeShortClickCommand>(this));
    return true;
}
#endif
