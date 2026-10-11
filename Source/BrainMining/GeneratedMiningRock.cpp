#include "GeneratedMiningRock.h"
#include "Components/DynamicMeshComponent.h"
#include "FogChamberComponent.h"
#include "UDynamicMesh.h"
#include "MiningChunkGeometry.h"
#include "Generators/SphereGenerator.h"
#include "PhysicsEngine/BodySetup.h"
#include "Misc/ScopeExit.h"
#include "Async/Async.h"
#include "Materials/MaterialInstanceDynamic.h"

using namespace UE::Geometry;

struct FMiningSimplificationResult
{
	FDynamicMesh3 Mesh;
	uint64 Generation = 0;
	int32 ChunkIndex = 0;
	int32 Cuts = 0;
	int32 Removed = 0;
	double Milliseconds = 0;
};

AGeneratedMiningRock::AGeneratedMiningRock()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	InitialMesh = CreateDefaultSubobject<UDynamicMesh>(TEXT("InitialGeneratedMesh"));
	FogChamber->bEnabled = false;
	bUseProceduralCutFallback = true;
}

void AGeneratedMiningRock::SetGeneratedMesh(FDynamicMesh3&& Mesh)
{
	Modify();
	ClearChunks();
	InitialMesh->Modify();
	InitialMesh->SetMesh(MoveTemp(Mesh));
	bHasGeneratedMesh = true;
	ResetRock();
}

void AGeneratedMiningRock::ResetRock()
{
	GuideMaterial = nullptr;
	UMaterialInterface* SurfaceMaterial = GetMiningMaterial();
	++MeshGeneration;
	CompletedSimplifications = DiscardedSimplifications = 0;
	SetActorHiddenInGame(false);
	if (bChunked)
	{
		RockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		for (FMiningRockChunk& Chunk : Chunks)
		{
			if (!Chunk.Mesh || !Chunk.InitialMesh) continue;
			FDynamicMesh3 Geometry;
			Chunk.InitialMesh->ProcessMesh([&Geometry](const FDynamicMesh3& Source) { Geometry = Source; });
			Chunk.Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Chunk.Mesh->SetMesh(MoveTemp(Geometry));
			Chunk.Mesh->GetBodySetup()->SetFlags(RF_Transient);
			Chunk.Mesh->UpdateCollision(false);
			Chunk.Mesh->SetMaterial(0, SurfaceMaterial);
			Chunk.Cuts = 0;
		}
		UpdateChunkStats();
		SuccessfulCuts = 0;
		LastTouchedChunks = 0;
		SimplifiedTriangles = 0;
		LastBooleanMilliseconds = LastCollisionMilliseconds = LastTotalMilliseconds = LastSimplificationMilliseconds = 0;
		LastStatus = TEXT("Chunked tunnel restored");
		return;
	}
	RockMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	if (!bHasGeneratedMesh)
	{
		Super::ResetRock();
		return;
	}
	FDynamicMesh3 Mesh;
	InitialMesh->ProcessMesh([&Mesh](const FDynamicMesh3& Source) { Mesh = Source; });
	FogChamber->bEnabled = false;
	CommitMesh(MoveTemp(Mesh));
	if (RockMaterial)
	{
		RockMesh->SetMaterial(0, SurfaceMaterial);
	}
	SuccessfulCuts = 0;
	LastBooleanMilliseconds = 0.0f;
	LastTotalMilliseconds = 0.0f;
	LastStatus = TEXT("Generated tunnel restored");
}

bool AGeneratedMiningRock::MineSphere(FVector WorldCenter, float WorldRadius)
{
	if (SegmentIndex != INDEX_NONE && SegmentState != EMiningSegmentState::Active)
	{
		LastStatus = SegmentState == EMiningSegmentState::Waiting
			? TEXT("Enter the next room fully to activate this segment")
			: TEXT("Completed segment is frozen - continue forward");
		return false;
	}
	const bool bResult = bChunked ? MineChunks(WorldCenter, WorldRadius) : Super::MineSphere(WorldCenter, WorldRadius);
	if (!bResult && LastStatus.StartsWith(TEXT("Triangle budget reached")))
	{
		LastStatus = TEXT("This segment reached its mesh budget; other segments are independent");
	}
	return bResult;
}

