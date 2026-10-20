#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "DiggableRock.h"
#include "EndingComicRock.h"
#include "MiningComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"

namespace
{
class FEndingExcavationChecks : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    UWorld* World=nullptr;
    ADiggableRock* Membrane=nullptr;
    UMiningComponent* Mining=nullptr;
    TArray<AEndingComicRock*> Tunnel;
    IConsoleVariable* Budget=nullptr;
    float SavedBudget=0;
    double Deadline=FPlatformTime::Seconds()+120;
    int32 Attempts=0;
    bool bMembraneCleared=false;
public:
    explicit FEndingExcavationChecks(FAutomationTestBase* InTest):Test(InTest){}
    ~FEndingExcavationChecks()
    {
        if(Budget)Budget->Set(SavedBudget,ECVF_SetByCode);
        if(World){GEngine->DestroyWorldContext(World);World->DestroyWorld(false);}
    }
    bool Update() override
    {
        if(FPlatformTime::Seconds()>Deadline||Attempts>600){Test->AddError(TEXT("Ending excavation stopped progressing"));return true;}
        if(!World)
        {
            World=UWorld::CreateWorld(EWorldType::Game,false);
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            Membrane=World->SpawnActor<ADiggableRock>(FVector(0,1500,0),FRotator::ZeroRotator);
            Membrane->RockSize=FVector(220,1300,1300);Membrane->bUseCircularRockMesh=true;Membrane->ResetRock();
            auto* Owner=World->SpawnActor<AActor>();
            Mining=NewObject<UMiningComponent>(Owner);Owner->AddInstanceComponent(Mining);Mining->RegisterComponent();Mining->DownwardOffset=0;
            for(int32 Index=0;Index<12;++Index)
            {
                auto* Rock=World->SpawnActor<AEndingComicRock>(FVector(610+Index*620+(Index==11?30:0),0,0),FRotator::ZeroRotator);
                Rock->VisualSeed=Index+137;Rock->bContinuousVolume=true;Rock->RockSize=FVector(Index==11?680:620,700,700);Rock->MaxTriangles=12000;Rock->ResetRock();
                if(!Tunnel.IsEmpty()){Rock->Previous=Tunnel.Last();Tunnel.Last()->Next=Rock;}
                Tunnel.Add(Rock);
            }
            Budget=IConsoleManager::Get().FindConsoleVariable(TEXT("bm.Mining.RockBudgetMs"));
            if(!Test->TestNotNull(TEXT("Deferred terrain budget is available"),Budget))return true;
            SavedBudget=Budget->GetFloat();Budget->Set(0.f,ECVF_SetByCode);
        }
        World->Tick(LEVELTICK_All,.02f);
        if(Membrane->IsTerrainCutPending())return false;
        for(auto* Rock:Tunnel)if(Rock->IsTerrainCutPending())return false;
        const FCollisionQueryParams Query(SCENE_QUERY_STAT(EndingMobileExcavation),true);
        FHitResult Hit;
        if(!bMembraneCleared)
        {
            if(Membrane->ActorLineTraceSingle(Hit,FVector(-750,1500,0),FVector(750,1500,0),ECC_Visibility,Query))
            {
                FHitResult Contact(Membrane,Membrane->RockMesh,Hit.ImpactPoint,Hit.ImpactNormal);
                Contact.bBlockingHit=true;Contact.ImpactPoint=Hit.ImpactPoint;Contact.ImpactNormal=Hit.ImpactNormal;
                Mining->ApplyPickaxeHit(Contact);++Attempts;return false;
            }
            Test->TestTrue(TEXT("The cell membrane has actual committed pickaxe cuts"),Membrane->SuccessfulCuts>0);
            bMembraneCleared=true;Attempts=0;
        }
        for(auto* Rock:Tunnel)
        {
            if(!Rock->ActorLineTraceSingle(Hit,FVector::ZeroVector,FVector(8200,0,0),ECC_Visibility,Query))continue;
            const FVector Direction=-Hit.ImpactNormal.GetSafeNormal();
            const FQuat Rotation=FRotationMatrix::MakeFromXZ(Direction,FVector::UpVector).ToQuat();
            Rock->MinePickaxe(FTransform(Rotation,Hit.ImpactPoint+Direction*174.f*.05f),FVector(174),0,1);
            ++Attempts;return false;
        }
        for(auto* Rock:Tunnel)Test->TestTrue(TEXT("Every finale segment commits cuts and remains within its mobile mesh budget"),Rock->SuccessfulCuts>0&&Rock->TriangleCount<=12000);
        Test->AddInfo(FString::Printf(TEXT("Cell membrane and all twelve finale segments excavated; tunnel strikes: %d"),Attempts));
        return true;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndingMobileExcavationTest,"BrainMining.Mobile.EndingExcavation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEndingMobileExcavationTest::RunTest(const FString&)
{
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FEndingExcavationChecks>(this));
    return true;
}
#endif
