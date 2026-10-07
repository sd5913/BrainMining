#include "MiningRoutePlanner.h"
#include "MiningDeveloperMapData.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningTunnelShapeTest, "BrainMining.TunnelShape.Sections", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningTunnelShapeTest::RunTest(const FString& Parameters)
{
	for (int32 Wall = 0; Wall < 6; ++Wall)
	{
		MiningRoutes::FPlan Plan;
		FString Error;
		const FVector Direction = MiningRoutes::WallDirection(Wall);
		const FBox First(FVector(-500), FVector(500));
		const FBox Second = First.ShiftBy(Direction * 6000);
		TestTrue(TEXT("All six opposite room faces supported"), MiningRoutes::Direct(First, Second, Wall, Wall ^ 1, Plan, Error));
		const TArray<FVector> Original = Plan.FaceCorners;
		const int32 SideAxis = Wall / 2 == 0 ? 1 : 0;
		FVector Offset = FVector::ZeroVector;
		Offset[SideAxis] = 1800;
		TArray<MiningRoutes::FShapeFrame> Frames = {{Direction * 4000 - Offset, 1600, 800}, {Direction * 2000 + Offset, 800, 1600}};
		TestTrue(TEXT("Unordered frames create a shaped tunnel"), MiningRoutes::Shape(Plan, Frames, Error));
		TestEqual(TEXT("Two frames create three convex pieces"), Plan.FaceSections.Num(), 3);
		TestEqual(TEXT("Endpoint faces are preserved"), Plan.FaceCorners, Original);
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			TestEqual(TEXT("First face remains full-sized"), Plan.FaceSections[0][Corner], Original[Corner]);
			TestEqual(TEXT("Last face remains full-sized"), Plan.FaceSections.Last()[Corner + 4], Original[Corner + 4]);
			TestEqual(TEXT("Sections share identical seams"), Plan.FaceSections[0][Corner + 4], Plan.FaceSections[1][Corner]);
		}
		TestTrue(TEXT("Bent route reports actual increased length"), Plan.Length > 5000);
		TArray<FMiningMapRegion> Regions;
		for (const TArray<FVector>& Corners : Plan.FaceSections) Regions.Add(MiningDeveloperMap::Convex(TEXT("Shape"), EMiningMapRegionKind::Tunnel, 0, Corners));
		TestTrue(TEXT("Map recognizes shaped frame"), MiningDeveloperMap::FindRegion(Regions, Frames[1].Center) != INDEX_NONE);
		TestEqual(TEXT("Map does not fill the empty space inside the bend"), MiningDeveloperMap::FindRegion(Regions, Direction * 2000), INDEX_NONE);
		FVector Anchor;
		TestTrue(TEXT("Branch resolves edited geometry"), MiningRoutes::BranchAnchor(Plan, Frames[1].Center + Offset * 0.3, false, 300, Anchor));
		TestTrue(TEXT("Branch attaches nearer actual bend than old direct line"), FMath::Abs(Anchor[SideAxis]) > 1000);
		const MiningRoutes::FPlan Valid = Plan;
		Frames[0].Center = Direction * 6000;
		TestFalse(TEXT("Control beyond endpoint rejected"), MiningRoutes::Shape(Plan, Frames, Error));
		TestEqual(TEXT("Rejected plan leaves points unchanged"), Plan.Points, Valid.Points);
		Frames[0] = Frames[1];
		TestFalse(TEXT("Coincident sections rejected"), MiningRoutes::Shape(Plan, Frames, Error));
		Frames.SetNum(1);
		Frames[0].Width = 100;
		TestFalse(TEXT("Too narrow section rejected"), MiningRoutes::Shape(Plan, Frames, Error));
	}
	return true;
}

#endif