void AGeneratedMiningRock::SetSegmentState(EMiningSegmentState State)
{
	SegmentState = State;
}

void AGeneratedMiningRock::RecycleGeometry()
{
	if (SegmentState != EMiningSegmentState::Frozen) return;
	++MeshGeneration;
	SetSegmentState(EMiningSegmentState::Recycled);
	RockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CommitMesh(FDynamicMesh3());
	for (FMiningRockChunk& Chunk : Chunks)
	{
		Chunk.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Chunk.Mesh->SetMesh(FDynamicMesh3());
		Chunk.Mesh->UpdateCollision(false);
	}
	if (bChunked) UpdateChunkStats();
	SetActorHiddenInGame(true);
	LastStatus = TEXT("Completed excavation recycled behind bedrock seal");
}

void AGeneratedMiningRock::ClearChunks()
{
	EditorGrid.Reset();
	++MeshGeneration;
	for (FMiningRockChunk& Chunk : Chunks)
	{
		if (Chunk.Mesh) Chunk.Mesh->DestroyComponent();
	}
	Chunks.Reset();
	bChunked = false;
}

UMaterialInterface* AGeneratedMiningRock::GetMiningMaterial()
{
	if (GuideMaterialVolumes.IsEmpty() || !RockMaterial) return RockMaterial;
	if (GuideMaterial) return GuideMaterial;
	GuideMaterial = UMaterialInstanceDynamic::Create(RockMaterial, this);
	GuideMaterial->SetScalarParameterValue(TEXT("GuideCount"), FMath::Min(GuideMaterialVolumes.Num(), 16));
	for (int32 Index = 0; Index < FMath::Min(GuideMaterialVolumes.Num(), 16); ++Index)
	{
		const FMiningGuideVolume& Volume = GuideMaterialVolumes[Index];
		GuideMaterial->SetVectorParameterValue(FName(*FString::Printf(TEXT("GuideStart%d"), Index)), FLinearColor(Volume.Start.X, Volume.Start.Y, Volume.Start.Z, Volume.Radius));
		GuideMaterial->SetVectorParameterValue(FName(*FString::Printf(TEXT("GuideEnd%d"), Index)), FLinearColor(Volume.End.X, Volume.End.Y, Volume.End.Z, Volume.Radius));
	}
	return GuideMaterial;
}

void AGeneratedMiningRock::CopyEditorPreviewMesh(FDynamicMesh3& Mesh) const
{
	if (bChunked && InitialMesh && InitialMesh->GetTriangleCount() > 0)
		InitialMesh->ProcessMesh([&Mesh](const FDynamicMesh3& Source) { Mesh = Source; });
	else CopyMiningMesh(Mesh);
}

