#include "GeneratedMiningRock.h"
#include "Showcase/MiningTreasure.h"
#include "MiningChunkGeometry.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/PlatformProcess.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningAuthoredVeinCutTest,"BrainMining.Terrain.AuthoredVeinHitNotDropped",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningAuthoredVeinCutTest::RunTest(const FString&)
{
    using namespace UE::Geometry;
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    ON_SCOPE_EXIT{World->DestroyWorld(false);};
    auto* Rock=World->SpawnActor<AGeneratedMiningRock>();
    Rock->bChunked=true;Rock->MaxTriangles=100000;
    for(int Id:{286,287,298,299,454,455,466,467})
    {
        TArray<FString> Lines;FDynamicMesh3 Mesh;
        if(!TestTrue(TEXT("Captured authored chunk loads"),FFileHelper::LoadFileToStringArray(Lines,*(FPaths::ProjectDir()/FString::Printf(TEXT("Source/BrainMining/Tests/Fixtures/VeinCut42/chunk_%d.obj"),Id)))))return false;
        for(const auto& Line:Lines)
        {
            TArray<FString> P;Line.ParseIntoArrayWS(P);if(P.Num()!=4)continue;
            if(P[0]==TEXT("v"))Mesh.AppendVertex(FVector3d(FCString::Atod(*P[1]),FCString::Atod(*P[2]),FCString::Atod(*P[3])));
            if(P[0]==TEXT("f"))Mesh.AppendTriangle(FCString::Atoi(*P[1])-1,FCString::Atoi(*P[2])-1,FCString::Atoi(*P[3])-1);
        }
        TestTrue(FString::Printf(TEXT("Authored chunk %d starts closed"),Id),Mesh.IsClosed());
        auto& Chunk=Rock->Chunks.AddDefaulted_GetRef();
        Chunk.InitialMesh=NewObject<UDynamicMesh>(Rock);Chunk.InitialMesh->SetMesh(FDynamicMesh3(Mesh));
        Chunk.Mesh=NewObject<UDynamicMeshComponent>(Rock);Chunk.Mesh->RegisterComponent();
        Chunk.Mesh->AttachToComponent(Rock->GetRootComponent(),FAttachmentTransformRules::KeepRelativeTransform);
        const auto Bounds=Mesh.GetBounds();Chunk.Bounds=FBox(FVector(Bounds.Min),FVector(Bounds.Max));
        Chunk.Triangles=Mesh.TriangleCount();Chunk.Mesh->SetMesh(MoveTemp(Mesh));
    }
    const FVector Center(-12299.927506278818,4335.127503671189,8118.486851229218);
    TArray<bool> Covered;
    AMiningTreasure::MarkCoveredPoints({Rock},{Center},Covered);
    TestTrue(TEXT("Captured impact centre starts inside solid rock"),Covered[0]);
    TestTrue(TEXT("Ordinary authored vein hit accepted"),Rock->MineSphere(Center,320));
    const double Deadline=FPlatformTime::Seconds()+3;
    while(Rock->bCutPending && FPlatformTime::Seconds()<Deadline){FPlatformProcess::Sleep(.001f);Rock->Tick(.01f);}
    TestFalse(TEXT("Ordinary hit finishes"),Rock->bCutPending);
    TestEqual(TEXT("Accepted hit changes actual terrain"),Rock->SuccessfulCuts,1);
    AMiningTreasure::MarkCoveredPoints({Rock},{Center},Covered);
    TestFalse(TEXT("The actual cavity is open at the impact centre"),Covered[0]);
    AddInfo(Rock->LastStatus);
    for(const auto& Chunk:Rock->Chunks)TestTrue(TEXT("Result remains watertight"),Chunk.Mesh->GetDynamicMesh()->GetMeshRef().IsClosed());
    return true;
}
#endif
