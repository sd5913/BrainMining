#include "MiningTunnelRoute.h"
#include "Components/SplineComponent.h"
#include "Components/TextRenderComponent.h"
#include "MiningTunnelShapeSection.h"
#include "MiningTunnelGenerator.h"
#include "MiningLayoutRoom.h"
#include "MiningRoutePlanner.h"
#include "EngineUtils.h"
#if WITH_EDITOR
#include "Editor.h"
#include "ScopedTransaction.h"
#endif

AMiningTunnelRoute::AMiningTunnelRoute()
{
	PrimaryActorTick.bCanEverTick = false;
	bIsEditorOnlyActor = true;
	Path = CreateDefaultSubobject<USplineComponent>(TEXT("RouteControlPoints"));
	SetRootComponent(Path);
	Path->SetClosedLoop(false);
	Path->SetDrawDebug(true);
	Path->SetUnselectedSplineSegmentColor(FLinearColor(0.1f, 0.9f, 0.5f));
	StatusLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RouteStatusLabel"));
	StatusLabel->SetupAttachment(Path);
	StatusLabel->SetHorizontalAlignment(EHTA_Center);
	StatusLabel->SetWorldSize(80);
	StatusLabel->SetHiddenInGame(true);
	StatusLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AMiningTunnelRoute::LockRouteForEditing()
{
	if (bDirectConnection) return;
	Modify();
	bAutomatic = false;
}

void AMiningTunnelRoute::RestoreAutomaticRoute()
{
	Modify();
	bAutomatic = true;
}

TArray<AMiningTunnelShapeSection*> AMiningTunnelRoute::GetShapeSections() const
{
	TArray<AMiningTunnelShapeSection*> Sections;
	if (!GetWorld()) return Sections;
	for (TActorIterator<AMiningTunnelShapeSection> Section(GetWorld()); Section; ++Section)
	{
		if (Section->GetLevel() == GetLevel() && Section->Tunnel == this) Sections.Add(*Section);
	}
	Sections.Sort([](const AMiningTunnelShapeSection& First, const AMiningTunnelShapeSection& Second) { return First.GetPathName() < Second.GetPathName(); });
	return Sections;
}

void AMiningTunnelRoute::AddShapeSection()
{
	CreateShapeSection();
}

AMiningTunnelShapeSection* AMiningTunnelRoute::CreateShapeSection()
{
#if WITH_EDITOR
	AMiningTunnelGenerator* Generator = Cast<AMiningTunnelGenerator>(GetOwner());
	if (!Generator || !Generator->bDirectRoomFaces || !IsValid(FromRoom) || !IsValid(ToRoom) || GetWorld()->IsGameWorld()) return nullptr;
	MiningRoutes::FPlan Plan;
	FString Error;
	if (!MiningRoutes::Direct(FromRoom->GetRoomBounds(), ToRoom->GetRoomBounds(), static_cast<int32>(StartWall) - 1, static_cast<int32>(EndWall) - 1, Plan, Error)) { RouteStatus = Error; return nullptr; }
	TArray<MiningRoutes::FShapeFrame> Frames;
	for (const AMiningTunnelShapeSection* Section : GetShapeSections())
	{
		if (!Section->bEnabled) continue;
		const FVector2D Size = Section->GetWorldSize(Plan.StartWall);
		Frames.Add({Section->GetActorLocation(), Size.X, Size.Y});
	}
	if (Frames.Num() >= 24 || !MiningRoutes::Shape(Plan, Frames, Error)) { RouteStatus = TEXT("Fix existing sections before adding another: ") + Error; return nullptr; }
	const TArray<FVector>* Widest = nullptr;
	double LargestGap = 199;
	for (const TArray<FVector>& Corners : Plan.FaceSections)
	{
		const double Gap = (Corners[4] - Corners[0]).Dot(MiningRoutes::WallDirection(Plan.StartWall));
		if (Gap > LargestGap) { LargestGap = Gap; Widest = &Corners; }
	}
	if (!Widest) { RouteStatus = TEXT("No gap large enough; shape sections need 100 cm spacing"); return nullptr; }
	const FScopedTransaction Transaction(NSLOCTEXT("Mining", "AddTunnelShape", "Add tunnel shape section"));
	FActorSpawnParameters Spawn;
	Spawn.OverrideLevel = GetLevel();
	Spawn.ObjectFlags |= RF_Transactional;
	Spawn.Owner = this;
	const FVector Center = ((*Widest)[0] + (*Widest)[2] + (*Widest)[4] + (*Widest)[6]) * 0.25;
	AMiningTunnelShapeSection* Section = GetWorld()->SpawnActor<AMiningTunnelShapeSection>(Center, FRotator::ZeroRotator, Spawn);
	if (!Section) return nullptr;
	Section->Tunnel = this;
	Section->Width = (FVector::Distance((*Widest)[0], (*Widest)[1]) + FVector::Distance((*Widest)[4], (*Widest)[5])) * 0.5;
	Section->Height = (FVector::Distance((*Widest)[0], (*Widest)[3]) + FVector::Distance((*Widest)[4], (*Widest)[7])) * 0.5;
	ResolvedStartWall = Plan.StartWall;
	Section->SetActorLabel(FString::Printf(TEXT("SHAPE_%02d_TO_%02d"), FromRoom->ConnectionOrder, ToRoom->ConnectionOrder));
	Section->SetFolderPath(FName(*(TEXT("AutoMine/") + Generator->LayoutId.ToString() + TEXT("/TunnelShapes_EDIT"))));
	Section->RefreshGuides();
	Generator->RequestRebuild();
	if (GEditor) { GEditor->SelectNone(false, true); GEditor->SelectActor(Section, true, true, true); }
	Section->MarkPackageDirty();
	return Section;
#else
	return nullptr;
#endif
}

void AMiningTunnelRoute::RestoreStraightTunnel()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld()) return;
	const FScopedTransaction Transaction(NSLOCTEXT("Mining", "RestoreStraightTunnel", "Restore straight tunnel"));
	for (AMiningTunnelShapeSection* Section : GetShapeSections())
	{
		Section->Modify();
		Section->bEnabled = false;
		Section->RefreshGuides();
	}
	if (AMiningTunnelGenerator* Generator = Cast<AMiningTunnelGenerator>(GetOwner())) Generator->RequestRebuild();
#endif
}

TArray<FVector> AMiningTunnelRoute::GetPoints() const
{
	TArray<FVector> Points;
	for (int32 Index = 0; Index < Path->GetNumberOfSplinePoints(); ++Index)
	{
		Points.Add(Path->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World));
	}
	return Points;
}

void AMiningTunnelRoute::SetPoints(const TArray<FVector>& Points)
{
	Modify();
	Path->Modify();
	Path->SetSplinePoints(Points, ESplineCoordinateSpace::World, false);
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		Path->SetSplinePointType(Index, ESplinePointType::Linear, false);
	}
	Path->UpdateSpline();
}
