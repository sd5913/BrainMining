#include "Showcase/MiningTreasure.h"
#include "DiggableRock.h"
#include "GeneratedMiningRock.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "UDynamicMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningCoverageCacheTest,"BrainMining.Treasure.CoverageCacheInvalidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningCoverageCacheTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    ON_SCOPE_EXIT{World->DestroyWorld(false);};
    auto* Rock=World->SpawnActor<ADiggableRock>();Rock->RockSize=FVector(1000);Rock->ResetRock();
    TArray<TObjectPtr<ADiggableRock>> Rocks={Rock};
    TArray<FVector> Points={FVector(470,0,0),FVector(-470,0,0),FVector(700,0,0)};TArray<bool> Covered;
    auto Query=[&](){AMiningTreasure::MarkCoveredPoints(Rocks,Points,Covered);};
    Query();TestTrue(TEXT("Cold cache samples solid and air correctly"),Covered[0]&&Covered[1]&&!Covered[2]);
    Query();TestTrue(TEXT("Warm cache agrees"),Covered[0]&&Covered[1]&&!Covered[2]);
    TestTrue(TEXT("Cut succeeds"),Rock->MineSphere(FVector(500,0,0),150));Query();
    TestTrue(TEXT("Mesh replacement invalidates only the removed support"),!Covered[0]&&Covered[1]&&!Covered[2]);
    Rock->ResetRock();Query();TestTrue(TEXT("Reset restores covered samples"),Covered[0]&&Covered[1]);
    Rock->MineSphere(FVector(-500,0,0),150);Query();
    TestTrue(TEXT("Same cut count after reset cannot reuse stale coverage"),Covered[0]&&!Covered[1]);
    auto* Overlap=World->SpawnActor<ADiggableRock>();Overlap->RockSize=FVector(1000);Overlap->ResetRock();Rocks.Add(Overlap);Query();
    TestTrue(TEXT("Uncut overlapping rock still supports fragments"),Covered[0]&&Covered[1]);
    Overlap->Destroy();Query();TestFalse(TEXT("Destroyed support is excluded"),Covered[1]);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningClueCommitTest,"BrainMining.Treasure.CluesCommitWithTerrain",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningClueCommitTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    ON_SCOPE_EXIT{World->DestroyWorld(false);};
    auto* FragmentMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    for(bool bChunks:{false,true})
    {
        auto* Rock=World->SpawnActor<AGeneratedMiningRock>();Rock->RockSize=FVector(1000);Rock->ResetRock();
        if(bChunks)
        {
            UE::Geometry::FDynamicMesh3 Geometry;
            Rock->RockMesh->GetDynamicMesh()->ProcessMesh([&](const auto& Mesh){Geometry=Mesh;});
            TestTrue(TEXT("Build real chunked rock"),Rock->SetChunkedMesh(Geometry,500,true,true));
        }
        // Simulate a serialized map carrying the old RT setting.
        Rock->RockMesh->SetEnableRaytracing(true);
        for(auto& Chunk:Rock->Chunks)Chunk.Mesh->SetEnableRaytracing(true);
        Rock->ResetRock();
        TestFalse(TEXT("Rock reset disables unsafe dynamic RT geometry"),Rock->RockMesh->GetEnableRaytracing());
        for(const auto& Chunk:Rock->Chunks)TestFalse(TEXT("Loaded chunks also disable dynamic RT geometry"),Chunk.Mesh->GetEnableRaytracing());
        auto* Item=World->SpawnActor<AMiningTreasure>();Item->CoveringRocks={Rock};
        Item->Model->SetMaterial(0,UMaterial::GetDefaultMaterial(MD_Surface));
        FMiningTreasurePlacement Placement;
        Placement.FragmentSupports={FVector(470,50,50),FVector(-470,50,50),FVector(50,470,50)};
        for(const FVector& Point:Placement.FragmentSupports)Placement.Fragments.Add(FTransform(FQuat::Identity,Point,FVector(.1)));
        Item->ConfigureCrystalClues(Placement,FragmentMesh);
        auto AssertSurvivors=[&](const TCHAR* Context)
        {
            TArray<bool> Covered;AMiningTreasure::MarkCoveredPoints(Item->CoveringRocks,Placement.FragmentSupports,Covered);
            int32 Expected=0;for(bool Solid:Covered)Expected+=Solid;
            TestEqual(Context,Item->CrystalClues->GetInstanceCount(),Expected);
            for(int32 Slot=0;Slot<Item->CrystalClues->GetInstanceCount();++Slot)
            {
                FTransform Actual;Item->CrystalClues->GetInstanceTransform(Slot,Actual,true);
                const int32 Fragment=Placement.Fragments.IndexOfByPredicate([&](const FTransform& T){return T.Equals(Actual);});
                TestTrue(TEXT("Each surviving instance keeps its authored transform and solid support"),Covered.IsValidIndex(Fragment)&&Covered[Fragment]);
            }
        };
        TestEqual(TEXT("All fragments exist before any swing"),Item->CrystalClues->GetInstanceCount(),3);
        TestTrue(TEXT("First cut commits"),Rock->MineSphere(FVector(500,50,50),150));
        AssertSurvivors(TEXT("First cut removes chips before any field tick/render frame"));
        TestEqual(TEXT("First cut leaves two chips"),Item->CrystalClues->GetInstanceCount(),2);
        Item->bCollected=true;Item->Model->SetVisibility(false);
        TestEqual(TEXT("Collection does not remove clues"),Item->CrystalClues->GetInstanceCount(),2);
        TestTrue(TEXT("Second cut commits after collection"),Rock->MineSphere(FVector(-500,50,50),150));
        AssertSurvivors(TEXT("Collected vein still reacts synchronously"));
        TestEqual(TEXT("Untouched chip remains"),Item->CrystalClues->GetInstanceCount(),1);
        TestFalse(TEXT("Invalid cut is rejected"),Rock->MineSphere(FVector::ZeroVector,-1));
        AssertSurvivors(TEXT("Rejected cut never changes instances"));
        Rock->ResetRock();AssertSurvivors(TEXT("Reset restores support and instances together"));
        TestEqual(TEXT("Reset restores all three chips"),Item->CrystalClues->GetInstanceCount(),3);
        auto* Overlap=World->SpawnActor<ADiggableRock>();Overlap->RockSize=FVector(1000);Overlap->ResetRock();Item->CoveringRocks.Add(Overlap);
        Rock->MineSphere(FVector(500,50,50),150);
        AssertSurvivors(TEXT("An overlapping uncut rock still supports its chip"));
        Item->Destroy();Rock->ResetRock();Rock->Destroy();Overlap->Destroy();
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningThinEntranceBudgetTest,"BrainMining.Terrain.ThinEntranceCutBudget",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningThinEntranceBudgetTest::RunTest(const FString&)
{
    // Actual TunnelSoilRight after its first hit; the next failed boolean took 12.99 seconds.
    TArray<FString> Lines;
    if(!TestTrue(TEXT("Load actual failure mesh"),FFileHelper::LoadFileToStringArray(Lines,*(FPaths::ProjectDir()/TEXT("Source/BrainMining/Tests/Fixtures/ThinEntranceAfterFirstHit.obj")))))return false;
    UE::Geometry::FDynamicMesh3 Source;
    for(const FString& Line:Lines)
    {
        TArray<FString> P;Line.ParseIntoArrayWS(P);if(P.Num()!=4)continue;
        if(P[0]==TEXT("v"))Source.AppendVertex(FVector3d(FCString::Atod(*P[1]),FCString::Atod(*P[2]),FCString::Atod(*P[3])));
        if(P[0]==TEXT("f"))Source.AppendTriangle(FCString::Atoi(*P[1])-1,FCString::Atoi(*P[2])-1,FCString::Atoi(*P[3])-1);
    }
    TestTrue(TEXT("Captured terrain is closed"),Source.IsClosed());
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    ON_SCOPE_EXIT{World->DestroyWorld(false);};
    auto* Rock=World->SpawnActor<AGeneratedMiningRock>();
    Rock->SetGeneratedMesh(UE::Geometry::FDynamicMesh3(Source));
    int Changed=0;double Worst=0;
    for(double X:{-220.,0.,220.})for(double Y:{-250.,-100.,50.})for(double Z:{0.,140.})
    {
        Rock->ResetRock();
        for(int Hit=0;Hit<3;++Hit)
        {
            const int Before=Rock->TriangleCount;const double Started=FPlatformTime::Seconds();
            const bool Cut=Rock->MineSphere(FVector(X,Y+Hit*55,Z),160);
            const double Milliseconds=(FPlatformTime::Seconds()-Started)*1000;Worst=FMath::Max(Worst,Milliseconds);
            Changed+=Cut && Before!=Rock->TriangleCount;
            if(!TestTrue(TEXT("Interactive failure must not freeze the editor"),Milliseconds<250))return false;
        }
    }
    TestTrue(TEXT("Budget guard still permits actual excavation"),Changed>=5);
    AddInfo(FString::Printf(TEXT("Captured entrance: 54 cuts, %d changed terrain, worst %.2f ms"),Changed,Worst));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDeferredCutTest,"BrainMining.Terrain.DeferredHitNotDropped",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningDeferredCutTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    ON_SCOPE_EXIT{World->DestroyWorld(false);};
    auto* Rock=World->SpawnActor<AGeneratedMiningRock>();Rock->RockSize=FVector(1000);Rock->ResetRock();
    UE::Geometry::FDynamicMesh3 Mesh;Rock->CopyMiningMesh(Mesh);
    TestTrue(TEXT("Chunk seam test terrain"),Rock->SetChunkedMesh(Mesh,500,true,true));
    Rock->InteractiveCutBudgetMilliseconds=.01f;
    auto* Item=World->SpawnActor<AMiningTreasure>();Item->CoveringRocks={Rock};
    Item->Model->SetMaterial(0,UMaterial::GetDefaultMaterial(MD_Surface));
    FMiningTreasurePlacement P;P.FragmentSupports={FVector(470,50,50),FVector(-470,50,50),FVector(50,470,50)};
    for(const auto& Point:P.FragmentSupports)P.Fragments.Add(FTransform(FQuat::Identity,Point,FVector(.1)));
    Item->ConfigureCrystalClues(P,LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    const int Before=Rock->TriangleCount;
    const double Started=FPlatformTime::Seconds();
    TestTrue(TEXT("Timed-out hit is accepted"),Rock->MineSphere(FVector(500,50,50),150));
    TestTrue(TEXT("Game thread remains responsive"),(FPlatformTime::Seconds()-Started)*1000<100);
    TestTrue(TEXT("Slow work is pending"),Rock->bCutPending);
    TestEqual(TEXT("No partial geometry before commit"),Rock->TriangleCount,Before);
    TestEqual(TEXT("No early crystal disappearance"),Item->CrystalClues->GetInstanceCount(),3);
    TestTrue(TEXT("Second hit queues without dropping"),Rock->MineSphere(FVector(-500,50,50),150));
    auto Drain=[&]()
    {
        const double Deadline=FPlatformTime::Seconds()+3;
        while(Rock->bCutPending && FPlatformTime::Seconds()<Deadline)
        {
            FPlatformProcess::Sleep(.001f);Rock->Tick(.01f);
            TArray<bool> Covered;AMiningTreasure::MarkCoveredPoints(Item->CoveringRocks,P.FragmentSupports,Covered);
            int Count=0;for(bool B:Covered)Count+=B;
            TestEqual(TEXT("Every commit updates geometry and crystal support together"),Item->CrystalClues->GetInstanceCount(),Count);
        }
        TestFalse(TEXT("Queued cuts complete"),Rock->bCutPending);
    };
    Drain();TestEqual(TEXT("Both accepted hits actually excavate"),Rock->SuccessfulCuts,2);
    TestEqual(TEXT("Both went through the background path"),Rock->DeferredCutsCompleted,2);
    TestEqual(TEXT("Unmined fragment remains"),Item->CrystalClues->GetInstanceCount(),1);
    Rock->MineSphere(FVector(50,500,50),150);Rock->ResetRock();
    FPlatformProcess::Sleep(.08f);Rock->Tick(.1f);
    TestEqual(TEXT("Reset discards obsolete pending cut"),Rock->SuccessfulCuts,0);
    TestEqual(TEXT("Reset preserves original clues"),Item->CrystalClues->GetInstanceCount(),3);
    return true;
}
#endif
