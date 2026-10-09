#include "MiningSpatialBuild.h"
#include "MiningLayoutRoom.h"
#include "MiningSpatialEnvelope.h"
#include "Serialization/MemoryWriter.h"

namespace MiningSpatial
{
	namespace
	{
		const TMap<const AMiningSpatialRegion*, FBuildPtr>* PreparedBuilds = nullptr;
		void MakeKey(FRegionInput& Input)
		{
			FMemoryWriter Writer(Input.Key);
			Writer << Input.Origin << Input.Bounds << Input.Rooms[0] << Input.Rooms[1] << Input.Bedrock << Input.Tolerance << Input.StartWall << Input.EndWall;
			Writer << Input.bAutomaticBedrock << Input.ExcavationMargin;
			Writer << Input.SceneRooms << Input.PartitionIndex << Input.PartitionCount;
			int32 Count = Input.Sources.Num(); Writer << Count;
			for (FSourceInput& Source : Input.Sources)
			{
				Writer << Source.Name << Source.Kind << Source.bHidden << Source.bMain << Source.bOwned << Source.Tolerance << Source.Transform << Source.Size << Source.Up;
				Count = Source.Knots.Num(); Writer << Count;
				for (FMiningSpatialKnot& Knot : Source.Knots)
				{
					Writer << Knot.Position << Knot.Arrive << Knot.Leave << Knot.Roll << Knot.SectionScale << Knot.bLocked;
					Count = Knot.Profile.Num(); Writer << Count;
					for (FMiningPenPoint& Point : Knot.Profile) Writer << Point.Position << Point.Arrive << Point.Leave << Point.Mode << Point.bLocked << Point.GuideStation;
				}
				for (FPortalInput& Portal : Source.Portals) Writer << Portal.bBound << Portal.Wall << Portal.Transform << Portal.HalfSize << Portal.Center << Portal.Normal;
			}
		}
	}

