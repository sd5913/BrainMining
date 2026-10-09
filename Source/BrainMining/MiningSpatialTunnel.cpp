#include "MiningSpatialTunnel.h"
#include "MiningSpatialGeometry.h"
#include "MiningSpatialEnvelope.h"
#include "MiningSpatialEditBridge.h"
#include "MiningLayoutRoom.h"
#include "EngineUtils.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

AMiningSpatialTunnel::AMiningSpatialTunnel()
{
	bIsEditorOnlyActor = true;
	PrimaryActorTick.bCanEverTick = false;
	Path = CreateDefaultSubobject<UMiningSpatialPathComponent>(TEXT("SpatialPath"));
	SetRootComponent(Path);
	FMiningSpatialKnot First; First.Profile = MiningSpatial::Rectangle();
	FMiningSpatialKnot Last = First; Last.Position = FVector(1500, 0, 0);
	Knots = {First, Last};
	EndPortal.Wall = EMiningSpatialWall::NegativeX;
}

TArray<AMiningSpatialTunnel*> AMiningSpatialRegion::GetTunnels() const
{
	TArray<AMiningSpatialTunnel*> Result;
	if (GetWorld())
		for (TActorIterator<AMiningSpatialTunnel> Item(GetWorld()); Item; ++Item)
			if (Item->Region == this && Item->GetLevel() == GetLevel()) Result.Add(*Item);
	Result.Sort([](const AMiningSpatialTunnel& First, const AMiningSpatialTunnel& Last) { return First.GetPathName() < Last.GetPathName(); });
	return Result;
}

uint32 AMiningSpatialRegion::GetGeometryHash() const
{
	uint32 Hash = HashCombineFast(Super::GetGeometryHash(), GetTypeHash(MainTunnel.Get()));
	Hash = HashCombineFast(Hash, GetTypeHash(bAutomaticBedrock));
	Hash = HashCombineFast(Hash, GetTypeHash(ExcavationMargin));
	Hash = HashCombineFast(Hash, GetTypeHash(AutomaticBedrockThickness));
	for (const AMiningSpatialTunnel* Tunnel : GetTunnels()) Hash = HashCombineFast(Hash, HashCombineFast(GetTypeHash(Tunnel), Tunnel->GetGeometryHash()));
	return Hash;
}

FBox AMiningSpatialRegion::GetRockBounds() const
{
	if (!bAutomaticBedrock) return Super::GetRockBounds();
	TArray<AMiningSpatialRegion*> Regions = MiningSpatial::SceneRegions(*this);
	if (Regions.IsEmpty()) Regions.Add(const_cast<AMiningSpatialRegion*>(this));
	uint32 Hash = 0; float Margin = 10;
	TArray<AMiningLayoutRoom*> Rooms;
	TArray<AMiningSpatialTunnel*> Tunnels;
	for (const AMiningSpatialRegion* Region : Regions)
	{
		Hash = HashCombineFast(Hash, Region->GetGeometryHash());
		if (FMath::IsFinite(Region->ExcavationMargin)) Margin = FMath::Max(Margin, Region->ExcavationMargin);
		Rooms.AddUnique(Region->FromRoom); Rooms.AddUnique(Region->ToRoom); Tunnels.Append(Region->GetTunnels());
	}
	if (PreviewBounds.IsValid && PreviewBoundsHash == Hash) return PreviewBounds;
	FBox Result(ForceInit);
	for (const AMiningLayoutRoom* Room : Rooms)
		if (IsValid(Room) && !Room->RoomSize.ContainsNaN() && !Room->GetActorTransform().ContainsNaN()) Result += Room->GetRoomBounds();
	for (const AMiningSpatialTunnel* Tunnel : Tunnels)
	{
		if (!Tunnel->bEnabled) continue;
		if (Tunnel->Kind == EMiningManualKind::SideRoom)
		{
			if (!Tunnel->RoomSize.ContainsNaN() && !Tunnel->GetActorTransform().ContainsNaN()) Result += FBox(-Tunnel->RoomSize * 0.5, Tunnel->RoomSize * 0.5).TransformBy(Tunnel->GetActorTransform());
			continue;
		}
		TArray<FMiningSpatialKnot> Knots; TArray<MiningSpatial::FRing> Rings; FString Error;
		if (Tunnel->ResolveKnots(Knots, Error) && MiningSpatial::Sample(Knots, Tunnel->GetActorTransform().TransformVectorNoScale(Tunnel->InitialUp), Tunnel->Region ? Tunnel->Region->CurveTolerance : CurveTolerance, Rings, Error, nullptr, true))
			for (const MiningSpatial::FRing& Ring : Rings)
				for (const FVector2D& Point : Ring.Profile) Result += Ring.Center + Ring.Right * Point.X + Ring.Up * Point.Y;
		else
			for (const FMiningSpatialKnot& Knot : Knots)
				if (!Knot.Position.ContainsNaN()) Result += Knot.Position;
	}
	PreviewBoundsHash = Hash;
	PreviewBounds = Result.IsValid ? Result.ExpandBy(Margin) : Super::GetRockBounds();
	return PreviewBounds;
}

