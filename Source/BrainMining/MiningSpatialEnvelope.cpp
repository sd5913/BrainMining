#include "MiningSpatialEnvelope.h"
#include "MiningTunnelGenerator.h"
#include "MiningLayoutRoom.h"
#include "EngineUtils.h"

TArray<AMiningSpatialRegion*> MiningSpatial::SceneRegions(const AMiningSpatialRegion& Region)
{
	TArray<AMiningSpatialRegion*> Result;
	if (Region.GetWorld())
		for (TActorIterator<AMiningTunnelGenerator> Generator(Region.GetWorld()); Generator; ++Generator)
			if (Generator->UsesHollowRegion(&Region))
			{
				for (AMiningHollowRegion* Item : Generator->GetHollowRegions())
				{
					if (AMiningSpatialRegion* Spatial = Cast<AMiningSpatialRegion>(Item)) Result.AddUnique(Spatial);
					else return {};
				}
				break;
			}
	if (Result.IsEmpty()) Result.Add(const_cast<AMiningSpatialRegion*>(&Region));
	Result.Sort([](const AMiningSpatialRegion& First, const AMiningSpatialRegion& Last)
	{
		const int32 FirstOrder = IsValid(First.FromRoom) ? First.FromRoom->ConnectionOrder : INDEX_NONE;
		const int32 LastOrder = IsValid(Last.FromRoom) ? Last.FromRoom->ConnectionOrder : INDEX_NONE;
		return FirstOrder != LastOrder ? FirstOrder < LastOrder : First.GetPathName() < Last.GetPathName();
	});
	return Result;
}

FBox MiningSpatial::ScenePartition(const FBox& Bounds, int32 Index, int32 Count)
{
	if (!Bounds.IsValid || Count < 1 || Index < 0 || Index >= Count) return FBox(ForceInit);
	const FVector Size = Bounds.GetSize();
	const int32 Axis = Size.X >= Size.Y && Size.X >= Size.Z ? 0 : Size.Y >= Size.Z ? 1 : 2;
	FBox Result = Bounds;
	Result.Min[Axis] = Bounds.Min[Axis] + Size[Axis] * Index / Count;
	Result.Max[Axis] = Bounds.Min[Axis] + Size[Axis] * (Index + 1) / Count;
	return Result;
}
