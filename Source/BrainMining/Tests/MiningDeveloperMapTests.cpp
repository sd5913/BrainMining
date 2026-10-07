#include "MiningDeveloperMapData.h"
#include "MiningDeveloperMapComponent.h"
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
	Map->ToggleExpanded(); TestTrue(TEXT("Expand"), Map->bExpanded);
	Map->CycleView(); TestTrue(TEXT("Top view"), Map->View == EMiningMapView::TopXY);
	Map->CycleView(); TestTrue(TEXT("Other side view"), Map->View == EMiningMapView::SideYZ);
	Map->CycleView(); TestTrue(TEXT("View wraps"), Map->View == EMiningMapView::SideXZ);
	for (int32 Step = 0; Step < 30; ++Step) Map->ZoomIn();
	TestEqual(TEXT("Maximum zoom"), Map->Zoom, 16.f);
	for (int32 Step = 0; Step < 50; ++Step) Map->ZoomOut();
	TestEqual(TEXT("Minimum zoom"), Map->Zoom, 0.25f);
	Map->ToggleFollow(); TestTrue(TEXT("Follow"), Map->bFollowPlayer);
	TestEqual(TEXT("Follow resets zoom"), Map->Zoom, 1.f);
	Map->ToggleFollow(); TestFalse(TEXT("Fit"), Map->bFollowPlayer);
	Map->ToggleLabels(); TestFalse(TEXT("Hide labels"), Map->bLabels);
	Map->ToggleMap(); TestFalse(TEXT("Hide map"), Map->bVisible);
	Map->ZoomIn(); TestEqual(TEXT("Hidden map ignores zoom"), Map->Zoom, 1.f);
	Map->ToggleMap(); TestTrue(TEXT("Show map"), Map->bVisible);
	return true;
}

#endif
