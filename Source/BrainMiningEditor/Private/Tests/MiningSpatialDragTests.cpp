#include "MiningSpatialDirectEditing.h"
#include "MiningSpatialSafeEditing.h"
#include "MiningSpatialBuild.h"
#include "MiningLayoutRoom.h"
#include "MiningTunnelGenerator.h"
#include "GeneratedMiningRock.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
#include "FileHelpers.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "MiningDesignerLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	using namespace MiningSpatialSafeEditing;
	struct FDragWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
		AMiningSpatialRegion* Region;
		AMiningSpatialTunnel* Tunnel;
		AMiningTunnelGenerator* Generator;
		FDragWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World); Reset();
			AMiningLayoutRoom* Start = Spawn<AMiningLayoutRoom>(); AMiningLayoutRoom* Finish = Spawn<AMiningLayoutRoom>();
			Start->RoomSize = Finish->RoomSize = FVector(1200, 1200, 1000); Start->bStartRoom = true; Finish->ConnectionOrder = 1;
			Start->bFloorAnchored = Finish->bFloorAnchored = true; Finish->SetActorLocation(FVector(4200, 0, -100));
			Region = Spawn<AMiningSpatialRegion>(); Region->SetActorLocation(FVector(2100, 0, 500)); Region->RockSize = FVector(3200, 3200, 2400); Region->FromRoom = Start; Region->ToRoom = Finish;
			Region->bAutomaticBedrock = false; Region->bFreeEditing = false;
			Tunnel = Spawn<AMiningSpatialTunnel>(); Tunnel->Region = Region; Region->MainTunnel = Tunnel; Tunnel->StartPortal.Room = Start; Tunnel->EndPortal.Room = Finish; Tunnel->FitPortals(); MiningSpatial::Insert(Tunnel->Knots, 0, 0.5);
			Generator = Spawn<AMiningTunnelGenerator>(); Generator->bHollowRegionPrototype = true; Generator->bDirectRoomFaces = true; Generator->bAutomaticChunks = true; Generator->HollowRegion = Region; Generator->bAutoRebuild = false;
			Generator->RebuildTunnels(); Observe(World);
		}
		~FDragWorld() { MiningSpatialDirectEditing::End(true); FinishInteractive(true); Reset(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
		template<typename ActorType> ActorType* Spawn()
		{
			FActorSpawnParameters Parameters; Parameters.ObjectFlags |= RF_Transactional; return World->SpawnActor<ActorType>(Parameters);
		}
		MiningSpatial::FBuildPtr Compute(MiningSpatial::FBuildPtr Previous = nullptr)
		{
			MiningSpatial::FRegionInput Input; FString Error;
			if (!MiningSpatial::Capture(*Region, FVector::ZeroVector, Generator->BedrockThickness, Input, Error)) return nullptr;
			return MiningSpatial::Compute(MoveTemp(Input), Generator->ChunkLength, Generator->SegmentTriangleBudget, Previous);
		}
	};
	bool SameSurface(const UE::Geometry::FDynamicMesh3& First, const UE::Geometry::FDynamicMesh3& Second)
	{
		using namespace UE::Geometry;
		if (!First.TriangleCount() || !Second.TriangleCount()) return First.TriangleCount() == Second.TriangleCount();
		FDynamicMeshAABBTree3 Tree(&Second, true);
		const auto Near = [&](FVector3d Position)
		{
			double DistanceSquared = 0; Tree.FindNearestTriangle(Position, DistanceSquared); return DistanceSquared < 1.e-8;
		};
		for (int32 Vertex : First.VertexIndicesItr()) if (!Near(First.GetVertex(Vertex))) return false;
		for (int32 Triangle : First.TriangleIndicesItr()) if (!Near(First.GetTriCentroid(Triangle))) return false;
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpatialIncrementalGridTest, "BrainMining.Spatial.Performance.IncrementalGridAndCollision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpatialIncrementalGridTest::RunTest(const FString& Parameters)
{
	FDragWorld Scene;
	auto Before = Scene.Compute(); if (!TestTrue(TEXT("Baseline computes"), Before && Before->bSuccess)) return false;
	Scene.Tunnel->Knots[1].Position.Y += 35;
	auto After = Scene.Compute(Before); if (!TestTrue(TEXT("Edited terrain computes"), After && After->bSuccess)) { if (After) AddError(After->Error); return false; }
	auto Full = Scene.Compute(); if (!TestTrue(TEXT("Uncached reference computes"), Full && Full->bSuccess)) return false;
	TestEqual(TEXT("Grid is stable"), After->Grid->Cells.Num(), Full->Grid->Cells.Num());
	UE::Geometry::FDynamicMesh3::FSameAsOptions Options; Options.bCheckAttributes = true; Options.EpsilonDouble = 0; Options.EpsilonFloat = 0;
	for (int32 Index = 0; Index < After->Grid->Cells.Num(); ++Index)
		TestTrue(TEXT("Incremental cell has the same surface in both directions"), SameSurface(*After->Grid->Cells[Index].Mesh, *Full->Grid->Cells[Index].Mesh) && SameSurface(*Full->Grid->Cells[Index].Mesh, *After->Grid->Cells[Index].Mesh));
	AGeneratedMiningRock* Rock = Scene.Generator->GeneratedSegments[0]; Rock->ApplySpatialGrid(Before->Rock, Before->Grid);
	TArray<UDynamicMeshComponent*> Components; TArray<UBodySetup*> Collisions;
	for (const FMiningRockChunk& Chunk : Rock->Chunks) { Components.Add(Chunk.Mesh); Collisions.Add(Chunk.Mesh->GetBodySetup()); }
	Rock->ApplySpatialGrid(After->Rock, After->Grid);
	TestTrue(TEXT("Unchanged components are actually reused"), Rock->LastReusedEditorChunks > 0);
	TestTrue(TEXT("Changed components really receive new geometry"), Rock->LastRebuiltEditorChunks > 0);
	for (const FMiningRockChunk& Chunk : Rock->Chunks)
	{
		const int32 Previous = Components.IndexOfByKey(Chunk.Mesh.Get());
		if (Previous != INDEX_NONE) TestTrue(TEXT("Untouched collision object is preserved"), Chunk.Mesh->GetBodySetup() == Collisions[Previous]);
	}
	Rock->ApplySpatialGrid(After->Rock, After->Grid);
	TestEqual(TEXT("Identical geometry never cooks collision again"), Rock->LastRebuiltEditorChunks, 0);
	TestTrue(TEXT("Budget is unchanged"), Rock->LargestChunkTriangles <= 30000 && Rock->Chunks.Num() <= 128);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpatialDragQueueTest, "BrainMining.Spatial.Performance.LatestTargetAndLiveCommits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpatialDragQueueTest::RunTest(const FString& Parameters)
{
	FDragWorld Scene; if (!TestTrue(TEXT("Initial map is applied"), Scene.Generator->bLayoutApplied)) return false;
	const FVector Original = Scene.Tunnel->Knots[1].Position;
	AGeneratedMiningRock* Rock = Scene.Generator->GeneratedSegments[0];
	BeginInteractive();
	for (int32 Step = 1; Step <= 20; ++Step)
	{
		Scene.Tunnel->Knots[1].Position = Original + FVector(0, Step * 2, 0);
		TestTrue(TEXT("Input queues without rebuilding"), QueueInteractive(Scene.Tunnel));
		TestTrue(TEXT("Unvalidated target never replaces accepted source"), Scene.Tunnel->Knots[1].Position == Original);
	}
	TestTrue(TEXT("Only latest pending target is retained"), DragStats().Coalesced >= 18);
	const double Deadline = FPlatformTime::Seconds() + 20;
	while (HasPendingInteractive() && FPlatformTime::Seconds() < Deadline) { FPlatformProcess::Sleep(0.001f); PumpInteractive(); }
	TestFalse(TEXT("Worker finishes"), HasPendingInteractive());
	TestTrue(TEXT("Terrain commits while gesture is still active"), IsInteractive() && DragStats().Committed > 0);
	TestTrue(TEXT("Newest absolute target wins"), Scene.Tunnel->Knots[1].Position.Equals(Original + FVector(0, 40, 0)));
	TestTrue(TEXT("Generated actor identity survives drag"), Scene.Generator->GeneratedSegments[0] == Rock);
	TestTrue(TEXT("Source, terrain and collision are applied together"), Scene.Generator->bLayoutApplied && FinishInteractive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpatialDragInvalidTest, "BrainMining.Spatial.Performance.InvalidTargetCancelAndStaleResult", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpatialDragInvalidTest::RunTest(const FString& Parameters)
{
	FDragWorld Scene; const FVector Original = Scene.Tunnel->Knots[1].Position;
	const int32 Revision = Scene.Generator->BuildRevision;
	BeginInteractive(); Scene.Tunnel->Knots[1].Position.Y = 100000; TestTrue(TEXT("Bad target is checked in background"), QueueInteractive(Scene.Tunnel)); FinishInteractive();
	TestTrue(TEXT("Rejected request preserves source"), Scene.Tunnel->Knots[1].Position == Original);
	TestEqual(TEXT("Rejected request never commits terrain"), Scene.Generator->BuildRevision, Revision);
	TestTrue(TEXT("Rejected request explains limitation"), Issue().bLimited && !Issue().Reason.IsEmpty());
	BeginInteractive(); Scene.Tunnel->Knots[1].Position.Y += 30; QueueInteractive(Scene.Tunnel); FinishInteractive(true);
	const double Deadline = FPlatformTime::Seconds() + 3;
	while (FPlatformTime::Seconds() < Deadline && DragStats().Discarded == 0) { FPlatformProcess::Sleep(0.001f); PumpInteractive(); }
	TestEqual(TEXT("Esc cannot apply a late worker result"), Scene.Generator->BuildRevision, Revision);
	TestTrue(TEXT("Canceled worker is discarded"), DragStats().Discarded > 0);
	BeginInteractive(); Scene.Tunnel->Knots[1].Position.Y += 25; QueueInteractive(Scene.Tunnel);
	Scene.Tunnel->Knots[1].Position.Y = 19;
	FinishInteractive();
	TestTrue(TEXT("External source edit is never overwritten by a stale result"), FMath::IsNearlyEqual(Scene.Tunnel->Knots[1].Position.Y, 19.0));
	TestEqual(TEXT("Stale geometry not committed"), Scene.Generator->BuildRevision, Revision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpatialDragUndoTest, "BrainMining.Spatial.Performance.OneUndoAndSaveFlush", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpatialDragUndoTest::RunTest(const FString& Parameters)
{
	using namespace MiningSpatialDirectEditing;
	FDragWorld Scene; const FVector Original = Scene.Tunnel->Knots[1].Position;
	FHandle Handle{Scene.Tunnel, EHandle::Knot, 1}; Select(Handle); Begin(); BeginInteractive();
	Translate(Handle, FVector(0, 15, 0)); Translate(Handle, FVector(0, 40, 0)); End();
	AddInfo(Status() + TEXT(" / ") + Issue().Reason);
	const FVector Edited = Scene.Tunnel->Knots[1].Position;
	TestTrue(TEXT("Released drag is applied"), !Edited.Equals(Original));
	GEditor->UndoTransaction(); Apply(Scene.Tunnel);
	TestTrue(TEXT("One undo restores all drag samples"), Scene.Tunnel->Knots[1].Position.Equals(Original));
	GEditor->RedoTransaction(); Apply(Scene.Tunnel);
	TestTrue(TEXT("One redo restores final accepted target"), Scene.Tunnel->Knots[1].Position.Equals(Edited));
	Select(Handle); Begin(); BeginInteractive(); Translate(Handle, FVector(0, 10, 0));
	TestTrue(TEXT("Save and PIE preparation flush pending target"), PrepareWorld(Scene.World));
	TestFalse(TEXT("Nothing can arrive after save preparation"), HasPendingInteractive());
	TestTrue(TEXT("Saved source matches final target"), Scene.Tunnel->Knots[1].Position.Y > Edited.Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpatialUserMapBenchmark, "MiningBenchmarks.SpatialDrag.UserMap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpatialUserMapBenchmark::RunTest(const FString& Parameters)
{
	using namespace MiningSpatialDirectEditing;
	UWorld* World = UEditorLoadingAndSavingUtils::NewMapFromTemplate(TEXT("/Game/MiningPrototype/Maps/Lvl_MiningSpatial_Demo"), false);
	if (!TestNotNull(TEXT("Disposable copy loads"), World)) return false;
	AMiningTunnelGenerator* Generator = nullptr;
	AMiningSpatialRegion* Region = nullptr;
	for (TActorIterator<AMiningTunnelGenerator> Item(World); Item; ++Item) Generator = *Item;
	for (TActorIterator<AMiningSpatialRegion> Item(World); Item; ++Item) Region = *Item;
	if (!Generator || !Region || !Region->MainTunnel) { AddError(TEXT("User map has no spatial setup")); return false; }
	AMiningSpatialTunnel* Tunnel = Region->MainTunnel;
	if (!TestTrue(TEXT("User terrain is valid"), Generator->RebuildTunnels())) { AddError(Generator->LastBuildStatus); return false; }
	Observe(World);
	int32 Chunks = 0; for (AGeneratedMiningRock* Rock : Generator->GeneratedSegments) Chunks += Rock->Chunks.Num();
	const int32 Knot = FMath::Min(3, Tunnel->Knots.Num() - 2);
	FHandle Handle{Tunnel, EHandle::Knot, Knot}; Select(Handle); Begin(); BeginInteractive();
	const FVector Original = Tunnel->Knots[Knot].Position;
	double MaxTickMilliseconds = 0;
	for (int32 Step = 1; Step <= 100; ++Step)
	{
		const double Started = FPlatformTime::Seconds();
		const FVector Target = Original + FVector(0, Step * 0.4, 0);
		Translate(Handle, Tunnel->GetActorTransform().TransformVectorNoScale(Target - Tunnel->Knots[Knot].Position));
		PumpInteractive();
		MaxTickMilliseconds = FMath::Max(MaxTickMilliseconds, (FPlatformTime::Seconds() - Started) * 1000);
		FPlatformProcess::Sleep(0.016f);
	}
	const int32 LiveCommits = DragStats().Committed;
	const double Released = FPlatformTime::Seconds(); End();
	const double ReleaseMilliseconds = (FPlatformTime::Seconds() - Released) * 1000;
	int32 Reused = 0, Rebuilt = 0;
	for (AGeneratedMiningRock* Rock : Generator->GeneratedSegments) { Reused += Rock->LastReusedEditorChunks; Rebuilt += Rock->LastRebuiltEditorChunks; }
	TestTrue(TEXT("Actual map commits legal terrain before mouse release"), LiveCommits > 1);
	TestTrue(TEXT("Latest user target is applied"), Tunnel->Knots[Knot].Position.Equals(Original + FVector(0, 40, 0)));
	TestTrue(TEXT("Budget and untouched chunks preserved"), Generator->SegmentTriangleBudget == 30000 && Reused > 0 && Rebuilt > 0);
	const FString Report = FString::Printf(TEXT("{\"chunks\":%d,\"live_commits\":%d,\"max_input_plus_commit_ms\":%.3f,\"release_ms\":%.3f,\"reused\":%d,\"rebuilt\":%d,\"metrics\":%s}"), Chunks, LiveCommits, MaxTickMilliseconds, ReleaseMilliseconds, Reused, Rebuilt, *UMiningDesignerLibrary::SpatialDragPerformance());
	FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("Validation/spatial_drag_user_continuous.json")));
	AddInfo(Report);
	return true;
}
#endif
