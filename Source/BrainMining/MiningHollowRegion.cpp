#include "MiningHollowRegion.h"
#include "MiningHollowChamber.h"
#include "MiningLayoutRoom.h"
#include "MiningTunnelGenerator.h"
#include "MiningSpatialEditBridge.h"
#include "Components/SplineComponent.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

AMiningHollowRegion::AMiningHollowRegion()
{
	bIsEditorOnlyActor = true;
	PrimaryActorTick.bCanEverTick = false;
	Path = CreateDefaultSubobject<USplineComponent>(TEXT("EditablePassagePath"));
	SetRootComponent(Path);
	Path->SetClosedLoop(false);
	Path->bInputSplinePointsToConstructionScript = true;
	Path->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Path->SetHiddenInGame(true);
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("OrdinaryRockBounds"));
	Bounds->SetupAttachment(Path);
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetHiddenInGame(true);
	Bounds->ShapeColor = FColor(210, 160, 80);
	Bounds->SetLineThickness(3);
	for (float Position : {0.25f, 0.5f, 0.75f})
	{
		FMiningHollowPlug Plug;
		Plug.Position = Position;
		Plugs.Add(Plug);
	}
}

void AMiningHollowRegion::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	for (int32 Index = 0; Index < Path->GetNumberOfSplinePoints(); ++Index) Path->SetSplinePointType(Index, ESplinePointType::Linear, false);
	Path->UpdateSpline();
	NotifyGenerators();
}

TArray<FVector> AMiningHollowRegion::GetPathPoints() const
{
	TArray<FVector> Points;
	for (int32 Index = 0; Index < Path->GetNumberOfSplinePoints(); ++Index) Points.Add(Path->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World));
	return Points;
}

uint32 AMiningHollowRegion::GetGeometryHash() const
{
	uint32 Hash = GetTypeHash(GetActorTransform().ToString());
	for (const AMiningHollowChamber* Chamber : GetChambers()) Hash = HashCombineFast(Hash, HashCombineFast(GetTypeHash(Chamber), Chamber->GetGeometryHash()));
	for (float Value : {RegionWidth, RockAbove, RockBelow, PassageRadius, MaximumSlope}) Hash = HashCombineFast(Hash, GetTypeHash(Value));
	Hash = HashCombineFast(Hash, GetTypeHash(FromRoom.Get()));
	Hash = HashCombineFast(Hash, GetTypeHash(ToRoom.Get()));
	for (const FVector& Point : GetPathPoints()) Hash = HashCombineFast(Hash, GetTypeHash(Point));
	for (const FMiningHollowPlug& Plug : Plugs)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Plug.Position));
		Hash = HashCombineFast(Hash, GetTypeHash(Plug.Thickness));
		Hash = HashCombineFast(Hash, GetTypeHash(Plug.bEnabled));
	}
	return Hash;
}

TArray<AMiningHollowChamber*> AMiningHollowRegion::GetChambers() const
{
	TArray<AMiningHollowChamber*> Chambers;
	if (GetWorld())
		for (TActorIterator<AMiningHollowChamber> Chamber(GetWorld()); Chamber; ++Chamber)
			if (Chamber->GetLevel() == GetLevel() && Chamber->Region == this) Chambers.Add(*Chamber);
	Chambers.Sort([](const AMiningHollowChamber& First, const AMiningHollowChamber& Second) { return First.GetPathName() < Second.GetPathName(); });
	return Chambers;
}

void AMiningHollowRegion::RefreshBounds(const FBox& WorldBounds)
{
	ResolvedSize = WorldBounds.GetSize();
	Bounds->SetWorldLocation(WorldBounds.GetCenter());
	Bounds->SetBoxExtent(WorldBounds.GetExtent());
}

void AMiningHollowRegion::FitPathBetweenRooms()
{
	if (!IsValid(FromRoom) || !IsValid(ToRoom) || !GetWorld() || GetWorld()->IsGameWorld()) return;
#if WITH_EDITOR
	const FScopedTransaction Transaction(NSLOCTEXT("Mining", "FitHollowPath", "Fit hollow passage between rooms"));
#endif
	Modify();
	Path->Modify();
	const FBox First = FromRoom->GetRoomBounds();
	const FBox Second = ToRoom->GetRoomBounds();
	const FVector Start(First.Max.X, First.GetCenter().Y, First.Min.Z + PassageRadius);
	const FVector End(Second.Min.X, Second.GetCenter().Y, Second.Min.Z + PassageRadius);
	TArray<FVector> Points;
	for (int32 Index = 0; Index < 5; ++Index) Points.Add(FMath::Lerp(Start, End, Index / 4.0));
	Path->SetSplinePoints(Points, ESplineCoordinateSpace::World);
	OnConstruction(GetActorTransform());
	MarkPackageDirty();
}

void AMiningHollowRegion::RebuildRegion()
{
	if (!GetWorld() || GetWorld()->IsGameWorld()) return;
	for (TActorIterator<AMiningTunnelGenerator> Generator(GetWorld()); Generator; ++Generator)
		if (Generator->GetLevel() == GetLevel() && Generator->UsesHollowRegion(this)) Generator->RebuildTunnels();
}

void AMiningHollowRegion::NotifyGenerators()
{
	if (!GetWorld() || GetWorld()->IsGameWorld()) return;
	for (TActorIterator<AMiningTunnelGenerator> Generator(GetWorld()); Generator; ++Generator)
		if (Generator->GetLevel() == GetLevel() && Generator->UsesHollowRegion(this)) Generator->RequestRebuild();
}

#if WITH_EDITOR
void AMiningHollowRegion::PostEditMove(bool bFinished) { Super::PostEditMove(bFinished); bInteractiveEdit = !bFinished; if (!MiningSpatialEditing::Handle(this)) NotifyGenerators(); }
void AMiningHollowRegion::PostEditChangeProperty(FPropertyChangedEvent& Event) { Super::PostEditChangeProperty(Event); if (!MiningSpatialEditing::Handle(this)) NotifyGenerators(); }
void AMiningHollowRegion::PostEditUndo() { Super::PostEditUndo(); bInteractiveEdit = false; if (!MiningSpatialEditing::Handle(this)) NotifyGenerators(); }
#endif
