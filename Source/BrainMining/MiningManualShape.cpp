#include "MiningManualShape.h"
#include "MiningManualRegion.h"
#include "MiningLayoutRoom.h"
#include "MiningTunnelGenerator.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

AMiningManualShape::AMiningManualShape()
{
	bIsEditorOnlyActor = true;
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ShapeOrigin")));
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("ShapeBounds"));
	Bounds->SetupAttachment(RootComponent);
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetHiddenInGame(true);
	Bounds->SetLineThickness(3);
	for (const FVector2D& Position : {FVector2D(-500, 0), FVector2D(500, 0), FVector2D(500, 700), FVector2D(-500, 700)})
	{
		FMiningPenPoint Point; Point.Position = Position; BoundaryA.Add(Point);
	}
	EndPortal.Wall = EMiningPortalWall::NegativeX;
}

FBox AMiningManualShape::GetRoomBounds() const
{
	return FBox(GetActorLocation() - FVector(Size.X * 0.5, Depth * 0.5, 0), GetActorLocation() + FVector(Size.X * 0.5, Depth * 0.5, Size.Y));
}

void AMiningManualShape::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	FBox Box = GetRoomBounds();
	if (Kind != EMiningManualKind::SideRoom)
	{
		Box = FBox(ForceInit);
		for (bool bSecond : {false, true})
		{
			if (bSecond && Kind != EMiningManualKind::Passage) continue;
			TArray<FMiningPenPoint> Points; FString Error;
			ResolveBoundary(bSecond, Points, Error);
			for (const FMiningPenPoint& Point : Points)
				for (const FVector2D& Position : {Point.Position, Point.Position + Point.Arrive, Point.Position + Point.Leave})
				{
					Box += FVector(Position.X, GetActorLocation().Y - Depth * 0.5, Position.Y);
					Box += FVector(Position.X, GetActorLocation().Y + Depth * 0.5, Position.Y);
				}
		}
	}
	if (Box.IsValid) { Bounds->SetWorldLocation(Box.GetCenter()); Bounds->SetBoxExtent(Box.GetExtent()); }
	Bounds->ShapeColor = Kind == EMiningManualKind::SideRoom ? FColor(80, 240, 170) : bHiddenCavity ? FColor(250, 100, 200) : FColor(70, 190, 255);
	Changed();
}

bool AMiningManualShape::ResolvePortal(const FMiningManualPortal& Portal, FVector& Center, FVector& Normal, FString& Error) const
{
	const AMiningLayoutRoom* MainRoom = Cast<AMiningLayoutRoom>(Portal.Room);
	const AMiningManualShape* SideRoom = Cast<AMiningManualShape>(Portal.Room);
	if (!IsValid(Portal.Room) || Portal.Room->GetLevel() != GetLevel() ||
		(!MainRoom && (!SideRoom || SideRoom->Kind != EMiningManualKind::SideRoom || !SideRoom->bEnabled || SideRoom->Region != Region)))
	{
		Error = TEXT("入口需要同关卡主房间或同矿区启用的支线房间"); return false;
	}
	if (!Portal.Room->GetActorRotation().IsNearlyZero() || !Portal.Room->GetActorScale3D().Equals(FVector::OneVector))
	{
		Error = TEXT("入口房间请保持旋转 0、缩放 1，用尺寸编辑大小"); return false;
	}
	const FBox Box = MainRoom ? MainRoom->GetRoomBounds() : SideRoom->GetRoomBounds();
	const bool bVerticalWall = Portal.Wall == EMiningPortalWall::PositiveX || Portal.Wall == EMiningPortalWall::NegativeX;
	const bool bPositive = Portal.Wall == EMiningPortalWall::PositiveX || Portal.Wall == EMiningPortalWall::PositiveZ;
	const int32 Axis = bVerticalWall ? 0 : 2;
	const int32 Along = bVerticalWall ? 2 : 0;
	if (!FMath::IsFinite(Portal.Offset) || !FMath::IsFinite(Portal.Aperture) || !FMath::IsFinite(Portal.DepthOffset) ||
		Portal.Aperture < 100 || FMath::Abs(Portal.Offset) + Portal.Aperture * 0.5 > Box.GetExtent()[Along] - 1 ||
		FMath::Abs(Portal.DepthOffset) + Depth * 0.5 > Box.GetExtent().Y - 1)
	{
		Error = TEXT("入口超出房间墙面/深度；请调整入口尺寸或偏移，不会自动缩小"); return false;
	}
	Center = Box.GetCenter(); Center[Axis] = bPositive ? Box.Max[Axis] : Box.Min[Axis];
	Center[Along] += Portal.Offset; Center.Y += Portal.DepthOffset;
	Normal = FVector::ZeroVector; Normal[Axis] = bPositive ? 1 : -1;
	if (FMath::Abs(Center.Y - GetActorLocation().Y) > 0.1)
	{
		Error = TEXT("横截面通道须保持相同 Y 中心；调整通道 Y 或入口深度偏移"); return false;
	}
	return true;
}

