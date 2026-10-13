#include "GeneratedMiningRock.h"
#include "MiningTerrainSource.h"
#include "MiningDebrisBurst.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "UDynamicMesh.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningSharedTerrainTest, "BrainMining.Startup.SharedTerrainDuplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningSharedTerrainTest::RunTest(const FString&)
{
	UWorld* EditorWorld = UWorld::CreateWorld(EWorldType::Editor, false);
	UWorld* GameWorld = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { GameWorld->DestroyWorld(false); EditorWorld->DestroyWorld(false); };
	auto* Original = EditorWorld->SpawnActor<AGeneratedMiningRock>(FVector(100000, 0, 0), FRotator::ZeroRotator);
	Original->RockSize = FVector(1000);
	Original->ResetRock();
	UE::Geometry::FDynamicMesh3 Geometry;
	Original->CopyMiningMesh(Geometry);
	if (!TestTrue(TEXT("Create chunked source"), Original->SetChunkedMesh(Geometry, 500, true, true))) return false;
	auto* Source = NewObject<UMiningTerrainSource>(GetTransientPackage());
	if (!TestTrue(TEXT("Extract external terrain source"), Original->ExtractTerrainSource(Source))) return false;
	const int32 PreviewTriangles = Source->Preview->GetTriangleCount();
	const int32 FirstChunkTriangles = Source->Meshes[0]->GetTriangleCount();
	FObjectDuplicationParameters Parameters(Original, GameWorld->PersistentLevel);
	Parameters.DuplicateMode = EDuplicateMode::PIE;
	auto* Copy = Cast<AGeneratedMiningRock>(StaticDuplicateObjectEx(Parameters));
	if (!TestNotNull(TEXT("PIE actor copy exists"), Copy)) return false;
	GameWorld->PersistentLevel->Actors.Add(Copy);
	TestTrue(TEXT("Source asset is shared with PIE"), Copy->TerrainSource == Source);
	TestEqual(TEXT("Chunk descriptors survive"), Copy->Chunks.Num(), Original->Chunks.Num());
	for (int32 Index = 0; Index < Copy->Chunks.Num(); ++Index)
	{
		TestNull(TEXT("Runtime component is not copied into PIE"), Copy->Chunks[Index].Mesh.Get());
		TestTrue(TEXT("Original mesh is shared without copying"), Copy->Chunks[Index].InitialMesh == Source->Meshes[Index]);
	}
	Copy->RegisterAllComponents();
	Copy->DispatchBeginPlay();
	TestEqual(TEXT("All distant chunks remain deferred"), Copy->DeferredRuntimeChunks, Copy->Chunks.Num());
	TestEqual(TEXT("BeginPlay cannot clear shared preview"), Source->Preview->GetTriangleCount(), PreviewTriangles);
	Copy->PrepareRuntimeGeometry(Copy->GetActorLocation(), 1500);
	for (const auto& Chunk : Copy->Chunks)
	{
		TestNotNull(TEXT("Activation creates missing runtime component"), Chunk.Mesh.Get());
		TestTrue(TEXT("Runtime data is separate from shared source"), Chunk.Mesh->GetDynamicMesh() != Chunk.InitialMesh);
	}
	FHitResult Hit;
	TestTrue(TEXT("PIE source has collision before player arrives"), GameWorld->LineTraceSingleByChannel(Hit,
		Copy->GetActorLocation() + FVector(1000, 50, 50), Copy->GetActorLocation() + FVector(0, 50, 50), ECC_Visibility));
	Copy->ResetRock();
	TestEqual(TEXT("Reset cannot clear shared chunk"), Source->Meshes[0]->GetTriangleCount(), FirstChunkTriangles);
	auto* EditorCopy = DuplicateObject<AGeneratedMiningRock>(Original, EditorWorld->PersistentLevel);
	EditorWorld->PersistentLevel->Actors.Add(EditorCopy);
	EditorCopy->RegisterAllComponents();
	for (const auto& Chunk : EditorCopy->Chunks)
	{
		if (!TestNotNull(TEXT("Editor duplicate reconstructs visible component"), Chunk.Mesh.Get())) return false;
		TestEqual(TEXT("Editor duplicate has full preview geometry"), Chunk.Mesh->GetDynamicMesh()->GetTriangleCount(), Chunk.InitialMesh->GetTriangleCount());
	}
	EditorCopy->SetGeneratedMesh(MoveTemp(Geometry));
	TestNull(TEXT("Editing detaches shared source"), EditorCopy->TerrainSource.Get());
	TestEqual(TEXT("Editing duplicate preserves shared preview"), Source->Preview->GetTriangleCount(), PreviewTriangles);
	TestEqual(TEXT("Editing duplicate preserves original chunks"), Source->Meshes[0]->GetTriangleCount(), FirstChunkTriangles);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDebrisPoolTest, "BrainMining.Startup.DebrisPoolReuse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDebrisPoolTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	TArray<TObjectPtr<UStaticMesh>> Meshes = { LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")) };
	auto* Material = UMaterial::GetDefaultMaterial(MD_Surface);
	auto* Burst = World->SpawnActor<AMiningDebrisBurst>();
	Burst->PreparePool(Meshes, 64);
	TestEqual(TEXT("Warmup creates at most four pieces at once"), Burst->GetPreparedChunkCount(), 4);
	for (int32 Step = 0; Step < 20; ++Step) Burst->PreparePool(Meshes, 64);
	TestEqual(TEXT("Pool has fixed capacity"), Burst->GetPreparedChunkCount(), 64);
	TInlineComponentArray<UStaticMeshComponent*> Before(Burst);
	for (int32 Count : {12, 36, 56, 64})
	{
		Burst->Initialize(Meshes, Material, FVector::UpVector, 160, Count, 1);
		TestEqual(TEXT("Requested debris count is active"), Burst->ChunkCount, Count);
		TestTrue(TEXT("Pooled actor is active"), Burst->IsPoolActive());
		int32 Simulating = 0;
		for (auto* Component : Before) if (Component->IsSimulatingPhysics()) ++Simulating;
		TestEqual(TEXT("Only displayed debris simulates physics"), Simulating, Count);
		Burst->LifeSpanExpired();
		TestFalse(TEXT("Lifespan recycles instead of destroying"), Burst->IsActorBeingDestroyed());
		TestFalse(TEXT("Expired burst is reusable"), Burst->IsPoolActive());
		for (auto* Component : Before) TestFalse(TEXT("Idle pool has no active physics"), Component->IsSimulatingPhysics());
	}
	TInlineComponentArray<UStaticMeshComponent*> After(Burst);
	TestEqual(TEXT("Repeated effects cannot grow component count"), After.Num(), Before.Num());
	for (int32 Index = 0; Index < Before.Num(); ++Index) TestTrue(TEXT("Component identity is reused"), Before[Index] == After[Index]);
	auto* Blast = World->SpawnActor<AMiningDebrisBurst>();
	Blast->InitializeBlast(Meshes, Material, FVector::UpVector, 160, 8, 1);
	TestFalse(TEXT("Bomb fallback remains independent"), Blast->IsPooled());
	TestEqual(TEXT("Bomb retains requested count"), Blast->ChunkCount, 8);
	Blast->LifeSpanExpired();
	TestTrue(TEXT("Nonpooled lifespan destroys normally"), Blast->IsActorBeingDestroyed());
	return true;
}
#endif
