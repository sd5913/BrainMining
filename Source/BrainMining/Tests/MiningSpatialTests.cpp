#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "MiningSpatialGeometry.h"
#include "MiningChunkGeometry.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningSpatialHashTest, "BrainMining.Spatial.AuthoringHash", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningSpatialHashTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	AMiningSpatialTunnel* Tunnel = World->SpawnActor<AMiningSpatialTunnel>();
	const uint32 Original = Tunnel->GetGeometryHash();
	Tunnel->Knots[0].Position.Y += 35;
	TestNotEqual(TEXT("Nested XYZ changes invalidate geometry"), Tunnel->GetGeometryHash(), Original);
	Tunnel->Knots[0].Position.Y -= 35;
	TestEqual(TEXT("Restored authoring has original hash"), Tunnel->GetGeometryHash(), Original);
	Tunnel->Knots[0].Profile[0].Position.X += 20;
	TestNotEqual(TEXT("Nested profile changes invalidate geometry"), Tunnel->GetGeometryHash(), Original);
	Tunnel->Knots[0].Profile[0].Position.X -= 20;
	Tunnel->Knots[0].Roll = 360;
	TestNotEqual(TEXT("Full rotation differs from untwisted authoring"), Tunnel->GetGeometryHash(), Original);
	Tunnel->Knots[0].Roll = 0;
	Tunnel->StartPortal.Offset.X = 25;
	TestNotEqual(TEXT("Portal offsets invalidate geometry"), Tunnel->GetGeometryHash(), Original);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningSpatialSweepTest, "BrainMining.Spatial.Sweep", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningSpatialSweepTest::RunTest(const FString& Parameters)
{
	FMiningSpatialKnot First, Last;
	First.Profile = MiningSpatial::Rectangle(400, 300); First.Leave = FVector(600, 0, 0);
	Last = First; Last.Position = FVector(2000, 0, 0); Last.Arrive = FVector(-600, 0, 0); Last.Roll = 720;
	TArray<FMiningSpatialKnot> Knots{First, Last}; TArray<MiningSpatial::FRing> Rings;
	FString Error; UE::Geometry::FDynamicMesh3 Mesh;
	if (!TestTrue(TEXT("Unwrapped 720 degree sweep samples"), MiningSpatial::Sample(Knots, FVector::UpVector, 8, Rings, Error))) { AddError(Error); return false; }
	TestTrue(TEXT("Twist forces sufficient samples"), Rings.Num() >= 91);
	if (!TestTrue(TEXT("Twisted noncircular tube builds closed"), MiningSpatial::Sweep(Rings, Mesh, Error))) { AddError(Error); return false; }
	double Volume = 0;
	for (int32 Triangle : Mesh.TriangleIndicesItr())
	{
		FVector3d FirstVertex, SecondVertex, ThirdVertex;
		Mesh.GetTriVertices(Triangle, FirstVertex, SecondVertex, ThirdVertex);
		Volume += FirstVertex.Dot(SecondVertex.Cross(ThirdVertex)) / 6;
	}
	TestTrue(TEXT("Unreal native negative winding"), Volume < 0);
	FMiningMapRegion Map; MiningSpatial::StoreSurface(Mesh, Map);
	TestTrue(TEXT("Center contained"), Map.Contains(FVector(900, 0, 0)));
	TestFalse(TEXT("Bounding box corner not mistaken for tunnel"), Map.Contains(FVector(900, 240, 200)));
	Knots[1].Roll = 0; Knots[1].Position = FVector(0, 0, 2000); Knots[0].Leave = FVector(0, 0, 600); Knots[1].Arrive = FVector(0, 0, -600);
	TestTrue(TEXT("Vertical tangent parallel to initial up has stable fallback"), MiningSpatial::Sample(Knots, FVector::UpVector, 8, Rings, Error) && MiningSpatial::Sweep(Rings, Mesh, Error));
	Knots[1].Position = FVector(1500, 1300, -600); Knots[0].Leave = FVector(600, 0, 0); Knots[1].Arrive = FVector(0, -600, 0);
	TestTrue(TEXT("True XYZ curve with transported frame builds"), MiningSpatial::Sample(Knots, FVector::UpVector, 8, Rings, Error) && MiningSpatial::Sweep(Rings, Mesh, Error));
	Knots[1].Position = Knots[0].Position;
	TestFalse(TEXT("Duplicate knots rejected"), MiningSpatial::Sample(Knots, FVector::UpVector, 8, Rings, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningSpatialInsertTest, "BrainMining.Spatial.Insert", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningSpatialInsertTest::RunTest(const FString& Parameters)
{
	FMiningSpatialKnot First, Last; First.Profile = MiningSpatial::Rectangle(); Last.Profile = First.Profile;
	First.Leave = FVector(400, 900, -200); Last.Position = FVector(2000, -700, 1000); Last.Arrive = FVector(-700, 200, 400); Last.Roll = 480;
	TArray<FMiningSpatialKnot> Knots{First, Last}; const double Split = 0.37;
	TestTrue(TEXT("Insert node"), MiningSpatial::Insert(Knots, 0, Split));
	for (int32 Step = 0; Step <= 100; ++Step)
	{
		const double Parameter = Step / 100.0;
		const FVector Actual = Parameter <= Split ? MiningSpatial::Evaluate(Knots[0], Knots[1], Parameter / Split) : MiningSpatial::Evaluate(Knots[1], Knots[2], (Parameter - Split) / (1 - Split));
		if (!TestTrue(TEXT("De Casteljau preserves XYZ centerline"), Actual.Equals(MiningSpatial::Evaluate(First, Last, Parameter), 1e-6))) break;
	}
	TestTrue(TEXT("Roll interpolates without wrap"), FMath::IsNearlyEqual(Knots[1].Roll, 177.6f, 0.001f));
	Knots[0].bLocked = true; TestFalse(TEXT("Locked knot insertion rejected"), MiningSpatial::Insert(Knots, 0, 0.5));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningSpatialContainmentTest, "BrainMining.Spatial.ConcavityAndFoldedMap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMiningSpatialContainmentTest::RunTest(const FString& Parameters)
{
	TArray<FMiningSpatialKnot> Knots;
	const TArray<FVector> Positions{{0, 0, 0}, {2000, 0, 0}, {4000, 1500, 1000}, {2000, 3000, 1500}, {0, 3000, 1800}};
	for (int32 Index = 0; Index < Positions.Num(); ++Index)
	{
		FMiningSpatialKnot Knot; Knot.Position = Positions[Index]; Knot.Profile = MiningSpatial::Rectangle(120, 120);
		Knot.Leave = (Positions[FMath::Min(Index + 1, Positions.Num() - 1)] - Positions[FMath::Max(0, Index - 1)]) * 0.25;
		Knot.Arrive = -Knot.Leave; Knots.Add(Knot);
	}
	TArray<MiningSpatial::FRing> Rings; UE::Geometry::FDynamicMesh3 Mesh; FString Error;
	if (!TestTrue(TEXT("Folded XYZ route builds"), MiningSpatial::Sample(Knots, FVector::UpVector, 8, Rings, Error) && MiningSpatial::Sweep(Rings, Mesh, Error))) { AddError(Error); return false; }
	FMiningMapRegion Map; MiningSpatial::StoreSurface(Mesh, Map);
	TestTrue(TEXT("Outbound route contained"), Map.Contains(Positions[1]));
	TestTrue(TEXT("Return route contained at different height"), Map.Contains(Positions[3]));
	TestFalse(TEXT("Large gap inside projected hull is NOT tunnel"), Map.Contains(FVector(2000, 1500, 750)));
	FMiningSpatialKnot First, Last; First.Profile.Reset();
	for (const FVector2D& Position : {FVector2D(-200, -200), FVector2D(200, -200), FVector2D(200, 200), FVector2D(-200, 200), FVector2D(-200, 100), FVector2D(0, 100), FVector2D(0, -100), FVector2D(-200, -100)})
	{
		FMiningPenPoint Point; Point.Position = Position; First.Profile.Add(Point);
	}
	Last = First; Last.Position = FVector(1000, 0, 0);
	if (!TestTrue(TEXT("Concave section stays closed without convex hull"), MiningSpatial::Sample({First, Last}, FVector::UpVector, 8, Rings, Error) && MiningSpatial::Sweep(Rings, Mesh, Error))) { AddError(Error); return false; }
	MiningSpatial::StoreSurface(Mesh, Map);
	TestTrue(TEXT("Concave profile interior contained"), Map.Contains(FVector(500, 100, 0)));
	TestFalse(TEXT("Concave notch remains outside"), Map.Contains(FVector(500, -100, 0)));
	UE::Geometry::FMinimalBoxMeshGenerator Box;
	Box.Box = UE::Geometry::FOrientedBox3d(FVector(500, 0, 0), FVector(700, 600, 600)); Box.Generate();
	UE::Geometry::FDynamicMesh3 Solid(&Box), Carved;
	TestTrue(TEXT("Swept concave cutter subtracts from native rock"), MiningChunks::Subtract(Solid, Mesh, Carved) && Carved.IsClosed());
	TArray<UE::Geometry::FDynamicMesh3> Chunks; TArray<FBox> Bounds;
	TestTrue(TEXT("Spatial cut cells remain closed"), MiningChunks::SplitSpatial(Carved, 600, Chunks, Bounds));
	for (const UE::Geometry::FDynamicMesh3& Chunk : Chunks) TestTrue(TEXT("Closed spatial chunk"), Chunk.IsClosed());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpatialRelaxedCrossingTest, "BrainMining.Spatial.Freedom.SelfCrossingUnion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpatialRelaxedCrossingTest::RunTest(const FString& Parameters)
{
	TArray<FMiningSpatialKnot> Knots;
	const double Step = (UE_DOUBLE_PI + 0.5) / 8;
	for (int32 Index = 0; Index <= 8; ++Index)
	{
		const double Parameter = -0.25 + Index * Step;
		FMiningSpatialKnot& Knot = Knots.AddDefaulted_GetRef();
		Knot.Position = FVector(1600 * FMath::Sin(Parameter), 800 * FMath::Sin(2 * Parameter), 0);
		Knot.Leave = FVector(1600 * FMath::Cos(Parameter), 1600 * FMath::Cos(2 * Parameter), 0) * Step / 3;
		Knot.Arrive = -Knot.Leave; Knot.Profile = MiningSpatial::Rectangle(180, 120);
	}
	TArray<MiningSpatial::FRing> Rings; FString Error; UE::Geometry::FDynamicMesh3 Mesh;
	if (!TestTrue(TEXT("Free curve samples without fixed turn-angle rejection"), MiningSpatial::Sample(Knots, FVector::UpVector, 8, Rings, Error, nullptr, true))) { AddError(Error); return false; }
	if (!TestTrue(TEXT("Self crossing becomes one closed volume"), MiningSpatial::Sweep(Rings, Mesh, Error, true, nullptr, true))) { AddError(Error); return false; }
	TestTrue(TEXT("Joined surface remains valid and closed"), Mesh.IsClosed() && Mesh.CheckValidity());
	FMiningMapRegion Map; MiningSpatial::StoreSurface(Mesh, Map);
	TestTrue(TEXT("Crossing center is part of opening"), Map.Contains(FVector::ZeroVector));
	return true;
}
#endif