bool AMiningManualShape::ResolveBoundary(bool bSecond, TArray<FMiningPenPoint>& Points, FString& Error) const
{
	Points = bSecond ? BoundaryB : BoundaryA;
	const FVector2D Origin(GetActorLocation().X, GetActorLocation().Z);
	for (FMiningPenPoint& Point : Points) Point.Position += Origin;
	if (Kind != EMiningManualKind::Passage || Points.Num() < 2) return true;
	for (bool bEnd : {false, true})
	{
		const FMiningManualPortal& Portal = bEnd ? EndPortal : StartPortal;
		if (!Portal.Room) continue;
		FVector Center, Normal;
		if (!ResolvePortal(Portal, Center, Normal, Error)) return false;
		const FVector2D Across(-Normal.Z, Normal.X);
		FMiningPenPoint& Point = bEnd ? Points.Last() : Points[0];
		Point.Position = FVector2D(Center.X, Center.Z) + Across * (Portal.Aperture * 0.5 * (bEnd ? -1 : 1) * (bSecond ? -1 : 1));
	}
	return true;
}

uint32 AMiningManualShape::GetGeometryHash() const
{
	uint32 Hash = GetTypeHash(GetActorTransform().ToString());
	Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Kind)));
	Hash = HashCombineFast(Hash, GetTypeHash(bEnabled)); Hash = HashCombineFast(Hash, GetTypeHash(bHiddenCavity));
	Hash = HashCombineFast(Hash, GetTypeHash(Depth)); Hash = HashCombineFast(Hash, GetTypeHash(Size));
	for (const TArray<FMiningPenPoint>* Points : {&BoundaryA, &BoundaryB})
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Points->Num()));
		for (const FMiningPenPoint& Point : *Points)
		{
			Hash = HashCombineFast(Hash, GetTypeHash(Point.Position)); Hash = HashCombineFast(Hash, GetTypeHash(Point.Arrive));
			Hash = HashCombineFast(Hash, GetTypeHash(Point.Leave)); Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Point.Mode)));
		}
	}
	for (const FMiningManualPortal* Portal : {&StartPortal, &EndPortal})
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Portal->Room.Get()));
		Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Portal->Wall)));
		for (float Value : {Portal->Offset, Portal->DepthOffset, Portal->Aperture}) Hash = HashCombineFast(Hash, GetTypeHash(Value));
		if (const AMiningLayoutRoom* Room = Cast<AMiningLayoutRoom>(Portal->Room))
		{
			Hash = HashCombineFast(Hash, GetTypeHash(Room->GetActorTransform().ToString()));
			Hash = HashCombineFast(Hash, GetTypeHash(Room->RoomSize));
		}
	}
	return Hash;
}

void AMiningManualShape::Changed()
{
	if (!GetWorld() || GetWorld()->IsGameWorld()) return;
	for (TActorIterator<AMiningTunnelGenerator> Generator(GetWorld()); Generator; ++Generator)
		if (Generator->GetLevel() == GetLevel() && Generator->UsesHollowRegion(Region)) Generator->RequestRebuild();
}