bool AGeneratedMiningRock::SetChunkedMesh(const FDynamicMesh3& Mesh, double Length, bool bSpatial, bool bAllowLargeSpatialGrid)
{
	if (bSpatial && GetWorld() && !GetWorld()->IsGameWorld())
	{
		const FAxisAlignedBox3d MeshBounds = Mesh.GetBounds();
		FIntVector Counts;
		const bool bIncrementalGrid = !bAllowLargeSpatialGrid || Mesh.TriangleCount() == 0 ||
			(MiningChunks::SpatialChunkCounts(FVector(MeshBounds.Max - MeshBounds.Min), Length, Counts) &&
			 static_cast<int64>(Counts.X) * Counts.Y * Counts.Z <= 128);
		if (bIncrementalGrid)
		{
			auto Grid = MakeShared<MiningChunks::FSpatialGrid, ESPMode::ThreadSafe>();
			if (!MiningChunks::SplitSpatialIncremental(Mesh, Length, *Grid)) return false;
			ApplySpatialGrid(Mesh, Grid); return true;
		}
	}
	TArray<FDynamicMesh3> Pieces;
	TArray<FBox> Bounds;
	const bool bSplit = bSpatial
		? (bAllowLargeSpatialGrid ? MiningChunks::SplitExternalSpatial(Mesh, Length, Pieces, Bounds) : MiningChunks::SplitSpatial(Mesh, Length, Pieces, Bounds))
		: MiningChunks::Split(Mesh, Length, Pieces, Bounds);
	if (!bSplit) return false;
	Modify();
	ClearChunks();
	InitialMesh->Modify();
	InitialMesh->SetMesh(bSpatial ? FDynamicMesh3(Mesh) : FDynamicMesh3());
	bHasGeneratedMesh = true;
	bChunked = true;
	RockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CommitMesh(FDynamicMesh3());
	for (int32 Index = 0; Index < Pieces.Num(); ++Index)
	{
		FMiningRockChunk& Chunk = Chunks.AddDefaulted_GetRef();
		Chunk.Bounds = Bounds[Index];
		Chunk.InitialMesh = NewObject<UDynamicMesh>(this, NAME_None, RF_Transactional);
		Chunk.InitialMesh->SetMesh(MoveTemp(Pieces[Index]));
		Chunk.Mesh = NewObject<UDynamicMeshComponent>(this, NAME_None, RF_Transactional);
		AddInstanceComponent(Chunk.Mesh);
		Chunk.Mesh->SetupAttachment(RootComponent);
		Chunk.Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		Chunk.Mesh->SetGenerateOverlapEvents(false);
		Chunk.Mesh->SetCanEverAffectNavigation(false);
		Chunk.Mesh->SetComplexAsSimpleCollisionEnabled(true, false);
		Chunk.Mesh->SetDeferredCollisionUpdatesEnabled(true, false);
		Chunk.Mesh->bUseAsyncCooking = false;
		Chunk.Mesh->RegisterComponent();
	}
	ResetRock();
	return true;
}

void AGeneratedMiningRock::ApplySpatialGrid(const FDynamicMesh3& Mesh, TSharedPtr<const MiningChunks::FSpatialGrid, ESPMode::ThreadSafe> Grid)
{
	check(IsInGameThread() && Grid && GetWorld() && !GetWorld()->IsGameWorld());
	Modify(); ++MeshGeneration;
	InitialMesh->Modify(); InitialMesh->SetMesh(FDynamicMesh3(Mesh));
	bHasGeneratedMesh = true; bChunked = true;
	RockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (RockMesh->GetDynamicMesh()->GetTriangleCount()) CommitMesh(FDynamicMesh3());
	GuideMaterial = nullptr;
	UMaterialInterface* Material = GetMiningMaterial();
	TArray<FMiningRockChunk> Updated;
	TSet<UDynamicMeshComponent*> Retained;
	LastRebuiltEditorChunks = LastReusedEditorChunks = 0;
	FDynamicMesh3::FSameAsOptions Equality;
	Equality.bCheckAttributes = true; Equality.EpsilonDouble = 0; Equality.EpsilonFloat = 0;
	for (int32 CellIndex = 0; CellIndex < Grid->Cells.Num(); ++CellIndex)
	{
		const MiningChunks::FSpatialCell& Cell = Grid->Cells[CellIndex];
		if (!Cell.Mesh || !Cell.Mesh->TriangleCount()) continue;
		FMiningRockChunk* Existing = Chunks.FindByPredicate([&](const FMiningRockChunk& Chunk)
		{
			if (!Chunk.Mesh || !Chunk.InitialMesh || Chunk.Cuts || Retained.Contains(Chunk.Mesh)) return false;
			if (EditorGrid && Chunk.SpatialCell == CellIndex && EditorGrid->Cells.IsValidIndex(CellIndex) && EditorGrid->Cells[CellIndex].Mesh == Cell.Mesh) return true;
			if (Chunk.Bounds != Cell.Bounds) return false;
			bool bEqual = false;
			Chunk.InitialMesh->ProcessMesh([&](const FDynamicMesh3& Old) { bEqual = Old.IsSameAs(*Cell.Mesh, Equality); });
			return bEqual;
		});
		FMiningRockChunk& Chunk = Updated.AddDefaulted_GetRef();
		if (Existing) { Chunk = *Existing; Retained.Add(Chunk.Mesh); ++LastReusedEditorChunks; }
		else
		{
			Chunk.InitialMesh = NewObject<UDynamicMesh>(this, NAME_None, RF_Transactional);
			Chunk.InitialMesh->SetMesh(FDynamicMesh3(*Cell.Mesh));
			Chunk.Mesh = NewObject<UDynamicMeshComponent>(this, NAME_None, RF_Transactional);
			AddInstanceComponent(Chunk.Mesh); Chunk.Mesh->SetupAttachment(RootComponent);
			Chunk.Mesh->SetVisibility(false);
			Chunk.Mesh->SetCollisionProfileName(TEXT("BlockAll"));
			Chunk.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Chunk.Mesh->SetGenerateOverlapEvents(false); Chunk.Mesh->SetCanEverAffectNavigation(false);
			Chunk.Mesh->SetComplexAsSimpleCollisionEnabled(true, false);
			Chunk.Mesh->SetDeferredCollisionUpdatesEnabled(true, false); Chunk.Mesh->bUseAsyncCooking = false;
			Chunk.Mesh->RegisterComponent(); Chunk.Mesh->SetMesh(FDynamicMesh3(*Cell.Mesh));
			Chunk.Mesh->GetBodySetup()->SetFlags(RF_Transient); Chunk.Mesh->UpdateCollision(false);
			++LastRebuiltEditorChunks;
		}
		Chunk.SpatialCell = CellIndex; Chunk.Bounds = Cell.Bounds; Chunk.Cuts = 0;
		Chunk.Mesh->SetMaterial(0, Material);
	}
	for (FMiningRockChunk& Chunk : Updated) { Chunk.Mesh->SetVisibility(true); Chunk.Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); }
	for (FMiningRockChunk& Chunk : Chunks) if (Chunk.Mesh && !Retained.Contains(Chunk.Mesh)) Chunk.Mesh->DestroyComponent();
	Chunks = MoveTemp(Updated); EditorGrid = MoveTemp(Grid);
	SuccessfulCuts = 0; SimplifiedTriangles = 0; LastTouchedChunks = 0;
	UpdateChunkStats(); LastStatus = TEXT("Spatial terrain and collision applied");
}

