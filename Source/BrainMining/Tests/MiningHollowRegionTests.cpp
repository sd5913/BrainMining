#include "MiningHollowGeometry.h"
#include "MiningChunkGeometry.h"
#include "MiningDeveloperMapData.h"
#include "Generators/SphereGenerator.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "Operations/MeshBoolean.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace UE::Geometry;

namespace
{
	MiningHollow::FSettings SampleRegion()
	{
		MiningHollow::FSettings Settings;
		Settings.FirstRoom = FBox(FVector(-600, -600, 0), FVector(600, 600, 1000));
		Settings.LastRoom = FBox(FVector(6600, -600, -800), FVector(7800, 600, 200));
		Settings.Points = {FVector(600, 0, 350), FVector(2100, 150, 150), FVector(3600, 0, -50), FVector(5100, -150, -250), FVector(6600, 0, -450)};
		for (float Position : {0.25f, 0.5f, 0.75f}) { FMiningHollowPlug Plug; Plug.Position = Position; Settings.Plugs.Add(Plug); }
		return Settings;
	}

	double Winding(const FDynamicMesh3& Mesh, const FVector& Point)
	{
		double Angle = 0;
		for (int32 Triangle : Mesh.TriangleIndicesItr())
		{
			FVector3d First, Second, Third;
			Mesh.GetTriVertices(Triangle, First, Second, Third);
			First -= Point; Second -= Point; Third -= Point;
			const double Denominator = First.Length() * Second.Length() * Third.Length() + First.Dot(Second) * Third.Length() + Second.Dot(Third) * First.Length() + Third.Dot(First) * Second.Length();
			Angle += 2 * FMath::Atan2(First.Dot(Second.Cross(Third)), Denominator);
		}
		return Angle / (4 * UE_DOUBLE_PI);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningHollowPlanTest, "BrainMining.HollowRegion.Plan", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningHollowPlanTest::RunTest(const FString& Parameters)
{
	MiningHollow::FSettings Settings = SampleRegion();
	MiningHollow::FPlan Plan;
	FString Error;
	if (!TestTrue(TEXT("Bent descending path accepted"), MiningHollow::Plan(Settings, Plan, Error))) { AddError(Error); return false; }
	TestEqual(TEXT("Three plugs"), Plan.PlugIntervals.Num(), 3);
	TestEqual(TEXT("Plugs spanning knots split for material"), Plan.Guides.Num(), 6);
	TestTrue(TEXT("Length follows actual path"), Plan.Length > 6000);
	const TArray<FVector> SavedPoints = Plan.Points;
	Settings.Points[2].X = Settings.Points[1].X;
	TestFalse(TEXT("Backtracking rejected"), MiningHollow::Plan(Settings, Plan, Error));
	TestEqual(TEXT("Invalid plan keeps previous result"), Plan.Points, SavedPoints);
	Settings = SampleRegion(); Settings.Plugs[1].Position = 0.25f;
	TestFalse(TEXT("Overlapping plugs rejected"), MiningHollow::Plan(Settings, Plan, Error));
	Settings = SampleRegion(); Settings.Radius = 600;
	TestFalse(TEXT("Oversized portal rejected"), MiningHollow::Plan(Settings, Plan, Error));
	Settings = SampleRegion(); Settings.Points[2].Y = 2000;
	TestFalse(TEXT("Escaping outer rock rejected"), MiningHollow::Plan(Settings, Plan, Error));
	Settings = SampleRegion(); Settings.Points[2].Z = 200;
	TestFalse(TEXT("Ascending span rejected"), MiningHollow::Plan(Settings, Plan, Error));
	Settings = SampleRegion(); Settings.Plugs.Reset();
	TestTrue(TEXT("Zero plugs is a valid editable state"), MiningHollow::Plan(Settings, Plan, Error));
	FDynamicMesh3 Tube;
	TestTrue(TEXT("Closed outward tube"), MiningHollow::Tube(Plan.Points, Plan.Radius, Tube));
	TestTrue(TEXT("Tube follows Unreal generator winding"), Winding(Tube, FVector(3100, 50, 0)) < -0.99);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningHollowGeometryTest, "BrainMining.HollowRegion.GeometryAndChunks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningHollowGeometryTest::RunTest(const FString& Parameters)
{
	MiningHollow::FPlan Plan;
	FString Error;
	FDynamicMesh3 Rock;
	if (!TestTrue(TEXT("Plan and fill"), MiningHollow::Plan(SampleRegion(), Plan, Error) && MiningHollow::Fill(Plan, Rock, Error)))
	{
		AddError(Error);
		FMinimalBoxMeshGenerator Box;
		Box.Box = FOrientedBox3d(FVector3d(Plan.Bounds.GetCenter()), FVector3d(Plan.Bounds.GetExtent())); Box.Generate();
		FDynamicMesh3 Source(&Box), Cutter, Result;
		TArray<FVector> Points = MiningHollow::Slice(Plan, 0, Plan.PlugIntervals[0].X);
		Points[0] -= (Points[1] - Points[0]).GetSafeNormal() * Plan.Radius;
		MiningHollow::Tube(Points, Plan.Radius, Cutter);
		Source.DiscardAttributes(); Cutter.DiscardAttributes();
		FMeshBoolean Operation(&Source, &Cutter, &Result, FMeshBoolean::EBooleanOp::Difference);
		const bool bResult = Operation.Compute();
		AddInfo(FString::Printf(TEXT("Diagnostic box winding %.2f tube %.2f compute %d boundary %d triangles %d closed %d valid %d"), Winding(Source, Plan.Bounds.GetCenter()), Winding(Cutter, (Points[0] + Points.Last()) * 0.5), bResult, Operation.CreatedBoundaryEdges.Num(), Result.TriangleCount(), Result.IsClosed(), Result.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly)));
		return false;
	}
	TestTrue(TEXT("Watertight ordinary rock"), Rock.IsClosed());
	for (int32 Index = 1; Index < 20; ++Index)
	{
		const FVector Point = MiningHollow::Slice(Plan, Plan.Length * Index / 20, Plan.Length * Index / 20 + 1)[0];
		const bool bGuide = MiningHollow::IsGuide(Point, Plan.Guides);
		TestTrue(TEXT("Only plugs fill the hollow path"), bGuide ? Winding(Rock, Point) < -0.99 : FMath::Abs(Winding(Rock, Point)) < 0.01);
	}
	TestTrue(TEXT("Ordinary rock surrounds passage"), Winding(Rock, FVector(3600, 1000, 0)) < -0.99);
	TestFalse(TEXT("Side rock is not purple"), MiningHollow::IsGuide(FVector(3600, 1000, 0), Plan.Guides));
	TArray<FDynamicMesh3> Pieces;
	TArray<FBox> Bounds;
	if (!TestTrue(TEXT("Spatial chunk grid"), MiningChunks::SplitSpatial(Rock, 1600, Pieces, Bounds))) return false;
	TestTrue(TEXT("Budget unchanged"), Pieces.Num() <= 128);
	for (int32 Index = 0; Index < Pieces.Num(); ++Index)
	{
		TestTrue(TEXT("Chunk closed and valid"), Pieces[Index].IsClosed() && Pieces[Index].CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly));
		TestTrue(TEXT("All three dimensions bounded"), Bounds[Index].GetSize().GetMax() <= 1600.01);
		TestTrue(TEXT("Initial triangles below existing cap"), Pieces[Index].TriangleCount() < 30000);
	}
	const int32 Count = Pieces.Num();
	TestFalse(TEXT("Oversized grid rejected"), MiningChunks::SplitSpatial(Rock, 400, Pieces, Bounds));
	TestEqual(TEXT("Rejected split does not damage previous chunks"), Pieces.Num(), Count);
	const FVector Seam(3600, 0, -50);
	FSphereGenerator Sphere;
	Sphere.Radius = 180; Sphere.NumTheta = 16; Sphere.NumPhi = 12; Sphere.Generate();
	FDynamicMesh3 Cutter(&Sphere);
	for (int32 Vertex : Cutter.VertexIndicesItr()) Cutter.SetVertex(Vertex, Cutter.GetVertex(Vertex) + Seam);
	int32 Touched = 0;
	for (int32 Index = 0; Index < Pieces.Num(); ++Index)
	{
		if (!Bounds[Index].ExpandBy(180).IsInsideOrOn(Seam)) continue;
		FDynamicMesh3 Result;
		TestTrue(TEXT("Mine across spatial seams"), MiningChunks::Subtract(Pieces[Index], Cutter, Result));
		TestTrue(TEXT("No residual solid at cut center"), FMath::Abs(Winding(Result, Seam + FVector(2, 3, 4))) < 0.01);
		++Touched;
	}
	TestTrue(TEXT("Cut crosses multiple cells"), Touched >= 2);
	TArray<FMiningMapRegion> Regions;
	Regions.Add(MiningDeveloperMap::Box(TEXT("Rock"), EMiningMapRegionKind::RockRegion, 0, Plan.Bounds));
	Regions.Add(MiningDeveloperMap::Box(TEXT("Plug"), EMiningMapRegionKind::GuidePlug, 0, FBox(Seam - FVector(10), Seam + FVector(10))));
	TestEqual(TEXT("Map prefers plug over encompassing rock"), MiningDeveloperMap::FindRegion(Regions, Seam), 1);
	return true;
}
#endif
