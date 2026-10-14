#include "MiningPickaxeGeometry.h"

UE::Geometry::FDynamicMesh3 MiningPickaxe::MakeCutter(const FTransform& Frame, const FVector& HalfSize, int32 Seed)
{
	using namespace UE::Geometry;
	FDynamicMesh3 Mesh;
	Mesh.EnableTriangleGroups(0);
	FRandomStream Random(Seed);
	constexpr int32 Sides = 16;
	TArray<FVector2D> Outline;
	TArray<double> ShoulderDepth;
	TArray<double> InnerDepth;
	TArray<double> ShoulderWidth;
	TArray<double> InnerWidth;
	for (int32 Corner = 0; Corner < Sides; ++Corner)
	{
		const double Angle = (Corner + .5) * TWO_PI / Sides + Random.FRandRange(-.045f, .045f);
		const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
		const double SquareRadius = 1. / FMath::Max(FMath::Abs(Direction.X), FMath::Abs(Direction.Y));
		Outline.Add(Direction * SquareRadius * Random.FRandRange(.86f, 1.08f));
		ShoulderDepth.Add(Random.FRandRange(.05f, .42f));
		InnerDepth.Add(Random.FRandRange(.50f, .88f));
		ShoulderWidth.Add(Random.FRandRange(.80f, .96f));
		InnerWidth.Add(Random.FRandRange(.36f, .60f));
	}
	for (int32 Layer = 0; Layer < 3; ++Layer)
	{
		for (int32 Corner = 0; Corner < Sides; ++Corner)
		{
			const double Depth = Layer == 0 ? -1. : (Layer == 1 ? ShoulderDepth[Corner] : InnerDepth[Corner]);
			const double Width = Layer == 0 ? 1. : (Layer == 1 ? ShoulderWidth[Corner] : InnerWidth[Corner]);
			Mesh.AppendVertex(FVector3d(Frame.TransformPositionNoScale(FVector(Depth * HalfSize.X,
				Outline[Corner].X * HalfSize.Y * Width, Outline[Corner].Y * HalfSize.Z * Width))));
		}
	}
	const int32 Front = Mesh.AppendVertex(FVector3d(Frame.TransformPositionNoScale(FVector(-HalfSize.X, 0, 0))));
	const int32 Back = Mesh.AppendVertex(FVector3d(Frame.TransformPositionNoScale(FVector(HalfSize.X, 0, 0))));
	for (int32 Corner = 0; Corner < Sides; ++Corner)
	{
		const int32 Next = (Corner + 1) % Sides;
		Mesh.AppendTriangle(Front, Corner, Next);
		Mesh.AppendTriangle(Back, 2 * Sides + Next, 2 * Sides + Corner);
		for (int32 Layer = 0; Layer < 2; ++Layer)
		{
			const int32 Lower = Layer * Sides;
			const int32 Upper = Lower + Sides;
			if ((Corner + Seed) % 2 == 0)
			{
				Mesh.AppendTriangle(Lower + Corner, Upper + Next, Lower + Next);
				Mesh.AppendTriangle(Lower + Corner, Upper + Corner, Upper + Next);
			}
			else
			{
				Mesh.AppendTriangle(Lower + Corner, Upper + Corner, Lower + Next);
				Mesh.AppendTriangle(Lower + Next, Upper + Corner, Upper + Next);
			}
		}
	}
	return Mesh;
}