	bool Capture(const AMiningSpatialRegion& Region, FVector Origin, double Bedrock, FRegionInput& Input, FString& Error)
	{
		check(IsInGameThread());
		Input = FRegionInput();
		const AMiningSpatialTunnel* Main = Region.MainTunnel;
		if ((!Region.bAutomaticBedrock && (Region.RockSize.ContainsNaN() || Region.RockSize.GetMin() < 500 || !Region.GetActorRotation().IsNearlyZero() || !Region.GetActorScale3D().Equals(FVector::OneVector))) || !IsValid(Region.FromRoom) || !IsValid(Region.ToRoom) || !IsValid(Main) || !Main->bEnabled || Main->Kind != EMiningManualKind::Passage || Main->Region != &Region || Main->StartPortal.Room != Region.FromRoom || Main->EndPortal.Room != Region.ToRoom)
		{ Error = TEXT("三维矿区需有效岩石范围和主通道，主通道起终点分别绑定FromRoom/ToRoom；矿区旋转为0、缩放为1"); return false; }
		if (!Region.GetShapes().IsEmpty() || !Region.GetChambers().IsEmpty() || !Region.Plugs.IsEmpty()) { Error = TEXT("三维矿区请只使用三维空间；旧二维空间请保留在原矿区"); return false; }
		Input.bAutomaticBedrock = Region.bAutomaticBedrock; Input.ExcavationMargin = Region.ExcavationMargin;
		Input.Bounds = Region.bAutomaticBedrock ? FBox(ForceInit) : Region.GetRockBounds(); Input.Origin = Origin; Input.Bedrock = Region.bAutomaticBedrock ? Region.AutomaticBedrockThickness : Bedrock; Input.Tolerance = Region.CurveTolerance;
		if (Input.bAutomaticBedrock && (!FMath::IsFinite(Input.ExcavationMargin) || Input.ExcavationMargin < 10 || !FMath::IsFinite(Input.Bedrock) || Input.Bedrock < 30 || Input.Bedrock > 500)) { Error = TEXT("场景外扩余量至少10cm，基岩厚度30–500cm；设计保留，参数需为有限数值。"); return false; }
		Input.Rooms[0] = Region.FromRoom->GetRoomBounds(); Input.Rooms[1] = Region.ToRoom->GetRoomBounds();
		Input.StartWall = static_cast<int32>(Main->StartPortal.Wall); Input.EndWall = static_cast<int32>(Main->EndPortal.Wall);
		const TArray<AMiningSpatialRegion*> Regions = Input.bAutomaticBedrock ? SceneRegions(Region) : TArray<AMiningSpatialRegion*>{const_cast<AMiningSpatialRegion*>(&Region)};
		if (Regions.IsEmpty()) { Error = TEXT("场景级基岩不能与旧二维矿区共用生成器；设计保留。"); return false; }
		Input.PartitionIndex = Regions.IndexOfByKey(&Region); Input.PartitionCount = Regions.Num();
		TArray<AMiningSpatialTunnel*> Sources;
		for (const AMiningSpatialRegion* Item : Regions)
		{
			if (Input.bAutomaticBedrock && (!Item->bAutomaticBedrock || !FMath::IsFinite(Item->ExcavationMargin) || Item->ExcavationMargin < 10 || !FMath::IsFinite(Item->AutomaticBedrockThickness) || Item->AutomaticBedrockThickness < 30 || Item->AutomaticBedrockThickness > 500))
			{ Error = TEXT("场景级基岩要求所有三维矿区启用自动外围，且余量与厚度有效；不能与旧局部边界混用。"); return false; }
			if (Input.bAutomaticBedrock) { Input.ExcavationMargin = FMath::Max(Input.ExcavationMargin, double(Item->ExcavationMargin)); Input.Bedrock = FMath::Max(Input.Bedrock, double(Item->AutomaticBedrockThickness)); }
			for (const AMiningLayoutRoom* Room : {Item->FromRoom.Get(), Item->ToRoom.Get()})
			{
				if (!IsValid(Room) || Room->RoomSize.ContainsNaN() || Room->RoomSize.GetMin() < 250 || Room->GetActorTransform().ContainsNaN()) { Error = TEXT("场景主房间尺寸或位置无效；设计保留。"); return false; }
				Input.SceneRooms.AddUnique(Room->GetRoomBounds());
			}
			TArray<AMiningSpatialTunnel*> LocalSources = Item->GetTunnels();
			LocalSources.RemoveAll([](const AMiningSpatialTunnel* Source) { return !Source->bEnabled; });
			if (LocalSources.Num() > 64) { Error = TEXT("每矿区最多64个启用的三维空间，请拆分矿区"); return false; }
			Sources.Append(LocalSources);
		}
		for (const AMiningSpatialTunnel* Actor : Sources)
		{
			if (!Actor->bEnabled) continue;
			FSourceInput& Source = Input.Sources.AddDefaulted_GetRef();
			Source.Name = Actor->GetActorNameOrLabel(); Source.Kind = Actor->Kind; Source.bHidden = Actor->bHiddenCavity; Source.bMain = Actor == Main;
			Source.bOwned = Actor->Region == &Region; Source.Tolerance = Actor->Region ? Actor->Region->CurveTolerance : Input.Tolerance;
			Source.Transform = Actor->GetActorTransform(); Source.Size = Actor->RoomSize; Source.Up = Source.Transform.TransformVectorNoScale(Actor->InitialUp);
			if (Source.Kind == EMiningManualKind::SideRoom)
			{
				if (Source.Size.ContainsNaN() || Source.Size.GetMin() < 250 || Source.Transform.ContainsNaN() || !Actor->GetActorScale3D().Equals(FVector::OneVector)) { Error = TEXT("支线房间尺寸至少250cm，缩放为1；支持任意旋转"); return false; }
			}
			else if (!Actor->ResolveKnots(Source.Knots, Error)) return false;
			if (Source.Kind != EMiningManualKind::Passage) continue;
			for (int32 End = 0; End < 2; ++End)
			{
				const FMiningSpatialPortal& Portal = End ? Actor->EndPortal : Actor->StartPortal;
				FPortalInput& Copy = Source.Portals[End]; Copy.bBound = Portal.Room != nullptr; Copy.Wall = static_cast<int32>(Portal.Wall);
				if (!Copy.bBound) continue;
				if (const AMiningLayoutRoom* Room = Cast<AMiningLayoutRoom>(Portal.Room); Room && !Region.bAutomaticBedrock)
					if ((Room != Region.FromRoom && Room != Region.ToRoom) || (Room == Region.FromRoom && Portal.Wall != Main->StartPortal.Wall) || (Room == Region.ToRoom && Portal.Wall != Main->EndPortal.Wall)) { Error = TEXT("主房间额外入口只能用对应连接墙，避免绕过主线回收"); return false; }
				if (!Actor->ResolvePortal(Portal, Copy.Transform, Copy.HalfSize, Copy.Center, Copy.Normal, Error)) return false;
			}
		}
		MakeKey(Input); return true;
	}

