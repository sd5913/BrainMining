#include "MiningToolEffectsComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningDrillSpinPivotTest, "BrainMining.Tools.DrillShaftAxisAndPivot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningDrillSpinPivotTest::RunTest(const FString& Parameters)
{
	for (const FVector Scale : {FVector(.72), FVector(.72, -.72, .72)})
	{
		// A moved, tilted model as authored in the character blueprint viewport.
		const FTransform Pose(FRotator(-10, 27, 18), FVector(-21.432637, 10, -16.375114), Scale);
		for (const float Angle : {0.f, 45.f, 90.f, 180.f, 270.f, 360.f, -90.f})
		{
			const FTransform Spin = UMiningToolEffectsComponent::BuildDrillSpinTransform(Pose, FVector::ForwardVector, FVector::ZeroVector, Angle);
			for (float Along : {-30.f, 0.f, 20.f})
			{
				const FVector ShaftPoint = Pose.TransformPosition(FVector(Along, 0, 0));
				TestTrue(TEXT("Every point along the tilted shaft stays fixed; model cannot orbit the mount"), Spin.TransformPosition(ShaftPoint).Equals(ShaftPoint, .0001));
			}
			const FVector OffAxis = Pose.TransformPosition(FVector(0, 6, 3));
			TestTrue(TEXT("Rotation preserves blade distance from the shaft"), FMath::IsNearlyEqual(
				FVector::Distance(Spin.TransformPosition(OffAxis), Pose.GetLocation()), FVector::Distance(OffAxis, Pose.GetLocation()), .0001));
			if (Angle == 90.f) TestFalse(TEXT("Blades actually spin while their shaft remains fixed"), Spin.TransformPosition(OffAxis).Equals(OffAxis, .001));
			if (Angle == 360.f) TestTrue(TEXT("One turn returns geometry to its original position"), Spin.TransformPosition(OffAxis).Equals(OffAxis, .0001));
		}
	}
	const FTransform Pose(FRotator(12, -9, 0), FVector(-30, 4, -18), FVector(.7));
	const FVector Pivot(2, 3, -4);
	const FTransform Spin = UMiningToolEffectsComponent::BuildDrillSpinTransform(Pose, FVector::UpVector, Pivot, 90);
	TestTrue(TEXT("Replacement mesh can use a custom shaft axis and off-origin pivot"),
		Spin.TransformPosition(Pose.TransformPosition(Pivot + FVector(0, 0, 20))).Equals(Pose.TransformPosition(Pivot + FVector(0, 0, 20)), .0001));
	return true;
}
#endif
