#if WITH_DEV_AUTOMATION_TESTS
#include "MiningTerrainFragments.h"
#include "DynamicMeshEditor.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "Misc/AutomationTest.h"

using namespace UE::Geometry;
namespace
{
void AddSolid(FDynamicMesh3& Mesh,FVector Center,FVector Extent)
{
    FMinimalBoxMeshGenerator Generator;Generator.Box=FOrientedBox3d(FVector3d(Center),FVector3d(Extent));Generator.Generate();
    FDynamicMesh3 Part(&Generator);FMeshIndexMappings Map;FDynamicMeshEditor(&Mesh).AppendMesh(&Part,Map);
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningFloatingFragmentTest,"BrainMining.Mining.FloatingFragments",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningFloatingFragmentTest::RunTest(const FString&)
{
    const FBox Original(FVector(-100),FVector(100));
    FDynamicMesh3 Mesh;
    AddSolid(Mesh,FVector(0,0,-80),FVector(100,100,20));
    AddSolid(Mesh,FVector(0,0,30),FVector(8));
    AddSolid(Mesh,FVector(55,0,-52),FVector(8));
    TestEqual(TEXT("Only detached floating particle removed"),MiningTerrainFragments::RemoveSmallFloatingPieces(Mesh,Original,false),12);
    TestEqual(TEXT("Floor and resting solid survive"),Mesh.TriangleCount(),24);
    TestTrue(TEXT("Remaining collision geometry stays closed"),Mesh.IsClosed());
    FDynamicMesh3 Seam;
    AddSolid(Seam,FVector(92,0,20),FVector(8));
    TestEqual(TEXT("Possible neighbor connection at chunk edge retained"),MiningTerrainFragments::RemoveSmallFloatingPieces(Seam,Original,true),0);
    TestEqual(TEXT("Same isolated solid away from chunk support removed"),MiningTerrainFragments::RemoveSmallFloatingPieces(Seam,Original,false),12);
    FDynamicMesh3 Ledge;
    AddSolid(Ledge,FVector::ZeroVector,FVector(24));
    TestEqual(TEXT("Large freestanding terrain is preserved"),MiningTerrainFragments::RemoveSmallFloatingPieces(Ledge,Original,false),0);
    return true;
}
#endif
