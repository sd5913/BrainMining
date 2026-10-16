#include "MiningComponent.h"
#include "MiningPickaxeGeometry.h"
#include "DiggableRock.h"
#include "GeneratedMiningRock.h"
#include "Components/DynamicMeshComponent.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "UDynamicMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "HAL/PlatformProcess.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace UE::Geometry;
namespace
{
double PickaxeVolume(const FDynamicMesh3& Mesh)
{
	double Volume = 0;
	for (int32 Triangle : Mesh.TriangleIndicesItr())
	{
		const FIndex3i Vertices = Mesh.GetTriangle(Triangle);
		Volume += Mesh.GetVertex(Vertices.A).Dot(Mesh.GetVertex(Vertices.B).Cross(Mesh.GetVertex(Vertices.C))) / 6.;
	}
	return -Volume;
}

UMiningComponent* CreatePickaxe(UWorld* World)
{
	AActor* Owner = World->SpawnActor<AActor>();
	auto* Mining = NewObject<UMiningComponent>(Owner);
	Owner->AddInstanceComponent(Mining);
	Mining->RegisterComponent();
	Mining->Radius = 100;
	Mining->DownwardOffset = 0;
	return Mining;
}

FHitResult PickaxeHit(ADiggableRock* Rock, FVector Point, FVector Normal = -FVector::ForwardVector)
{
	FHitResult Hit(Rock, Rock->RockMesh, Point, Normal);
	Hit.bBlockingHit = true;
	Hit.ImpactPoint = Point;
	Hit.ImpactNormal = Normal;
	return Hit;
}

bool PickaxeBlocked(ADiggableRock* Rock, double Across, double Up)
{
	FHitResult Hit;
	return Rock->ActorLineTraceSingle(Hit, FVector(-300, Across, Up), FVector(300, Across, Up), ECC_Visibility,
		FCollisionQueryParams(SCENE_QUERY_STAT(PickaxeTest), true));
}

FHitResult PickaxeSurface(ADiggableRock* Rock, double Across = 0, double Up = 0)
{
	FHitResult Surface;
	if (!Rock->ActorLineTraceSingle(Surface, FVector(-1200, Across, Up), FVector(1200, Across, Up), ECC_Visibility,
		FCollisionQueryParams(SCENE_QUERY_STAT(PickaxeProgressTest), true))) return FHitResult();
	return PickaxeHit(Rock, Surface.ImpactPoint, Surface.ImpactNormal);
}

void DrainPickaxe(AGeneratedMiningRock* Rock)
{
	const double Deadline = FPlatformTime::Seconds() + 10;
	while (Rock->bCutPending && FPlatformTime::Seconds() < Deadline) { FPlatformProcess::Sleep(.001f); Rock->Tick(.01f); }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningPickaxeThreeHitsTest, "BrainMining.Pickaxe.ThreeHitsAndSquareCollision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningPickaxeThreeHitsTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* Rock = World->SpawnActor<ADiggableRock>();
	Rock->RockSize = FVector(50, 1000, 1000);
	Rock->ResetRock();
	auto* Mining = CreatePickaxe(World);
	const FDynamicMesh3 Original = Rock->RockMesh->GetDynamicMesh()->GetMeshRef();
	const FHitResult First = PickaxeHit(Rock, FVector(-25, 0, 0));
	TestTrue(TEXT("Thin wall initially blocks the player direction"), PickaxeBlocked(Rock, 0, 0));
	TestTrue(TEXT("First strike lands"), Mining->ApplyPickaxeHit(First));
	TestEqual(TEXT("First strike is local progress 1 of 3"), Mining->TargetPickaxeHits, 1);
	TestEqual(TEXT("First strike performs one real surface excavation"), Rock->SuccessfulCuts, 1);
	TestFalse(TEXT("First strike deforms the actual terrain mesh"), Original.IsSameAs(Rock->RockMesh->GetDynamicMesh()->GetMeshRef(), FDynamicMesh3::FSameAsOptions()));
	const double FirstVolume = PickaxeVolume(Rock->RockMesh->GetDynamicMesh()->GetMeshRef());
	const double FirstDepth = PickaxeSurface(Rock).ImpactPoint.X;
	TestTrue(TEXT("First strike makes a shallow dent without penetrating the thin wall"), FirstDepth > -25 && FirstDepth < 0);
	TArray<UDynamicMeshComponent*> Marks;
	Rock->GetComponents(Marks);
	TestEqual(TEXT("Damage uses the terrain itself without overlay marks"), Marks.Num(), 1);
	TestTrue(TEXT("Second strike tolerates aim movement on the deformed surface"), Mining->ApplyPickaxeHit(PickaxeSurface(Rock, 15, 8)));
	TestEqual(TEXT("Nearby aim shares the same progress"), Mining->TargetPickaxeHits, 2);
	TestTrue(TEXT("Second strike still blocks the thin wall"), PickaxeBlocked(Rock, 0, 0));
	TestTrue(TEXT("Second strike removes more material"), PickaxeVolume(Rock->RockMesh->GetDynamicMesh()->GetMeshRef()) < FirstVolume);
	TestTrue(TEXT("Second strike deepens the same cavity"), PickaxeSurface(Rock).ImpactPoint.X > FirstDepth);
	TestTrue(TEXT("Third strike breaks the selected area"), Mining->ApplyPickaxeHit(PickaxeSurface(Rock)));
	TestEqual(TEXT("Three strikes commit three progressive excavations"), Rock->SuccessfulCuts, 3);
	TestFalse(TEXT("Centre of hole has no remaining wall collision"), PickaxeBlocked(Rock, 0, 0));
	TestFalse(TEXT("Opening retains broad squared shoulders"), PickaxeBlocked(Rock, 65, 65));
	TestTrue(TEXT("Rock just beyond the square stays solid"), PickaxeBlocked(Rock, 120, 0));
	TestTrue(TEXT("Square excavation remains watertight"), Rock->RockMesh->GetDynamicMesh()->GetMeshRef().IsClosed());
	Marks.Reset(); Rock->GetComponents(Marks);
	TestEqual(TEXT("Breaking removes the floating surface mark"), Marks.Num(), 1);
	Mining->ResetField();
	TestEqual(TEXT("Reset clears damage progress"), Mining->TargetPickaxeHits, 0);
	TestTrue(TEXT("Reset restores the original collision"), PickaxeBlocked(Rock, 0, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningPickaxeLocalProgressTest, "BrainMining.Pickaxe.LocalProgressAndReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningPickaxeLocalProgressTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* Rock = World->SpawnActor<ADiggableRock>();
	Rock->RockSize = FVector(1000, 2000, 1000); Rock->ResetRock();
	auto* Mining = CreatePickaxe(World);
	const FHitResult First = PickaxeHit(Rock, FVector(-500, 0, 0));
	const FHitResult Other = PickaxeHit(Rock, FVector(-500, 600, 0));
	Mining->ApplyPickaxeHit(First); Mining->ApplyPickaxeHit(PickaxeSurface(Rock));
	Mining->ApplyPickaxeHit(Other);
	TestFalse(TEXT("Two hits on A do not make B break in one hit"), Mining->bLastPickaxeBreak);
	TestEqual(TEXT("B starts its own damage state"), Mining->TargetPickaxeHits, 1);
	TestTrue(TEXT("Returning to A finishes its third hit"), Mining->ApplyPickaxeHit(PickaxeSurface(Rock)));
	TestTrue(TEXT("Returning to A completes its existing excavation"), Mining->bLastPickaxeBreak);
	Mining->ApplyPickaxeHit(PickaxeSurface(Rock, 600));
	TestEqual(TEXT("An unrelated cut preserves B progress"), Mining->TargetPickaxeHits, 2);
	Rock->ResetRock();
	Mining->ApplyPickaxeHit(Other);
	TestEqual(TEXT("Direct terrain reset clears stored damage"), Mining->TargetPickaxeHits, 1);
	Mining->SetInputSuspended(true);
	TestFalse(TEXT("Upgrade panel blocks a pending strike"), Mining->ApplyPickaxeHit(Other));
	Mining->SetInputSuspended(false);
	Mining->PushEncounterBlock();
	TestFalse(TEXT("Encounter block prevents mining"), Mining->ApplyPickaxeHit(Other));
	Mining->PopEncounterBlock();
	Mining->ActivateDrill();
	TestFalse(TEXT("Drill never uses three-hit pickaxe damage"), Mining->ApplyPickaxeHit(Other));
	Mining->SelectPickaxe();
	Mining->ApplyPickaxeHit(PickaxeSurface(Rock, 600));
	TestEqual(TEXT("Switching tools alone preserves local progress"), Mining->TargetPickaxeHits, 2);
	Rock->MineBlast(FVector(-470, 600, 0), 140, .2f, 42);
	Mining->ApplyPickaxeHit(PickaxeSurface(Rock, 600));
	TestEqual(TEXT("Excavating the damaged surface clears stale progress"), Mining->TargetPickaxeHits, 1);
	Mining->ResetField();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningPickaxeChunkTest, "BrainMining.Pickaxe.ChunkBoundariesAndDepthUpgrade", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningPickaxeChunkTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	const FTransform Frame(FRotator(0, 0, 0), FVector(-670, 0, 0));
	const FVector HalfSize(100, 100, 100);
	const FDynamicMesh3 Cutter = MiningPickaxe::MakeCutter(Frame, HalfSize);
	TestTrue(TEXT("Square cutter is closed"), Cutter.IsClosed());
	TestTrue(TEXT("Square cutter winding faces outwards"), PickaxeVolume(Cutter) > 0);
	TestTrue(TEXT("Square cutter is a valid manifold"), Cutter.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly));
	TestTrue(TEXT("Same seed preserves the fracture pattern"), Cutter.IsSameAs(MiningPickaxe::MakeCutter(Frame, HalfSize), FDynamicMesh3::FSameAsOptions()));
	TestFalse(TEXT("Different patches have different silhouettes"), Cutter.IsSameAs(MiningPickaxe::MakeCutter(Frame, HalfSize, 37), FDynamicMesh3::FSameAsOptions()));
	for (int32 Seed = 0; Seed < 24; ++Seed)
	{
		const FDynamicMesh3 Variant = MiningPickaxe::MakeCutter(Frame, HalfSize, Seed);
		TestTrue(TEXT("Randomized cutters remain closed and valid"), Variant.IsClosed() && Variant.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly));
	}
	auto* Single = World->SpawnActor<ADiggableRock>();
	Single->RockSize = FVector(1400, 1600, 1600); Single->ResetRock();
	TestTrue(TEXT("Unchunked rotated square cut succeeds"), Single->MinePickaxe(FTransform(FRotator(0, 12, 15), Frame.GetLocation()), HalfSize));
	const double Removed = 1400. * 1600. * 1600. - PickaxeVolume(Single->RockMesh->GetDynamicMesh()->GetMeshRef());
	FMinimalBoxMeshGenerator Box;
	Box.Box = FOrientedBox3d(FVector3d::Zero(), FVector3d(700, 800, 800)); Box.Generate();
	auto* Chunked = World->SpawnActor<AGeneratedMiningRock>();
	TestTrue(TEXT("Chunked fixture builds"), Chunked->SetChunkedMesh(FDynamicMesh3(&Box), 400, true));
	Chunked->InteractiveCutBudgetMilliseconds = .01f;
	TestTrue(TEXT("Square cut spanning chunks is accepted"), Chunked->MinePickaxe(FTransform(FRotator(0, 12, 15), Frame.GetLocation()), HalfSize));
	double Deadline = FPlatformTime::Seconds() + 10;
	while (Chunked->bCutPending && FPlatformTime::Seconds() < Deadline) { FPlatformProcess::Sleep(.001f); Chunked->Tick(.01f); }
	TestFalse(TEXT("Queued square cut finishes"), Chunked->bCutPending);
	TestEqual(TEXT("One request becomes one excavation"), Chunked->SuccessfulCuts, 1);
	TestTrue(TEXT("Cut reaches multiple terrain chunks"), Chunked->LastTouchedChunks > 1);
	FDynamicMesh3 Combined; Chunked->CopyMiningMesh(Combined);
	TestTrue(TEXT("Chunk borders preserve the complete square volume"), FMath::Abs((1400. * 1600. * 1600. - PickaxeVolume(Combined)) - Removed) < Removed * .002);
	for (const auto& Chunk : Chunked->Chunks) TestTrue(TEXT("Every chunk remains watertight"), Chunk.Mesh->GetDynamicMesh()->GetMeshRef().IsClosed());
	Chunked->SegmentIndex = 0; Chunked->SetSegmentState(EMiningSegmentState::Frozen);
	TestFalse(TEXT("Square excavation respects segment gates"), Chunked->MinePickaxe(Frame, HalfSize));
	Chunked->SetSegmentState(EMiningSegmentState::Active); Chunked->ResetRock();
	auto* Mining = CreatePickaxe(World); Mining->DepthMultiplier = 2;
	for (int32 Strike = 1; Strike <= 3; ++Strike)
	{
		const auto Hit = PickaxeSurface(Chunked);
		TestTrue(TEXT("Each upgraded strike submits a progressive cut"), Mining->ApplyPickaxeHit(Hit));
		if (Chunked->bCutPending) TestFalse(TEXT("Extra strikes cannot duplicate a pending stage"), Mining->ApplyPickaxeHit(Hit));
		DrainPickaxe(Chunked);
		TestEqual(TEXT("Each progressive stage commits once"), Chunked->SuccessfulCuts, Strike);
	}
	Deadline = FPlatformTime::Seconds() + 10;
	while (Chunked->bCutPending && FPlatformTime::Seconds() < Deadline) { FPlatformProcess::Sleep(.001f); Chunked->Tick(.01f); }
	TestEqual(TEXT("Depth upgrade completes three coherent stages"), Chunked->SuccessfulCuts, 3);
	FHitResult Back;
	TestTrue(TEXT("Deeper rock is still available to mine"), Chunked->ActorLineTraceSingle(Back, FVector(-900, 0, 0), FVector(700, 0, 0), ECC_Visibility, FCollisionQueryParams(SCENE_QUERY_STAT(PickaxeDepth), true)));
	TestTrue(TEXT("Depth upgrade doubles forward reach of the cavity"), Back.ImpactPoint.X > -455 && Back.ImpactPoint.X < -420);
	TestTrue(TEXT("New back-wall strike is accepted"), Mining->ApplyPickaxeHit(PickaxeHit(Chunked, Back.ImpactPoint, Back.ImpactNormal)));
	TestEqual(TEXT("New rock at the back restarts at one hit"), Mining->TargetPickaxeHits, 1);
	Mining->ResetField();
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningPickaxeConfiguredStagesTest, "BrainMining.Pickaxe.ConfiguredStagesAndSnapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningPickaxeConfiguredStagesTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Rock = World->SpawnActor<ADiggableRock>();
    Rock->RockSize = FVector(50, 1000, 1000);
    Rock->ResetRock();
    auto* Mining = CreatePickaxe(World);
    auto Three = UMiningPickaxeSettings::MakeRecommendedLevels()[0];
    Three.bUseHardness=false; // Explicit coverage of the retained legacy stage API.
    Three.Width = 200; Three.Depth = 130;
    auto Two = Three;
    Two.Stages = UMiningPickaxeSettings::MakeRecommendedLevels()[4].Stages;
    TestTrue(TEXT("Three-hit profile applies"), Mining->ApplyPickaxeBalance(Three));
    TestTrue(TEXT("First hit cuts real terrain"), Mining->ApplyPickaxeHit(PickaxeSurface(Rock)));
    TestTrue(TEXT("Thin wall survives first hit"), PickaxeBlocked(Rock, 0, 0));
    TestTrue(TEXT("Upgrade can change profile mid-cavity"), Mining->ApplyPickaxeBalance(Two));
    TestTrue(TEXT("Second hit continues existing cavity"), Mining->ApplyPickaxeHit(PickaxeSurface(Rock)));
    TestFalse(TEXT("Existing cavity keeps its original three-hit requirement"), Mining->bLastPickaxeBreak);
    TestTrue(TEXT("Existing thin wall survives second hit after upgrade"), PickaxeBlocked(Rock, 0, 0));
    TestTrue(TEXT("Third hit finishes old cavity"), Mining->ApplyPickaxeHit(PickaxeSurface(Rock)));
    TestTrue(TEXT("Old cavity opens on third hit"), Mining->bLastPickaxeBreak && !PickaxeBlocked(Rock, 0, 0));
    Mining->ResetField();
    TestTrue(TEXT("New cavity starts with two-hit profile"), Mining->ApplyPickaxeHit(PickaxeSurface(Rock)));
    TestTrue(TEXT("First of two hits retains wall"), PickaxeBlocked(Rock, 0, 0));
    TestTrue(TEXT("Second configured hit succeeds"), Mining->ApplyPickaxeHit(PickaxeSurface(Rock)));
    TestTrue(TEXT("Two-hit profile opens on second hit"), Mining->bLastPickaxeBreak && !PickaxeBlocked(Rock, 0, 0));
    Mining->ResetField();
    auto Single = Two;
    Single.Stages.RemoveAt(0);
    TestTrue(TEXT("One-hit profile is supported"), Mining->ApplyPickaxeBalance(Single));
    TestTrue(TEXT("Single final stage succeeds"), Mining->ApplyPickaxeHit(PickaxeSurface(Rock)));
    TestFalse(TEXT("One-hit profile immediately opens thin wall"), PickaxeBlocked(Rock, 0, 0));
    TestTrue(TEXT("Configured cuts remain watertight"), Rock->RockMesh->GetDynamicMesh()->GetMeshRef().IsClosed());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningPickaxeIndependentDimensionsTest, "BrainMining.Pickaxe.IndependentDimensionsAndRoughness", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningPickaxeIndependentDimensionsTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Rock = World->SpawnActor<ADiggableRock>();
    Rock->RockSize = FVector(1000, 1000, 1000);
    auto* Mining = CreatePickaxe(World);
    auto Profile = UMiningPickaxeSettings::MakeRecommendedLevels()[0];
    Profile.bUseHardness=false;
    Profile.Width = 200; Profile.Depth = 130; Profile.Stages.RemoveAt(0, 2);
    const auto CutDepth = [&]()
    {
        Mining->ResetField();
        Rock->ResetRock();
        Mining->ApplyPickaxeBalance(Profile);
        TestTrue(TEXT("Configured final cut succeeds"), Mining->ApplyPickaxeHit(PickaxeSurface(Rock)));
        return PickaxeSurface(Rock).ImpactPoint.X + 500;
    };
    const double Baseline = CutDepth();
    Profile.Width = 400;
    TestTrue(TEXT("Doubling width does not double centre depth"), FMath::IsNearlyEqual(CutDepth(), Baseline, 1.));
    Profile.Depth = 260;
    TestTrue(TEXT("Explicit depth setting controls centre depth"), FMath::IsNearlyEqual(CutDepth(), Baseline * 2, 1.));
    Mining->PickaxeSizeScale = .7f;
    TestTrue(TEXT("Character size still scales depth"), FMath::IsNearlyEqual(CutDepth(), Baseline * 2 * .7, 1.));
    for (float Strength : {0.f, .5f, 1.f})
    {
        const FDynamicMesh3 Cutter = MiningPickaxe::MakeCutter(FTransform::Identity, FVector(100), 37, Strength);
        TestTrue(TEXT("All roughness settings keep closed valid cutters"), Cutter.IsClosed() && Cutter.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly));
        TestTrue(TEXT("Same patch roughness stays deterministic"), Cutter.IsSameAs(MiningPickaxe::MakeCutter(FTransform::Identity, FVector(100), 37, Strength), FDynamicMesh3::FSameAsOptions()));
    }
    TestFalse(TEXT("Roughness changes actual cutter geometry"), MiningPickaxe::MakeCutter(FTransform::Identity, FVector(100), 37, 0).IsSameAs(
        MiningPickaxe::MakeCutter(FTransform::Identity, FVector(100), 37, 1), FDynamicMesh3::FSameAsOptions()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningPickaxeConfiguredChunkTest, "BrainMining.Pickaxe.ConfiguredChunkStages", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningPickaxeConfiguredChunkTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    FMinimalBoxMeshGenerator Box;
    Box.Box = FOrientedBox3d(FVector3d::Zero(), FVector3d(25, 500, 500));
    Box.Generate();
    auto* Rock = World->SpawnActor<AGeneratedMiningRock>();
    if (!TestTrue(TEXT("Thin chunked wall builds"), Rock->SetChunkedMesh(FDynamicMesh3(&Box), 400, true))) return false;
    Rock->InteractiveCutBudgetMilliseconds = .01f;
    auto* Mining = CreatePickaxe(World);
    auto Profile = UMiningPickaxeSettings::MakeRecommendedLevels()[4];
    Profile.bUseHardness=false;
    Profile.Width = 500; Profile.Depth = 130; Profile.Roughness = .5f;
    Mining->ApplyPickaxeBalance(Profile);
    for (int32 Strike = 1; Strike <= 2; ++Strike)
    {
        const auto Hit = PickaxeSurface(Rock);
        TestTrue(TEXT("Configured chunked stage accepted"), Mining->ApplyPickaxeHit(Hit));
        if (Rock->bCutPending) TestFalse(TEXT("Pending stage cannot be submitted twice"), Mining->ApplyPickaxeHit(Hit));
        DrainPickaxe(Rock);
        TestFalse(TEXT("Configured stage finishes asynchronous work"), Rock->bCutPending);
        TestEqual(TEXT("One cut per configured stage"), Rock->SuccessfulCuts, Strike);
        TestEqual(TEXT("Thin chunked wall opens only on the final stage"), PickaxeBlocked(Rock, 0, 0), Strike < 2);
    }
    for (const auto& Chunk : Rock->Chunks)
        TestTrue(TEXT("Configurable cut preserves watertight chunks"), Chunk.Mesh->GetDynamicMesh()->GetMeshRef().IsClosed());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningAudioContactTest, "BrainMining.AudioPilot.ContactAndCommittedBreak", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningAudioContactTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Rock = World->SpawnActor<ADiggableRock>();
    Rock->RockSize = FVector(50, 1000, 1000);
    Rock->ResetRock();
    auto* Mining = CreatePickaxe(World);
    TArray<int32> Events;
    Mining->OnPickaxeContact.AddLambda([&](const FHitResult& Hit, int32 Stage)
    {
        TestTrue(TEXT("Contact identifies the mined rock"), Hit.GetActor() == Rock);
        Events.Add(Stage);
    });
    Mining->OnPickaxeBreak.AddLambda([&](const FVector& Position)
    {
        TestEqual(TEXT("Break follows a real final terrain commit"), Rock->SuccessfulCuts, 3);
        TestTrue(TEXT("Break has a valid world position"), !Position.ContainsNaN());
        Events.Add(99);
    });
    TestFalse(TEXT("Invalid hits are silent"), Mining->ApplyPickaxeHit(FHitResult()));
    TestEqual(TEXT("No event for invalid hit"), Events.Num(), 0);
    for (int32 Stage = 1; Stage <= 3; ++Stage)
    {
        TestTrue(TEXT("Progressive strike succeeds"), Mining->ApplyPickaxeHit(PickaxeSurface(Rock)));
        TestEqual(TEXT("Only final stage adds a break"), Events.Num(), Stage == 3 ? 4 : Stage);
    }
    TestTrue(TEXT("Contact precedes synchronous break"), Events == TArray<int32>({1, 2, 3, 99}));
    Rock->ResetRock();
    TestEqual(TEXT("Reset is silent"), Events.Num(), 4);
    TestTrue(TEXT("First hit after reset succeeds"), Mining->ApplyPickaxeHit(PickaxeSurface(Rock)));
    Rock->ResetRock();
    TestEqual(TEXT("Reset of damaged rock adds no break"), Events.Num(), 5);
    Mining->OnPickaxeContact.Clear();
    Mining->OnPickaxeBreak.Clear();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningAudioDeferredTest, "BrainMining.AudioPilot.DeferredBreakAndCancellation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningAudioDeferredTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    FMinimalBoxMeshGenerator Box;
    Box.Box = FOrientedBox3d(FVector3d::Zero(), FVector3d(700, 800, 800));
    Box.Generate();
    auto* Rock = World->SpawnActor<AGeneratedMiningRock>();
    TestTrue(TEXT("Async fixture builds"), Rock->SetChunkedMesh(FDynamicMesh3(&Box), 400, true));
    Rock->InteractiveCutBudgetMilliseconds = .01f;
    auto* Mining = CreatePickaxe(World);
    int32 Contacts = 0;
    int32 Breaks = 0;
    Mining->OnPickaxeContact.AddLambda([&](const FHitResult&, int32) { ++Contacts; });
    Mining->OnPickaxeBreak.AddLambda([&](const FVector&) { ++Breaks; });
    for (int32 Pass = 0; Pass < 2; ++Pass)
    {
        for (int32 Stage = 1; Stage <= 3; ++Stage)
        {
            const auto Hit = PickaxeSurface(Rock);
            TestTrue(TEXT("Strike accepted"), Mining->ApplyPickaxeHit(Hit));
            TestTrue(TEXT("Fixture uses deferred cuts"), Rock->bCutPending);
            TestEqual(TEXT("Acceptance alone never plays break"), Breaks, 0);
            const int32 BeforeDuplicate = Contacts;
            TestFalse(TEXT("Pending strike cannot repeat"), Mining->ApplyPickaxeHit(Hit));
            TestEqual(TEXT("Pending strike does not duplicate contact"), Contacts, BeforeDuplicate);
            if (Pass == 0 && Stage == 3) Rock->ResetRock();
            else DrainPickaxe(Rock);
            TestFalse(TEXT("No cut remains pending"), Rock->bCutPending);
        }
        TestEqual(TEXT("Cancelled final cut silent; committed final cut once"), Breaks, Pass);
    }
    Rock->ResetRock();
    TestEqual(TEXT("Later reset does not replay break"), Breaks, 1);
    Mining->OnPickaxeContact.Clear();
    Mining->OnPickaxeBreak.Clear();
    return true;
}
#endif