uint32 AMiningSpatialTunnel::GetGeometryHash() const
{
	FString Data;
	for (TFieldIterator<FProperty> Property(StaticClass()); Property; ++Property)
		if (Property->HasAnyPropertyFlags(CPF_Edit)) Property->ExportText_InContainer(0, Data, this, nullptr, const_cast<AMiningSpatialTunnel*>(this), PPF_None);
	uint32 Hash = HashCombineFast(GetTypeHash(Data), GetTypeHash(GetActorTransform().ToString()));
	if (Region) { Hash = HashCombineFast(Hash, GetTypeHash(Region->bAutomaticBedrock)); Hash = HashCombineFast(Hash, GetTypeHash(Region->bFreeEditing)); }
	for (const FMiningSpatialPortal* Portal : {&StartPortal, &EndPortal})
		if (IsValid(Portal->Room))
		{
			Hash = HashCombineFast(Hash, GetTypeHash(Portal->Room->GetActorTransform().ToString()));
			if (const AMiningSpatialTunnel* Room = Cast<AMiningSpatialTunnel>(Portal->Room)) Hash = HashCombineFast(Hash, GetTypeHash(Room->RoomSize));
			if (const AMiningLayoutRoom* Room = Cast<AMiningLayoutRoom>(Portal->Room)) Hash = HashCombineFast(Hash, GetTypeHash(Room->RoomSize));
		}
	return Hash;
}

bool AMiningSpatialTunnel::ResolvePortal(const FMiningSpatialPortal& Portal, FTransform& RoomTransform, FVector& HalfSize, FVector& Center, FVector& Normal, FString& Error) const
{
	if (!IsValid(Portal.Room) || Portal.Room->GetLevel() != GetLevel() || Portal.Offset.ContainsNaN()) { Error = TEXT("入口房间无效或不在同一关卡"); return false; }
	if (const AMiningLayoutRoom* Room = Cast<AMiningLayoutRoom>(Portal.Room))
	{
		RoomTransform = FTransform(Room->GetRoomBounds().GetCenter()); HalfSize = Room->RoomSize * 0.5;
	}
	else if (const AMiningSpatialTunnel* SideRoom = Cast<AMiningSpatialTunnel>(Portal.Room); SideRoom && SideRoom->Kind == EMiningManualKind::SideRoom && SideRoom->Region == Region && SideRoom->bEnabled)
	{
		RoomTransform = SideRoom->GetActorTransform(); HalfSize = SideRoom->RoomSize * 0.5;
	}
	else { Error = TEXT("入口只能绑定主房间或同矿区启用的三维支线房间"); return false; }
	if (HalfSize.ContainsNaN() || HalfSize.GetMin() < 125 || !RoomTransform.GetScale3D().Equals(FVector::OneVector)) { Error = TEXT("房间尺寸至少250，缩放须为1"); return false; }
	const int32 Wall = static_cast<int32>(Portal.Wall), Axis = Wall / 2;
	if (Wall > 5) { Error = TEXT("入口墙面无效"); return false; }
	FVector Local = FVector::ZeroVector, Direction = FVector::ZeroVector;
	Direction[Axis] = Wall % 2 ? -1 : 1;
	Local[Axis] = Direction[Axis] * HalfSize[Axis];
	Local[(Axis + 1) % 3] = Portal.Offset.X; Local[(Axis + 2) % 3] = Portal.Offset.Y;
	Center = RoomTransform.TransformPosition(Local); Normal = RoomTransform.TransformVectorNoScale(Direction);
	return true;
}