void AMiningManualShape::ResetRectangle()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld()) return;
	const FScopedTransaction Transaction(NSLOCTEXT("Mining", "ManualRectangle", "手绘空间：替换为矩形")); Modify();
#endif
	BoundaryA.Reset(); BoundaryB.Reset();
	for (const FVector2D& Position : {FVector2D(-Size.X * 0.5, 0), FVector2D(Size.X * 0.5, 0), FVector2D(Size.X * 0.5, Size.Y), FVector2D(-Size.X * 0.5, Size.Y)})
	{
		FMiningPenPoint Point; Point.Position = Position; BoundaryA.Add(Point);
	}
	OnConstruction(GetActorTransform()); MarkPackageDirty();
}

void AMiningManualShape::ResetEllipse()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld()) return;
	const FScopedTransaction Transaction(NSLOCTEXT("Mining", "ManualEllipse", "手绘空间：替换为椭圆")); Modify();
#endif
	BoundaryA.Reset(); BoundaryB.Reset();
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const double Angle = Index * UE_DOUBLE_PI * 0.5;
		FMiningPenPoint Point;
		Point.Position = FVector2D(FMath::Cos(Angle) * Size.X * 0.5, (FMath::Sin(Angle) + 1) * Size.Y * 0.5);
		Point.Leave = FVector2D(-FMath::Sin(Angle) * Size.X, FMath::Cos(Angle) * Size.Y) * (0.5522847498307936 * 0.5);
		Point.Arrive = -Point.Leave; Point.Mode = EMiningPenMode::Smooth; BoundaryA.Add(Point);
	}
	OnConstruction(GetActorTransform()); MarkPackageDirty();
}

bool AMiningManualShape::FitPortals()
{
	if (!GetWorld() || GetWorld()->IsGameWorld() || Kind != EMiningManualKind::Passage) return false;
	FVector Start, Finish, StartNormal, FinishNormal; FString Error;
	if (!ResolvePortal(StartPortal, Start, StartNormal, Error) || !ResolvePortal(EndPortal, Finish, FinishNormal, Error)) { ShapeStatus = Error; return false; }
#if WITH_EDITOR
	const FScopedTransaction Transaction(NSLOCTEXT("Mining", "ManualFit", "手绘空间：初始化入口连接")); Modify();
#endif
	BoundaryA.Reset(); BoundaryB.Reset();
	const double Landing = FMath::Min(500.0, FVector::Distance(Start, Finish) * 0.2);
	for (bool bSecond : {false, true})
	{
		TArray<FMiningPenPoint>& Points = bSecond ? BoundaryB : BoundaryA;
		const FVector First = Start + FVector(-StartNormal.Z, 0, StartNormal.X) * StartPortal.Aperture * 0.5 * (bSecond ? -1 : 1);
		const FVector Last = Finish + FVector(FinishNormal.Z, 0, -FinishNormal.X) * EndPortal.Aperture * 0.5 * (bSecond ? -1 : 1);
		for (const FVector& Position : {First, First + StartNormal * Landing, Last + FinishNormal * Landing, Last})
		{
			FMiningPenPoint Point; const FVector Local = Position - GetActorLocation(); Point.Position = FVector2D(Local.X, Local.Z); Points.Add(Point);
		}
	}
	OnConstruction(GetActorTransform()); MarkPackageDirty(); return true;
}

#if WITH_EDITOR
void AMiningManualShape::PostEditChangeProperty(FPropertyChangedEvent& Event) { Super::PostEditChangeProperty(Event); Changed(); }
void AMiningManualShape::PostEditMove(bool bFinished) { Super::PostEditMove(bFinished); if (bFinished) Changed(); }
void AMiningManualShape::PostEditUndo() { Super::PostEditUndo(); OnConstruction(GetActorTransform()); }
#endif
