#include "MiningExternalGeometry.h"
#include "MiningChunkGeometry.h"
#include "Misc/AutomationTest.h"
#include "MeshQueries.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
using namespace UE::Geometry;

namespace
{
	double ExternalTestWinding(const FDynamicMesh3& Mesh, const FVector& Point)
	{
		double Angle = 0;
		for (int32 Triangle : Mesh.TriangleIndicesItr())
		{
			FVector3d A, B, C;
			Mesh.GetTriVertices(Triangle, A, B, C);
			A -= Point; B -= Point; C -= Point;
			Angle += 2 * FMath::Atan2(A.Dot(B.Cross(C)), A.Length() * B.Length() * C.Length() + A.Dot(B) * C.Length() + B.Dot(C) * A.Length() + C.Dot(A) * B.Length());
		}
		return FMath::Abs(Angle / (4 * UE_DOUBLE_PI));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExternalShellTest, "BrainMining.ExternalMining.SolidAndShell", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FExternalShellTest::RunTest(const FString& Parameters)
{
	FDynamicMesh3 Rock, Shell;
	FString Error;
	if (!TestTrue(TEXT("Build a solid rock with independent shell"), MiningExternal::Build(FVector(2400), 150, {}, Rock, Shell, Error))) { AddError(Error); return false; }
	TestTrue(TEXT("Solid center is diggable rock"), ExternalTestWinding(Rock, FVector::ZeroVector) > 0.99);
	TestTrue(TEXT("Shell does not fill interior"), ExternalTestWinding(Shell, FVector::ZeroVector) < 0.01);
	for (const FVector& Axis : {FVector(1,0,0), FVector(-1,0,0), FVector(0,1,0), FVector(0,-1,0), FVector(0,0,1), FVector(0,0,-1)})
	{
		TestTrue(TEXT("All six sides contain bedrock"), ExternalTestWinding(Shell, Axis * 1275) > 0.99);
		TestTrue(TEXT("Exterior remains empty"), ExternalTestWinding(Shell, Axis * 1400) < 0.01);
	}
	TestTrue(TEXT("Both meshes closed"), Rock.IsClosed() && Shell.IsClosed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExternalPortsTest, "BrainMining.ExternalMining.PortsAndInteriorCavity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FExternalPortsTest::RunTest(const FString& Parameters)
{
	FString Error;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		FVector ToolSize(600); ToolSize[Axis] = 3200;
		FDynamicMesh3 Rock, Shell;
		if (!TestTrue(TEXT("Through passage boolean"), MiningExternal::Build(FVector(2400), 150, {MiningExternal::Box(ToolSize)}, Rock, Shell, Error))) { AddError(Error); return false; }
		FVector Direction = FVector::ZeroVector; Direction[Axis] = 1;
		for (double Sign : {-1., 1.})
		{
			TestTrue(TEXT("Port removes rock"), ExternalTestWinding(Rock, Direction * Sign * 1100) < 0.01);
			TestTrue(TEXT("Port pierces full shell"), ExternalTestWinding(Shell, Direction * Sign * 1275) < 0.01);
			FVector Nearby = Direction * Sign * 1275; Nearby[(Axis + 1) % 3] = 600;
			TestTrue(TEXT("Bedrock beside port remains solid"), ExternalTestWinding(Shell, Nearby) > 0.99);
		}
		TestTrue(TEXT("Rock cavity center empty"), ExternalTestWinding(Rock, FVector::ZeroVector) < 0.01);
	}
	FDynamicMesh3 Rock, Shell;
	if (!TestTrue(TEXT("Interior chamber boolean"), MiningExternal::Build(FVector(2400), 150, {MiningExternal::Box(FVector(800))}, Rock, Shell, Error))) { AddError(Error); return false; }
	TestTrue(TEXT("Interior chamber empty"), ExternalTestWinding(Rock, FVector::ZeroVector) < 0.01);
	TestTrue(TEXT("Interior chamber does not open shell"), ExternalTestWinding(Shell, FVector(1275,0,0)) > 0.99);
	// Two touching/overlapping independent solids are subtracted sequentially, not appended into an invalid soup.
	FDynamicMesh3 Branch = MiningExternal::Box(FVector(600,1800,600));
	for (int32 V : Branch.VertexIndicesItr()) Branch.SetVertex(V, Branch.GetVertex(V) + FVector3d(0,500,0));
	TestTrue(TEXT("Multiple overlapping cutters supported"), MiningExternal::Build(FVector(2400), 150, {MiningExternal::Box(FVector(3200,600,600)), Branch}, Rock, Shell, Error));
	TestTrue(TEXT("Branch carved"), ExternalTestWinding(Rock, FVector(0,800,0)) < 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExternalValidationTest, "BrainMining.ExternalMining.InvalidCutterRollback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FExternalValidationTest::RunTest(const FString& Parameters)
{
	FString Error;
	FDynamicMesh3 Reversed = MiningExternal::Box(FVector(3200,600,600));
	Reversed.ReverseOrientation();
	TestTrue(TEXT("Reversed normals repaired"), MiningExternal::PrepareCutter(Reversed, Error));
	TestTrue(TEXT("Volume orientation agrees with UE"), TMeshQueries<FDynamicMesh3>::GetVolumeArea(Reversed).X * TMeshQueries<FDynamicMesh3>::GetVolumeArea(MiningExternal::Box(FVector(10))).X > 0);
	FDynamicMesh3 Rock, Shell;
	if (!MiningExternal::Build(FVector(2400),150,{Reversed},Rock,Shell,Error)) { AddError(Error); return false; }
	FDynamicMesh3 SavedRock = Rock, SavedShell = Shell;
	FDynamicMesh3 Open = MiningExternal::Box(FVector(600));
	Open.RemoveTriangle(0);
	TestFalse(TEXT("Open tube rejected"), MiningExternal::Build(FVector(2400),150,{Open},Rock,Shell,Error));
	TestTrue(TEXT("Rejected input preserves both previous outputs"), Rock.IsSameAs(SavedRock, FDynamicMesh3::FSameAsOptions()) && Shell.IsSameAs(SavedShell, FDynamicMesh3::FSameAsOptions()));
	TestFalse(TEXT("Nonfinite dimensions rejected"), MiningExternal::Build(FVector(std::numeric_limits<double>::quiet_NaN(),1000,1000),150,{},Rock,Shell,Error));
	TestFalse(TEXT("Zero shell thickness rejected"), MiningExternal::Build(FVector(2400),0,{},Rock,Shell,Error));
	TestFalse(TEXT("Removing whole world rejected"), MiningExternal::Build(FVector(2400),150,{MiningExternal::Box(FVector(4000))},Rock,Shell,Error));
	FDynamicMesh3 Soup = MiningExternal::Box(FVector(800));
	FDynamicMesh3 Other = MiningExternal::Box(FVector(800));
	TMap<int32,int32> Map;
	for (int32 V : Other.VertexIndicesItr()) Map.Add(V, Soup.AppendVertex(Other.GetVertex(V) + FVector3d(200,0,0)));
	for (int32 T : Other.TriangleIndicesItr()) { const FIndex3i I = Other.GetTriangle(T); Soup.AppendTriangle(Map[I.A],Map[I.B],Map[I.C]); }
	TestFalse(TEXT("Joined overlapping solids rejected as self intersecting"), MiningExternal::PrepareCutter(Soup,Error));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExternalSpatialCapacityTest, "BrainMining.ExternalMining.LargeSpatialCapacity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FExternalSpatialCapacityTest::RunTest(const FString& Parameters)
{
	FIntVector Counts;
	// Actual Blender route bounds, padded with rock around the walls.
	TestTrue(TEXT("Full 147m route fits at default 16m chunk size"), MiningChunks::SpatialChunkCounts(FVector(15400,14800,10600),1600,Counts));
	TestEqual(TEXT("Padded route requires 700 chunks"), Counts.X * Counts.Y * Counts.Z, 700);
	TestTrue(TEXT("Current wide route fits at unchanged 16m chunk size"), MiningChunks::SpatialChunkCounts(FVector(30943.187501,21456.30882,19130.803617),1600,Counts));
	TestEqual(TEXT("Current wide route requires 3360 chunks"), Counts.X * Counts.Y * Counts.Z, 3360);
	TestTrue(TEXT("Exact 8192-cell limit accepted"), MiningChunks::SpatialChunkCounts(FVector(12800,12800,3200),400,Counts));
	TestEqual(TEXT("Grid reaches the new limit"), Counts.X * Counts.Y * Counts.Z, MiningChunks::MaxSpatialChunks);
	TestFalse(TEXT("Above capacity rejected before allocation"), MiningChunks::SpatialChunkCounts(FVector(13200,12800,3200),400,Counts));
	TestFalse(TEXT("Nonfinite size rejected"), MiningChunks::SpatialChunkCounts(FVector(std::numeric_limits<double>::infinity(),400,400),400,Counts));
	TArray<FDynamicMesh3> Pieces;
	TArray<FBox> Bounds;
	if (!TestTrue(TEXT("Spatial boolean splitting works beyond former 2048 limit"), MiningChunks::SplitExternalSpatial(MiningExternal::Box(FVector(6400,6400,4800)),400,Pieces,Bounds))) return false;
	TestEqual(TEXT("All 3072 cells generated"), Pieces.Num(), 3072);
	for (const FDynamicMesh3& Piece : Pieces) TestTrue(TEXT("Large-grid cells remain closed"), Piece.IsClosed());
	return true;
}
#endif
