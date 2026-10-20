#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "DiggableRock.h"
#include "MiningComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"

namespace
{
class FDeferredRockChecks : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    UWorld* World = nullptr;
    ADiggableRock* Rock = nullptr;
    UMiningComponent* Mining = nullptr;
    IConsoleVariable* Budget = nullptr;
    float OriginalBudget = 0;
    double Deadline = FPlatformTime::Seconds() + 30;
    int32 Phase = 0;

    bool Strike()
    {
        FHitResult Hit;
        if (!Rock->ActorLineTraceSingle(Hit, FVector(-750, 0, 0), FVector(750, 0, 0), ECC_Visibility,
            FCollisionQueryParams(SCENE_QUERY_STAT(DeferredRockTest), true))) return false;
        FHitResult Contact(Rock, Rock->RockMesh, Hit.ImpactPoint, Hit.ImpactNormal);
        Contact.bBlockingHit = true;
        Contact.ImpactPoint = Hit.ImpactPoint;
        Contact.ImpactNormal = Hit.ImpactNormal;
        return Mining->ApplyPickaxeHit(Contact);
    }

public:
    explicit FDeferredRockChecks(FAutomationTestBase* InTest) : Test(InTest) {}
    ~FDeferredRockChecks()
    {
        if (Budget) Budget->Set(OriginalBudget, ECVF_SetByCode);
        if (World) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
    }

    bool Update() override
    {
        if (FPlatformTime::Seconds() > Deadline) { Test->AddError(TEXT("Deferred rock cut timed out")); return true; }
        if (!World)
        {
            World = UWorld::CreateWorld(EWorldType::Game, false);
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            Rock = World->SpawnActor<ADiggableRock>();
            Rock->RockSize = FVector(1000);
            Rock->ResetRock();
            auto* Owner = World->SpawnActor<AActor>();
            Mining = NewObject<UMiningComponent>(Owner);
            Owner->AddInstanceComponent(Mining);
            Mining->RegisterComponent();
            Mining->DownwardOffset = 0;
            Budget = IConsoleManager::Get().FindConsoleVariable(TEXT("bm.Mining.RockBudgetMs"));
            if (!Test->TestNotNull(TEXT("Runtime boolean budget exists"), Budget)) return true;
            OriginalBudget = Budget->GetFloat();
            Budget->Set(0.f, ECVF_SetByCode);
            Test->TestTrue(TEXT("A cut exceeding the frame budget is accepted asynchronously"), Strike());
            Test->TestTrue(TEXT("Accepted terrain work remains pending"), Rock->IsTerrainCutPending());
            Test->TestFalse(TEXT("Repeated strikes cannot duplicate pending work"), Strike());
            Test->TestEqual(TEXT("Background work cannot immediately change terrain"), Rock->SuccessfulCuts, 0);
            Phase = 1;
        }
        World->Tick(LEVELTICK_All, .02f);
        if (Rock->IsTerrainCutPending()) return false;
        if (Phase <= 3)
        {
            Test->TestEqual(TEXT("Each accepted stage commits once"), Rock->SuccessfulCuts, Phase);
            if (Phase < 3)
            {
                Test->TestTrue(TEXT("Same location remains mineable after deferred contact"), Strike());
                ++Phase;
                return false;
            }
            Test->TestTrue(TEXT("Final deferred strike completes the pickaxe patch"), Mining->bLastPickaxeBreak);
            Test->TestTrue(TEXT("Another cut can begin after completion"), Strike());
            Rock->ResetRock();
            Phase = 4;
            return false;
        }
        if (Phase == 4)
        {
            Test->TestEqual(TEXT("Reset discards stale worker results"), Rock->SuccessfulCuts, 0);
            if (!Strike()) return false;
            Phase = 5;
            return false;
        }
        Test->TestEqual(TEXT("Reset rock can be excavated again"), Rock->SuccessfulCuts, 1);
        return true;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDeferredRockTest, "BrainMining.Pickaxe.DeferredOrdinaryRock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDeferredRockTest::RunTest(const FString&)
{
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDeferredRockChecks>(this));
    return true;
}
#endif
