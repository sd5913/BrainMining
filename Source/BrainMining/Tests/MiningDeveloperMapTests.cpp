#include "MiningDeveloperMapData.h"
#include "MiningDeveloperMapComponent.h"
#include "MiningHUD.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Misc/ScopeExit.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMapProjectionTest, "BrainMining.DeveloperMap.Projection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningMapProjectionTest::RunTest(const FString& Parameters)
{
	using namespace MiningDeveloperMap;
	TestEqual(TEXT("Side view retains altitude"), Project(FVector(100, 200, -300), EMiningMapView::SideXZ), FVector2D(100, 300));
	TestEqual(TEXT("Top view retains side caves"), Project(FVector(100, 200, -300), EMiningMapView::TopXY), FVector2D(100, -200));
	TestEqual(TEXT("Y aligned layouts have a side view"), Project(FVector(100, 200, -300), EMiningMapView::SideYZ), FVector2D(200, 300));
	FMiningMapRegion Room = Box(TEXT("Room"), EMiningMapRegionKind::Room, 0, FBox(FVector(-500, -400, -300), FVector(500, 400, 300)));
	for (EMiningMapView View : {EMiningMapView::SideXZ, EMiningMapView::TopXY, EMiningMapView::SideYZ})
	{
		TestEqual(TEXT("Eight corners project to four hull vertices"), Outline(Room, View).Num(), 4);
		Room.bEllipsoid = true;
		TestEqual(TEXT("Ellipsoid remains an ellipse"), Outline(Room, View).Num(), 32);
		Room.bEllipsoid = false;
	}
	TestEqual(TEXT("Duplicate and interior points removed"), Hull({FVector2D(0, 0), FVector2D(0, 0), FVector2D(10, 0), FVector2D(5, 5), FVector2D(10, 10), FVector2D(0, 10)}).Num(), 4);
	TestEqual(TEXT("Collinear projection is bounded"), Hull({FVector2D(0, 0), FVector2D(2, 0), FVector2D(1, 0)}).Num(), 2);
	TestEqual(TEXT("Empty hull"), Hull({}).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMapContainmentTest, "BrainMining.DeveloperMap.Containment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningMapContainmentTest::RunTest(const FString& Parameters)
{
	using namespace MiningDeveloperMap;
	FMiningMapRegion Room = Box(TEXT("Room"), EMiningMapRegionKind::Room, 0, FBox(FVector(-100), FVector(100)));
	TestEqual(TEXT("Box has six unique planes"), Room.Planes.Num(), 6);
	TestTrue(TEXT("Room center"), Room.Contains(FVector::ZeroVector));
	TestTrue(TEXT("Room boundary"), Room.Contains(FVector(100, 100, 100)));
	TestFalse(TEXT("Off-plane player is not inside room"), Room.Contains(FVector(0, 200, 0)));
	FMiningMapRegion Cave = Room;
	Cave.Kind = EMiningMapRegionKind::Space;
	Cave.bEllipsoid = true;
	TestFalse(TEXT("Ellipse bounding-box corner is not cave"), Cave.Contains(FVector(90, 90, 90)));
	TestTrue(TEXT("Ellipse center is cave"), Cave.Contains(FVector(0, 0, 0)));
	TArray<FVector> Sloped = Room.Vertices;
	for (FVector& Vertex : Sloped) Vertex.Z += Vertex.X * 2 + 500;
	const FMiningMapRegion Tunnel = Convex(TEXT("Slope"), EMiningMapRegionKind::Tunnel, 0, Sloped);
	TestTrue(TEXT("Actual sloping tunnel interior"), Tunnel.Contains(FVector(50, 0, 600)));
	TestFalse(TEXT("Inside AABB but outside sloping tunnel"), Tunnel.Contains(FVector(-90, 0, 750)));
	TestEqual(TEXT("True 3D membership prioritizes room"), FindRegion({Cave, Room}, FVector::ZeroVector), 1);
	TestEqual(TEXT("Outside all geometry"), FindRegion({Cave, Room}, FVector(200)), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMapClippingTest, "BrainMining.DeveloperMap.Clipping", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningMapClippingTest::RunTest(const FString& Parameters)
{
	using namespace MiningDeveloperMap;
	const FBox2D Clip(FVector2D(10, 20), FVector2D(210, 120));
	const TArray<FVector2D> Enclosing = {FVector2D(-500), FVector2D(500, -500), FVector2D(500), FVector2D(-500, 500)};
	const TArray<FVector2D> Clipped = ClipPolygon(Enclosing, Clip);
	TestEqual(TEXT("Enclosing polygon clips to viewport"), Clipped.Num(), 4);
	for (const FVector2D& Vertex : Clipped) TestTrue(TEXT("No fill escapes viewport"), Clip.IsInsideOrOn(Vertex));
	TestTrue(TEXT("Outside polygon discarded"), ClipPolygon({FVector2D(-20), FVector2D(-10, -20), FVector2D(-10)}, Clip).IsEmpty());
	FVector2D Start(-100, 70), Finish(400, 70);
	TestTrue(TEXT("Crossing line retained"), ClipLine(Start, Finish, Clip));
	TestEqual(TEXT("Line clipped at left"), Start, FVector2D(10, 70));
	TestEqual(TEXT("Line clipped at right"), Finish, FVector2D(210, 70));
	Start = FVector2D(-20); Finish = FVector2D(-10);
	TestFalse(TEXT("Outside line discarded"), ClipLine(Start, Finish, Clip));
	FRandomStream Random(2381);
	for (int32 Sample = 0; Sample < 400; ++Sample)
	{
		Start = FVector2D(Random.FRandRange(-500, 500), Random.FRandRange(-500, 500));
		Finish = FVector2D(Random.FRandRange(-500, 500), Random.FRandRange(-500, 500));
		if (ClipLine(Start, Finish, Clip)) TestTrue(TEXT("Random clipped endpoints remain inside"), Clip.ExpandBy(0.00001).IsInsideOrOn(Start) && Clip.ExpandBy(0.00001).IsInsideOrOn(Finish));
	}
	TestEqual(TEXT("Fit preserves common scale on both axes"), FitScale(FBox2D(FVector2D(0), FVector2D(2000, 1000)), FVector2D(200, 400)), 0.1);
	TestTrue(TEXT("Degenerate bounds produce finite scale"), FMath::IsFinite(FitScale(FBox2D(FVector2D(0), FVector2D(0)), FVector2D(200))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMapControlsTest, "BrainMining.DeveloperMap.Controls", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningMapControlsTest::RunTest(const FString& Parameters)
{
	UMiningDeveloperMapComponent* Map = NewObject<UMiningDeveloperMapComponent>();
	TestTrue(TEXT("Visible by default"), Map->bVisible);
	TestTrue(TEXT("3D map is the default"), Map->View == EMiningMapView::Orbit3D);
	Map->ToggleExpanded(); TestTrue(TEXT("Expand"), Map->bExpanded);
	Map->CycleView(); TestTrue(TEXT("First flat view"), Map->View == EMiningMapView::SideXZ);
	Map->CycleView(); TestTrue(TEXT("Top view"), Map->View == EMiningMapView::TopXY);
	Map->CycleView(); TestTrue(TEXT("Other side view"), Map->View == EMiningMapView::SideYZ);
	Map->CycleView(); TestTrue(TEXT("View wraps to 3D"), Map->View == EMiningMapView::Orbit3D);
	Map->OrbitBy(60, 200);
	TestEqual(TEXT("Orbit yaw"), Map->OrbitRotation.Yaw, 15.0);
	TestEqual(TEXT("Orbit pitch prevents flipping"), Map->OrbitRotation.Pitch, 85.0);
	Map->PanBy(FVector2D(100, -200));
	TestEqual(TEXT("Pan changes only the map"), Map->PanOffset, FVector2D(100, -200));
	for (int32 Step = 0; Step < 30; ++Step) Map->ZoomIn();
	TestEqual(TEXT("Maximum zoom"), Map->Zoom, 16.f);
	for (int32 Step = 0; Step < 50; ++Step) Map->ZoomOut();
	TestEqual(TEXT("Minimum zoom"), Map->Zoom, 0.25f);
	Map->ToggleFollow(); TestTrue(TEXT("Follow"), Map->bFollowPlayer);
	TestEqual(TEXT("Follow recenters the player"), Map->PanOffset, FVector2D::ZeroVector);
	TestEqual(TEXT("Follow resets zoom"), Map->Zoom, 1.f);
	Map->ToggleFollow(); TestFalse(TEXT("Fit"), Map->bFollowPlayer);
	Map->ToggleLabels(); TestFalse(TEXT("Hide labels"), Map->bLabels);
	Map->ToggleMap(); TestFalse(TEXT("Hide map"), Map->bVisible);
	Map->ZoomIn(); TestEqual(TEXT("Hidden map ignores zoom"), Map->Zoom, 1.f);
	Map->OrbitBy(30, -20); Map->PanBy(FVector2D(50));
	TestEqual(TEXT("Hidden map ignores orbit"), Map->OrbitRotation.Yaw, 15.0);
	TestEqual(TEXT("Hidden map ignores pan"), Map->PanOffset, FVector2D::ZeroVector);
	Map->ToggleMap(); TestTrue(TEXT("Show map"), Map->bVisible);
	Map->ResetView();
	TestEqual(TEXT("Reset restores default camera"), Map->OrbitRotation, FRotator(-30, -45, 0));
	Map->CycleView(); Map->OrbitBy(20, 20); Map->PanBy(FVector2D(100));
	TestEqual(TEXT("Flat views ignore orbit"), Map->OrbitRotation, FRotator(-30, -45, 0));
	TestEqual(TEXT("Flat views ignore pan"), Map->PanOffset, FVector2D::ZeroVector);
	Map->ResetView();
	TestTrue(TEXT("Reset returns from flat view to 3D"), Map->View == EMiningMapView::Orbit3D);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMap3DProjectionTest, "BrainMining.DeveloperMap.OrbitProjection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningMap3DProjectionTest::RunTest(const FString& Parameters)
{
	using namespace MiningDeveloperMap;
	const FVector Point(320, -270, 890);
	for (double Pitch : {-85.0, -30.0, 0.0, 85.0})
	for (double Yaw : {-180.0, -45.0, 35.0, 180.0})
	{
		const FRotator Rotation(Pitch, Yaw, 0);
		const FVector Projected = Project3D(Point, Rotation);
		TestFalse(TEXT("Orbit projection is finite"), Projected.ContainsNaN());
		TestTrue(TEXT("Projection retains all three dimensions"), FMath::IsNearlyEqual(Projected.SizeSquared(), Point.SizeSquared(), 0.00001));
		const FVector Right = Project3D(FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y), Rotation);
		TestTrue(TEXT("Map horizontal drag aligns with screen right"), Right.Equals(FVector(1, 0, 0), 0.00001));
		const FVector Forward = Project3D(Rotation.Vector(), Rotation);
		TestTrue(TEXT("Depth is independent of screen coordinates"), Forward.Equals(FVector(0, 0, 1), 0.00001));
	}
	const FRotator Rotation(-30, -45, 0);
	TestFalse(TEXT("Different depth layers no longer collapse to XZ"), FVector2D(Project3D(FVector(0, 100, 0), Rotation)).Equals(FVector2D::ZeroVector));
	TestFalse(TEXT("Rotating the map changes the view"), Project3D(Point, Rotation).Equals(Project3D(Point, FRotator(-30, 45, 0))));
	TestTrue(TEXT("Default project supports the new view without altering old enum values"), Project(Point, EMiningMapView::Orbit3D).Equals(FVector2D(Project3D(Point, Rotation))));
	TestEqual(TEXT("Legacy XZ serialized value retained"), static_cast<int32>(EMiningMapView::SideXZ), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMap3DSurfaceTest, "BrainMining.DeveloperMap.Surfaces3D", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningMap3DSurfaceTest::RunTest(const FString& Parameters)
{
	using namespace MiningDeveloperMap;
	const FMiningMapRegion Room = Box(TEXT("Room"), EMiningMapRegionKind::Room, 0, FBox(FVector(-100), FVector(100)));
	const FSurface RoomSurface = BuildSurface(Room);
	TestEqual(TEXT("Room has actual box faces"), RoomSurface.Triangles.Num(), 12);
	int32 VisibleEdges = 0;
	for (const FSurfaceEdge& Edge : RoomSurface.Edges) if (Edge.Visible(FRotator(-30, -45, 0).Vector())) ++VisibleEdges;
	TestEqual(TEXT("Box face diagonals do not clutter map"), VisibleEdges, 12);
	FMiningMapRegion Cavern = Room; Cavern.bEllipsoid = true;
	const FSurface CavernSurface = BuildSurface(Cavern);
	TestEqual(TEXT("Ellipsoid uses a low-detail curved surface"), CavernSurface.Triangles.Num(), 224);
	for (const FVector& Vertex : CavernSurface.Vertices) TestTrue(TEXT("Ellipsoid surface is not a bounding box"), FMath::IsNearlyEqual(Vertex.Size(), 100.0, 0.00001));
	FMiningMapRegion Concave;
	Concave.Name = TEXT("Concave");
	Concave.PolygonXZ = {FVector2D(0, 0), FVector2D(400, 0), FVector2D(400, 100), FVector2D(100, 100), FVector2D(100, 400), FVector2D(0, 400)};
	Concave.PolygonTriangles = {FIntVector(0, 1, 2), FIntVector(0, 2, 3), FIntVector(0, 3, 5), FIntVector(3, 4, 5)};
	for (double Depth : {-80.0, 80.0}) for (const FVector2D& Point : Concave.PolygonXZ)
	{
		const FVector Vertex(Point.X, Depth, Point.Y); Concave.Vertices.Add(Vertex); Concave.Bounds += Vertex;
	}
	const FSurface ConcaveSurface = BuildSurface(Concave);
	TestEqual(TEXT("Concave footprint is extruded with real side walls"), ConcaveSurface.Triangles.Num(), 20);
	FMiningMapRegion SurfaceRegion = Concave;
	SurfaceRegion.SurfaceTriangles = ConcaveSurface.Triangles;
	TestFalse(TEXT("Concave gap is not filled with a convex hull"), SurfaceRegion.Contains(FVector(200, 0, 200)));
	TestTrue(TEXT("Valid interior remains mapped"), SurfaceRegion.Contains(FVector(50, 0, 200)));
	const FSurface Preserved = BuildSurface(SurfaceRegion);
	TestTrue(TEXT("Authored 3D surface preserves exact vertices and faces"), Preserved.Vertices == SurfaceRegion.Vertices && Preserved.Triangles == SurfaceRegion.SurfaceTriangles);
	SurfaceRegion.SurfaceTriangles.Add(FIntVector(-1, 8000, 0));
	TestEqual(TEXT("Malformed faces cannot reach renderer"), BuildSurface(SurfaceRegion).Triangles.Num(), Preserved.Triangles.Num());
	FMiningMapRegion Legacy;
	Legacy.Bounds = Room.Bounds;
	TestTrue(TEXT("Old bounds-only layouts retain explicit fallback"), BuildSurface(Legacy).bBoundsFallback);
	Legacy.Bounds = FBox(FVector::ZeroVector, FVector::ZeroVector);
	TestTrue(TEXT("Degenerate legacy bounds terminate without recursion"), BuildSurface(Legacy).Triangles.IsEmpty());
	TestTrue(TEXT("Missing geometry is safe"), BuildSurface(FMiningMapRegion()).Triangles.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningMapInputCaptureTest, "BrainMining.DeveloperMap.InputCapture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningMapInputCaptureTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	Controller->SetAsLocalPlayerController();
	AMiningHUD* HUD = World->SpawnActor<AMiningHUD>();
	HUD->PlayerOwner = Controller;
	UMiningDeveloperMapComponent* Map = HUD->DeveloperMap;
	UInputComponent* Input = NewObject<UInputComponent>(HUD);
	Map->BindInput(Input);
	const auto Key = [Input](EInputEvent Event)
	{
		for (const FInputKeyBinding& Binding : Input->KeyBindings)
			if (Binding.Chord.Key == EKeys::LeftAlt && Binding.KeyEvent == Event) Binding.KeyDelegate.Execute(EKeys::LeftAlt);
	};
	TestTrue(TEXT("Fixture uses local controller"), Controller->IsLocalController());
	Key(IE_Pressed);
	TestTrue(TEXT("Holding Alt targets map rather than gameplay look"), Map->bControllingMap && Controller->IsLookInputIgnored());
	Key(IE_Pressed); Key(IE_Released);
	TestFalse(TEXT("Repeated press cannot strand the look input lock"), Controller->IsLookInputIgnored());
	Controller->SetIgnoreLookInput(true);
	Key(IE_Pressed); Key(IE_Released);
	TestTrue(TEXT("Map does not clear another system's existing look lock"), Controller->IsLookInputIgnored());
	Controller->SetIgnoreLookInput(false);
	Key(IE_Pressed); Map->ToggleMap();
	TestFalse(TEXT("Hiding the map always releases its look lock"), Map->bControllingMap || Controller->IsLookInputIgnored());
	Key(IE_Pressed);
	TestFalse(TEXT("Hidden map cannot capture look"), Map->bControllingMap);
	Map->ToggleMap(); Key(IE_Pressed); Map->CycleView();
	TestFalse(TEXT("Switching to a flat view releases look"), Map->bControllingMap || Controller->IsLookInputIgnored());
	Key(IE_Pressed);
	TestFalse(TEXT("Flat views leave mouse look alone"), Map->bControllingMap);
	Map->ResetView(); Key(IE_Pressed);
	Map->TickComponent(0, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Losing the game viewport releases look even without key-up"), Map->bControllingMap || Controller->IsLookInputIgnored());
	return true;
}

#endif
