#include "MiningManualGeometry.h"
#include "MiningChunkGeometry.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningManualPolygonTest, "BrainMining.Manual.ConcaveGeometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningManualPolygonTest::RunTest(const FString& Parameters)
{
	const TArray<FVector2D> Polygon = {{0, 0}, {800, 0}, {800, 800}, {0, 800}, {0, 500}, {500, 500}, {500, 300}, {0, 300}};
	TArray<FIntVector> Triangles; FString Error;
	TestTrue(TEXT("Concave C-shape triangulates"), MiningManual::Triangulate(Polygon, Triangles, Error));
	TestEqual(TEXT("Concavity retains all boundary nodes"), Triangles.Num(), Polygon.Num() - 2);
	TestFalse(TEXT("Notch does not become a convex hull"), MiningManual::Contains(Polygon, FVector2D(200, 400)));
	TestTrue(TEXT("Arm is inside"), MiningManual::Contains(Polygon, FVector2D(200, 700)));
	UE::Geometry::FDynamicMesh3 Mesh;
	TestTrue(TEXT("Concave prism is closed"), MiningManual::Extrude(Polygon, -300, 300, Mesh, Error) && Mesh.IsClosed());
	double Volume = 0;
	for (int32 Triangle : Mesh.TriangleIndicesItr())
	{
		const UE::Geometry::FIndex3i Indices = Mesh.GetTriangle(Triangle);
		Volume += FVector::DotProduct(FVector(Mesh.GetVertex(Indices.A)), FVector::CrossProduct(FVector(Mesh.GetVertex(Indices.B)), FVector(Mesh.GetVertex(Indices.C)))) / 6;
	}
	TestTrue(TEXT("Engine winding and expected concave volume"), FMath::Abs(Volume + 540000.0 * 600) < 1);
	UE::Geometry::FMinimalBoxMeshGenerator Box;
	Box.Box = UE::Geometry::FOrientedBox3d(FVector3d(400, 0, 400), FVector3d(1000, 200, 1000)); Box.Generate();
	UE::Geometry::FDynamicMesh3 Solid(&Box), Carved;
	TestTrue(TEXT("Concave cavity subtracts across Unreal box boundary with matching winding"), MiningChunks::Subtract(Solid, Mesh, Carved) && Carved.IsClosed());
	TArray<FVector2D> Reverse = Polygon; Algo::Reverse(Reverse);
	TestTrue(TEXT("Clockwise authored outline also extrudes"), MiningManual::Extrude(Reverse, -300, 300, Mesh, Error));
	TestFalse(TEXT("Self-intersection rejected"), MiningManual::Triangulate({{0, 0}, {500, 500}, {0, 500}, {500, 0}}, Triangles, Error));
	TestFalse(TEXT("Duplicate point rejected"), MiningManual::Triangulate({{0, 0}, {500, 0}, {500, 0}, {0, 500}}, Triangles, Error));
	TestFalse(TEXT("Self-touch rejected"), MiningManual::Triangulate({{0, 0}, {500, 0}, {500, 500}, {250, 0}, {0, 500}}, Triangles, Error));
	FMiningMapRegion Region; Region.PolygonXZ = Polygon;
	Region.Bounds = FBox(FVector(0, -300, 0), FVector(800, 300, 800));
	TestFalse(TEXT("Minimap location ignores notch"), Region.Contains(FVector(200, 0, 400)));
	TestTrue(TEXT("Minimap knows concave interior"), Region.Contains(FVector(700, 0, 400)));
	TestFalse(TEXT("Minimap respects depth"), Region.Contains(FVector(700, 301, 400)));
	TestEqual(TEXT("Side outline preserves concavity"), MiningDeveloperMap::Outline(Region, EMiningMapView::SideXZ).Num(), Polygon.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningManualCurveTest, "BrainMining.Manual.PenCurves", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningManualCurveTest::RunTest(const FString& Parameters)
{
	FMiningPenPoint First, Last;
	First.Position = FVector2D(0, 0); First.Leave = FVector2D(700, 900); First.Mode = EMiningPenMode::Independent;
	Last.Position = FVector2D(1000, 100); Last.Arrive = FVector2D(-200, -900); Last.Mode = EMiningPenMode::Independent;
	TArray<FMiningPenPoint> Points{First, Last};
	TArray<FVector2D> Sampled; FString Error;
	TestTrue(TEXT("Cubic with handles samples"), MiningManual::Sample(Points, false, 3, Sampled, Error));
	TestTrue(TEXT("Curves contain more than endpoint samples"), Sampled.Num() > 5);
	TestTrue(TEXT("Bend changes geometry not only display"), FMath::Abs(Sampled[Sampled.Num() / 3].Y) > 10);
	const double Split = 0.37;
	TestTrue(TEXT("Insert preserves cubic via De Casteljau"), MiningManual::Insert(Points, 0, Split, false));
	for (int32 Step = 0; Step <= 100; ++Step)
	{
		const double Parameter = Step / 100.0;
		const FVector2D Expected = MiningManual::Evaluate(First, Last, Parameter);
		const FVector2D Actual = Parameter <= Split ? MiningManual::Evaluate(Points[0], Points[1], Parameter / Split) : MiningManual::Evaluate(Points[1], Points[2], (Parameter - Split) / (1 - Split));
		if (!TestTrue(TEXT("Inserted curve matches original at sample"), Expected.Equals(Actual, 0.000001))) break;
	}
	Points[0].bLocked = true;
	TestFalse(TEXT("Locked nodes do not change during insertion"), MiningManual::Insert(Points, 0, 0.5, false));
	Points[1].Position.X = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("Nonfinite input rejected"), MiningManual::Sample(Points, false, 8, Sampled, Error));
	TestFalse(TEXT("Too few points remains an editable invalid draft"), MiningManual::Sample({First}, true, 8, Sampled, Error));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningManualBendTest, "BrainMining.Manual.StableBending", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningManualBendTest::RunTest(const FString& Parameters)
{
	TArray<FMiningPenPoint> Points;
	for (const FVector2D& Position : {FVector2D(0, 0), FVector2D(700, 0), FVector2D(700, -400), FVector2D(200, -400), FVector2D(200, -800), FVector2D(1000, -800)})
	{
		FMiningPenPoint Point; Point.Position = Position; Point.Leave = FVector2D(25, 0); Point.Arrive = -Point.Leave; Points.Add(Point);
	}
	TestTrue(TEXT("Folded path binds by authored order"), MiningManual::BindGuide(Points) && MiningManual::GuideBound(Points));
	Points[2].bLocked = true;
	const TArray<FMiningPenPoint> Original = Points;
	MiningManual::Bend(Points, 0.5, FVector2D(0, 300));
	TestTrue(TEXT("First endpoint preserved"), Points[0].Position.Equals(Original[0].Position));
	TestTrue(TEXT("Last endpoint preserved"), Points.Last().Position.Equals(Original.Last().Position));
	TestTrue(TEXT("Locked detail preserved"), Points[2].Position.Equals(Original[2].Position));
	TestFalse(TEXT("Unlocked middle bends"), Points[3].Position.Equals(Original[3].Position));
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		TestEqual(TEXT("Station is not reassociated after deformation"), Points[Index].GuideStation, Original[Index].GuideStation);
		TestTrue(TEXT("Local curve handle preserved"), Points[Index].Leave.Equals(Original[Index].Leave));
	}
	return true;
}
#endif