	FBuildPtr Compute(FRegionInput Input, double ChunkLength, int32 Budget, FBuildPtr Previous)
	{
		if (Previous && Previous->bSuccess && Previous->Input.Key == Input.Key && Previous->Grid && Previous->Grid->Length == ChunkLength)
		{
			bool bWithinBudget = true;
			for (const MiningChunks::FSpatialCell& Cell : Previous->Grid->Cells) if (Cell.Mesh && Cell.Mesh->TriangleCount() > Budget) bWithinBudget = false;
			if (bWithinBudget) return Previous;
		}
		const double Started = FPlatformTime::Seconds();
		auto Result = MakeShared<FRegionBuild, ESPMode::ThreadSafe>(); Result->Input = MoveTemp(Input);
		if (BuildGeometry(Result->Input, *Result))
		{
			auto Grid = MakeShared<MiningChunks::FSpatialGrid, ESPMode::ThreadSafe>();
			FBox Dirty(ForceInit);
			if (Previous && Previous->bSuccess && Previous->Grid)
			{
				const FRegionInput& Old = Previous->Input; const FRegionInput& New = Result->Input;
				bool bSameDomain = Old.Origin == New.Origin && Old.Bounds == New.Bounds && Old.Rooms[0] == New.Rooms[0] && Old.Rooms[1] == New.Rooms[1] && Old.SceneRooms == New.SceneRooms && Old.PartitionCount == New.PartitionCount && Old.PartitionIndex == New.PartitionIndex && Old.Sources.Num() == New.Sources.Num() && Previous->Cutters.Num() == Result->Cutters.Num();
				if (bSameDomain) for (int32 Index = 0; Index < New.Sources.Num(); ++Index)
					if (Old.Sources[Index].Name != New.Sources[Index].Name || Old.Sources[Index].Kind != New.Sources[Index].Kind) { bSameDomain = false; break; }
				if (bSameDomain)
				{
					for (int32 Index = 0; Index < Result->Cutters.Num(); ++Index)
					{
						const FBox Changed = MiningChunks::ChangedBounds(Previous->Cutters[Index], Result->Cutters[Index]);
						if (Changed.IsValid) Dirty += Changed;
					}
				}
				else { Dirty = Previous->Hollow.Bounds; Dirty += Result->Hollow.Bounds; }
			}
			if (!MiningChunks::SplitSpatialIncremental(Result->Rock, ChunkLength, *Grid, Previous && Previous->bSuccess ? Previous->Grid.Get() : nullptr, Dirty)) Result->Error = TEXT("chunk generation failed or exceeds 128 chunks per region");
			else
			{
				Result->bSuccess = true;
				for (const MiningChunks::FSpatialCell& Cell : Grid->Cells)
					if (Cell.Mesh && Cell.Mesh->TriangleCount() > Budget) { Result->bSuccess = false; Result->Error = TEXT("initial space geometry exceeds the existing triangle budget"); break; }
				Result->Grid = MoveTemp(Grid); Result->Plan.SpatialGrid = Result->Grid;
			}
		}
		Result->ComputeMilliseconds = (FPlatformTime::Seconds() - Started) * 1000;
		return Result;
	}
	const FRegionBuild* Prepared(const AMiningSpatialRegion* Region)
	{
		check(IsInGameThread());
		const FBuildPtr* Found = PreparedBuilds ? PreparedBuilds->Find(Region) : nullptr;
		return Found ? Found->Get() : nullptr;
	}
	FPreparedScope::FPreparedScope(const TMap<const AMiningSpatialRegion*, FBuildPtr>& Builds) : Previous(PreparedBuilds) { check(IsInGameThread()); PreparedBuilds = &Builds; }
	FPreparedScope::~FPreparedScope() { PreparedBuilds = Previous; }
}
