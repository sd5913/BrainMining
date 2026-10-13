#include "GeneratedMiningRock.h"
#include "Showcase/MiningTreasure.h"
#include "Showcase/MiningExhibitHall.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UDynamicMesh.h"
#include "MiningTerrainSource.h"
#include "MiningDebrisBurst.h"
#include "Materials/Material.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningLazyStartupTest, "BrainMining.Startup.DeferredGeometryPreservesTerrain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningLazyStartupTest::RunTest(const FString&)
{
	auto* Lazy = IConsoleManager::Get().FindConsoleVariable(TEXT("bm.Mining.LazyRuntimeChunks"));
	const int32 PreviousLazy = Lazy->GetInt();
	Lazy->Set(1, ECVF_SetByConsole);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); Lazy->Set(PreviousLazy, ECVF_SetByConsole); };
	const FVector Location(100000, 0, 0);
	auto* Rock = World->SpawnActor<AGeneratedMiningRock>(Location, FRotator::ZeroRotator);
	Rock->RockSize = FVector(1000);
	Rock->ResetRock();
	Rock->bSimplifyChunks = false;
	UE::Geometry::FDynamicMesh3 Original;
	Rock->CopyMiningMesh(Original);
	if (!TestTrue(TEXT("Create small chunked fixture"), Rock->SetChunkedMesh(Original, 500, true, true))) return false;
	const int32 OriginalTriangles = Rock->TriangleCount;
	Rock->UnregisterAllComponents();
	Rock->RegisterAllComponents();
	for (const auto& Chunk : Rock->Chunks)
	{
		TestFalse(TEXT("Loaded distant chunks skip registration before BeginPlay"), Chunk.Mesh->IsRegistered());
		TestEqual(TEXT("Loaded runtime copies are released before registration"), Chunk.Mesh->GetDynamicMesh()->GetTriangleCount(), 0);
		TestTrue(TEXT("Serialized source geometry survives early deferral"), Chunk.InitialMesh->GetTriangleCount() > 0);
	}
	Rock->DispatchBeginPlay();
	TestEqual(TEXT("Distant chunks have no runtime copies at startup"), Rock->DeferredRuntimeChunks, Rock->Chunks.Num());
	TestEqual(TEXT("Logical terrain retains all triangles"), Rock->TriangleCount, OriginalTriangles);
	for (const auto& Chunk : Rock->Chunks)
	{
		TestEqual(TEXT("No distant runtime geometry"), Chunk.Mesh->GetDynamicMesh()->GetTriangleCount(), 0);
		TestTrue(TEXT("Distant collision is disabled"), Chunk.Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
		TestFalse(TEXT("Distant component stays unregistered"), Chunk.Mesh->IsRegistered());
	}
	const FVector Support = Location + FVector(470, 50, 50);
	TArray<bool> Covered;
	AMiningTreasure::MarkCoveredPoints({Rock}, {Support}, Covered);
	TestTrue(TEXT("Deferred terrain still supports buried minerals"), Covered[0]);
	Rock->PrepareRuntimeGeometry(FVector::ZeroVector, 5000);
	TestEqual(TEXT("Unrelated regions stay deferred"), Rock->DeferredRuntimeChunks, Rock->Chunks.Num());
	Rock->PrepareRuntimeGeometry(Location + FVector(500, 50, 50), 151);
	TestTrue(TEXT("Only nearby chunks activate"), Rock->DeferredRuntimeChunks > 0 && Rock->DeferredRuntimeChunks < Rock->Chunks.Num());
	FHitResult Hit;
	TestTrue(TEXT("Activated wall has real collision before movement"), World->LineTraceSingleByChannel(
		Hit, Location + FVector(1000, 50, 50), Location + FVector(0, 50, 50), ECC_Visibility));
	TestTrue(TEXT("The collision belongs to the activated rock"), Hit.GetActor() == Rock);
	for (const auto& Chunk : Rock->Chunks)
		TestEqual(TEXT("Only activated components register"), Chunk.Mesh->IsRegistered(), !Chunk.bRuntimeDeferred);
	TestTrue(TEXT("Mining an activated wall is accepted"), Rock->MineSphere(Location + FVector(500, 50, 50), 150));
	TestEqual(TEXT("Runtime first strike never performs its boolean inline"), Rock->SuccessfulCuts, 0);
	TestTrue(TEXT("Runtime first strike is queued"), Rock->bCutPending);
	const double Deadline = FPlatformTime::Seconds() + 5;
	while (Rock->bCutPending && FPlatformTime::Seconds() < Deadline)
	{
		FPlatformProcess::Sleep(.001f);
		Rock->Tick(.01f);
	}
	TestFalse(TEXT("Accepted cut drains"), Rock->bCutPending);
	AMiningTreasure::MarkCoveredPoints({Rock}, {Support}, Covered);
	TestFalse(TEXT("Coverage uses the excavated runtime geometry"), Covered[0]);
	Rock->UnregisterAllComponents();
	Rock->RegisterAllComponents();
	AMiningTreasure::MarkCoveredPoints({Rock}, {Support}, Covered);
	TestFalse(TEXT("Re-registering after BeginPlay preserves excavation"), Covered[0]);
	Rock->ResetRock();
	AMiningTreasure::MarkCoveredPoints({Rock}, {Support}, Covered);
	TestTrue(TEXT("Reset restores support without activating distant geometry"), Covered[0]);
	TestEqual(TEXT("Reset returns distant terrain to deferred state"), Rock->DeferredRuntimeChunks, Rock->Chunks.Num());
	AGeneratedMiningRock::PrepareTerrainAt(World, Location);
	TestEqual(TEXT("Teleport destination is prepared synchronously"), Rock->DeferredRuntimeChunks, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningRegistrationFallbackTest, "BrainMining.Startup.EditorAndEagerRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningRegistrationFallbackTest::RunTest(const FString&)
{
	auto* Lazy = IConsoleManager::Get().FindConsoleVariable(TEXT("bm.Mining.LazyRuntimeChunks"));
	const int32 PreviousLazy = Lazy->GetInt();
	ON_SCOPE_EXIT { Lazy->Set(PreviousLazy, ECVF_SetByConsole); };
	for (const auto WorldType : {EWorldType::Editor, EWorldType::Game})
	{
		Lazy->Set(WorldType == EWorldType::Editor ? 1 : 0, ECVF_SetByConsole);
		UWorld* World = UWorld::CreateWorld(WorldType, false);
		ON_SCOPE_EXIT { World->DestroyWorld(false); };
		auto* Rock = World->SpawnActor<AGeneratedMiningRock>();
		Rock->RockSize = FVector(1000);
		Rock->ResetRock();
		UE::Geometry::FDynamicMesh3 Original;
		Rock->CopyMiningMesh(Original);
		if (!TestTrue(TEXT("Create registration fallback fixture"), Rock->SetChunkedMesh(Original, 500, true, true))) return false;
		Rock->UnregisterAllComponents();
		Rock->RegisterAllComponents();
		for (const auto& Chunk : Rock->Chunks)
		{
			TestTrue(TEXT("Editor preview and opt-out retain registered geometry"), Chunk.Mesh->IsRegistered());
			TestTrue(TEXT("Editor preview and opt-out retain mesh data"), Chunk.Mesh->GetDynamicMesh()->GetTriangleCount() > 0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningCampaignStartupTest, "BrainMining.Startup.CampaignMineralsInitializeOnEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningCampaignStartupTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* Hall = World->SpawnActor<AMiningExhibitHall>();
	Hall->bContinuousCampaign = true;
	auto* Rock = World->SpawnActor<AGeneratedMiningRock>();
	Rock->RockSize = FVector(1000);
	Rock->ResetRock();
	auto* Field = World->SpawnActor<AMiningTreasureField>();
	Field->bUseAuthoredPlacements = true;
	Field->CrystalClueMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	FMiningTreasureDefinition Definition = Field->Definitions[0];
	Definition.Model = Field->CrystalClueMesh;
	Definition.Regions = {TEXT("BM_Campaign_Mine01"), TEXT("BM_Campaign_Mine02")};
	Field->Definitions = {Definition};
	for (const FName AreaTag : Definition.Regions)
	{
		FMiningTreasurePlacement Placement;
		Placement.TreasureId = Definition.Id;
		Placement.Value = Definition.ValuesBySize[0];
		Placement.AreaTag = AreaTag;
		Placement.Hosts = {Rock};
		Placement.FragmentSupports = {FVector(470, 50, 50)};
		Placement.Fragments = {FTransform(FQuat::Identity, Placement.FragmentSupports[0], FVector(.1f))};
		Field->AuthoredPlacements.Add(Placement);
	}
	Field->ResetExhibit();
	if (!TestEqual(TEXT("Campaign retains every authored reward identity"), Field->Items.Num(), 2)) return false;
	for (const auto& Item : Field->Items)
	{
		TestNull(TEXT("Unvisited minerals have no configured model"), Item->Model->GetStaticMesh());
		TestEqual(TEXT("No unvisited surface sampling"), Item->GetSurfaceSampleCount(), 0);
		TestEqual(TEXT("No unvisited fragment copies"), Item->CrystalTransforms.Num(), 0);
	}
	Field->ActiveAreaTag = Definition.Regions[0];
	Field->SetAreaActive(true);
	auto* First = Field->Items[0].Get();
	auto* Second = Field->Items[1].Get();
	TestTrue(TEXT("Entering mine creates sampleable ore"), First->GetSurfaceSampleCount() > 0);
	TestEqual(TEXT("Entering mine creates supported fragments"), First->CrystalClues->GetInstanceCount(), 1);
	TestFalse(TEXT("Ready ore becomes visible"), First->IsHidden());
	TestNull(TEXT("Later mine remains deferred"), Second->Model->GetStaticMesh());
	First->bCollected = true;
	const auto* FirstMaterial = First->Model->GetMaterial(0);
	Field->ActiveAreaTag = Definition.Regions[1];
	Field->SetAreaActive(true);
	TestTrue(TEXT("Next mine initializes when entered"), Second->GetSurfaceSampleCount() > 0);
	Field->ActiveAreaTag = Definition.Regions[0];
	Field->SetAreaActive(true);
	Field->ResetExhibit();
	TestTrue(TEXT("Revisit preserves collected state"), First->bCollected);
	TestTrue(TEXT("Revisit preserves existing material and samples"), First->Model->GetMaterial(0) == FirstMaterial);
	TestEqual(TEXT("Revisit does not duplicate fragments"), First->CrystalClues->GetInstanceCount(), 1);
	TestEqual(TEXT("Reset cannot mint duplicate campaign rewards"), Field->Items.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningTaskBudgetTest, "BrainMining.Startup.BackgroundTaskBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningTaskBudgetTest::RunTest(const FString&)
{
	if (!TestEqual(TEXT("No previous background jobs remain"), AGeneratedMiningRock::GetBackgroundTaskCount(), 0)) return false;
	auto* Limit = IConsoleManager::Get().FindConsoleVariable(TEXT("bm.Mining.MaxBackgroundTasks"));
	const int32 PreviousLimit = Limit->GetInt();
	Limit->Set(1, ECVF_SetByConsole);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); Limit->Set(PreviousLimit, ECVF_SetByConsole); };
	TArray<AGeneratedMiningRock*> Rocks;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		auto* Rock = World->SpawnActor<AGeneratedMiningRock>(FVector(Index * 3000, 0, 0), FRotator::ZeroRotator);
		Rock->RockSize = FVector(1000);
		Rock->ResetRock();
		UE::Geometry::FDynamicMesh3 Geometry;
		Rock->CopyMiningMesh(Geometry);
		if (!TestTrue(TEXT("Create budget fixture"), Rock->SetChunkedMesh(Geometry, 500, true, true))) return false;
		Rock->bSimplifyChunks = false;
		Rock->InteractiveCutBudgetMilliseconds = .01f;
		Rocks.Add(Rock);
	}
	for (auto* Rock : Rocks)
		TestTrue(TEXT("Every hit is accepted while capacity is limited"), Rock->MineSphere(Rock->GetActorLocation() + FVector(500, 50, 50), 150));
	TestEqual(TEXT("Only one job/result owns mesh copies"), AGeneratedMiningRock::GetBackgroundTaskCount(), 1);
	Rocks[1]->ResetRock();
	const double Deadline = FPlatformTime::Seconds() + 8;
	while (Rocks.ContainsByPredicate([](const auto* Rock) { return Rock->bCutPending; }) && FPlatformTime::Seconds() < Deadline)
	{
		FPlatformProcess::Sleep(.001f);
		for (auto* Rock : Rocks) Rock->Tick(.01f);
		if (!TestTrue(TEXT("Budget remains bounded while jobs drain"), AGeneratedMiningRock::GetBackgroundTaskCount() <= 1)) break;
	}
	TestFalse(TEXT("Queued jobs finish without starvation"), Rocks.ContainsByPredicate([](const auto* Rock) { return Rock->bCutPending; }));
	TestEqual(TEXT("First accepted hit commits"), Rocks[0]->SuccessfulCuts, 1);
	TestEqual(TEXT("Reset cancels its queued old hit"), Rocks[1]->SuccessfulCuts, 0);
	TestEqual(TEXT("Later accepted hit is not dropped"), Rocks[2]->SuccessfulCuts, 1);
	TestEqual(TEXT("Completed results release capacity"), AGeneratedMiningRock::GetBackgroundTaskCount(), 0);
	return true;
}
#endif
