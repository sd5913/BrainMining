#include "MiningBlastGeometry.h"
#include "Generators/SphereGenerator.h"

UE::Geometry::FDynamicMesh3 MiningBlast::MakeCutter(FVector Center, float Radius, float Roughness, int32 Seed)
{
	using namespace UE::Geometry;
	FSphereGenerator Generator;
	Generator.Radius = 1;
	Generator.NumPhi = 10;
	Generator.NumTheta = 20;
	Generator.Generate();
	FDynamicMesh3 Cutter(&Generator);
	Cutter.DiscardAttributes();
	FRandomStream Random(Seed);
	const FQuat Rotation = Random.VRand().ToOrientationQuat() * FQuat(FVector::ForwardVector, Random.FRand() * PI);
	TArray<FVector> Lobes;
	for (int32 I=0; I<7; ++I) Lobes.Add(Random.VRand());
	const double Phase = Random.FRand() * 10;
	const double Amount = FMath::Clamp(Roughness, 0.f, .45f);
	for (int32 ID : Cutter.VertexIndicesItr())
	{
		const FVector Direction = Rotation.RotateVector(FVector(Cutter.GetVertex(ID))).GetSafeNormal();
		double Broad = 0;
		for (const FVector& Lobe : Lobes) Broad += FMath::Pow(FMath::Max(0., FVector::DotProduct(Direction, Lobe)), 8.);
		const double Facet = FMath::Sin(Direction.X*17 + Phase) * FMath::Cos(Direction.Y*13 - Phase) * FMath::Sin(Direction.Z*11 + Phase);
		const double Displacement = FMath::Clamp((Broad-.55)*.65 + Facet*.32, -.8, 1.);
		const double Distance = Radius * (1 + Amount * Displacement);
		Cutter.SetVertex(ID, FVector3d(Center + Direction * Distance));
	}
	return Cutter;
}
