#include "MiningPickaxeSettings.h"
#include "MiningComponent.h"
#include "DiggableRock.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningHardnessCurveTest,"BrainMining.Pickaxe.HardnessCurveAndGrowth",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningHardnessCurveTest::RunTest(const FString&)
{
    const float Expected[]={0,.25f,.5f,.75f,1,1.5f,2,2.5f,3};
    for(int32 D=0;D<=4;++D)for(int32 H=0;H<=4;++H)
        TestEqual(*FString::Printf(TEXT("Power %d minus hardness %d"),D,H),UMiningPickaxeSettings::DepthFactor(D,H),Expected[D-H+4]);
    auto* Settings=NewObject<UMiningPickaxeSettings>();Settings->Levels[0].Width=200;Settings->Levels[0].Depth=120;Settings->Levels[0].Interval=.4f;
    for(int32 L=0;L<=4;++L)
    {
        const auto P=Settings->GetLevel(L);
        TestEqual(TEXT("Power matches purchased level"),P.DestructionLevel,L);
        TestEqual(TEXT("Range bonus starts at 2 and never compounds"),P.Width,L>=2?250.f:200.f);
        TestTrue(TEXT("Interval bonus starts at 3 and never compounds"),FMath::IsNearlyEqual(P.Interval,L>=3?.3f:.4f));
        TestEqual(TEXT("Shared base depth reaches every level"),P.Depth,120.f);
        TestEqual(TEXT("Each swing independently cuts terrain"),P.Stages.Num(),1);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningHardnessGeometryTest,"BrainMining.Pickaxe.HardnessActualCutDepthAndBlock",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningHardnessGeometryTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);ON_SCOPE_EXIT{World->DestroyWorld(false);};
    auto* Rock=World->SpawnActor<ADiggableRock>();Rock->RockSize=FVector(3000,1600,1600);
    auto* Owner=World->SpawnActor<AActor>();auto* Mining=NewObject<UMiningComponent>(Owner);Owner->AddInstanceComponent(Mining);Mining->RegisterComponent();Mining->DownwardOffset=0;
    auto P=UMiningPickaxeSettings::MakeRecommendedLevels()[0];P.Depth=100;P.Width=200;P.Roughness=0;
    const auto Surface=[&]()
    {
        FHitResult SurfaceHit;
        if(!Rock->ActorLineTraceSingle(SurfaceHit,FVector(-1800,0,0),FVector(1800,0,0),ECC_Visibility,FCollisionQueryParams(NAME_None,true)))return FHitResult();
        FHitResult Hit(Rock,Rock->RockMesh,SurfaceHit.ImpactPoint,SurfaceHit.ImpactNormal);
        Hit.bBlockingHit=true;Hit.ImpactPoint=SurfaceHit.ImpactPoint;Hit.ImpactNormal=SurfaceHit.ImpactNormal;
        return Hit;
    };
    for(int32 Delta=-4;Delta<=4;++Delta)
    {
        Rock->ResetRock();P.DestructionLevel=FMath::Max(0,Delta);Rock->HardnessLevel=FMath::Max(0,-Delta);Mining->ApplyPickaxeBalance(P);
        const auto Before=Surface();const int32 Count=Rock->TriangleCount;const bool Accepted=Mining->ApplyPickaxeHit(Before);const auto After=Surface();
        if(Delta==-4)
        {TestFalse(TEXT("Underpowered hit is rejected"),Accepted);TestEqual(TEXT("Blocked hit never edits terrain"),Rock->TriangleCount,Count);TestEqual(TEXT("Blocked hit never commits a cut"),Rock->SuccessfulCuts,0);}
        else
        {TestTrue(TEXT("Allowed hit edits actual terrain"),Accepted);TestTrue(*FString::Printf(TEXT("Actual depth at difference %d"),Delta),FMath::IsNearlyEqual(After.ImpactPoint.X-Before.ImpactPoint.X,100*UMiningPickaxeSettings::DepthFactor(P.DestructionLevel,Rock->HardnessLevel),1.));}
        TestTrue(TEXT("All resulting terrain remains watertight"),Rock->RockMesh->GetDynamicMesh()->GetMeshRef().IsClosed());
    }
    Rock->ResetRock();Rock->HardnessLevel=4;P.DestructionLevel=0;Mining->ApplyPickaxeBalance(P);
    TestTrue(TEXT("Rock hardness does not block direct drill/bomb terrain API"),Rock->MineSphere(FVector(-1450,0,0),80));
    return true;
}
#endif
