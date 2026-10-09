#include "MiningHollowGeometry.h"
#include "MiningHollowBranches.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningHollowChainBoundsTest, "BrainMining.HollowChain.Bounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningHollowChainBoundsTest::RunTest(const FString& Parameters)
{
	TArray<FBox> Rooms;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FVector Floor(Index * 7200, 0, Index * -800);
		Rooms.Add(FBox(Floor - FVector(600, 600, 0), Floor + FVector(600, 600, 1000)));
	}
	TArray<MiningHollow::FPlan> Plans;
	FString Error;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		MiningHollow::FSettings Settings;
		Settings.FirstRoom = Rooms[Index];
		Settings.LastRoom = Rooms[Index + 1];
		Settings.Points = {FVector::ZeroVector, FVector::ZeroVector};
		if (!TestTrue(TEXT("Each region plans independently"), MiningHollow::Plan(Settings, Plans.AddDefaulted_GetRef(), Error))) return false;
	}
	TestTrue(TEXT("Four rooms / three separated regions accepted"), MiningHollow::ValidateChain(Plans, Rooms, 100, Error));
	TestTrue(TEXT("Legacy single region accepted"), MiningHollow::ValidateChain({Plans[0]}, {Rooms[0], Rooms[1]}, 100, Error));
	TestFalse(TEXT("Missing region rejected"), MiningHollow::ValidateChain({Plans[0]}, Rooms, 100, Error));
	TestFalse(TEXT("Oversized shared bedrock rejected"), MiningHollow::ValidateChain(Plans, Rooms, 700, Error));
	TArray<MiningHollow::FPlan> Overlap = Plans;
	Overlap[1].Bounds = Plans[0].Bounds;
	TestFalse(TEXT("Overlapping regions rejected"), MiningHollow::ValidateChain(Overlap, Rooms, 100, Error));
	Rooms[3] = Plans[0].Bounds;
	TestFalse(TEXT("Unrelated room cannot bypass progression"), MiningHollow::ValidateChain(Plans, Rooms, 100, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningHollowChainMapTest, "BrainMining.HollowChain.MapOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningHollowChainMapTest::RunTest(const FString& Parameters)
{
	MiningHollowBranches::FPlan Branch;
	Branch.Name = TEXT("Region three cave");
	Branch.Chamber = FBox(FVector(1000, 1000, 0), FVector(1900, 1900, 700));
	Branch.Points = {FVector(1000, 0, 300), FVector(1000, 1000, 300)};
	Branch.OpenPoints = Branch.Points;
	Branch.SealPoints = {Branch.Points[0], FVector(1000, 200, 300)};
	Branch.bHidden = true;
	TArray<FMiningMapRegion> Regions;
	MiningHollowBranches::AddMapRegions({Branch}, FVector(200, 300, 400), Regions, 2);
	TestTrue(TEXT("Branch, cave and hidden entrance are represented"), Regions.Num() >= 3);
	for (const FMiningMapRegion& Region : Regions) TestEqual(TEXT("Every cave map item belongs to its own progression segment"), Region.GroupIndex, 2);
	return true;
}
#endif