void AGeneratedMiningRock::UpdateChunkStats()
{
	TriangleCount = 0;
	LargestChunkTriangles = 0;
	for (FMiningRockChunk& Chunk : Chunks)
	{
		Chunk.Triangles = Chunk.Mesh ? Chunk.Mesh->GetDynamicMesh()->GetTriangleCount() : 0;
		TriangleCount += Chunk.Triangles;
		LargestChunkTriangles = FMath::Max(LargestChunkTriangles, Chunk.Triangles);
	}
}

void AGeneratedMiningRock::CopyMiningMesh(FDynamicMesh3& Mesh) const
{
	Mesh.Clear();
	if (!bChunked)
	{
		RockMesh->GetDynamicMesh()->ProcessMesh([&Mesh](const FDynamicMesh3& Source) { Mesh = Source; });
		return;
	}
	for (const FMiningRockChunk& Chunk : Chunks)
	{
		if (!Chunk.Mesh) continue;
		Chunk.Mesh->GetDynamicMesh()->ProcessMesh([&Mesh](const FDynamicMesh3& Source)
		{
			TMap<int32, int32> Vertices;
			for (int32 Vertex : Source.VertexIndicesItr()) Vertices.Add(Vertex, Mesh.AppendVertex(Source.GetVertex(Vertex)));
			for (int32 Triangle : Source.TriangleIndicesItr())
			{
				const FIndex3i Indices = Source.GetTriangle(Triangle);
				Mesh.AppendTriangle(Vertices[Indices.A], Vertices[Indices.B], Vertices[Indices.C]);
			}
		});
	}
}

UDynamicMesh* AGeneratedMiningRock::GetCombinedMesh() const
{
	UDynamicMesh* Combined = NewObject<UDynamicMesh>();
	FDynamicMesh3 Geometry;
	CopyMiningMesh(Geometry);
	Combined->SetMesh(MoveTemp(Geometry));
	return Combined;
}

