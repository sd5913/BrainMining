#include "MiningSpatialSafeEditing.h"
#include "MiningSpatialOverview.h"
#include "MiningSpatialVisualizer.h"
#include "MiningSpatialGeometry.h"
#include "MiningTunnelGenerator.h"
#include "MiningLayoutRoom.h"
#include "GeneratedMiningRock.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/ObjectSaveContext.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "FileHelpers.h"
#include "PlayInEditorDataTypes.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "UObject/Linker.h"
#include "UObject/UnrealType.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorViewportClient.h"
#include "ComponentVisualizerManager.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	struct FSafeWorld
	{
		UWorld* World;
		FSafeWorld(UPackage* Package = nullptr) : World(UWorld::CreateWorld(EWorldType::Editor, false, NAME_None, Package)) { GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World); }
		~FSafeWorld() { MiningSpatialSafeEditing::Reset(); FMiningSpatialSelection::Select(nullptr); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
	};
	bool ShapeValid(AMiningSpatialTunnel* Tunnel)
	{
		UE::Geometry::FDynamicMesh3 Mesh; TArray<MiningSpatial::FRing> Rings; FString Error;
		return MiningSpatial::Shape(*Tunnel, IsValid(Tunnel->Region) ? Tunnel->Region->CurveTolerance : 8, Mesh, Rings, Error);
	}
	class FSpatialTestViewport : public FEditorViewportClient
	{
	public:
		UWorld* TestWorld;
		explicit FSpatialTestViewport(UWorld* World) : FEditorViewportClient(nullptr), TestWorld(World) {}
		virtual UWorld* GetWorld() const override { return TestWorld; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningSpatialWidgetTest, "BrainMining.Spatial.SafeEditing.NodeWidgetTargets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningSpatialWidgetTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> AdvancedControls(MiningSpatialOverview::Options().bDirectManipulation, false);
	FSafeWorld Scene;
	FSpatialTestViewport Client(Scene.World);
	ON_SCOPE_EXIT { FMiningSpatialSelection::Select(nullptr); };
	FActorSpawnParameters Spawn; Spawn.ObjectFlags |= RF_Transactional;
	AMiningSpatialTunnel* Tunnel = Scene.World->SpawnActor<AMiningSpatialTunnel>(Spawn);
	Tunnel->SetActorLocation(FVector(2400, 600, 100));
	MiningSpatial::Insert(Tunnel->Knots, 0, 0.5);
	MiningSpatialSafeEditing::Observe(Scene.World);
	const FTransform ActorTransform = Tunnel->GetActorTransform();
	const FVector OriginalPoint = Tunnel->Knots[1].Position;
	FMiningSpatialSelection::Select(Tunnel, 1, 0, &Client);
	FVector WidgetLocation;
	TestTrue(TEXT("Programmatic node selection activates component widget"), GUnrealEd->ComponentVisManager.GetWidgetLocation(&Client, WidgetLocation));
	TestTrue(TEXT("Widget sits on node, not tunnel actor origin"), WidgetLocation.Equals(ActorTransform.TransformPosition(OriginalPoint)) && !WidgetLocation.Equals(ActorTransform.GetLocation()));
	FVector Translate(0, 40, 0), Scale = FVector::ZeroVector; FRotator Rotate = FRotator::ZeroRotator;
	Client.SetCurrentWidgetAxis(EAxisList::Y);
	GUnrealEd->ComponentVisManager.TrackingStarted(&Client);
	TestTrue(TEXT("Native widget delta is consumed by node editor"), GUnrealEd->ComponentVisManager.HandleInputDelta(&Client, nullptr, Translate, Rotate, Scale));
	GUnrealEd->ComponentVisManager.TrackingStopped(&Client, true);
	TestTrue(TEXT("Only the selected node moves"), Tunnel->Knots[1].Position.Equals(OriginalPoint + Translate));
	TestTrue(TEXT("Node drag never translates the whole tunnel actor"), Tunnel->GetActorTransform().Equals(ActorTransform));
	const FVector PointBeforeHandle = Tunnel->Knots[1].Position, HandleBefore = Tunnel->Knots[1].Leave;
	FMiningSpatialSelection::Select(Tunnel, 1, 2, &Client);
	TestTrue(TEXT("Handle selection updates widget position"), GUnrealEd->ComponentVisManager.GetWidgetLocation(&Client, WidgetLocation) && WidgetLocation.Equals(ActorTransform.TransformPosition(PointBeforeHandle + HandleBefore)));
	Translate = FVector(20, 0, 0);
	Client.SetCurrentWidgetAxis(EAxisList::X);
	GUnrealEd->ComponentVisManager.TrackingStarted(&Client);
	GUnrealEd->ComponentVisManager.HandleInputDelta(&Client, nullptr, Translate, Rotate, Scale);
	GUnrealEd->ComponentVisManager.TrackingStopped(&Client, true);
	TestTrue(TEXT("Tangent drag edits only the selected handle"), Tunnel->Knots[1].Leave.Equals(HandleBefore + Translate) && Tunnel->Knots[1].Position.Equals(PointBeforeHandle));
	FMiningSpatialSelection::Select(Tunnel, 2, 0, &Client);
	TestTrue(TEXT("Next node selection retargets widget without viewport click"), GUnrealEd->ComponentVisManager.GetWidgetLocation(&Client, WidgetLocation) && WidgetLocation.Equals(ActorTransform.TransformPosition(Tunnel->Knots[2].Position)));
	Tunnel->Knots[1].Profile.SetNum(2);
	FMiningSpatialSelection::Select(Tunnel, 1, 0, &Client);
	TestTrue(TEXT("Invalid source still exposes an editable node widget"), GUnrealEd->ComponentVisManager.GetWidgetLocation(&Client, WidgetLocation));
	AMiningSpatialTunnel* Room = Scene.World->SpawnActor<AMiningSpatialTunnel>(); Room->Kind = EMiningManualKind::SideRoom;
	FMiningSpatialSelection::Select(Room, 0, 0, &Client);
	TestFalse(TEXT("Side rooms retain normal actor transforms, not node widgets"), GUnrealEd->ComponentVisManager.IsActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningLegacyRepairTest, "BrainMining.Spatial.SafeEditing.LegacyRepairSession", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningLegacyRepairTest::RunTest(const FString& Parameters)
{
	FSafeWorld Scene;
	FActorSpawnParameters Spawn; Spawn.ObjectFlags |= RF_Transactional;
	AMiningSpatialTunnel* Tunnel = Scene.World->SpawnActor<AMiningSpatialTunnel>(Spawn);
	Tunnel->Knots[0].Leave.X = -1500;
	const uint32 Original = Tunnel->GetGeometryHash();
	MiningSpatialSafeEditing::Observe(Scene.World); MiningSpatialSafeEditing::Apply(Tunnel, true);
	TestFalse(TEXT("Repair mode never starts implicitly"), MiningSpatialSafeEditing::IsRepairing());
	TestTrue(TEXT("Legacy invalid source can explicitly enter repair"), MiningSpatialSafeEditing::BeginRepair());
	for (double Distance : {-1450.0, -1300.0})
	{
		Tunnel->Knots[0].Leave.X = Distance;
		TestFalse(TEXT("Partial repair is not reported as applied terrain"), MiningSpatialSafeEditing::Apply(Tunnel));
		TestEqual(TEXT("Partial repair drag is retained instead of snapping back"), Tunnel->Knots[0].Leave.X, Distance);
	}
	TestTrue(TEXT("Unfinished repair stays visibly active"), MiningSpatialSafeEditing::IsRepairing());
	TestTrue(TEXT("Cancel restores original legacy source"), MiningSpatialSafeEditing::CancelRepair());
	TestEqual(TEXT("Cancel recovers complete authoring snapshot"), Tunnel->GetGeometryHash(), Original);
	TestFalse(TEXT("Cancel leaves repair mode"), MiningSpatialSafeEditing::IsRepairing());
	TestTrue(TEXT("Repair can be resumed explicitly"), MiningSpatialSafeEditing::BeginRepair());
	{
		FScopedTransaction Transaction(FText::FromString(TEXT("Repair partial movement"))); Tunnel->Modify();
		Tunnel->Knots[0].Leave.X = -1400; Tunnel->Changed();
	}
	GEditor->UndoTransaction(); MiningSpatialSafeEditing::Apply(Tunnel);
	TestEqual(TEXT("Undo works on retained invalid repair steps"), Tunnel->GetGeometryHash(), Original);
	GEditor->RedoTransaction(); MiningSpatialSafeEditing::Apply(Tunnel);
	TestEqual(TEXT("Redo restores retained repair step"), Tunnel->Knots[0].Leave.X, -1400.0);
	Tunnel->Knots[0].Leave = FVector(200, 0, 0);
	TestTrue(TEXT("A full legal repair is accepted"), MiningSpatialSafeEditing::Apply(Tunnel));
	TestFalse(TEXT("Successful repair automatically restores constrained editing"), MiningSpatialSafeEditing::IsRepairing());
	Tunnel->Knots[0].Leave.X = -1500;
	MiningSpatialSafeEditing::Apply(Tunnel);
	TestTrue(TEXT("Subsequent normal drag cannot recreate invalid source"), ShapeValid(Tunnel));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningSafeDragTest, "BrainMining.Spatial.SafeEditing.ContinuousConstraints", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningSafeDragTest::RunTest(const FString& Parameters)
{
	FSafeWorld Scene;
	AMiningSpatialTunnel* Tunnel = Scene.World->SpawnActor<AMiningSpatialTunnel>();
	MiningSpatialSafeEditing::Observe(Scene.World);
	const FVector OriginalHandle = Tunnel->Knots[0].Leave;
	Tunnel->Knots[0].Leave = FVector(-2000, 0, 0);
	TestTrue(TEXT("Invalid drag is clamped to an accepted safe step"), MiningSpatialSafeEditing::Apply(Tunnel));
	TestTrue(TEXT("Clamped handle moves in requested direction"), Tunnel->Knots[0].Leave.X < OriginalHandle.X && Tunnel->Knots[0].Leave.X > 0);
	TestTrue(TEXT("Accepted result has closed nonintersecting geometry"), ShapeValid(Tunnel));
	TestTrue(TEXT("Limit feedback distinguishes clamp from full acceptance"), MiningSpatialSafeEditing::Issue().bLimited);
	Tunnel->Knots[0].Leave.X += 30;
	Tunnel->Changed();
	TestTrue(TEXT("Dragging away from a constraint is immediately possible"), ShapeValid(Tunnel));
	TestTrue(TEXT("Successful edit clears old limit"), MiningSpatialSafeEditing::Issue().Reason.IsEmpty());
	const uint32 Before = Tunnel->GetGeometryHash();
	Tunnel->Knots[0].Position.X = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("Nonfinite drag rejected"), MiningSpatialSafeEditing::Apply(Tunnel));
	TestEqual(TEXT("Nonfinite candidate cannot leak into source"), Tunnel->GetGeometryHash(), Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningSafeSectionTest, "BrainMining.Spatial.SafeEditing.SectionAndTopology", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningSafeSectionTest::RunTest(const FString& Parameters)
{
	FSafeWorld Scene;
	AMiningSpatialTunnel* Tunnel = Scene.World->SpawnActor<AMiningSpatialTunnel>();
	MiningSpatialSafeEditing::Observe(Scene.World);
	Tunnel->Knots[0].Profile[0].Position = FVector2D(500, 200);
	MiningSpatialSafeEditing::Apply(Tunnel);
	TestTrue(TEXT("Crossing profile point cannot leave an invalid contour"), ShapeValid(Tunnel));
	TestTrue(TEXT("Crossing profile point stops short of requested position"), !Tunnel->Knots[0].Profile[0].Position.Equals(FVector2D(500, 200)));
	const uint32 Before = Tunnel->GetGeometryHash();
	Tunnel->Knots[0].Profile.SetNum(2);
	TestFalse(TEXT("Invalid topology command is rejected rather than blended"), MiningSpatialSafeEditing::Apply(Tunnel));
	TestEqual(TEXT("All profile data restored after rejection"), Tunnel->GetGeometryHash(), Before);
	FMiningSpatialSelection::Select(Tunnel, 0);
	TestTrue(TEXT("Bezier split inserts a safe node"), FMiningSpatialSelection::Insert());
	TestEqual(TEXT("Inserted node selected"), FMiningSpatialSelection::Knot, 1);
	TestTrue(TEXT("Inserted curve remains legal"), ShapeValid(Tunnel));
	FMiningSpatialSelection::Delete();
	TestTrue(TEXT("Delete command remains legal even if refused"), ShapeValid(Tunnel));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningSafeLegacyTest, "BrainMining.Spatial.SafeEditing.LegacyDraftAndWorldIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningSafeLegacyTest::RunTest(const FString& Parameters)
{
	FSafeWorld Scene;
	AMiningSpatialTunnel* Tunnel = Scene.World->SpawnActor<AMiningSpatialTunnel>();
	Tunnel->Knots[0].Leave.X = -1500;
	const uint32 Original = Tunnel->GetGeometryHash();
	MiningSpatialSafeEditing::Observe(Scene.World);
	TestEqual(TEXT("Opening does not silently repair a legacy draft"), Tunnel->GetGeometryHash(), Original);
	TestFalse(TEXT("Invalid legacy draft is not marked applied"), MiningSpatialSafeEditing::Apply(Tunnel, true));
	TestTrue(TEXT("Legacy problem has a finite focus target"), MiningSpatialSafeEditing::Issue().Geometry.Bounds.IsValid != 0);
	Tunnel->Knots[0].Leave = FVector(200, 0, 0);
	TestTrue(TEXT("Complete valid repair establishes a safe baseline"), MiningSpatialSafeEditing::Apply(Tunnel));
	TestTrue(TEXT("Repaired shape is valid"), ShapeValid(Tunnel));
	UWorld* Other = UWorld::CreateWorld(EWorldType::Editor, false);
	AMiningSpatialTunnel* OtherTunnel = Other->SpawnActor<AMiningSpatialTunnel>();
	OtherTunnel->Knots[0].Leave.X = -1500;
	TestFalse(TEXT("Different worlds are never intercepted by the active session"), MiningSpatialSafeEditing::Handle(OtherTunnel));
	MiningSpatialSafeEditing::Observe(Other);
	TestTrue(TEXT("Changing worlds clears old diagnostics"), MiningSpatialSafeEditing::Issue().Reason.IsEmpty());
	MiningSpatialSafeEditing::Reset(); Other->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningSafeLayoutTest, "BrainMining.Spatial.SafeEditing.AtomicTerrainAndRooms", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningSafeLayoutTest::RunTest(const FString& Parameters)
{
	FSafeWorld Scene;
	FActorSpawnParameters Spawn; Spawn.ObjectFlags |= RF_Transactional;
	AMiningLayoutRoom* Start = Scene.World->SpawnActor<AMiningLayoutRoom>(Spawn);
	AMiningLayoutRoom* End = Scene.World->SpawnActor<AMiningLayoutRoom>(Spawn);
	Start->RoomSize = End->RoomSize = FVector(1200, 1000, 1000); Start->bStartRoom = true; End->ConnectionOrder = 1;
	Start->bFloorAnchored = End->bFloorAnchored = true; End->SetActorLocation(FVector(4200, 0, 0));
	AMiningSpatialRegion* Region = Scene.World->SpawnActor<AMiningSpatialRegion>(Spawn);
	Region->bAutomaticBedrock = false; Region->bFreeEditing = false;
	Region->SetActorLocation(FVector(2100, 0, 500)); Region->RockSize = FVector(3000, 1600, 1600); Region->FromRoom = Start; Region->ToRoom = End;
	AMiningSpatialTunnel* Tunnel = Scene.World->SpawnActor<AMiningSpatialTunnel>(Spawn);
	Tunnel->Region = Region; Region->MainTunnel = Tunnel; Tunnel->StartPortal.Room = Start; Tunnel->EndPortal.Room = End; Tunnel->FitPortals();
	AMiningTunnelGenerator* Generator = Scene.World->SpawnActor<AMiningTunnelGenerator>(Spawn);
	Generator->bHollowRegionPrototype = true; Generator->bDirectRoomFaces = true; Generator->bAutomaticChunks = true; Generator->HollowRegion = Region; Generator->bAutoRebuild = false;
	if (!TestTrue(TEXT("Fixture generates initial legal terrain"), Generator->RebuildTunnels())) { AddError(Generator->LastBuildStatus); return false; }
	MiningSpatialSafeEditing::Observe(Scene.World);
	const int32 BeforeRevision = Generator->BuildRevision;
	Tunnel->Knots[0].SectionScale = FVector2D(1.1, 1.1);
	Tunnel->Changed();
	TestTrue(TEXT("Accepted edit rebuilds collision and terrain synchronously"), Generator->bLayoutApplied && Generator->BuildRevision > BeforeRevision);
	TestTrue(TEXT("Fixed triangle budget preserved"), Generator->SegmentTriangleBudget == 30000);
	for (AGeneratedMiningRock* Segment : Generator->GeneratedSegments) TestTrue(TEXT("Generated chunk budget preserved"), Segment->LargestChunkTriangles <= 30000);
	Start->RoomSize = FVector(1200, 260, 260);
	FPropertyChangedEvent Change(AMiningLayoutRoom::StaticClass()->FindPropertyByName(TEXT("RoomSize")));
	Start->PostEditChangeProperty(Change);
	TestTrue(TEXT("Room shrink is constrained by its portal"), Start->RoomSize.Y > 500 && Start->RoomSize.Z > 500);
	TestTrue(TEXT("Room constraint retains applied terrain"), Generator->bLayoutApplied);
	const FVector BeforeRock = Region->RockSize;
	Region->RockSize = FVector(510, 510, 510);
	MiningSpatialSafeEditing::Apply(Region);
	TestTrue(TEXT("Rock bounds cannot clip valid tunnel"), Region->RockSize.X > 2900 && Region->RockSize.X <= BeforeRock.X);
	TestTrue(TEXT("Rock bounds rollback keeps generation applied"), Generator->bLayoutApplied);
	const auto Segments = Generator->GeneratedSegments;
	const uint32 SourceHash = Tunnel->GetGeometryHash();
	Tunnel->Knots.RemoveAt(0);
	TestFalse(TEXT("Breaking main tunnel is refused"), MiningSpatialSafeEditing::Apply(Tunnel));
	TestEqual(TEXT("Failed command restores original source"), Tunnel->GetGeometryHash(), SourceHash);
	TestTrue(TEXT("Failed command preserves exact live segment objects"), Generator->GeneratedSegments == Segments);
	TestTrue(TEXT("Failed command does not leave NOT APPLIED state behind"), Generator->bLayoutApplied);
	Tunnel->Region = nullptr;
	TestFalse(TEXT("Removing main ownership cannot bypass the guard"), MiningSpatialSafeEditing::Apply(Tunnel));
	TestTrue(TEXT("Rejected ownership edit restores original region"), Tunnel->Region == Region);
	Generator->SegmentTriangleBudget = 1;
	TestFalse(TEXT("Full chunk-budget failure is rejected"), MiningSpatialSafeEditing::Apply(Generator));
	TestEqual(TEXT("Budget failure restores exact budget"), Generator->SegmentTriangleBudget, 30000);
	TestTrue(TEXT("Budget failure preserves exact segment instances"), Generator->GeneratedSegments == Segments);
	TestTrue(TEXT("Budget failure restores applied flag"), Generator->bLayoutApplied);
	AMiningSpatialTunnel* Added = Scene.World->SpawnActor<AMiningSpatialTunnel>(); Added->Region = Region; Added->Knots.SetNum(1);
	TestFalse(TEXT("Invalid tool creation is refused"), MiningSpatialSafeEditing::Add(Added));
	TestTrue(TEXT("Rejected provisional creation retains applied terrain"), Generator->bLayoutApplied);
	Added->Destroy();
	AMiningSpatialTunnel* External = Scene.World->SpawnActor<AMiningSpatialTunnel>(); External->Region = Region; External->Knots.SetNum(1);
	TestFalse(TEXT("Invalid externally created source is detected"), MiningSpatialSafeEditing::Apply(Generator));
	TestFalse(TEXT("Unknown source cannot pretend old geometry is newly applied"), Generator->bLayoutApplied);
	External->Destroy();
	TestTrue(TEXT("Removing rejected external draft restores valid layout"), MiningSpatialSafeEditing::Apply(Generator, true));
	AMiningSpatialTunnel* Optional = Scene.World->SpawnActor<AMiningSpatialTunnel>(); Optional->Region = Region; Optional->Kind = EMiningManualKind::SideRoom;
	Optional->RoomSize = FVector(400); Optional->SetActorLocation(Region->GetActorLocation());
	TestTrue(TEXT("Valid optional room is accepted"), MiningSpatialSafeEditing::Add(Optional));
	Optional->Destroy();
	TestTrue(TEXT("Valid native deletion rebuilds the layout"), MiningSpatialSafeEditing::Apply(Generator));
	const int32 AfterDeletion = Generator->BuildRevision;
	MiningSpatialSafeEditing::Apply(Generator);
	TestEqual(TEXT("Retired snapshots do not trigger endless rebuilds"), Generator->BuildRevision, AfterDeletion);
	Tunnel->SetFlags(RF_Transactional);
	const uint32 BeforeUndoHash = Tunnel->GetGeometryHash();
	{
		FScopedTransaction Transaction(FText::FromString(TEXT("Safe terrain undo test"))); Tunnel->Modify();
		Tunnel->Knots[1].SectionScale *= 0.98; Tunnel->Changed();
	}
	const uint32 AfterUndoHash = Tunnel->GetGeometryHash();
	TestNotEqual(TEXT("Transactional geometry change is accepted"), BeforeUndoHash, AfterUndoHash);
	GEditor->UndoTransaction(); Generator->ProcessEditorRebuild();
	TestEqual(TEXT("Terrain undo restores authored source"), Tunnel->GetGeometryHash(), BeforeUndoHash);
	TestTrue(TEXT("Terrain undo leaves a fully applied live layout"), Generator->bLayoutApplied && Generator->GeneratedSegments.ContainsByPredicate([](const auto& Segment) { return IsValid(Segment); }));
	GEditor->RedoTransaction(); Generator->ProcessEditorRebuild();
	TestEqual(TEXT("Terrain redo restores accepted source"), Tunnel->GetGeometryHash(), AfterUndoHash);
	TestTrue(TEXT("Terrain redo leaves a fully applied live layout"), Generator->bLayoutApplied && Generator->GeneratedSegments.ContainsByPredicate([](const auto& Segment) { return IsValid(Segment); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningSafeUndoTest, "BrainMining.Spatial.SafeEditing.UndoRedoAndMetadata", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningSafeUndoTest::RunTest(const FString& Parameters)
{
	FSafeWorld Scene;
	FActorSpawnParameters Spawn; Spawn.ObjectFlags |= RF_Transactional;
	AMiningSpatialTunnel* Tunnel = Scene.World->SpawnActor<AMiningSpatialTunnel>(Spawn);
	MiningSpatialSafeEditing::Observe(Scene.World);
	const uint32 Before = Tunnel->GetGeometryHash();
	{
		FScopedTransaction Transaction(FText::FromString(TEXT("Safe edit undo test"))); Tunnel->Modify();
		Tunnel->Knots[1].Position.Y += 100; Tunnel->Changed();
	}
	const uint32 After = Tunnel->GetGeometryHash();
	TestNotEqual(TEXT("Transaction accepted changed source"), Before, After);
	GEditor->UndoTransaction(); MiningSpatialSafeEditing::Apply(Tunnel);
	TestEqual(TEXT("Undo restores complete authoring values"), Tunnel->GetGeometryHash(), Before);
	GEditor->RedoTransaction(); MiningSpatialSafeEditing::Apply(Tunnel);
	TestEqual(TEXT("Redo restores accepted source"), Tunnel->GetGeometryHash(), After);
	Tunnel->Kind = EMiningManualKind::Cavity; Tunnel->bHiddenCavity = true; Tunnel->Knots[0].bLocked = true;
	TestTrue(TEXT("Discrete valid metadata edit is accepted"), MiningSpatialSafeEditing::Apply(Tunnel));
	Tunnel->Kind = EMiningManualKind::Passage; Tunnel->bHiddenCavity = false; Tunnel->Knots[0].bLocked = false; Tunnel->Knots.SetNum(1);
	TestFalse(TEXT("Invalid metadata plus topology edit rejected atomically"), MiningSpatialSafeEditing::Apply(Tunnel));
	TestTrue(TEXT("Enum, flags and locks restored, not silently reset"), Tunnel->Kind == EMiningManualKind::Cavity && Tunnel->bHiddenCavity && Tunnel->Knots[0].bLocked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningSafeSaveTest, "BrainMining.Spatial.SafeEditing.SaveFeedback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningSafeSaveTest::RunTest(const FString& Parameters)
{
	UPackage* Package = CreatePackage(*(TEXT("/Temp/MiningSafeSave_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	FSafeWorld Scene(Package);
	Scene.World->SetFlags(RF_Public | RF_Standalone);
	AMiningSpatialTunnel* Tunnel = Scene.World->SpawnActor<AMiningSpatialTunnel>(); Tunnel->Knots[0].Leave.X = -1500;
	MiningSpatialSafeEditing::Observe(Scene.World); MiningSpatialSafeEditing::Apply(Tunnel, true);
	const FString GeometryIssue = MiningSpatialSafeEditing::Issue().Reason;
	Package->MarkPackageDirty();
	const FString Filename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Validation"), FPaths::GetBaseFilename(Package->GetName()) + TEXT(".umap"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
	const bool bSaved = UPackage::SavePackage(Package, Scene.World, *Filename, Args);
	TestTrue(TEXT("Temporary map is actually saved to disk"), bSaved && IFileManager::Get().FileSize(*Filename) > 0);
	TestTrue(TEXT("Successful native save reports success"), MiningSpatialSafeEditing::SaveStatus().Contains(TEXT("保存成功")));
	TestTrue(TEXT("Saving standalone source explicitly reports draft only"), MiningSpatialSafeEditing::SaveStatus().Contains(TEXT("仅草稿保存成功")) && MiningSpatialSafeEditing::SaveStatus().Contains(TEXT("地形未生效")));
	TestEqual(TEXT("Saving does not claim to fix invalid terrain"), MiningSpatialSafeEditing::Issue().Reason, GeometryIssue);
	FObjectSaveContextData Context; Context.bSaveSucceeded = false;
	UPackage::PackageSavedWithContextEvent.Broadcast(Filename, Package, FObjectPostSaveContext(Context));
	TestTrue(TEXT("Failed save has explicit feedback"), MiningSpatialSafeEditing::SaveStatus().Contains(TEXT("保存失败")));
	Context.bSaveSucceeded = true; Context.SaveFlags = SAVE_FromAutosave;
	UPackage::PackageSavedWithContextEvent.Broadcast(Filename, Package, FObjectPostSaveContext(Context));
	TestTrue(TEXT("Autosave does not overwrite manual-save failure"), MiningSpatialSafeEditing::SaveStatus().Contains(TEXT("保存失败")));
	Package->SetDirtyFlag(false); Package->ClearPackageFlags(PKG_NewlyCreated);
	FWorldContext& EditorContext = GEditor->GetEditorWorldContext();
	UWorld* PreviousContextWorld = EditorContext.World(); UWorld* PreviousGlobalWorld = GWorld;
	EditorContext.SetCurrentWorld(Scene.World); GWorld = Scene.World;
	const bool bNoOpSaved = MiningSpatialSafeEditing::Save();
	GWorld = PreviousGlobalWorld; EditorContext.SetCurrentWorld(PreviousContextWorld);
	TestTrue(TEXT("No-op successful save replaces stale failure feedback"), bNoOpSaved && !MiningSpatialSafeEditing::SaveStatus().Contains(TEXT("保存失败")) && MiningSpatialSafeEditing::SaveStatus().Contains(TEXT("无未保存修改")));
	Package->MarkPackageDirty();
	TestTrue(TEXT("Editing after saving is visibly dirty"), MiningSpatialSafeEditing::SaveStatus().Contains(TEXT("有未保存修改")));
	if (IFileManager::Get().FileExists(*Filename)) IFileManager::Get().Delete(*Filename);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningSafeSaveSyncTest, "BrainMining.Spatial.SafeEditing.NativeSaveSyncAndPlayGate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningSafeSaveSyncTest::RunTest(const FString& Parameters)
{
	UClass* ValidationClass = LoadClass<UObject>(nullptr, TEXT("/Script/DataValidation.DataValidationSettings"));
	UObject* ValidationSettings = ValidationClass ? ValidationClass->GetDefaultObject() : nullptr;
	FBoolProperty* ValidateOnSave = ValidationClass ? FindFProperty<FBoolProperty>(ValidationClass, TEXT("bValidateOnSave")) : nullptr;
	if (!TestTrue(TEXT("Temporary save fixture can suspend unrelated asset validation"), ValidationSettings && ValidateOnSave)) return false;
	const bool bValidateOnSave = ValidateOnSave->GetPropertyValue_InContainer(ValidationSettings);
	ValidateOnSave->SetPropertyValue_InContainer(ValidationSettings, false);
	ON_SCOPE_EXIT { ValidateOnSave->SetPropertyValue_InContainer(ValidationSettings, bValidateOnSave); };
	const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString Mount = TEXT("/Temp/Validation/SaveSync/") + Suffix + TEXT("/");
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Validation/SaveSync"), Suffix) + TEXT("/");
	IFileManager::Get().MakeDirectory(*Directory, true);
	UPackage* Package = CreatePackage(*(Mount + TEXT("TestMap")));
	FSafeWorld Scene(Package);
	Scene.World->SetFlags(RF_Public | RF_Standalone);
	FWorldContext& EditorContext = GEditor->GetEditorWorldContext();
	UWorld* PreviousWorld = EditorContext.World(); UWorld* PreviousGlobalWorld = GWorld;
	EditorContext.SetCurrentWorld(Scene.World); GWorld = Scene.World;
	ON_SCOPE_EXIT { GWorld = PreviousGlobalWorld; EditorContext.SetCurrentWorld(PreviousWorld); };
	AMiningLayoutRoom* Start = Scene.World->SpawnActor<AMiningLayoutRoom>();
	AMiningLayoutRoom* End = Scene.World->SpawnActor<AMiningLayoutRoom>();
	Start->RoomSize = End->RoomSize = FVector(1200, 1000, 1000); Start->bStartRoom = true; End->ConnectionOrder = 1;
	Start->bFloorAnchored = End->bFloorAnchored = true; End->SetActorLocation(FVector(4200, 0, 0));
	AMiningSpatialRegion* Region = Scene.World->SpawnActor<AMiningSpatialRegion>();
	Region->bAutomaticBedrock = false; Region->bFreeEditing = false;
	Region->SetActorLocation(FVector(2100, 0, 500)); Region->RockSize = FVector(3000, 1600, 1600); Region->FromRoom = Start; Region->ToRoom = End;
	AMiningSpatialTunnel* Tunnel = Scene.World->SpawnActor<AMiningSpatialTunnel>();
	Tunnel->Region = Region; Region->MainTunnel = Tunnel; Tunnel->StartPortal.Room = Start; Tunnel->EndPortal.Room = End; Tunnel->FitPortals();
	AMiningTunnelGenerator* Generator = Scene.World->SpawnActor<AMiningTunnelGenerator>();
	Generator->bHollowRegionPrototype = true; Generator->bDirectRoomFaces = true; Generator->bAutomaticChunks = true; Generator->HollowRegion = Region; Generator->bAutoRebuild = false;
	if (!TestTrue(TEXT("Save fixture has initial terrain"), MiningSpatialSafeEditing::PrepareWorld(Scene.World))) return false;
	const int32 OriginalRevision = Generator->BuildRevision;
	Tunnel->Knots[0].SectionScale = FVector2D(0.95, 0.95);
	const uint32 TargetHash = Tunnel->GetGeometryHash();
	const FString Filename = Directory + TEXT("TestMap.umap");
	ON_SCOPE_EXIT { IFileManager::Get().Delete(*Filename); };
	Package->MarkPackageDirty();
	TestTrue(TEXT("Native editor map save succeeds"), FEditorFileUtils::SaveMap(Scene.World, Filename));
	TestTrue(TEXT("Native save syncs pending edits before serialization"), Generator->bLayoutApplied && Generator->BuildRevision > OriginalRevision);
	TestEqual(TEXT("Saving does not constrain or reshape requested design"), Tunnel->GetGeometryHash(), TargetHash);
	TestTrue(TEXT("Applied save has no draft warning"), MiningSpatialSafeEditing::SaveStatus().Contains(TEXT("关卡保存成功")) && !MiningSpatialSafeEditing::SaveStatus().Contains(TEXT("仅草稿")));
	const auto GoodSegments = Generator->GeneratedSegments;
	const auto GoodKnots = Tunnel->Knots;
	Tunnel->Knots.SetNum(1);
	TestFalse(TEXT("Legacy fixture is truly invalid before save"), ShapeValid(Tunnel));
	const uint32 InvalidHash = Tunnel->GetGeometryHash();
	MiningSpatialSafeEditing::Reset(); MiningSpatialSafeEditing::Observe(Scene.World);
	Package->MarkPackageDirty();
	TestTrue(TEXT("Invalid authored draft still saves without data loss"), FEditorFileUtils::SaveMap(Scene.World, Filename));
	TestFalse(TEXT("A stale applied flag cannot survive failed save validation"), Generator->bLayoutApplied);
	TestEqual(TEXT("Invalid draft preserves exact authoring input"), Tunnel->GetGeometryHash(), InvalidHash);
	TestTrue(TEXT("Failed validation retains old geometry only as fallback"), Generator->GeneratedSegments == GoodSegments);
	TestTrue(TEXT("Native save explicitly reports draft and unapplied terrain"), MiningSpatialSafeEditing::SaveStatus().Contains(TEXT("仅草稿保存成功")) && MiningSpatialSafeEditing::TerrainStatus().Contains(TEXT("地形未生效")));
	TestTrue(TEXT("Unapplied generated layout can enter explicit repair"), MiningSpatialSafeEditing::BeginRepair());
	Tunnel->Knots[0].Position.Y += 50;
	const uint32 PartialRepairHash = Tunnel->GetGeometryHash();
	TestFalse(TEXT("Incomplete generated repair remains unapplied"), MiningSpatialSafeEditing::Apply(Tunnel));
	TestEqual(TEXT("Generated repair retains partial control-point edit"), Tunnel->GetGeometryHash(), PartialRepairHash);
	TestTrue(TEXT("Repair does not replace old terrain before full validation"), Generator->GeneratedSegments == GoodSegments);
	TestFalse(TEXT("Unfinished repair cannot pass save/play preparation"), MiningSpatialSafeEditing::PrepareWorld(Scene.World));
	TestTrue(TEXT("Cancel generated repair restores all source values"), MiningSpatialSafeEditing::CancelRepair());
	TestEqual(TEXT("Canceled repair preserves original saved draft"), Tunnel->GetGeometryHash(), InvalidHash);
	AddExpectedError(TEXT("Play-In-Editor canceled by plugin: 已阻止运行旧地形"), EAutomationExpectedErrorFlags::Contains, 2);
	for (bool bSimulate : {false, true})
	{
		FRequestPlaySessionParams Request;
		Request.WorldType = bSimulate ? EPlaySessionWorldType::SimulateInEditor : EPlaySessionWorldType::PlayInEditor;
		GEditor->RequestPlaySession(Request); GEditor->StartQueuedPlaySessionRequest();
		const bool bBlocked = !GEditor->IsPlaySessionInProgress() && MiningSpatialSafeEditing::Status().Contains(TEXT("已阻止运行旧地形"));
		TestTrue(bSimulate ? TEXT("Simulate is gated by actual authorizer") : TEXT("PIE is gated by actual authorizer"), bBlocked);
		if (GEditor->PlayWorld) GEditor->EndPlayMap();
	}
	TestEqual(TEXT("Rejected play leaves draft unchanged"), Tunnel->GetGeometryHash(), InvalidHash);
	Tunnel->Knots = GoodKnots;
	TestTrue(TEXT("Repaired design passes play preparation without reopening map"), MiningSpatialSafeEditing::PrepareWorld(Scene.World));
	TestTrue(TEXT("Play check applies current repaired design"), Generator->bLayoutApplied && Generator->BuildRevision > OriginalRevision);
	Generator->SegmentTriangleBudget = 1;
	TestFalse(TEXT("Save/play check includes full chunk budget validation"), MiningSpatialSafeEditing::PrepareWorld(Scene.World));
	TestEqual(TEXT("Save/play never silently increases triangle budget"), Generator->SegmentTriangleBudget, 1);
	Generator->SegmentTriangleBudget = 30000;
	FRequestPlaySessionParams RepairedPlay;
	GEditor->RequestPlaySession(RepairedPlay); GEditor->StartQueuedPlaySessionRequest();
	if (TestNotNull(TEXT("Repaired design starts a real PIE world"), GEditor->PlayWorld.Get()))
	{
		bool bFoundRuntimeTerrain = false, bFoundRuntimeSource = false;
		for (TActorIterator<AMiningTunnelGenerator> Item(GEditor->PlayWorld); Item; ++Item)
			bFoundRuntimeTerrain |= Item->bLayoutApplied && Item->BuildRevision == Generator->BuildRevision && !Item->GeneratedSegments.IsEmpty();
		for (TActorIterator<AMiningSpatialTunnel> Item(GEditor->PlayWorld); Item; ++Item)
			bFoundRuntimeSource |= Item->Knots.Num() == GoodKnots.Num() && Item->Knots[0].SectionScale.Equals(FVector2D(0.95, 0.95));
		TestTrue(TEXT("PIE uses the newly generated revision, not fallback terrain"), bFoundRuntimeTerrain);
		TestTrue(TEXT("PIE includes latest authored cross-section edit"), bFoundRuntimeSource);
		GEditor->EndPlayMap();
	}
	TestTrue(TEXT("Final repaired native save succeeds"), FEditorFileUtils::SaveMap(Scene.World, Filename));
	TestTrue(TEXT("Final native save stores an applied layout"), Generator->bLayoutApplied);
	const int32 SavedRevision = Generator->BuildRevision;
	MiningSpatialSafeEditing::Reset();
	TestTrue(TEXT("Release saved package name for independent disk reload"), Package->Rename(*(TEXT("/Temp/MiningSaveSyncLive_") + Suffix), nullptr, REN_DontCreateRedirectors | REN_NonTransactional));
	UPackage* ReloadedPackage = LoadPackage(nullptr, *Filename, LOAD_None);
	UWorld* ReloadedWorld = ReloadedPackage ? UWorld::FindWorldInPackage(ReloadedPackage) : nullptr;
	if (!TestNotNull(TEXT("Saved terrain reloads from disk"), ReloadedWorld)) return false;
	ON_SCOPE_EXIT { ReloadedWorld->DestroyWorld(false); ResetLoaders(ReloadedPackage); };
	AMiningTunnelGenerator* ReloadedGenerator = nullptr; AMiningSpatialTunnel* ReloadedTunnel = nullptr;
	for (AActor* Actor : ReloadedWorld->PersistentLevel->Actors)
	{
		if (auto* Candidate = Cast<AMiningTunnelGenerator>(Actor)) ReloadedGenerator = Candidate;
		if (auto* Candidate = Cast<AMiningSpatialTunnel>(Actor)) ReloadedTunnel = Candidate;
	}
	if (TestNotNull(TEXT("Reloaded generator exists"), ReloadedGenerator))
	{
		TestEqual(TEXT("New generated revision persists to disk"), ReloadedGenerator->BuildRevision, SavedRevision);
		TestTrue(TEXT("New generated terrain references reload"), !ReloadedGenerator->GeneratedSegments.IsEmpty() && ReloadedGenerator->GeneratedSegments.ContainsByPredicate([](const auto& Segment) { return IsValid(Segment); }));
	}
	if (TestNotNull(TEXT("Reloaded authored tunnel exists"), ReloadedTunnel)) TestEqual(TEXT("Latest authored shape persists to disk"), ReloadedTunnel->Knots[0].SectionScale, FVector2D(0.95, 0.95));
	TestTrue(TEXT("Reloaded saved design passes fresh play preparation"), MiningSpatialSafeEditing::PrepareWorld(ReloadedWorld));
	if (ReloadedGenerator) TestTrue(TEXT("Loaded construction invalidation is resolved before play"), ReloadedGenerator->bLayoutApplied);
	return true;
}
#endif
