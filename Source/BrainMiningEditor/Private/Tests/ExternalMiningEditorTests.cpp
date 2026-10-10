#include "MiningExternalRegion.h"
#include "GeneratedMiningRock.h"
#include "DynamicMeshActor.h"
#include "Components/DynamicMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "UDynamicMesh.h"
#include "MiningExternalGeometry.h"
#include "Generators/SphereGenerator.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExternalActorTest, "BrainMining.ExternalMining.EditorImportCollisionReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FExternalActorTest::RunTest(const FString& Parameters)
{
	// Run this suite in an isolated editor process: it creates a new unsaved test world.
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AMiningExternalRegion* Region = World->SpawnActor<AMiningExternalRegion>();
	if (!TestNotNull(TEXT("Independent generator class"), Region)) return false;
	Region->RockSize = FVector(2400);
	Region->ChunkLength = 1200;
	Region->ShellThickness = 150;
	FMiningExternalCutter Cutter;
	Cutter.Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Cutter.LocalTransform = FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(32,6,6));
	Region->Cutters.Add(Cutter);
	if (!TestTrue(TEXT("Import LOD0 mesh and generate both layers"), Region->GenerateRegion())) { AddError(Region->BuildStatus); return false; }
	AGeneratedMiningRock* Rock = Region->GeneratedRock;
	ADynamicMeshActor* Shell = Region->GeneratedShell;
	TestTrue(TEXT("Rock is recognized by existing digging system"), Rock->IsA<ADiggableRock>());
	TestFalse(TEXT("Shell can never be targeted as diggable rock"), Shell->IsA<ADiggableRock>());
	TestEqual(TEXT("Spatial chunks"), Rock->Chunks.Num(), 8);
	const auto Trace = [World](const FVector& Start, const FVector& End, AActor*& HitActor)
	{
		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, FCollisionQueryParams(NAME_None, true));
		HitActor = Hit.GetActor(); return bHit;
	};
	AActor* Hit = nullptr;
	TestFalse(TEXT("Entry and exit collision open"), Trace(FVector(-1600,0,0),FVector(1600,0,0),Hit));
	TestTrue(TEXT("Wall collision exists"), Trace(FVector::ZeroVector,FVector(0,600,0),Hit));
	TestTrue(TEXT("Wall hit resolves to generated diggable actor"), Hit == Rock);
	TestTrue(TEXT("Exterior shell blocks player"), Trace(FVector(0,1600,0),FVector(0,1250,0),Hit));
	TestTrue(TEXT("Exterior hit resolves to independent bedrock"), Hit == Shell);
	const int32 InitialTriangles = Rock->TriangleCount;
	const int32 ShellTriangles = Shell->GetDynamicMeshComponent()->GetDynamicMesh()->GetTriangleCount();
	TestTrue(TEXT("Cross chunk dig works"), Rock->MineSphere(FVector(0,310,0),160));
	TestTrue(TEXT("Multiple touched chunks"), Rock->LastTouchedChunks > 1);
	TestTrue(TEXT("Digging reaches the rock boundary"), Rock->MineSphere(FVector(0,1170,0),250));
	TestEqual(TEXT("Excavation cannot change bedrock geometry"), Shell->GetDynamicMeshComponent()->GetDynamicMesh()->GetTriangleCount(), ShellTriangles);
	TestTrue(TEXT("Bedrock blocks newly dug boundary cavity"), Trace(FVector(0,1100,0),FVector(0,1400,0),Hit));
	TestTrue(TEXT("Boundary cavity hits indestructible shell"), Hit == Shell);
	Region->ResetExcavation();
	TestEqual(TEXT("Reset restores initial cut geometry"), Rock->TriangleCount, InitialTriangles);
	TestFalse(TEXT("Reset does not fill initial passage"), Trace(FVector(-1600,0,0),FVector(1600,0,0),Hit));
	Region->Cutters[0].Mesh = nullptr;
	TestFalse(TEXT("Missing asset build fails safely"), Region->GenerateRegion());
	TestTrue(TEXT("Both prior actors retained"), Region->GeneratedRock == Rock && Region->GeneratedShell == Shell);
	Region->Cutters[0] = Cutter;
	Region->SetActorRotation(FRotator(0,35,0));
	Region->SetActorLocation(FVector(10000,5000,2000));
	const FTransform Placement = Region->GetActorTransform();
	TestFalse(TEXT("Moving and rotating region keeps port collision aligned"), Trace(Placement.TransformPosition(FVector(-1600,0,0)),Placement.TransformPosition(FVector(1600,0,0)),Hit));
	TestTrue(TEXT("Rebuild replaces actors without duplicates"), Region->GenerateRegion());
	int32 Owned = 0;
	for (TActorIterator<AActor> It(World); It; ++It) if (It->GetOwner() == Region) ++Owned;
	TestEqual(TEXT("Only rock and shell owned"), Owned, 2);
	GEditor->UndoTransaction();
	TestTrue(TEXT("Undo restores previous pair"), Region->GeneratedRock == Rock && Region->GeneratedShell == Shell && IsValid(Rock) && IsValid(Shell));
	GEditor->RedoTransaction();
	TestTrue(TEXT("Redo restores new pair"), IsValid(Region->GeneratedRock) && IsValid(Region->GeneratedShell) && Region->GeneratedRock != Rock);
	Region->ClearGeneratedRegion();
	TestTrue(TEXT("Clear removes only generated pair"), !Region->GeneratedRock && !Region->GeneratedShell && Region->Cutters.Num() == 1);
	GEditor->UndoTransaction();
	TestTrue(TEXT("Undo clear restores geometry"), IsValid(Region->GeneratedRock) && IsValid(Region->GeneratedShell));
	World->EditorDestroyActor(Region,true);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExternalHighTriangleBudgetTest, "BrainMining.ExternalMining.HighTriangleDigBudget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FExternalHighTriangleBudgetTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UE::Geometry::FSphereGenerator Generator;
	Generator.Radius = 500;
	Generator.NumPhi = 128;
	Generator.NumTheta = 256;
	Generator.Generate();
	UE::Geometry::FDynamicMesh3 Geometry(&Generator);
	TestTrue(TEXT("Dense route input exceeds previous 50000 cap"), Geometry.TriangleCount() > 50000);
	FString Error;
	if (!TestTrue(TEXT("Dense closed cutter accepted"), MiningExternal::PrepareCutter(Geometry,Error))) { AddError(Error); return false; }
	AGeneratedMiningRock* Rock = World->SpawnActor<AGeneratedMiningRock>();
	Rock->MaxTriangles = 100000;
	Rock->bSimplifyChunks = false;
	if (!TestTrue(TEXT("Dense geometry becomes a mineable chunk"), Rock->SetChunkedMesh(Geometry,1600,true))) return false;
	TestTrue(TEXT("Initial chunk exceeds former 30000 ceiling"), Rock->LargestChunkTriangles > 30000);
	const int32 InitialTriangles = Rock->TriangleCount;
	if (!TestTrue(TEXT("Actual excavation uses raised budget"), Rock->MineSphere(FVector::ZeroVector,100))) { AddError(Rock->LastStatus); return false; }
	TestTrue(TEXT("Excavated chunk retains more than 30000 triangles"), Rock->LargestChunkTriangles > 30000 && Rock->LargestChunkTriangles <= 100000);
	Rock->ResetRock();
	TestEqual(TEXT("Dense chunk reset restores original"), Rock->TriangleCount,InitialTriangles);
	World->EditorDestroyActor(Rock,true);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExternalGuideLayersTest, "BrainMining.ExternalMining.GuideLayersCollisionResetRollback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FExternalGuideLayersTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AMiningExternalRegion* Region = World->SpawnActor<AMiningExternalRegion>(FVector(9000,5000,3000), FRotator(0,35,0));
	Region->RockSize = FVector(2400); Region->ChunkLength = 1200;
	FMiningExternalCutter Cutter;
	Cutter.Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Cutter.LocalTransform = FTransform(FRotator::ZeroRotator, FVector(200,-200,0), FVector(32,6,6));
	Region->Cutters.Add(Cutter);
	if (!TestTrue(TEXT("Base mine generated"), Region->GenerateRegion())) { AddError(Region->BuildStatus); return false; }
	AGeneratedMiningRock* Rock = Region->GeneratedRock; ADynamicMeshActor* Shell = Region->GeneratedShell;
	for (double Offset : {-25.,25.})
	{
		FMiningExternalGuideLayer Layer;
		Layer.Mesh = Cutter.Mesh;
		Layer.LocalTransform = FTransform(FRotator::ZeroRotator, FVector(Offset,0,0), FVector(.04,1,1));
		Region->GuideLayers.Add(Layer);
	}
	if (!TestTrue(TEXT("Two mineable blockers generated separately"), Region->GenerateGuideLayers())) { AddError(Region->GuideBuildStatus); return false; }
	TestEqual(TEXT("Guide actors tracked"), Region->GeneratedGuideLayers.Num(), 2);
	AGeneratedMiningRock* Guide = Region->GeneratedGuideLayers[0];
	TestTrue(TEXT("Guide belongs to existing mining hierarchy"), Guide->IsA<ADiggableRock>() && Guide->ActorHasTag(TEXT("ExternalMiningGuideLayer")));
	const FVector LocalCenter = Cutter.LocalTransform.TransformPosition(FVector(-25,0,0));
	const FVector Center = Region->GetActorTransform().TransformPosition(LocalCenter);
	const FVector Axis = Region->GetActorTransform().TransformVectorNoScale(Cutter.LocalTransform.GetRotation().GetForwardVector());
	const auto Trace = [World,Center,Axis](FHitResult& Hit)
	{
		return World->LineTraceSingleByChannel(Hit,Center-Axis*200,Center+Axis*200,ECC_Visibility,FCollisionQueryParams(NAME_None,true));
	};
	FHitResult Hit;
	TestTrue(TEXT("Aligned blocker fills the passage"), Trace(Hit) && Hit.GetActor() == Guide);
	const int32 Initial = Guide->TriangleCount;
	if (!TestTrue(TEXT("Guide is actually excavatable"), Guide->MineSphere(Center,180))) { AddError(Guide->LastStatus); return false; }
	TestFalse(TEXT("Excavation opens the blocker"), Trace(Hit));
	Region->ResetExcavation();
	TestEqual(TEXT("Mine reset also restores guide geometry"), Guide->TriangleCount, Initial);
	TestTrue(TEXT("Reset closes guide passage again"), Trace(Hit) && Hit.GetActor() == Guide);
	Region->GuideLayers[1].Mesh = nullptr;
	TestFalse(TEXT("Invalid guide rejected without replacing previous guides"), Region->GenerateGuideLayers());
	TestTrue(TEXT("Previous guide and base layers retained"), Region->GeneratedGuideLayers[0] == Guide && IsValid(Guide) && Region->GeneratedRock == Rock && Region->GeneratedShell == Shell);
	Region->GuideLayers[1].Mesh = Cutter.Mesh;
	Region->ChunkLength = 300;
	TestFalse(TEXT("Failure after staging rolls new actors back"), Region->GenerateGuideLayers());
	Region->ChunkLength = 1200;
	TestTrue(TEXT("Guide regeneration succeeds"), Region->GenerateGuideLayers());
	int32 Owned = 0;
	for (TActorIterator<AActor> It(World); It; ++It) if (It->GetOwner() == Region) ++Owned;
	TestEqual(TEXT("Only base pair and two guides remain"), Owned, 4);
	GEditor->UndoTransaction();
	TestTrue(TEXT("Undo restores original guide set"), Region->GeneratedGuideLayers[0] == Guide && IsValid(Guide));
	GEditor->RedoTransaction();
	TestTrue(TEXT("Redo restores regenerated guide set"), Region->GeneratedGuideLayers.Num() == 2 && Region->GeneratedGuideLayers[0] != Guide && IsValid(Region->GeneratedGuideLayers[0]));
	Region->ClearGeneratedGuideLayers();
	TestTrue(TEXT("Guide-only clear preserves base mine"), Region->GeneratedGuideLayers.IsEmpty() && Region->GeneratedRock == Rock && Region->GeneratedShell == Shell);
	GEditor->UndoTransaction();
	TestEqual(TEXT("Undo guide clear restores both blockers"), Region->GeneratedGuideLayers.Num(),2);
	World->EditorDestroyActor(Region,true);
	Owned = 0;
	for (TActorIterator<AActor> It(World); It; ++It) if (It->GetOwner() == Region) ++Owned;
	TestEqual(TEXT("Deleting generator removes its base and guide layers"),Owned,0);
	return true;
}
#endif