bool AGeneratedMiningRock::MineChunks(FVector WorldCenter, float WorldRadius)
{
	const double Started = FPlatformTime::Seconds();
	ON_SCOPE_EXIT { LastTotalMilliseconds = (FPlatformTime::Seconds() - Started) * 1000; };
	LastBooleanMilliseconds = LastCollisionMilliseconds = LastSimplificationMilliseconds = 0;
	LastTouchedChunks = 0;
	const FVector Scale = GetActorScale3D();
	if (WorldCenter.ContainsNaN() || !FMath::IsFinite(WorldRadius) || WorldRadius <= 0 ||
		Scale.X <= UE_SMALL_NUMBER || !FMath::IsNearlyEqual(Scale.X, Scale.Y) || !FMath::IsNearlyEqual(Scale.X, Scale.Z))
	{
		LastStatus = TEXT("Use positive uniform scale and a valid radius");
		return false;
	}
	const FVector3d Center(GetActorTransform().InverseTransformPosition(WorldCenter));
	const double Radius = WorldRadius / Scale.X;
	if (ChunkCutter.TriangleCount() == 0)
	{
		FSphereGenerator Generator;
		Generator.Radius = 1;
		Generator.NumPhi = 8;
		Generator.NumTheta = 16;
		Generator.Generate();
		ChunkCutter = FDynamicMesh3(&Generator);
		ChunkCutter.DiscardAttributes();
	}
	FDynamicMesh3 Cutter = ChunkCutter;
	for (int32 Vertex : Cutter.VertexIndicesItr()) Cutter.SetVertex(Vertex, Cutter.GetVertex(Vertex) * Radius + Center);
	TArray<int32> Affected;
	TArray<FDynamicMesh3> Prepared;
	int32 Removed = 0;
	const int32 Budget = FMath::Clamp(MaxTriangles, 1000, MiningChunks::MaxChunkTriangles);
	for (int32 Index = 0; Index < Chunks.Num(); ++Index)
	{
		FMiningRockChunk& Chunk = Chunks[Index];
		if (Chunk.Triangles == 0 || Chunk.Bounds.ComputeSquaredDistanceToPoint(FVector(Center)) > Radius * Radius) continue;
		FDynamicMesh3 Source;
		Chunk.Mesh->GetDynamicMesh()->ProcessMesh([&Source](const FDynamicMesh3& Mesh) { Source = Mesh; });
		FDynamicMesh3 Result;
		const double BooleanStart = FPlatformTime::Seconds();
		const bool bValid = MiningChunks::Subtract(Source, Cutter, Result);
		LastBooleanMilliseconds += (FPlatformTime::Seconds() - BooleanStart) * 1000;
		if (!bValid)
		{
			LastStatus = TEXT("Boundary cut rejected: ALL chunks preserved");
			return false;
		}
		if (Source.IsSameAs(Result, FDynamicMesh3::FSameAsOptions())) continue;
		if (bSimplifyChunks && !GetWorld()->IsGameWorld() && ((Chunk.Cuts + 1) % 12 == 0 || Result.TriangleCount() > Budget * 0.8))
		{
			const double SimplifyStart = FPlatformTime::Seconds();
			Chunk.InitialMesh->ProcessMesh([&](const FDynamicMesh3& Initial)
			{
				Removed += MiningChunks::Simplify(Result, Initial, Chunk.Bounds, FMath::Clamp<double>(SimplificationTolerance / Scale.X, 0.05, 1));
			});
			LastSimplificationMilliseconds += (FPlatformTime::Seconds() - SimplifyStart) * 1000;
		}
		if (Result.TriangleCount() > Budget)
		{
			if (GetWorld()->IsGameWorld()) ScheduleSimplification(Index);
			LastStatus = TEXT("Local chunk budget reached: whole cut cancelled; dig elsewhere");
			return false;
		}
		MiningChunks::Normals(Result);
		Affected.Add(Index);
		Prepared.Add(MoveTemp(Result));
	}
	if (Affected.IsEmpty())
	{
		LastStatus = TriangleCount == 0 ? TEXT("Rock depleted") : TEXT("No new rock in mining sphere");
		return false;
	}
	const double CommitStart = FPlatformTime::Seconds();
	for (int32 Index = 0; Index < Affected.Num(); ++Index)
	{
		FMiningRockChunk& Chunk = Chunks[Affected[Index]];
		Chunk.Mesh->SetMesh(MoveTemp(Prepared[Index]));
		Chunk.Mesh->GetBodySetup()->SetFlags(RF_Transient);
		Chunk.Mesh->UpdateCollision(false);
		++Chunk.Cuts;
	}
	LastCollisionMilliseconds = (FPlatformTime::Seconds() - CommitStart) * 1000;
	UpdateChunkStats();
	++SuccessfulCuts;
	LastTouchedChunks = Affected.Num();
	SimplifiedTriangles += Removed;
	if (GetWorld()->IsGameWorld())
	{
		for (int32 Index : Affected)
		{
			if (Chunks[Index].Cuts % 12 == 0 || Chunks[Index].Triangles > Budget * 0.8) ScheduleSimplification(Index);
		}
	}
	LastStatus = FString::Printf(TEXT("Cut committed across %d chunk(s)"), LastTouchedChunks);
	return true;
}

