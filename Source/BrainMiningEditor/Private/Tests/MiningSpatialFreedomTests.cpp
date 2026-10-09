#include "MiningSpatialBuild.h"
#include "MiningSpatialSafeEditing.h"
#include "MiningSpatialDirectEditing.h"
#include "MiningTunnelGenerator.h"
#include "MiningLayoutRoom.h"
#include "GeneratedMiningRock.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMeshActor.h"
#include "UDynamicMesh.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Spatial/FastWinding.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	struct FFreedomWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
		AMiningLayoutRoom* Start;
		AMiningLayoutRoom* Finish;
		AMiningSpatialRegion* Region;
		AMiningSpatialTunnel* Tunnel;
		AMiningTunnelGenerator* Generator;
		FFreedomWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World); MiningSpatialSafeEditing::Reset();
			Start = Spawn<AMiningLayoutRoom>(); Finish = Spawn<AMiningLayoutRoom>();
			Start->RoomSize = Finish->RoomSize = FVector(1200, 1200, 1000); Start->bStartRoom = true; Finish->ConnectionOrder = 1;
			Start->bFloorAnchored = Finish->bFloorAnchored = true; Finish->SetActorLocation(FVector(4200, 0, 0));
			Region = Spawn<AMiningSpatialRegion>(); Region->FromRoom = Start; Region->ToRoom = Finish; Region->RockSize = FVector(1);
			Tunnel = Spawn<AMiningSpatialTunnel>(); Tunnel->Region = Region; Region->MainTunnel = Tunnel;
			Tunnel->StartPortal.Room = Start; Tunnel->EndPortal.Room = Finish; Tunnel->FitPortals(); MiningSpatial::Insert(Tunnel->Knots, 0, 0.5);
			Generator = Spawn<AMiningTunnelGenerator>(); Generator->bHollowRegionPrototype = true; Generator->HollowRegion = Region; Generator->bAutoRebuild = false;
		}
		~FFreedomWorld() { MiningSpatialSafeEditing::FinishInteractive(true); MiningSpatialSafeEditing::Reset(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
		template<typename ActorType> ActorType* Spawn()
		{
			FActorSpawnParameters Parameters; Parameters.ObjectFlags |= RF_Transactional; return World->SpawnActor<ActorType>(Parameters);
		}
		MiningSpatial::FBuildPtr Compute(FString& Error)
		{
			MiningSpatial::FRegionInput Input;
			if (!MiningSpatial::Capture(*Region, FVector::ZeroVector, 100, Input, Error)) return {};
			auto Result = MiningSpatial::Compute(MoveTemp(Input), Generator->ChunkLength, Generator->SegmentTriangleBudget); Error = Result->Error; return Result;
		}
	};
	bool Contains(const UE::Geometry::FDynamicMesh3& Mesh, FVector Point)
	{
		if (Mesh.TriangleCount() == 0) return false;
		UE::Geometry::FDynamicMeshAABBTree3 Tree(&Mesh, true);
		UE::Geometry::TFastWindingTree<UE::Geometry::FDynamicMesh3> Winding(&Tree);
		return FMath::Abs(Winding.FastWindingNumber(Point)) > 0.5;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpatialAutomaticEnvelopeTest, "BrainMining.Spatial.Freedom.AutomaticEnvelope", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpatialAutomaticEnvelopeTest::RunTest(const FString& Parameters)
{
	FFreedomWorld Scene; FString Error;
	auto Result = Scene.Compute(Error);
	if (!TestTrue(TEXT("Automatic envelope ignores tiny manual bounds"), Result && Result->bSuccess)) { AddError(Error); return false; }
	TestTrue(TEXT("Tunnel walls have actual diggable buffer"), Contains(Result->Rock, FVector(2100, 500, 500)));
	TestFalse(TEXT("Initial tunnel remains open"), Contains(Result->Rock, FVector(2100, 0, 500)));
	TestTrue(TEXT("Main room walls also have diggable buffer"), Contains(Result->Rock, FVector(-800, 0, 500)));
	TestFalse(TEXT("Main room interior is not filled"), Contains(Result->Rock, FVector(0, 0, 500)));
	UE::Geometry::FDynamicMesh3 Shell;
	TestTrue(TEXT("Automatic bedrock is closed"), MiningChunks::Subtract(Result->Plan.OuterDomain, Result->Plan.RockDomain, Shell) && Shell.IsClosed());
	TestTrue(TEXT("Fixed bedrock outside buffer"), Contains(Shell, FVector(2100, 700, 500)));
	TestFalse(TEXT("Bedrock does not hug initial tunnel"), Contains(Shell, FVector(2100, 500, 500)));
	TestFalse(TEXT("Envelope is not a single global bounding box"), Contains(Result->Plan.RockDomain, FVector(2100, 850, 500)));
	Scene.Tunnel->Knots[1].Position.Y += 1000;
	auto Moved = Scene.Compute(Error);
	if (!TestTrue(TEXT("Rock and bedrock follow moved path"), Moved && Moved->bSuccess)) { AddError(Error); return false; }
	TestTrue(TEXT("Envelope follows bend beyond old boundary"), Moved->Hollow.Bounds.Max.Y > Result->Hollow.Bounds.Max.Y);
	TestEqual(TEXT("Manual authoring box untouched"), Scene.Region->RockSize, FVector(1));
	TestTrue(TEXT("No chunk budget increase"), Moved->Grid->Cells.Num() <= 128);
	for (const auto& Cell : Moved->Grid->Cells) if (Cell.Mesh) TestTrue(TEXT("Per-chunk triangle ceiling retained"), Cell.Mesh->TriangleCount() <= 30000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpatialFreedomDraftTest, "BrainMining.Spatial.Freedom.InvalidDraftAndRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpatialFreedomDraftTest::RunTest(const FString& Parameters)
{
	FFreedomWorld Scene; MiningSpatialSafeEditing::Observe(Scene.World);
	if (!TestTrue(TEXT("Initial terrain builds"), MiningSpatialSafeEditing::Apply(Scene.Tunnel, true) && Scene.Generator->bLayoutApplied)) { AddError(Scene.Generator->LastBuildStatus); return false; }
	const auto GoodProfile = Scene.Tunnel->Knots[1].Profile;
	Scene.Tunnel->Knots[1].Profile[1].Position = Scene.Tunnel->Knots[1].Profile[0].Position;
	TestTrue(TEXT("Editing invalid intermediate state is accepted as draft"), MiningSpatialSafeEditing::Apply(Scene.Tunnel));
	TestEqual(TEXT("Control point not silently restored"), Scene.Tunnel->Knots[1].Profile[1].Position, Scene.Tunnel->Knots[1].Profile[0].Position);
	TestFalse(TEXT("Old terrain is not reported as new"), Scene.Generator->bLayoutApplied);
	MiningSpatial::FRing Frame;
	TestTrue(TEXT("Invalid profile still has editable handles"), MiningSpatialDirectEditing::Frame(*Scene.Tunnel, 1, Frame));
	TestFalse(TEXT("Play validation refuses stale geometry without changing draft"), MiningSpatialSafeEditing::PrepareWorld(Scene.World));
	TestTrue(TEXT("Feedback distinguishes draft"), MiningSpatialSafeEditing::TerrainStatus().Contains(TEXT("未生效")));
	Scene.Tunnel->Knots[1].Profile = GoodProfile;
	TestTrue(TEXT("Continue editing recovers without repair mode"), MiningSpatialSafeEditing::Apply(Scene.Tunnel) && Scene.Generator->bLayoutApplied);
	for (FMiningSpatialKnot& Knot : Scene.Tunnel->Knots) Knot.Profile = MiningSpatial::Rectangle(1600, 600);
	TestTrue(TEXT("Wide entrance no longer rejected for wall size"), MiningSpatialSafeEditing::Apply(Scene.Tunnel) && Scene.Generator->bLayoutApplied);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpatialFreedomAsyncTest, "BrainMining.Spatial.Freedom.AsyncDraftLatestTarget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpatialFreedomAsyncTest::RunTest(const FString& Parameters)
{
	FFreedomWorld Scene; MiningSpatialSafeEditing::Observe(Scene.World);
	if (!TestTrue(TEXT("Baseline built"), MiningSpatialSafeEditing::Apply(Scene.Tunnel, true) && Scene.Generator->bLayoutApplied)) { AddError(Scene.Generator->LastBuildStatus); return false; }
	const FVector Original = Scene.Tunnel->Knots[1].Position;
	MiningSpatialSafeEditing::BeginInteractive();
	for (int32 Step = 1; Step <= 5; ++Step)
	{
		Scene.Tunnel->Knots[1].Position = Original + FVector(0, Step * 40, 0);
		TestTrue(TEXT("Free drag queued"), MiningSpatialSafeEditing::QueueInteractive(Scene.Tunnel));
		TestEqual(TEXT("Latest source visible immediately"), Scene.Tunnel->Knots[1].Position, Original + FVector(0, Step * 40, 0));
	}
	MiningSpatialSafeEditing::FinishInteractive();
	TestEqual(TEXT("Old worker did not overwrite latest source"), Scene.Tunnel->Knots[1].Position, Original + FVector(0, 200, 0));
	TestTrue(TEXT("Latest terrain applied"), Scene.Generator->bLayoutApplied);
	MiningSpatialSafeEditing::BeginInteractive();
	Scene.Tunnel->Knots[1].Profile[1].Position = Scene.Tunnel->Knots[1].Profile[0].Position;
	TestTrue(TEXT("Invalid shape queues without rollback"), MiningSpatialSafeEditing::QueueInteractive(Scene.Tunnel));
	MiningSpatialSafeEditing::FinishInteractive();
	TestEqual(TEXT("Rejected geometry keeps editable source"), Scene.Tunnel->Knots[1].Profile[1].Position, Scene.Tunnel->Knots[1].Profile[0].Position);
	TestFalse(TEXT("Invalid latest result marked unapplied"), Scene.Generator->bLayoutApplied);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpatialFreedomCrossRegionTest, "BrainMining.Spatial.Freedom.CrossRegionUnion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpatialFreedomCrossRegionTest::RunTest(const FString& Parameters)
{
	FFreedomWorld Scene;
	AMiningLayoutRoom* End = Scene.Spawn<AMiningLayoutRoom>(); End->RoomSize = Scene.Finish->RoomSize; End->ConnectionOrder = 2; End->bFloorAnchored = true; End->SetActorLocation(FVector(8000, 0, 0));
	AMiningSpatialRegion* Second = Scene.Spawn<AMiningSpatialRegion>(); Second->FromRoom = Scene.Finish; Second->ToRoom = End;
	AMiningSpatialTunnel* Path = Scene.Spawn<AMiningSpatialTunnel>(); Path->Region = Second; Second->MainTunnel = Path; Path->StartPortal.Room = Scene.Finish; Path->EndPortal.Room = End; Path->FitPortals();
	Scene.Generator->HollowRegions = {Scene.Region, Second};
	AMiningSpatialTunnel* Cavity = Scene.Spawn<AMiningSpatialTunnel>(); Cavity->Region = Scene.Region; Cavity->Kind = EMiningManualKind::SideRoom;
	Cavity->RoomSize = FVector(600); Cavity->SetActorLocation(FVector(5400, 0, 500));
	MiningSpatialSafeEditing::Observe(Scene.World);
	if (!TestTrue(TEXT("Overlapping automatic regions build"), MiningSpatialSafeEditing::Apply(Scene.Generator, true) && Scene.Generator->bLayoutApplied)) { AddError(Scene.Generator->LastBuildStatus); return false; }
	TestTrue(TEXT("Cross-connected regions are accessible"), Scene.Generator->GeneratedFreeAccess.Contains(true));
	for (int32 Index = 0; Index < Scene.Generator->GeneratedSegments.Num(); ++Index)
	{
		const AGeneratedMiningRock* Rock = Scene.Generator->GeneratedSegments[Index];
		for (const FMiningRockChunk& Chunk : Rock->Chunks)
			Chunk.Mesh->GetDynamicMesh()->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Mesh) { TestFalse(TEXT("Other region does not refill cavity"), Contains(Mesh, FVector(5400, 0, 500))); });
		TestFalse(TEXT("Cross-connected region is not sealed or recycled"), Scene.Generator->GeneratedSealAllowed[Index]);
		TestTrue(TEXT("No runtime budget increase"), Rock->LargestChunkTriangles <= 30000 && Rock->Chunks.Num() <= 128);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpatialFreedomMiningTest, "BrainMining.Spatial.Freedom.BufferMiningKeepsBedrock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpatialFreedomMiningTest::RunTest(const FString& Parameters)
{
	FFreedomWorld Scene; MiningSpatialSafeEditing::Observe(Scene.World);
	if (!TestTrue(TEXT("Automatic terrain builds"), MiningSpatialSafeEditing::Apply(Scene.Generator, true) && Scene.Generator->bLayoutApplied)) return false;
	AGeneratedMiningRock* Rock = Scene.Generator->GeneratedSegments[0];
	UDynamicMesh* Bedrock = Scene.Generator->GeneratedBedrock->GetDynamicMeshComponent()->GetDynamicMesh();
	UE::Geometry::FDynamicMesh3 OriginalBedrock;
	Bedrock->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Mesh) { OriginalBedrock = Mesh; });
	Rock->SetSegmentState(EMiningSegmentState::Active);
	if (!TestTrue(TEXT("Player can dig beyond the initial tunnel into buffer"), Rock->MineSphere(FVector(2100, 490, 500), 240))) { AddError(Rock->LastStatus); return false; }
	for (const FMiningRockChunk& Chunk : Rock->Chunks)
		Chunk.Mesh->GetDynamicMesh()->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Mesh)
		{
			TestFalse(TEXT("Excavation actually removes buffer rock"), Contains(Mesh, FVector(2100, 500, 500)));
			TestTrue(TEXT("Remaining mining mesh stays closed"), Mesh.TriangleCount() == 0 || Mesh.IsClosed());
		});
	Bedrock->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Mesh)
	{
		TestTrue(TEXT("Outer bedrock never moves or erodes with mining"), Mesh.IsSameAs(OriginalBedrock, UE::Geometry::FDynamicMesh3::FSameAsOptions()));
		TestTrue(TEXT("Bedrock still blocks the dig radius outside the buffer"), Contains(Mesh, FVector(2100, 700, 500)));
	});
	TestTrue(TEXT("Mining keeps original triangle budget"), Rock->LargestChunkTriangles <= 30000);
	return true;
}
#endif
