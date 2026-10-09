#include "MiningSpatialOverview.h"
#include "MiningSpatialGeometry.h"
#include "MiningLayoutRoom.h"
#include "MiningTunnelGenerator.h"
#include "EditorModeRegistry.h"
#include "EditorModeManager.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningOverviewSceneTest, "BrainMining.Spatial.Overview.SceneAndBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningOverviewSceneTest::RunTest(const FString& Parameters)
{
	using namespace MiningSpatialOverview;
	TestTrue(TEXT("Editor overview mode registered"), FEditorModeRegistry::Get().GetFactoryMap().Contains(FName("EM_MiningSpatialOverview")));
	if (!IsRunningCommandlet())
	{
		const bool bWasEnabled = IsEnabled();
		SetEnabled(true);
		TestTrue(TEXT("Overview activates alongside editor tools"), IsEnabled());
		TestTrue(TEXT("Overview retains normal actor transform widgets"), GLevelEditorModeTools().UsesTransformWidget());
		SetEnabled(false);
		TestFalse(TEXT("Overview can be disabled"), IsEnabled());
		TestTrue(TEXT("Disabling overview retains normal editing"), GLevelEditorModeTools().UsesTransformWidget());
		SetEnabled(bWasEnabled);
	}
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	GEngine->CreateNewWorldContext(EWorldType::Editor).SetCurrentWorld(World);
	MiningSpatialOverview::FScene Scene;
	Scene.Refresh(World);
	TestFalse(TEXT("Empty map has no focus bounds"), Scene.GetBounds(EScope::Map, nullptr, nullptr).IsValid != 0);
	AMiningSpatialRegion* Region = World->SpawnActor<AMiningSpatialRegion>();
	Region->bAutomaticBedrock = false; Region->bFreeEditing = false;
	Region->RockSize = FVector(12000, 6000, 6000);
	AMiningLayoutRoom* Room = World->SpawnActor<AMiningLayoutRoom>();
	Room->SetActorLocation(FVector(-2000, 0, 0));
	Region->FromRoom = Room;
	AMiningSpatialTunnel* Main = World->SpawnActor<AMiningSpatialTunnel>();
	Main->Region = Region; Region->MainTunnel = Main;
	Main->Knots[1].Position = FVector(2000, 0, 0);
	AMiningSpatialTunnel* Branch = World->SpawnActor<AMiningSpatialTunnel>();
	Branch->Region = Region; Branch->SetActorLocation(FVector(0, 1400, 0));
	AMiningSpatialTunnel* Cavity = World->SpawnActor<AMiningSpatialTunnel>();
	Cavity->Region = Region; Cavity->Kind = EMiningManualKind::Cavity; Cavity->bHiddenCavity = true;
	Cavity->SetActorLocation(FVector(0, 0, 1600));
	AMiningSpatialTunnel* SideRoom = World->SpawnActor<AMiningSpatialTunnel>();
	SideRoom->Region = Region; SideRoom->Kind = EMiningManualKind::SideRoom;
	SideRoom->SetActorLocation(FVector(1400, 2000, 0)); SideRoom->SetActorRotation(FRotator(30, 45, 20));
	AMiningSpatialTunnel* Plug = World->SpawnActor<AMiningSpatialTunnel>();
	Plug->Region = Region; Plug->Kind = EMiningManualKind::RockBlock;
	AMiningSpatialTunnel* Other = World->SpawnActor<AMiningSpatialTunnel>();
	Other->SetActorLocation(FVector(20000, 0, 0)); Other->bEnabled = false;
	const uint32 Original = Main->GetGeometryHash();
	Scene.Refresh(World);
	TestEqual(TEXT("All rooms, tunnels, hidden cavities, plugs and orphan drafts shown without selection"), Scene.GetEntries().Num(), 8);
	for (const FEntry& Entry : Scene.GetEntries())
	{
		TestTrue(TEXT("Each object has finite focus bounds"), Entry.Bounds.IsValid && !Entry.Bounds.Min.ContainsNaN() && !Entry.Bounds.Max.ContainsNaN());
		TestTrue(TEXT("Each object has selectable outline"), Entry.Lines.Num() > 0);
	}
	const FBox MapBounds = Scene.GetBounds(EScope::Map, nullptr, nullptr);
	const FBox RegionBounds = Scene.GetBounds(EScope::Region, nullptr, Region);
	TestTrue(TEXT("Full map includes unassigned/disabled editable actors"), MapBounds.IsInsideOrOn(Other->GetActorLocation()));
	TestFalse(TEXT("Current region excludes unrelated actors"), RegionBounds.IsInsideOrOn(Other->GetActorLocation()));
	TestTrue(TEXT("Current region includes from room"), RegionBounds.IsInsideOrOn(Room->GetRoomBounds().GetCenter()));
	TestTrue(TEXT("Current region includes independent hidden cavity"), RegionBounds.IsInsideOrOn(Cavity->GetActorLocation()));
	TestTrue(TEXT("Selection bounds use full tunnel rather than empty root component"), Scene.GetBounds(EScope::Selection, Main, nullptr).GetSize().X >= 2000);
	TestTrue(TEXT("Selecting region frames its contents"), Scene.GetBounds(EScope::Selection, Region, nullptr).Equals(RegionBounds));
	TestTrue(TEXT("Optional rock bounds included"), Scene.GetBounds(EScope::Region, nullptr, Region, true).IsInsideOrOn(Region->GetRockBounds().Max));
	const FBox SideBounds = Scene.GetBounds(EScope::Selection, SideRoom, nullptr);
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector Local = SideRoom->RoomSize * FVector(Corner & 1 ? 0.5 : -0.5, Corner & 2 ? 0.5 : -0.5, Corner & 4 ? 0.5 : -0.5);
		TestTrue(TEXT("Rotated side room bounds enclose every corner"), SideBounds.ExpandBy(0.01).IsInsideOrOn(SideRoom->GetActorTransform().TransformPosition(Local)));
	}
	TestEqual(TEXT("Overview does not change geometry"), Main->GetGeometryHash(), Original);
	TestEqual(TEXT("Overview spawns no geometry or helpers"), Scene.GetEntries().Num(), 8);
	TestFalse(TEXT("No selection gives no focus target"), Scene.GetBounds(EScope::Selection, nullptr, nullptr).IsValid != 0);
	TestFalse(TEXT("No region gives no focus target"), Scene.GetBounds(EScope::Region, nullptr, nullptr).IsValid != 0);
	World->DestroyActor(Other);
	Scene.Refresh(World);
	TestEqual(TEXT("Deleted actors pruned from cache"), Scene.GetEntries().Num(), 7);
	Scene.Refresh(nullptr);
	TestTrue(TEXT("World teardown clears outlines"), Scene.GetEntries().IsEmpty());
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningOverviewRefreshTest, "BrainMining.Spatial.Overview.LiveEditsAndInvalidDrafts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningOverviewRefreshTest::RunTest(const FString& Parameters)
{
	using namespace MiningSpatialOverview;
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	AMiningSpatialRegion* Region = World->SpawnActor<AMiningSpatialRegion>();
	Region->bAutomaticBedrock = false; Region->bFreeEditing = false;
	AMiningLayoutRoom* Room = World->SpawnActor<AMiningLayoutRoom>();
	Room->SetActorLocation(FVector(-2000, 0, 0));
	AMiningSpatialTunnel* Tunnel = World->SpawnActor<AMiningSpatialTunnel>();
	Tunnel->Region = Region; Tunnel->StartPortal.Room = Room;
	Tunnel->Knots[1].Position = FVector(2000, 0, 0);
	MiningSpatialOverview::FScene Scene;
	auto EntryFor = [&Scene](AActor* Actor) -> const FEntry& { return *Scene.GetEntries().FindByPredicate([Actor](const FEntry& Entry) { return Entry.Actor == Actor; }); };
	Scene.Refresh(World);
	const uint32 InitialHash = EntryFor(Tunnel).Hash;
	Scene.Refresh(World);
	TestEqual(TEXT("Unchanged scene retains cached sample signature"), EntryFor(Tunnel).Hash, InitialHash);
	Tunnel->Knots[1].Position.Z += 800;
	Scene.Refresh(World);
	TestNotEqual(TEXT("Path edits update unselected outline"), EntryFor(Tunnel).Hash, InitialHash);
	Tunnel->Knots[1].Position.Z -= 800;
	Scene.Refresh(World);
	TestEqual(TEXT("Restoring node data restores signature"), EntryFor(Tunnel).Hash, InitialHash);
	Region->CurveTolerance += 1;
	Scene.Refresh(World);
	TestNotEqual(TEXT("Region sampling tolerance invalidates outline cache"), EntryFor(Tunnel).Hash, InitialHash);
	const uint32 BeforeRoomMove = EntryFor(Tunnel).Hash;
	Room->SetActorLocation(Room->GetActorLocation() + FVector(-300, 0, 0));
	Room->RoomSize.Y += 200;
	Scene.Refresh(World);
	TestNotEqual(TEXT("Portal room movement/resizing updates bound tunnel"), EntryFor(Tunnel).Hash, BeforeRoomMove);
	TestTrue(TEXT("Room bounds update live"), Scene.GetBounds(EScope::Selection, Room, nullptr).Equals(Room->GetRoomBounds()));
	const uint32 BeforeSection = EntryFor(Tunnel).Hash;
	Tunnel->Knots[1].SectionScale = FVector2D(2, 1); Tunnel->Knots[1].Roll = 90;
	Scene.Refresh(World);
	TestNotEqual(TEXT("Section width and twist update outline"), EntryFor(Tunnel).Hash, BeforeSection);
	Tunnel->Knots[1].Profile.Reset();
	Scene.Refresh(World);
	TestTrue(TEXT("Invalid profile still has selectable draft outline"), EntryFor(Tunnel).bDraft && EntryFor(Tunnel).Lines.Num() > 0);
	Tunnel->Knots.Reset();
	Scene.Refresh(World);
	TestTrue(TEXT("Empty draft remains selectable"), EntryFor(Tunnel).bDraft && EntryFor(Tunnel).Bounds.IsValid);
	FEntry Invalid;
	Invalid.AddLine(FVector(std::numeric_limits<double>::quiet_NaN()), FVector::ZeroVector);
	TestTrue(TEXT("Nonfinite coordinates never enter draw/focus data"), Invalid.Lines.IsEmpty() && !Invalid.Bounds.IsValid);
	Tunnel->SetIsTemporarilyHiddenInEditor(true);
	Scene.Refresh(World);
	TestEqual(TEXT("Explicit editor actor hiding respected"), Scene.GetEntries().Num(), 2);
	UWorld* OtherWorld = UWorld::CreateWorld(EWorldType::Editor, false);
	Scene.Refresh(OtherWorld);
	TestTrue(TEXT("Changing worlds drops old actor cache"), Scene.GetEntries().IsEmpty());
	OtherWorld->DestroyWorld(false);
	World->DestroyWorld(false);
	return true;
}

#endif
