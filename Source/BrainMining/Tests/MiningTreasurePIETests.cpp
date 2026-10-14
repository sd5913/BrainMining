#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Showcase/MiningShowcase.h"
#include "Showcase/MiningTreasure.h"
#include "MiningCollection.h"
#include "MiningComponent.h"
#include "DiggableRock.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/TextRenderActor.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"

namespace MiningTreasureTests
{
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
    class FWalkAndCollect : public IAutomationLatentCommand
    {
    public:
        explicit FWalkAndCollect(FAutomationTestBase* InTest) : Test(InTest) {}
        bool Update() override
        {
            if (FPlatformTime::Seconds() - Started > 180)
            {
                Test->AddError(FString::Printf(TEXT("Treasure walkthrough timed out at step %d"), Step));
                return true;
            }
            UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
            if (!World) return false;
            auto* Controller = Cast<AMiningShowcaseController>(World->GetFirstPlayerController());
            auto* Director = Controller ? Controller->GetDirector() : nullptr;
            auto* Field = Director ? Director->TreasureField.Get() : nullptr;
            auto* Character = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
            if (!Field || !Character) return false;
            if (Field->Items.Num() != 18) { Test->AddError(Field->GenerationStatus); return true; }
            auto* Mining = Character->FindComponentByClass<UMiningComponent>();
            if (!Mining) { Test->AddError(TEXT("Missing mining component")); return true; }
            if (!Pawn.IsValid()) Pawn = Character;
            auto Walk = [&](FVector Destination)
            {
                FVector Direction = Destination - Character->GetActorLocation();
                Direction.Z = 0;
                if (Direction.Size2D() > 24)
                {
                    Controller->SetControlRotation(Direction.Rotation());
                    Character->AddMovementInput(Direction.GetSafeNormal());
                    return false;
                }
                Character->GetCharacterMovement()->StopMovementImmediately();
                return true;
            };
            const FVector Stops[] = {FVector(-2200,-4000,88), FVector(-700,-4000,88), FVector(-700,-5250,88), FVector(-1040,-5580,88)};
            if (Step < 4)
            {
                if (Walk(Stops[Step])) { ++Step; Changed = FPlatformTime::Seconds(); }
                return false;
            }
            auto* Item = Field->Items[0].Get();
            FVector Eye;
            FRotator Rotation;
            Controller->GetPlayerViewPoint(Eye, Rotation);
            if (Step < 8) Controller->SetControlRotation((Item->GetActorLocation() - Eye).Rotation());
            if (FPlatformTime::Seconds() - Changed < .7) return false;
            if (Step == 4)
            {
                Test->TestEqual(TEXT("Walk enters annex region"), Director->GetActiveKind(), EMiningShowcaseKind::Treasure);
                Test->TestTrue(TEXT("Mining enabled only in annex"), !Mining->IsEncounterBlocked());
                Test->TestFalse(TEXT("Buried item cannot be claimed"), Field->CollectTarget(Controller));
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("TreasurePreview/03-BuriedGlow.png"), true, false);
            }
            else if (Step == 5)
            {
                Test->TestTrue(TEXT("Use real player digging path"), Mining->TryMine());
                if (++PickaxeStrikes == 3) ++Step;
                Changed = FPlatformTime::Seconds();
                return false;
            }
            else if (Step == 6)
            {
                Test->TestTrue(TEXT("Player cut passes exposure threshold"), Item->MeasureExposure() > .5f);
                Test->TestTrue(TEXT("Aim target selected"), Field->Target == Item);
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("TreasurePreview/01-Exposed.png"), true, false);
                ExpectedCoins = Item->Value;
            }
            else if (Step == 7) Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E, IE_Pressed, 1));
            else if (Step == 8)
            {
                Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E, IE_Released, 0));
                Test->TestTrue(TEXT("E collects and hides treasure"), Item->bCollected && Item->IsHidden());
                Test->TestEqual(TEXT("Exactly one reward"), Field->Coins, ExpectedCoins);
                Test->TestFalse(TEXT("Cannot claim twice"), Field->CollectTarget(Controller));
                Mining->ResetField();
                Test->TestEqual(TEXT("R does not duplicate currency"), Field->Coins, ExpectedCoins);
                Test->TestTrue(TEXT("R does not resurrect claimed item"), Item->bCollected);
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("TreasurePreview/02-Collected.png"), true, false);
            }
            else if (Step == 9) { if (!Walk(FVector(-700,-5250,88))) return false; }
            else if (Step == 10) { if (!Walk(FVector(-700,-4000,88))) return false; }
            else if (Step == 11)
            {
                Test->TestFalse(TEXT("Leaving annex disables its interaction"), Field->IsAreaActive());
                Test->TestEqual(TEXT("Coins persist in other region"), Field->Coins, ExpectedCoins);
                Test->TestFalse(TEXT("Inactive region cannot collect"), Field->CollectTarget(Controller));
                Director->RestartCurrent();
                Test->TestEqual(TEXT("Restarting mining does not reset treasure coins"), Field->Coins, ExpectedCoins);
            }
            else if (Step == 12) { if (!Walk(FVector(-700,-5250,88))) return false; }
            else if (Step == 13)
            {
                Test->TestTrue(TEXT("Returning preserves claimed item"), Field->Items[0]->bCollected);
                Director->RestartCurrent();
                Test->TestEqual(TEXT("F5 locally resets treasure coins"), Field->Coins, int64(0));
                Test->TestFalse(TEXT("F5 replaces collected treasure"), Field->Items[0]->bCollected);
                Test->TestTrue(TEXT("Same pawn throughout real walking"), Pawn.Get() == Character);
                Test->TestEqual(TEXT("One persistent level"), World->GetLevels().Num(), 1);
                Test->TestEqual(TEXT("No streamed levels"), World->GetStreamingLevels().Num(), 0);
                auto* Overview = World->SpawnActor<ACameraActor>(FVector(-2200,-4300,2300), (FVector(400,-6300,80) - FVector(-2200,-4300,2300)).Rotation());
                Controller->SetViewTarget(Overview);
            }
            else if (Step == 14)
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("TreasurePreview/00-Annex.png"), true, false);
            else if (Step == 15)
            {
                for (ADiggableRock* Rock : Field->Rocks) Rock->SetActorHiddenInGame(true);
                for (AMiningTreasure* Treasure : Field->Items) Treasure->SetActorHiddenInGame(true);
                const TCHAR* Names[] = {TEXT("COAL"),TEXT("COPPER"),TEXT("TIN"),TEXT("IRON"),TEXT("SULFUR"),TEXT("MALACHITE"),TEXT("LAPIS"),TEXT("AMETHYST"),TEXT("DIAMOND")};
                for (int32 Index = 0; Index < 9; ++Index)
                {
                    auto Definition = Field->Definitions[Index];
                    Definition.Scale = FVector(1.8);
                    const FVector Position(400+(Index%3-1)*300,-6200-(Index/3)*300,120);
                    auto* Preview = World->SpawnActor<AMiningTreasure>(Position, FRotator(0,20,0));
                    Preview->Configure(Definition,0,Field->TreasureMaterial);
                    auto* Label = World->SpawnActor<ATextRenderActor>(Position+FVector(0,100,-80),FRotator(0,90,0));
                    Label->GetTextRender()->SetText(FText::FromString(Names[Index]));
                    Label->GetTextRender()->SetHorizontalAlignment(EHTA_Center);
                    Label->GetTextRender()->SetWorldSize(20);
                }
                auto* Gallery = World->SpawnActor<ACameraActor>(FVector(400,-5500,1150),(FVector(400,-6500,40)-FVector(400,-5500,1150)).Rotation());
                Gallery->GetCameraComponent()->SetFieldOfView(55);
                Controller->SetViewTarget(Gallery);
                Director->bGuideVisible = false;
                Field->SetAreaActive(false);
            }
            else if (Step == 16)
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("TreasurePreview/04-MineralGallery.png"), true, false);
            else if (Step == 17)
            {
                return true;
            }
            ++Step;
            Changed = FPlatformTime::Seconds();
            return false;
        }
    private:
        FAutomationTestBase* Test;
        TWeakObjectPtr<APawn> Pawn;
        int32 Step = 0;
        int32 PickaxeStrikes = 0;
        int64 ExpectedCoins = 0;
        double Started = FPlatformTime::Seconds();
        double Changed = FPlatformTime::Seconds();
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTreasureWalkTest, "BrainMining.Treasure.ContinuousWalkAndCollect", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTreasureWalkTest::RunTest(const FString& Parameters)
{
    AutomationOpenMap(TEXT("/Game/Showcase/Maps/Lvl_ContinuousShowcase"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<MiningTreasureTests::FStart>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<MiningTreasureTests::FWalkAndCollect>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
