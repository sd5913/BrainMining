#include "DiggableRock.h"
#include "GeneratedMiningRock.h"
#include "MiningBlastGeometry.h"
#include "Components/DynamicMeshComponent.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "UDynamicMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "HAL/PlatformProcess.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace UE::Geometry;
namespace
{
double Volume(const FDynamicMesh3& Mesh)
{
	double Value=0;
	for(int32 ID:Mesh.TriangleIndicesItr())
	{
		const FIndex3i Tri=Mesh.GetTriangle(ID);
		Value+=Mesh.GetVertex(Tri.A).Dot(Mesh.GetVertex(Tri.B).Cross(Mesh.GetVertex(Tri.C)))/6.;
	}
	return FMath::Abs(Value);
}
double RockVolume(ADiggableRock* Rock)
{
	if(auto* Generated=Cast<AGeneratedMiningRock>(Rock))
	{
		FDynamicMesh3 Mesh;Generated->CopyMiningMesh(Mesh);return Volume(Mesh);
	}
	return Volume(Rock->RockMesh->GetDynamicMesh()->GetMeshRef());
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningBombFractureTest,"BrainMining.Bomb.FracturedCavityAndCollision",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningBombFractureTest::RunTest(const FString& Parameters)
{
	const FVector Center(0,0,960);
	const auto Cutter=MiningBlast::MakeCutter(Center,520,.35f,42);
	TestTrue(TEXT("Fracture cutter is closed and valid"),Cutter.IsClosed()&&Cutter.CheckValidity(FDynamicMesh3::FValidityOptions(),EValidityCheckFailMode::ReturnOnly));
	double Minimum=MAX_dbl,Maximum=0;
	for(int32 V:Cutter.VertexIndicesItr()) {const double R=(Cutter.GetVertex(V)-FVector3d(Center)).Length();Minimum=FMath::Min(Minimum,R);Maximum=FMath::Max(Maximum,R);}
	TestTrue(TEXT("Broad fractures vary radius by at least 15 percent"),Maximum-Minimum>520*.15);
	TestTrue(TEXT("Same seed gives the same boundary for adjacent chunks"),Cutter.IsSameAs(MiningBlast::MakeCutter(Center,520,.35f,42),FDynamicMesh3::FSameAsOptions()));
	TestFalse(TEXT("Different bombs do not stamp an identical cavity"),Cutter.IsSameAs(MiningBlast::MakeCutter(Center,520,.35f,43),FDynamicMesh3::FSameAsOptions()));
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
	ON_SCOPE_EXIT{World->DestroyWorld(false);};
	auto* Rock=World->SpawnActor<ADiggableRock>();Rock->RockSize=FVector(4000,4000,2000);Rock->ResetRock();
	const double Initial=RockVolume(Rock);
	TestTrue(TEXT("Existing pickaxe sphere still succeeds"),Rock->MineSphere(Center,160));
	const double PickaxeRemoved=Initial-RockVolume(Rock);
	Rock->ResetRock();
	TestTrue(TEXT("Bomb removes real rock, not just an effect"),Rock->MineBlast(Center,520,.35f,42));
	const double BombRemoved=Initial-RockVolume(Rock);
	TestTrue(TEXT("Bomb excavates substantially more than pickaxe"),BombRemoved>PickaxeRemoved*15);
	const auto& Result=Rock->RockMesh->GetDynamicMesh()->GetMeshRef();
	TestTrue(TEXT("Crater mesh stays closed and under budget"),Result.IsClosed()&&Result.TriangleCount()<Rock->MaxTriangles);
	TestTrue(TEXT("Collision is rebuilt for the crater"),Rock->RockMesh->GetBodySetup()!=nullptr && Rock->LastCollisionMilliseconds>0);
	TestFalse(TEXT("Invalid blast preserves the rock"),Rock->MineBlast(Center,-1,.35f,42));
	TestEqual(TEXT("Invalid request does not count as a cut"),Rock->SuccessfulCuts,1);
	Rock->ResetRock();TestTrue(TEXT("Reset restores the exact initial rock"),FMath::IsNearlyEqual(RockVolume(Rock),Initial,1.));
	FMinimalBoxMeshGenerator Box;Box.Box=FOrientedBox3d(FVector3d::Zero(),FVector3d(2000,2000,1000));Box.Generate();
	auto* Chunked=World->SpawnActor<AGeneratedMiningRock>();
	TestTrue(TEXT("Chunked cave fixture builds"),Chunked->SetChunkedMesh(FDynamicMesh3(&Box),800,true));
	const double ChunkInitial=RockVolume(Chunked);
	TestTrue(TEXT("Blast crosses cave chunk boundaries"),Chunked->MineBlast(FVector(0,0,960),520,.35f,42));
	const double Deadline=FPlatformTime::Seconds()+3;
	while(Chunked->bCutPending && FPlatformTime::Seconds()<Deadline)
	{FPlatformProcess::Sleep(.001f);Chunked->Tick(.01f);}
	TestFalse(TEXT("Accepted blast completes across all chunks"),Chunked->bCutPending);
	TestTrue(TEXT("Multiple chunks receive the same fracture volume"),Chunked->LastTouchedChunks>1);
	TestTrue(TEXT("Chunked excavation matches unchunked excavation"),FMath::Abs((ChunkInitial-RockVolume(Chunked))-BombRemoved)<BombRemoved*.001);
	for(const auto& Chunk:Chunked->Chunks) TestTrue(TEXT("Every touched chunk remains closed"),Chunk.Mesh->GetDynamicMesh()->GetMeshRef().IsClosed());
	Chunked->SegmentIndex=0;Chunked->SetSegmentState(EMiningSegmentState::Frozen);
	TestFalse(TEXT("Bomb respects frozen tunnel segment gate"),Chunked->MineBlast(Center,520,.35f,44));
	Chunked->SetSegmentState(EMiningSegmentState::Active);Chunked->ResetRock();
	TestTrue(TEXT("Cave reset restores the chunk volume"),FMath::IsNearlyEqual(RockVolume(Chunked),ChunkInitial,1.));
	return true;
}
#endif
