#include "MiningManualRegion.h"
#include "MiningManualShape.h"
#include "Components/SplineComponent.h"
#include "EngineUtils.h"

AMiningManualRegion::AMiningManualRegion()
{
	Plugs.Reset();
	Path->ClearSplinePoints();
}

void AMiningManualRegion::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshBounds(GetRockBounds());
}

FBox AMiningManualRegion::GetRockBounds() const
{
	return FBox(GetActorLocation() - RockSize * 0.5, GetActorLocation() + RockSize * 0.5);
}

TArray<AMiningManualShape*> AMiningManualRegion::GetShapes() const
{
	TArray<AMiningManualShape*> Shapes;
	if (GetWorld())
		for (TActorIterator<AMiningManualShape> Shape(GetWorld()); Shape; ++Shape)
			if (Shape->GetLevel() == GetLevel() && Shape->Region == this) Shapes.Add(*Shape);
	Shapes.Sort([](const AMiningManualShape& First, const AMiningManualShape& Second) { return First.GetPathName() < Second.GetPathName(); });
	return Shapes;
}

uint32 AMiningManualRegion::GetGeometryHash() const
{
	uint32 Hash = Super::GetGeometryHash();
	Hash = HashCombineFast(Hash, GetTypeHash(RockSize));
	Hash = HashCombineFast(Hash, GetTypeHash(CurveTolerance));
	Hash = HashCombineFast(Hash, GetTypeHash(MainPassage.Get()));
	for (const AMiningManualShape* Shape : GetShapes()) Hash = HashCombineFast(Hash, HashCombineFast(GetTypeHash(Shape), Shape->GetGeometryHash()));
	return Hash;
}