bool AMiningSpatialTunnel::ResolveKnots(TArray<FMiningSpatialKnot>& Resolved, FString& Error) const
{
	if (GetActorTransform().ContainsNaN() || !GetActorScale3D().Equals(FVector::OneVector)) { Error = TEXT("隧道使用截面倍率调整大小，Actor缩放请保持1"); return false; }
	Resolved = Knots;
	if (Resolved.Num() < 2 || Resolved.Num() > 64) { Error = TEXT("每条三维隧道需要2–64个节点"); return false; }
	for (FMiningSpatialKnot& Knot : Resolved)
	{
		Knot.Position = GetActorTransform().TransformPosition(Knot.Position);
		Knot.Arrive = GetActorTransform().TransformVectorNoScale(Knot.Arrive);
		Knot.Leave = GetActorTransform().TransformVectorNoScale(Knot.Leave);
	}
	for (int32 Endpoint = 0; Endpoint < 2; ++Endpoint)
	{
		const FMiningSpatialPortal& Portal = Endpoint ? EndPortal : StartPortal;
		if (!Portal.Room) continue;
		FTransform Transform; FVector HalfSize, Center, Normal;
		if (!ResolvePortal(Portal, Transform, HalfSize, Center, Normal, Error)) return false;
		FMiningSpatialKnot& Knot = Endpoint ? Resolved.Last() : Resolved[0];
		Knot.Position = Center;
		if (!Region || !Region->bAutomaticBedrock || Portal.bAlignToWall)
		{
			if (Endpoint) Knot.Arrive = Normal * FMath::Max(50.0, Knot.Arrive.Length());
			else Knot.Leave = Normal * FMath::Max(50.0, Knot.Leave.Length());
		}
	}
	return true;
}

bool AMiningSpatialTunnel::FitPortals()
{
	FTransform Transform; FVector HalfSize, Start, Finish, StartNormal, EndNormal; FString Error;
	if (!ResolvePortal(StartPortal, Transform, HalfSize, Start, StartNormal, Error) || !ResolvePortal(EndPortal, Transform, HalfSize, Finish, EndNormal, Error)) { ShapeStatus = Error; return false; }
	Modify();
	FMiningSpatialKnot First, Last;
	First.Profile = MiningSpatial::Rectangle(); Last.Profile = First.Profile;
	First.Position = GetActorTransform().InverseTransformPosition(Start); Last.Position = GetActorTransform().InverseTransformPosition(Finish);
	const double Length = FVector::Distance(Start, Finish) / 3;
	First.Leave = GetActorTransform().InverseTransformVectorNoScale(StartNormal * Length); First.Arrive = -First.Leave;
	Last.Arrive = GetActorTransform().InverseTransformVectorNoScale(EndNormal * Length); Last.Leave = -Last.Arrive;
	Knots = {First, Last}; Changed(); return true;
}

void AMiningSpatialTunnel::Changed()
{
#if WITH_EDITOR
	if (MiningSpatialEditing::Handle(this)) return;
	if (GEditor) GEditor->RedrawLevelEditingViewports();
#endif
	ShapeStatus = TEXT("待验证；尚未应用到游戏地形");
}

#if WITH_EDITOR
void AMiningSpatialTunnel::PostEditMove(bool bFinished) { Super::PostEditMove(bFinished); Changed(); }
void AMiningSpatialTunnel::PostEditChangeProperty(FPropertyChangedEvent& Event) { Super::PostEditChangeProperty(Event); Changed(); }
void AMiningSpatialTunnel::PostEditUndo() { Super::PostEditUndo(); Changed(); }
#endif
