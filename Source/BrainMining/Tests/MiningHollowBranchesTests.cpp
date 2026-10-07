#include "MiningHollowBranches.h"
#include "MiningChunkGeometry.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace UE::Geometry;

namespace
{
	double BranchWinding(const FDynamicMesh3& Mesh, const FVector& Point)
	{
		double Angle = 0;
		for (int32 Triangle : Mesh.TriangleIndicesItr())
		{
			FVector3d First, Second, Third;
			Mesh.GetTriVertices(Triangle, First, Second, Third);
			First -= Point; Second -= Point; Third -= Point;
			Angle += 2 * FMath::Atan2(First.Dot(Second.Cross(Third)), First.Length() * Second.Length() * Third.Length() + First.Dot(Second) * Third.Length() + Second.Dot(Third) * First.Length() + Third.Dot(First) * Second.Length());
		}
		return Angle / (4 * UE_DOUBLE_PI);
	}

	bool BranchFixture(MiningHollow::FPlan& Main, TArray<MiningHollowBranches::FSettings>& Settings, FString& Error)
	{
		MiningHollow::FSettings Input;
		Input.FirstRoom = FBox(FVector(-600, -600, 0), FVector(600, 600, 1000));
		Input.LastRoom = FBox(FVector(6600, -600, -800), FVector(7800, 600, 200));
		Input.Points = {FVector(600, 0, 350), FVector(6600, 0, -450)};
		Input.Width = 4600;
		if (!MiningHollow::Plan(Input, Main, Error)) return false;
		for (int32 Index = 0; Index < 2; ++Index)
		{
			MiningHollowBranches::FSettings& Branch = Settings.AddDefaulted_GetRef();
			Branch.Name = Index == 0 ? TEXT("Open") : TEXT("Hidden");
			Branch.Connection = Index == 0 ? 0.14 : 0.60;
			Branch.bHidden = Index == 1;
			const FVector Anchor = MiningHollow::Slice(Main, Main.Length * Branch.Connection, Main.Length * Branch.Connection + 1)[0];
			const FVector Floor = Anchor + FVector(0, Index == 0 ? 1500 : -1500, -Main.Radius);
			Branch.Chamber = FBox(Floor - FVector(450, 450, 0), Floor + FVector(450, 450, 700));
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowBranchesGeometryTest, "BrainMining.HollowBranches.GeometryOwnershipAndMap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHollowBranchesGeometryTest::RunTest(const FString& Parameters)
{
	MiningHollow::FPlan Main;
	TArray<MiningHollowBranches::FSettings> Inputs;
	TArray<MiningHollowBranches::FPlan> Plans;
	FString Error;
	FDynamicMesh3 Rock;
	if (!TestTrue(TEXT("Generate open and hidden branches"), BranchFixture(Main, Inputs, Error) && MiningHollowBranches::Plan(Main, Inputs, Plans, Error) && MiningHollow::Fill(Main, Rock, Error) && MiningHollowBranches::Carve(Plans, Rock, Error))) { AddError(Error); return false; }
	TestTrue(TEXT("Region remains watertight"), Rock.IsClosed());
	for (const MiningHollowBranches::FPlan& Branch : Plans)
	{
		TestTrue(TEXT("Chamber is an empty cavity"), FMath::Abs(BranchWinding(Rock, Branch.Chamber.GetCenter())) < 0.01);
		TestTrue(TEXT("Chamber side is ordinary same rock"), BranchWinding(Rock, Branch.Chamber.GetCenter() + FVector(Branch.Chamber.GetExtent().X + 35, 0, 0)) < -0.99);
		TestTrue(TEXT("Open branch section is empty"), FMath::Abs(BranchWinding(Rock, (Branch.OpenPoints[0] + Branch.OpenPoints.Last()) * 0.5)) < 0.01);
		if (Branch.bHidden) TestTrue(TEXT("Hidden entrance is solid ordinary rock"), BranchWinding(Rock, (Branch.SealPoints[0] + Branch.SealPoints.Last()) * 0.5) < -0.99);
	}
	TArray<FDynamicMesh3> Chunks;
	TArray<FBox> Bounds;
	TestTrue(TEXT("Spatial chunking succeeds under unchanged budget"), MiningChunks::SplitSpatial(Rock, 1600, Chunks, Bounds));
	TestTrue(TEXT("At most 128 chunks"), Chunks.Num() <= 128);
	for (const FDynamicMesh3& Chunk : Chunks) TestTrue(TEXT("Closed chunk under 30000 triangles"), Chunk.IsClosed() && Chunk.TriangleCount() <= 30000);
	TArray<FMiningMapRegion> Map;
	Map.Add(MiningDeveloperMap::Box(TEXT("Rock"), EMiningMapRegionKind::RockRegion, 0, Main.Bounds));
	MiningHollowBranches::AddMapRegions(Plans, FVector(100, 200, 300), Map);
	for (const FMiningMapRegion& Item : Map) TestEqual(TEXT("All additions belong to same progression group"), Item.GroupIndex, 0);
	for (const MiningHollowBranches::FPlan& Branch : Plans)
	{
		const int32 Found = MiningDeveloperMap::FindRegion(Map, Branch.Chamber.GetCenter() + FVector(100, 200, 300));
		TestTrue(TEXT("Map identifies chamber instead of outer rock"), Map.IsValidIndex(Found) && Map[Found].Kind == EMiningMapRegionKind::Space);
	}
	TestTrue(TEXT("Map contains hidden seal"), Map.ContainsByPredicate([](const FMiningMapRegion& Item) { return Item.Kind == EMiningMapRegionKind::HiddenEntrance && Item.bRockFilled; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHollowBranchesValidationTest, "BrainMining.HollowBranches.ValidationRollback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHollowBranchesValidationTest::RunTest(const FString& Parameters)
{
	MiningHollow::FPlan Main;
	TArray<MiningHollowBranches::FSettings> Inputs;
	TArray<MiningHollowBranches::FPlan> Plans;
	FString Error;
	if (!BranchFixture(Main, Inputs, Error) || !MiningHollowBranches::Plan(Main, Inputs, Plans, Error)) { AddError(Error); return false; }
	const TArray<MiningHollowBranches::FSettings> Original = Inputs;
	const FVector OriginalCenter = Plans[0].Chamber.GetCenter();
	Inputs[0].Chamber = Inputs[0].Chamber.ShiftBy(FVector(0, 10000, 0));
	TestFalse(TEXT("Out of bounds rejected"), MiningHollowBranches::Plan(Main, Inputs, Plans, Error));
	TestEqual(TEXT("Old successful plan preserved"), Plans[0].Chamber.GetCenter(), OriginalCenter);
	Inputs = Original; Inputs[1].Chamber = Inputs[0].Chamber; Inputs[1].Connection = Inputs[0].Connection;
	TestFalse(TEXT("Overlapping caves rejected"), MiningHollowBranches::Plan(Main, Inputs, Plans, Error));
	Inputs = Original; Inputs[0].Chamber = Inputs[0].Chamber.ShiftBy(FVector(0, -1300, 0));
	TestFalse(TEXT("Cave cannot bypass hidden entrance by touching main"), MiningHollowBranches::Plan(Main, Inputs, Plans, Error));
	Inputs = Original; Inputs[1].Thickness = 600;
	TestFalse(TEXT("Seal cannot overlap cave"), MiningHollowBranches::Plan(Main, Inputs, Plans, Error));
	Inputs = Original; Inputs[0].InteriorPoints.Add(Plans[0].Points[0] + FVector(0, -500, 0));
	TestFalse(TEXT("Backtracking branch rejected"), MiningHollowBranches::Plan(Main, Inputs, Plans, Error));
	Inputs = Original; Inputs[0].InteriorPoints.Add(Plans[0].Points[0] + FVector(100, 850, 0));
	TestTrue(TEXT("Gentle manual branch bend accepted"), MiningHollowBranches::Plan(Main, Inputs, Plans, Error));
	FDynamicMesh3 Rock;
	TestTrue(TEXT("Bent branch mesh and chamber booleans"), MiningHollow::Fill(Main, Rock, Error) && MiningHollowBranches::Carve(Plans, Rock, Error));
	Inputs = Original;
	Inputs[0].Radius = 400;
	Inputs[0].InteriorPoints.Add(FVector(Inputs[0].Chamber.GetCenter().X + 100, Inputs[0].Chamber.GetCenter().Y - 100, Inputs[0].Chamber.Min.Z + 400));
	TestFalse(TEXT("Short tight bend cannot fold the tube into itself"), MiningHollowBranches::Plan(Main, Inputs, Plans, Error));
	TestTrue(TEXT("Disabling/deleting all caves supported"), MiningHollowBranches::Plan(Main, {}, Plans, Error));
	TestTrue(TEXT("No stale plans after deletion"), Plans.IsEmpty());
	return true;
}
#endif