void AGeneratedMiningRock::ScheduleSimplification(int32 Index)
{
	if (!bSimplifyChunks || bSimplificationPending || !Chunks.IsValidIndex(Index) || Chunks[Index].Triangles < 128) return;
	FMiningRockChunk& Chunk = Chunks[Index];
	TSharedPtr<FMiningSimplificationResult, ESPMode::ThreadSafe> Result = MakeShared<FMiningSimplificationResult, ESPMode::ThreadSafe>();
	Result->Generation = MeshGeneration;
	Result->ChunkIndex = Index;
	Result->Cuts = Chunk.Cuts;
	Chunk.Mesh->GetDynamicMesh()->ProcessMesh([&Result](const FDynamicMesh3& Mesh) { Result->Mesh = Mesh; });
	FDynamicMesh3 Initial;
	Chunk.InitialMesh->ProcessMesh([&Initial](const FDynamicMesh3& Mesh) { Initial = Mesh; });
	const FBox Bounds = Chunk.Bounds;
	const double Tolerance = FMath::Clamp<double>(SimplificationTolerance / GetActorScale3D().X, 0.05, 1);
	bSimplificationPending = true;
	SimplificationTask = Async(EAsyncExecution::ThreadPool, [Result, Initial = MoveTemp(Initial), Bounds, Tolerance]()
	{
		const double Started = FPlatformTime::Seconds();
		Result->Removed = MiningChunks::Simplify(Result->Mesh, Initial, Bounds, Tolerance);
		if (Result->Removed > 0) MiningChunks::Normals(Result->Mesh);
		Result->Milliseconds = (FPlatformTime::Seconds() - Started) * 1000;
		return Result;
	});
}

void AGeneratedMiningRock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bSimplificationPending || !SimplificationTask.IsReady()) return;
	const TSharedPtr<FMiningSimplificationResult, ESPMode::ThreadSafe> Result = SimplificationTask.Get();
	bSimplificationPending = false;
	if (Result->Generation != MeshGeneration || !Chunks.IsValidIndex(Result->ChunkIndex) ||
		Chunks[Result->ChunkIndex].Cuts != Result->Cuts || SegmentState != EMiningSegmentState::Active || !bSimplifyChunks)
	{
		++DiscardedSimplifications;
		return;
	}
	LastSimplificationMilliseconds = Result->Milliseconds;
	if (Result->Removed <= 0 || Result->Mesh.TriangleCount() > FMath::Clamp(MaxTriangles, 1000, MiningChunks::MaxChunkTriangles)) return;
	FMiningRockChunk& Chunk = Chunks[Result->ChunkIndex];
	Chunk.Mesh->SetMesh(MoveTemp(Result->Mesh));
	Chunk.Mesh->GetBodySetup()->SetFlags(RF_Transient);
	Chunk.Mesh->UpdateCollision(false);
	SimplifiedTriangles += Result->Removed;
	++CompletedSimplifications;
	UpdateChunkStats();
}
