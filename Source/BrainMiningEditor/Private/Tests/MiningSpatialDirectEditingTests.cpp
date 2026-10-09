#include "MiningSpatialDirectEditing.h"
#include "MiningSpatialSafeEditing.h"
#include "MiningSpatialOverview.h"
#include "MiningSpatialVisualizer.h"
#include "MiningLayoutRoom.h"
#include "MiningTunnelGenerator.h"
#include "GeneratedMiningRock.h"
#include "DynamicMeshActor.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	using namespace MiningSpatialDirectEditing;
	struct FDirectWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
		FDirectWorld() { GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World); Reset(); AutoSmooth() = true; }
		~FDirectWorld() { Reset(); MiningSpatialSafeEditing::Reset(); FMiningSpatialSelection::Select(nullptr); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
		template<typename ActorType> ActorType* Spawn()
		{
			FActorSpawnParameters Parameters; Parameters.ObjectFlags |= RF_Transactional;
			return World->SpawnActor<ActorType>(Parameters);
		}
	};
	bool IsValidShape(AMiningSpatialTunnel* Tunnel)
	{
		UE::Geometry::FDynamicMesh3 Mesh; TArray<MiningSpatial::FRing> Rings; FString Error;
		return MiningSpatial::Shape(*Tunnel, Tunnel->Region ? Tunnel->Region->CurveTolerance : 8, Mesh, Rings, Error);
	}
	FBox2D ProfileBounds(const FMiningSpatialKnot& Knot)
	{
		TArray<FVector2D> Points; FString Error; MiningManual::Sample(Knot.Profile, true, 2, Points, Error); return FBox2D(Points);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDirectPathTest, "BrainMining.Spatial.Direct.PathSmoothAndConstraints", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDirectPathTest::RunTest(const FString& Parameters)
{
	FDirectWorld Scene;
	AMiningSpatialRegion* Region = Scene.Spawn<AMiningSpatialRegion>(); Region->RockSize = FVector(6000);
	Region->bAutomaticBedrock = false; Region->bFreeEditing = false;
	AMiningSpatialTunnel* Tunnel = Scene.Spawn<AMiningSpatialTunnel>(); Tunnel->Region = Region;
	MiningSpatial::Insert(Tunnel->Knots, 0, 0.5);
	const FTransform OriginalTransform = Tunnel->GetActorTransform();
	const FVector Original = Tunnel->Knots[1].Position;
	const auto OriginalProfile = Tunnel->Knots[1].Profile;
	MiningSpatialSafeEditing::Observe(Scene.World);
	FHandle Handle{Tunnel, EHandle::Knot, 1}; Select(Handle); Begin();
	TestTrue(TEXT("Ordinary node drag accepted"), Translate(Handle, FVector(0, 60, 35))); End();
	TestTrue(TEXT("Node follows requested displacement"), Tunnel->Knots[1].Position.Equals(Original + FVector(0, 60, 35), 0.01));
	TestTrue(TEXT("Auto smoothing aligns both tangents"), Tunnel->Knots[1].Arrive.Equals(-Tunnel->Knots[1].Leave));
	TestTrue(TEXT("Actor itself never moves when dragging a node"), Tunnel->GetActorTransform().Equals(OriginalTransform));
	for (int32 Point = 0; Point < OriginalProfile.Num(); ++Point) TestTrue(TEXT("Smoothing preserves every boundary anchor"), OriginalProfile[Point].Position.Equals(Tunnel->Knots[1].Profile[Point].Position));
	const FVector BeforeHugeMove = Tunnel->Knots[1].Position;
	TestTrue(TEXT("Oversized drag accepts a safe partial step"), Translate(Handle, FVector(0, 100000, 0)));
	TestTrue(TEXT("Feedback explicitly reports the limit"), MiningSpatialSafeEditing::Issue().bLimited && !MiningSpatialSafeEditing::Issue().Reason.IsEmpty());
	TestTrue(TEXT("Huge drag cannot leave rock bounds"), Tunnel->Knots[1].Position.Y > BeforeHugeMove.Y && Tunnel->Knots[1].Position.Y < 3000);
	TestTrue(TEXT("Accepted surface is still closed and nonintersecting"), IsValidShape(Tunnel));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDirectBoundaryTest, "BrainMining.Spatial.Direct.IndependentBoundaryAndPlane", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDirectBoundaryTest::RunTest(const FString& Parameters)
{
	FDirectWorld Scene;
	AMiningSpatialTunnel* Tunnel = Scene.Spawn<AMiningSpatialTunnel>();
	Tunnel->SetActorRotation(FRotator(25, 38, 12)); Tunnel->Knots[0].Roll = Tunnel->Knots[1].Roll = 20;
	MiningSpatialSafeEditing::Observe(Scene.World);
	MiningSpatial::FRing Ring; TestTrue(TEXT("Can locate the arbitrary 3D section plane"), Frame(*Tunnel, 0, Ring));
	const FBox2D Before = ProfileBounds(Tunnel->Knots[0]);
	const FBox2D OtherBefore = ProfileBounds(Tunnel->Knots[1]);
	TestTrue(TEXT("Can pull only right side"), Translate({Tunnel, EHandle::SectionSide, 0, 0}, Ring.Right * 40));
	const FBox2D After = ProfileBounds(Tunnel->Knots[0]);
	TestTrue(TEXT("Opposite boundary stays fixed"), FMath::IsNearlyEqual(After.Min.X, Before.Min.X, 0.01));
	TestTrue(TEXT("Right boundary follows hand"), FMath::IsNearlyEqual(After.Max.X, Before.Max.X + 40, 0.01));
	TestTrue(TEXT("No unwanted vertical resize"), FMath::IsNearlyEqual(After.Min.Y, Before.Min.Y, 0.01) && FMath::IsNearlyEqual(After.Max.Y, Before.Max.Y, 0.01));
	TestTrue(TEXT("Other section is not silently resized"), ProfileBounds(Tunnel->Knots[1]).Equals(OtherBefore, 0.01));
	const FVector2D Anchor = Tunnel->Knots[0].Profile[0].Position;
	TestTrue(TEXT("Can directly draw a boundary point"), Translate({Tunnel, EHandle::Profile, 0, 0}, Ring.Right * 15 + Ring.Up * 10 + Ring.Tangent * 90));
	TestTrue(TEXT("Point stays in its 3D section plane"), Tunnel->Knots[0].Profile[0].Position.Equals(Anchor + FVector2D(15, 10), 0.01));
	TestTrue(TEXT("Boundary edits retain valid geometry"), IsValidShape(Tunnel));
	const uint32 BeforeLock = Tunnel->GetGeometryHash();
	Tunnel->Knots[0].Profile[0].bLocked = true; MiningSpatialSafeEditing::Apply(Tunnel);
	TestFalse(TEXT("Locked boundary never silently moves"), Translate({Tunnel, EHandle::Profile, 0, 0}, Ring.Right * 20));
	TestTrue(TEXT("Lock explanation is visible"), Hint().Contains(TEXT("锁定")));
	Tunnel->Knots[0].Profile[0].bLocked = false; MiningSpatialSafeEditing::Apply(Tunnel);
	TestEqual(TEXT("Rejected lock drag did not alter geometry"), Tunnel->GetGeometryHash(), BeforeLock);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDirectRoomTest, "BrainMining.Spatial.Direct.RoomFacesAndFloorAnchor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDirectRoomTest::RunTest(const FString& Parameters)
{
	FDirectWorld Scene;
	AMiningLayoutRoom* Main = Scene.Spawn<AMiningLayoutRoom>(); Main->bFloorAnchored = true;
	AMiningSpatialTunnel* Side = Scene.Spawn<AMiningSpatialTunnel>(); Side->Kind = EMiningManualKind::SideRoom; Side->SetActorRotation(FRotator(30, 48, 17));
	MiningSpatialSafeEditing::Observe(Scene.World);
	const FBox Before = Main->GetRoomBounds();
	TestTrue(TEXT("Main room top face resizes without actor scaling"), Translate({Main, EHandle::RoomFace, 0, 4}, FVector(0, 0, 100)));
	TestTrue(TEXT("Main room floor stays fixed"), FMath::IsNearlyEqual(Main->GetRoomBounds().Min.Z, Before.Min.Z));
	TestTrue(TEXT("Top is at requested height"), FMath::IsNearlyEqual(Main->GetRoomBounds().Max.Z, Before.Max.Z + 100));
	TestTrue(TEXT("Main room side can be resized independently"), Translate({Main, EHandle::RoomFace, 0, 1}, FVector(-80, 0, 0)));
	TestTrue(TEXT("Opposite main room face does not move"), FMath::IsNearlyEqual(Main->GetRoomBounds().Max.X, Before.Max.X));
	TestTrue(TEXT("Room actor scale remains one"), Main->GetActorScale3D().Equals(FVector::OneVector));
	FVector FixedBefore, FixedAfter; Location({Side, EHandle::RoomFace, 0, 3}, FixedBefore);
	const FVector FaceDirection = Side->GetActorTransform().TransformVectorNoScale(FVector::RightVector);
	TestTrue(TEXT("Rotated branch room uses local face normal"), Translate({Side, EHandle::RoomFace, 0, 2}, FaceDirection * 60));
	Location({Side, EHandle::RoomFace, 0, 3}, FixedAfter);
	TestTrue(TEXT("Opposite rotated face stays fixed in world space"), FixedAfter.Equals(FixedBefore, 0.01));
	TestTrue(TEXT("Rotated branch room remains valid"), IsValidShape(Side));
	TArray<FControl> Items; Controls(Main, Items); TestEqual(TEXT("Six self-explanatory room controls"), Items.Num(), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDirectPortalTest, "BrainMining.Spatial.Direct.PortalSlidesOnWall", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDirectPortalTest::RunTest(const FString& Parameters)
{
	FDirectWorld Scene;
	AMiningLayoutRoom* Room = Scene.Spawn<AMiningLayoutRoom>(); Room->RoomSize = FVector(1000, 1400, 1000); Room->SetActorLocation(FVector(-1200, 0, 0));
	AMiningSpatialTunnel* Tunnel = Scene.Spawn<AMiningSpatialTunnel>(); Tunnel->StartPortal.Room = Room;
	MiningSpatialSafeEditing::Observe(Scene.World);
	const FVector Stored = Tunnel->Knots[0].Position;
	FVector Before, After; Location({Tunnel, EHandle::Knot, 0}, Before);
	TestTrue(TEXT("Bound entry can now slide along its wall"), Translate({Tunnel, EHandle::Knot, 0}, FVector(0, 40, 25)));
	Location({Tunnel, EHandle::Knot, 0}, After);
	TestTrue(TEXT("Portal tracks requested in-wall movement"), After.Equals(Before + FVector(0, 40, 25), 0.01));
	TestTrue(TEXT("Bound endpoint does not change hidden stored position"), Tunnel->Knots[0].Position.Equals(Stored));
	TestFalse(TEXT("Cannot detach portal by dragging through its wall"), Translate({Tunnel, EHandle::Knot, 0}, FVector(100, 0, 0)));
	TestTrue(TEXT("Portal wall constraint is explained"), Hint().Contains(TEXT("沿墙面")));
	TestTrue(TEXT("Excessive in-wall movement is continuously constrained"), Translate({Tunnel, EHandle::Knot, 0}, FVector(0, 10000, 0)));
	TestTrue(TEXT("Portal never exceeds usable room face"), Tunnel->StartPortal.Offset.X < 450 && Tunnel->StartPortal.Offset.X > 40);
	TestTrue(TEXT("Bound entry remains valid"), IsValidShape(Tunnel));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDirectUndoTest, "BrainMining.Spatial.Direct.PenInsertUndoAndCancel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDirectUndoTest::RunTest(const FString& Parameters)
{
	FDirectWorld Scene;
	AMiningSpatialTunnel* Tunnel = Scene.Spawn<AMiningSpatialTunnel>(); MiningSpatialSafeEditing::Observe(Scene.World);
	const uint32 Before = Tunnel->GetGeometryHash();
	FHandle Handle{Tunnel, EHandle::InsertKnot, 0}; Select(Handle); Begin();
	TestTrue(TEXT("Pen inserts at visible segment midpoint"), Insert(Handle)); Select(Handle);
	TestTrue(TEXT("New point immediately drags without panel edits"), Translate(Handle, FVector(0, 40, 20))); End();
	const uint32 After = Tunnel->GetGeometryHash();
	TestEqual(TEXT("Insertion creates one point"), Tunnel->Knots.Num(), 3);
	GEditor->UndoTransaction(); MiningSpatialSafeEditing::Apply(Tunnel);
	TestEqual(TEXT("One undo restores both insertion and drag"), Tunnel->GetGeometryHash(), Before);
	GEditor->RedoTransaction(); MiningSpatialSafeEditing::Apply(Tunnel);
	TestEqual(TEXT("Redo restores complete legal edit"), Tunnel->GetGeometryHash(), After);
	Select(Handle); Begin(); Translate(Handle, FVector(0, 20, 0)); End(true);
	TestEqual(TEXT("Escape restores start of drag"), Tunnel->GetGeometryHash(), After);
	FHandle Profile{Tunnel, EHandle::InsertProfile, 1, 0}; Select(Profile); Begin();
	TestTrue(TEXT("Pen can add boundary points in 3D"), Insert(Profile)); End();
	TestEqual(TEXT("One boundary point inserted"), Tunnel->Knots[1].Profile.Num(), 5);
	TestTrue(TEXT("Boundary point can be deleted without deleting actor"), Delete(Profile));
	TestEqual(TEXT("Boundary delete restores four points"), Tunnel->Knots[1].Profile.Num(), 4);
	TestTrue(TEXT("All topology commands remain legal"), IsValidShape(Tunnel));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDirectPreviewTest, "BrainMining.Spatial.Direct.PreviewAndContext", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDirectPreviewTest::RunTest(const FString& Parameters)
{
	FDirectWorld Scene;
	AMiningSpatialTunnel* Tunnel = Scene.Spawn<AMiningSpatialTunnel>();
	AMiningLayoutRoom* Room = Scene.Spawn<AMiningLayoutRoom>(); Room->SetActorLocation(FVector(-1200, 0, 0));
	const uint32 Original = Tunnel->GetGeometryHash();
	SetTool(ETool::Path); TArray<FControl> Items; Controls(Tunnel, Items);
	TestEqual(TEXT("Simple path shows just two anchors and an insertion point"), Items.Num(), 3);
	SetTool(ETool::Boundary); Controls(Tunnel, Items);
	TestTrue(TEXT("Boundary tool exposes independently draggable sides"), Items.ContainsByPredicate([](const FControl& Item) { return Item.Handle.Kind == EHandle::SectionSide; }));
	SetTool(ETool::Move); Controls(Tunnel, Items); TestTrue(TEXT("Moving whole space hides local controls"), Items.IsEmpty());
	MiningSpatialOverview::FScene Preview; Preview.Refresh(Scene.World);
	for (const auto& Entry : Preview.GetEntries())
	{
		TestFalse(TEXT("Matte preview has actual surfaces rather than only boxes"), Entry.Triangles.IsEmpty());
		for (const FIntVector& Triangle : Entry.Triangles) TestTrue(TEXT("Preview triangle indices stay valid"), Entry.Vertices.IsValidIndex(Triangle.X) && Entry.Vertices.IsValidIndex(Triangle.Y) && Entry.Vertices.IsValidIndex(Triangle.Z));
	}
	TestEqual(TEXT("Changing tool or preview never rewrites design"), Tunnel->GetGeometryHash(), Original);
	SetTool(ETool::Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDirectWorkflowTest, "BrainMining.Spatial.Direct.TwoRoomsBendAndCavity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDirectWorkflowTest::RunTest(const FString& Parameters)
{
	FDirectWorld Scene;
	AMiningLayoutRoom* Start = Scene.Spawn<AMiningLayoutRoom>();
	AMiningLayoutRoom* Finish = Scene.Spawn<AMiningLayoutRoom>();
	Start->RoomSize = Finish->RoomSize = FVector(1200, 1200, 1000); Start->bStartRoom = true; Finish->ConnectionOrder = 1;
	Start->bFloorAnchored = Finish->bFloorAnchored = true; Finish->SetActorLocation(FVector(4200, 0, -100));
	AMiningSpatialRegion* Region = Scene.Spawn<AMiningSpatialRegion>(); Region->SetActorLocation(FVector(2100, 0, 500)); Region->RockSize = FVector(3200, 3200, 2400); Region->FromRoom = Start; Region->ToRoom = Finish;
	Region->bAutomaticBedrock = false; Region->bFreeEditing = false;
	AMiningSpatialTunnel* Tunnel = Scene.Spawn<AMiningSpatialTunnel>(); Tunnel->Region = Region; Region->MainTunnel = Tunnel; Tunnel->StartPortal.Room = Start; Tunnel->EndPortal.Room = Finish; Tunnel->FitPortals();
	AMiningTunnelGenerator* Generator = Scene.Spawn<AMiningTunnelGenerator>(); Generator->bHollowRegionPrototype = true; Generator->bDirectRoomFaces = true; Generator->bAutomaticChunks = true; Generator->HollowRegion = Region; Generator->bAutoRebuild = false;
	if (!TestTrue(TEXT("Two-room scene builds"), Generator->RebuildTunnels())) { AddError(Generator->LastBuildStatus); return false; }
	const uint32 LoadedGeometryHash = Tunnel->GetGeometryHash();
	Generator->RequestRebuild();
	MiningSpatialSafeEditing::Observe(Scene.World);
	TestTrue(TEXT("Loaded valid terrain requests synchronization instead of repair"), MiningSpatialSafeEditing::Status().Contains(TEXT("等待同步")));
	TestTrue(TEXT("Successful 128-chunk limit text is not a budget error"), MiningSpatialSafeEditing::Issue().Reason.IsEmpty());
	TestTrue(TEXT("Pending terrain explains how to synchronize"), MiningSpatialSafeEditing::TerrainStatus().Contains(TEXT("等待同步")));
	TestTrue(TEXT("Loaded terrain synchronizes within the existing budgets"), MiningSpatialSafeEditing::PrepareWorld(Scene.World));
	TestEqual(TEXT("Synchronizing loaded terrain preserves authored shape"), Tunnel->GetGeometryHash(), LoadedGeometryHash);
	FHandle Handle{Tunnel, EHandle::InsertKnot, 0}; Select(Handle); Begin();
	TestTrue(TEXT("Add the bend directly"), Insert(Handle)); Select(Handle);
	TestTrue(TEXT("Pull bend sideways and downward"), Translate(Handle, FVector(0, 100, -50))); End();
	TestTrue(TEXT("Every accepted bend applies real terrain"), Generator->bLayoutApplied);
	TestTrue(TEXT("Enlarge destination room from its top face"), Translate({Finish, EHandle::RoomFace, 0, 4}, FVector(0, 0, 100)));
	TestTrue(TEXT("Room and attached entry rebuild together"), Generator->bLayoutApplied && IsValidShape(Tunnel));
	AMiningSpatialTunnel* Cavity = Scene.Spawn<AMiningSpatialTunnel>(); Cavity->Region = Region; Cavity->Kind = EMiningManualKind::Cavity; Cavity->SetActorLocation(FVector(2100, 450, 420));
	Cavity->Knots[0].Position = FVector(-150, 0, 0); Cavity->Knots[1].Position = FVector(150, 0, 0); Cavity->Knots[0].Leave = FVector(100, 0, 0); Cavity->Knots[1].Arrive = FVector(-100, 0, 0);
	for (auto& Knot : Cavity->Knots) Knot.Profile = MiningSpatial::Ellipse(800, 700);
	TestTrue(TEXT("Add editable side cavern without changing main tunnel"), MiningSpatialSafeEditing::Add(Cavity));
	MiningSpatial::FRing Ring; Frame(*Cavity, 0, Ring);
	TestTrue(TEXT("Pull cavern boundary directly"), Translate({Cavity, EHandle::SectionSide, 0, 0}, Ring.Right * 25));
	TestTrue(TEXT("Cavern applies to real terrain"), Generator->bLayoutApplied);
	Generator->ShowGameGeometry();
	const bool bBedrockWasHidden = Generator->GeneratedBedrock->IsTemporarilyHiddenInEditor();
	const uint32 AuthoredHash = Tunnel->GetGeometryHash();
	TArray<AActor*> PreviewActors; MiningSpatialOverview::GetPreviewTerrain(Scene.World, PreviewActors);
	TestTrue(TEXT("Preview isolates the generated spatial bedrock"), PreviewActors.Contains(Generator->GeneratedBedrock.Get()));
	TestFalse(TEXT("Preview does not hide editable source spaces"), PreviewActors.Contains(Tunnel) || PreviewActors.Contains(Start) || PreviewActors.Contains(Finish));
	TestEqual(TEXT("View-only preview never changes original actor visibility"), Generator->GeneratedBedrock->IsTemporarilyHiddenInEditor(), bBedrockWasHidden);
	TestTrue(TEXT("Preview does not change the generator's display mode"), Generator->PreviewMode == EMiningPreviewMode::Game);
	TestEqual(TEXT("View-only preview preserves authored geometry"), Tunnel->GetGeometryHash(), AuthoredHash);
	TestEqual(TEXT("Triangle budget is unchanged"), Generator->SegmentTriangleBudget, 30000);
	for (AGeneratedMiningRock* Segment : Generator->GeneratedSegments)
	{
		TestTrue(TEXT("Every chunk stays inside original budget"), Segment->LargestChunkTriangles <= 30000);
		TestTrue(TEXT("Region chunk ceiling is unchanged"), Segment->Chunks.Num() <= 128);
	}
	return true;
}
#endif
