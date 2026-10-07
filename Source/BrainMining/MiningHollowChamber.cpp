#include "MiningHollowChamber.h"
#include "MiningHollowRegion.h"
#include "MiningTunnelGenerator.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "EngineUtils.h"
#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

AMiningHollowChamber::AMiningHollowChamber()
{
	bIsEditorOnlyActor = true;
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ChamberFloorCenter")));
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("EditableChamberBounds"));
	Bounds->SetupAttachment(RootComponent);
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetHiddenInGame(true);
	Bounds->SetLineThickness(4);
	Path = CreateDefaultSubobject<USplineComponent>(TEXT("EditableBranchPath"));
	Path->SetupAttachment(RootComponent);
	Path->bInputSplinePointsToConstructionScript = true;
	Path->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Path->SetHiddenInGame(true);
	RefreshGuides();
}

FBox AMiningHollowChamber::GetChamberBounds() const
{
	const FVector Size = ChamberSize * GetActorScale3D();
	const FVector Center = GetActorLocation() + FVector(0, 0, Size.Z * 0.5);
	return FBox(Center - Size * 0.5, Center + Size * 0.5);
}

void AMiningHollowChamber::RefreshGuides()
{
	Bounds->SetRelativeLocation(FVector(0, 0, ChamberSize.Z * 0.5));
	Bounds->SetBoxExtent(ChamberSize.GetAbs().ComponentMax(FVector(1)) * 0.5);
	Bounds->ShapeColor = bHiddenEntrance ? FColor(255, 80, 180) : FColor(60, 200, 255);
	for (int32 Index = 0; Index < Path->GetNumberOfSplinePoints(); ++Index) Path->SetSplinePointType(Index, ESplinePointType::Linear, false);
	Path->UpdateSpline();
}

void AMiningHollowChamber::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshGuides();
	if (!IsValid(Region)) ChamberStatus = TEXT("NOT APPLIED: choose an owning hollow Region");
	NotifyGenerators();
}

TArray<FVector> AMiningHollowChamber::GetInteriorPoints() const
{
	TArray<FVector> Points;
	if (bManualBranchPath)
		for (int32 Index = 1; Index + 1 < Path->GetNumberOfSplinePoints(); ++Index)
			Points.Add(Path->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World));
	return Points;
}

uint32 AMiningHollowChamber::GetGeometryHash() const
{
	uint32 Hash = GetTypeHash(GetActorTransform().ToString());
	Hash = HashCombineFast(Hash, GetTypeHash(Region.Get()));
	Hash = HashCombineFast(Hash, GetTypeHash(ChamberSize));
	for (float Value : {ConnectionPosition, BranchRadius, EntranceThickness}) Hash = HashCombineFast(Hash, GetTypeHash(Value));
	for (bool Value : {bEnabled, bHiddenEntrance, bManualBranchPath}) Hash = HashCombineFast(Hash, GetTypeHash(Value));
	for (const FVector& Point : GetInteriorPoints()) Hash = HashCombineFast(Hash, GetTypeHash(Point));
	for (int32 Index = 0; Index < Path->GetNumberOfSplinePoints(); ++Index)
		Hash = HashCombineFast(Hash, GetTypeHash(Path->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World)));
	return HashCombineFast(Hash, GetTypeHash(GetActorNameOrLabel()));
}

void AMiningHollowChamber::ApplyResolvedPath(const TArray<FVector>& Points)
{
	Path->Modify();
	Path->SetSplinePoints(Points, ESplineCoordinateSpace::World, false);
	RefreshGuides();
}

void AMiningHollowChamber::ResetBranchPath()
{
	if (!GetWorld() || GetWorld()->IsGameWorld() || !IsValid(Region)) return;
#if WITH_EDITOR
	const FScopedTransaction Transaction(NSLOCTEXT("Mining", "ResetHollowBranch", "Reset editable hollow branch"));
#endif
	Modify();
	const bool bWasManual = bManualBranchPath;
	bManualBranchPath = false;
	Region->RebuildRegion();
	if (!Region->RegionStatus.StartsWith(TEXT("Ready"))) { bManualBranchPath = bWasManual; NotifyGenerators(); return; }
	if (Path->GetNumberOfSplinePoints() < 2) return;
	const FVector Start = Path->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
	const FVector End = Path->GetLocationAtSplinePoint(Path->GetNumberOfSplinePoints() - 1, ESplineCoordinateSpace::World);
	ApplyResolvedPath({Start, FMath::Lerp(Start, End, 0.8), End});
	bManualBranchPath = true;
	NotifyGenerators();
	MarkPackageDirty();
}

void AMiningHollowChamber::RebuildRegion() { if (IsValid(Region)) Region->RebuildRegion(); }

void AMiningHollowChamber::NotifyGenerators()
{
	if (!GetWorld() || GetWorld()->IsGameWorld()) return;
	bool bHasOwner = false;
	for (TActorIterator<AMiningTunnelGenerator> Generator(GetWorld()); Generator; ++Generator)
		if (Generator->GetLevel() == GetLevel() && Generator->bHollowRegionPrototype)
		{
			Generator->RequestRebuild();
			bHasOwner |= IsValid(Region) && Region->GetLevel() == GetLevel() && Generator->HollowRegion == Region;
		}
	ChamberStatus = !bHasOwner ? TEXT("NOT APPLIED: assign the hollow generator's Region in this level") : !bEnabled ? TEXT("Disabled; edits retained") : TEXT("Pending region rebuild; previous successful geometry remains visible");
}

#if WITH_EDITOR
void AMiningHollowChamber::PostEditMove(bool bFinished) { Super::PostEditMove(bFinished); bInteractiveEdit = !bFinished; RefreshGuides(); NotifyGenerators(); }
void AMiningHollowChamber::PostEditChangeProperty(FPropertyChangedEvent& Event) { Super::PostEditChangeProperty(Event); RefreshGuides(); NotifyGenerators(); }
void AMiningHollowChamber::PostEditUndo() { Super::PostEditUndo(); bInteractiveEdit = false; RefreshGuides(); NotifyGenerators(); }
#endif
