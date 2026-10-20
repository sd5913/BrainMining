#include "GeneratedMiningRock.h"
#include "MiningTerrainFragments.h"
#include "MiningPickaxeGeometry.h"
#include "MiningTerrainSource.h"
#include "MiningBlastGeometry.h"
#include "Components/DynamicMeshComponent.h"
#include "FogChamberComponent.h"
#include "UDynamicMesh.h"
#include "MiningChunkGeometry.h"
#include "Generators/SphereGenerator.h"
#include "PhysicsEngine/BodySetup.h"
#include "Misc/ScopeExit.h"
#include "Async/Async.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/ThreadSafeCounter.h"
#include "TimerManager.h"

namespace
{
TAutoConsoleVariable<float> CVarMiningInteractiveBudgetMilliseconds(
	TEXT("bm.Mining.InteractiveBudgetMs"), 3.0f,
	TEXT("Maximum synchronous boolean work per chunked cut; remaining work completes in the background."));

TAutoConsoleVariable<int32> CVarMiningLazyRuntimeChunks(
	TEXT("bm.Mining.LazyRuntimeChunks"), 1,
	TEXT("Initialize runtime terrain geometry and collision near players. Set before starting a world."));

TAutoConsoleVariable<int32> CVarMiningMaxBackgroundTasks(
	TEXT("bm.Mining.MaxBackgroundTasks"), 2,
	TEXT("Global limit for background terrain jobs and results waiting to be committed (1-4)."));

FThreadSafeCounter MiningBackgroundTasks;
double MiningCommitWindowStart = 0;
double MiningCommitWindowSeconds = 0;
uint64 TerrainPreparationFrame = MAX_uint64;
double TerrainPreparationSeconds = 0;

struct FMiningTaskPermit
{
	FMiningTaskPermit() { MiningBackgroundTasks.Increment(); }
	~FMiningTaskPermit() { MiningBackgroundTasks.Decrement(); }
};

TSharedPtr<FMiningTaskPermit, ESPMode::ThreadSafe> AcquireMiningTaskPermit()
{
	check(IsInGameThread());
	if (MiningBackgroundTasks.GetValue() >= FMath::Clamp(CVarMiningMaxBackgroundTasks.GetValueOnGameThread(), 1, 4)) return nullptr;
	return MakeShared<FMiningTaskPermit, ESPMode::ThreadSafe>();
}

// FullJourney contains over 15,000 terrain components. Keep their solid meshes
// and collision, but allocate render proxies only around the player's view.
void UpdateTerrainVisibility(AGeneratedMiningRock* Rock)
{
	if (!Rock->bChunked || !Rock->GetWorld() || !Rock->GetWorld()->IsGameWorld()) return;
	const auto* PC=Rock->GetWorld()->GetFirstPlayerController();
	const APawn* Pawn=PC?PC->GetPawn().Get():nullptr;
	if (Pawn)
 {
  // Collision immediately around the pawn is mandatory. Farther render/collision
  // chunks share a budget instead of all rebuilding on the area-entry frame.
  Rock->PrepareRuntimeGeometry(Pawn->GetActorLocation(), 600.f);
  Rock->PrepareRuntimeGeometry(Pawn->GetActorLocation(), 5000.f, true);
 }
	const FVector Local=Pawn?Rock->GetActorTransform().InverseTransformPosition(Pawn->GetPawnViewLocation()):FVector::ZeroVector;
	const double Radius=3200.0 / FMath::Max(.001,Rock->GetActorScale3D().GetMin());
	for(auto& Chunk:Rock->Chunks)if(Chunk.Mesh)
		Chunk.Mesh->SetVisibility(Pawn && Chunk.Bounds.ComputeSquaredDistanceToPoint(Local)<Radius*Radius);
}
}

using namespace UE::Geometry;

struct FMiningSimplificationResult
{
	TSharedPtr<FMiningTaskPermit, ESPMode::ThreadSafe> Permit;
	FDynamicMesh3 Mesh;
	uint64 Generation = 0;
	int32 ChunkIndex = 0;
	int32 Cuts = 0;
	int32 Removed = 0;
	double Milliseconds = 0;
};

struct FMiningDeferredRequest
{
	FDynamicMesh3 Cutter;
	FVector Center;
	float Radius=0;
	bool bPreserveEdges=false;
};
struct FMiningDeferredCut
{
	TSharedPtr<FMiningTaskPermit, ESPMode::ThreadSafe> Permit;
	uint64 Generation=0;
	TArray<int32> Indices, Cuts;
	TArray<FDynamicMesh3> Sources, Results;
	FBox Dirty;
	bool bValid=false;
	FString Failure;
	double Milliseconds=0;
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
	MakeSourceEditable();
	ClearChunks();
	InitialMesh->Modify();
	InitialMesh->SetMesh(MoveTemp(Mesh));
	bHasGeneratedMesh = true;
	ResetRock();
}

void AGeneratedMiningRock::InitializeForPlay()
{
	bRuntimeInitializationEnabled = CVarMiningLazyRuntimeChunks.GetValueOnGameThread() != 0;
	ResetRock();
	if (bChunked && InitialMesh)
	{
		if (TerrainSource) InitialMesh = nullptr;
		else InitialMesh->SetMesh(FDynamicMesh3());
	}
	UE_LOG(LogTemp, Display, TEXT("Mining startup %s: %d chunks, %d deferred runtime copies"),
		*GetName(), Chunks.Num(), DeferredRuntimeChunks);
}

void AGeneratedMiningRock::PostLoad()
{
	Super::PostLoad();
	for (FMiningRockChunk& Chunk : Chunks)
		if (Chunk.Mesh) Chunk.Mesh->SetFlags(RF_DuplicateTransient);
}

void AGeneratedMiningRock::EnsureChunkComponent(FMiningRockChunk& Chunk)
{
	if (Chunk.Mesh) return;
	Chunk.Mesh = NewObject<UDynamicMeshComponent>(this, NAME_None, RF_Transactional | RF_DuplicateTransient);
	AddInstanceComponent(Chunk.Mesh);
	Chunk.Mesh->SetupAttachment(RootComponent);
	Chunk.Mesh->SetEnableRaytracing(false);
	Chunk.Mesh->SetVisibleInRayTracing(false);
	Chunk.Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Chunk.Mesh->SetGenerateOverlapEvents(false);
	Chunk.Mesh->SetCanEverAffectNavigation(false);
	Chunk.Mesh->SetComplexAsSimpleCollisionEnabled(true, false);
	Chunk.Mesh->SetDeferredCollisionUpdatesEnabled(true, false);
	Chunk.Mesh->bUseAsyncCooking = false;
	Chunk.Mesh->SetMaterial(0, GetMiningMaterial());
}

void AGeneratedMiningRock::MakeSourceEditable()
{
	if (TerrainSource || !InitialMesh)
	{
		InitialMesh = NewObject<UDynamicMesh>(this, NAME_None, RF_Transactional);
		TerrainSource = nullptr;
	}
}

#if WITH_EDITOR
bool AGeneratedMiningRock::ExtractTerrainSource(UMiningTerrainSource* Destination)
{
	if (!Destination || TerrainSource || !bChunked || Chunks.IsEmpty() || !InitialMesh) return false;
	for (const FMiningRockChunk& Chunk : Chunks)
		if (!Chunk.InitialMesh || Chunk.InitialMesh->GetOuter() != this) return false;
	Destination->Preview = DuplicateObject<UDynamicMesh>(InitialMesh, Destination, TEXT("Preview"));
	Destination->Preview->ClearFlags(RF_DefaultSubObject | RF_ArchetypeObject);
	Destination->Preview->SetFlags(RF_Public);
	Destination->Meshes.Reserve(Chunks.Num());
	for (FMiningRockChunk& Chunk : Chunks)
	{
		if (!Chunk.InitialMesh->Rename(nullptr, Destination, REN_DontCreateRedirectors | REN_NonTransactional)) return false;
		Chunk.InitialMesh->SetFlags(RF_Public);
		Destination->Meshes.Add(Chunk.InitialMesh);
		if (Chunk.Mesh) Chunk.Mesh->SetFlags(RF_DuplicateTransient);
	}
	InitialMesh->SetMesh(FDynamicMesh3());
	InitialMesh = Destination->Preview;
	TerrainSource = Destination;
	MarkPackageDirty();
	return true;
}
#endif

void AGeneratedMiningRock::PreRegisterAllComponents()
{
	Super::PreRegisterAllComponents();
	if (!bChunked || !GetWorld() || HasActorBegunPlay()) return;
	if (!GetWorld()->IsGameWorld() || CVarMiningLazyRuntimeChunks.GetValueOnGameThread() == 0)
	{
		for (FMiningRockChunk& Chunk : Chunks)
		{
			if (Chunk.Mesh || !Chunk.InitialMesh) continue;
			EnsureChunkComponent(Chunk);
			Chunk.Mesh->SetMesh(FDynamicMesh3(Chunk.InitialMesh->GetMeshRef()));
			Chunk.Mesh->UpdateCollision(false);
		}
		return;
	}
	TRACE_CPUPROFILER_EVENT_SCOPE(Mining_DeferChunkRegistration);
	bRuntimeInitializationEnabled = true;
	DeferredRuntimeChunks = 0;
	RuntimeChunkBounds = FBox(ForceInit);
	for (FMiningRockChunk& Chunk : Chunks)
	{
		if (!Chunk.InitialMesh) continue;
		if (Chunk.Mesh)
		{
			Chunk.Mesh->bAutoRegister = false;
			Chunk.Mesh->SetDeferredCollisionUpdatesEnabled(true, false);
			Chunk.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Chunk.Mesh->SetVisibility(false);
			Chunk.Mesh->SetMesh(FDynamicMesh3());
		}
		Chunk.bRuntimeDeferred = true;
		RuntimeChunkBounds += Chunk.Bounds;
		++DeferredRuntimeChunks;
	}
	if (InitialMesh && !TerrainSource) InitialMesh->SetMesh(FDynamicMesh3());
}

void AGeneratedMiningRock::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(BackgroundWorkTimer);
	++MeshGeneration;
	CutQueue.Reset();
	CutTask = {};
	SimplificationTask = {};
	bCutPending = bSimplificationPending = false;
	Super::EndPlay(EndPlayReason);
}

UDynamicMesh* AGeneratedMiningRock::GetChunkQueryMesh(const FMiningRockChunk& Chunk) const
{
	if (SegmentState == EMiningSegmentState::Recycled) return Chunk.Mesh ? Chunk.Mesh->GetDynamicMesh() : nullptr;
	return Chunk.bRuntimeDeferred || !Chunk.Mesh ? Chunk.InitialMesh.Get() : Chunk.Mesh->GetDynamicMesh();
}

void AGeneratedMiningRock::PrepareRuntimeGeometry(const FVector& WorldPosition, float WorldRadius, bool bBudgeted)
{
	if (DeferredRuntimeChunks == 0 || SegmentState == EMiningSegmentState::Recycled ||
		WorldPosition.ContainsNaN() || !FMath::IsFinite(WorldRadius) || WorldRadius <= 0) return;
	const FVector LocalPosition = GetActorTransform().InverseTransformPosition(WorldPosition);
	const double Radius = WorldRadius / FMath::Max(.001, GetActorScale3D().GetAbsMin());
	if (!RuntimeChunkBounds.IsValid || RuntimeChunkBounds.ComputeSquaredDistanceToPoint(LocalPosition) > Radius * Radius) return;
	TRACE_CPUPROFILER_EVENT_SCOPE(Mining_RuntimeActivation);
 if(TerrainPreparationFrame!=GFrameCounter){TerrainPreparationFrame=GFrameCounter;TerrainPreparationSeconds=0;}
 const double Started=FPlatformTime::Seconds();
 ON_SCOPE_EXIT { TerrainPreparationSeconds+=FPlatformTime::Seconds()-Started; };
	for (FMiningRockChunk& Chunk : Chunks)
	{
		if (!Chunk.bRuntimeDeferred || !Chunk.InitialMesh ||
			Chunk.Bounds.ComputeSquaredDistanceToPoint(LocalPosition) > Radius * Radius) continue;
		if(bBudgeted && TerrainPreparationSeconds+FPlatformTime::Seconds()-Started>=.002)break;
		EnsureChunkComponent(Chunk);
		FDynamicMesh3 Geometry;
		Chunk.InitialMesh->ProcessMesh([&Geometry](const FDynamicMesh3& Source) { Geometry = Source; });
		Chunk.Mesh->SetMesh(MoveTemp(Geometry));
		Chunk.Mesh->UpdateCollision(false);
		Chunk.Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Chunk.Mesh->SetVisibility(true);
		Chunk.Mesh->bAutoRegister = true;
		if (!Chunk.Mesh->IsRegistered()) Chunk.Mesh->RegisterComponent();
		Chunk.bRuntimeDeferred = false;
		--DeferredRuntimeChunks;
	}
}

void AGeneratedMiningRock::PrepareTerrainAt(UWorld* World, const FVector& WorldPosition, float WorldRadius)
{
	if (!World || !World->IsGameWorld()) return;
	for (TActorIterator<AGeneratedMiningRock> Rock(World); Rock; ++Rock)
		Rock->PrepareRuntimeGeometry(WorldPosition, WorldRadius);
}

void AGeneratedMiningRock::PrewarmTerrainAt(UWorld* World, const FVector& WorldPosition, float WorldRadius)
{
 if (!World || !World->IsGameWorld()) return;
 for (TActorIterator<AGeneratedMiningRock> Rock(World); Rock; ++Rock)
  Rock->PrepareRuntimeGeometry(WorldPosition, WorldRadius, true);
}

int32 AGeneratedMiningRock::GetBackgroundTaskCount()
{
	return MiningBackgroundTasks.GetValue();
}

void AGeneratedMiningRock::ResetRock()
{
	if (GetWorld()) GetWorldTimerManager().ClearTimer(BackgroundWorkTimer);
	CutTask = {};
	SimplificationTask = {};
	bSimplificationPending = false;
	RockMesh->SetEnableRaytracing(false);
	RockMesh->SetVisibleInRayTracing(false);
	GuideMaterial = nullptr;
	UMaterialInterface* SurfaceMaterial = GetMiningMaterial();
	++MeshGeneration;
	CutQueue.Reset(); bCutPending=false; DeferredCutsCompleted=0;
	CompletedSimplifications = DiscardedSimplifications = 0;
	SetActorHiddenInGame(false);
	if (bChunked)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Mining_ResetChunks);
		DeferredRuntimeChunks = 0;
		RuntimeChunkBounds = FBox(ForceInit);
		RockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		for (FMiningRockChunk& Chunk : Chunks)
		{
			if (!Chunk.InitialMesh) continue;
			if (!Chunk.Mesh && bRuntimeInitializationEnabled)
			{
				RuntimeChunkBounds += Chunk.Bounds;
				Chunk.bRuntimeDeferred = true;
				Chunk.Cuts = 0;
				++DeferredRuntimeChunks;
				continue;
			}
			EnsureChunkComponent(Chunk);
			Chunk.Mesh->SetEnableRaytracing(false);
			Chunk.Mesh->SetVisibleInRayTracing(false);
			Chunk.Mesh->SetDeferredCollisionUpdatesEnabled(true, false);
			Chunk.Mesh->bUseAsyncCooking = false;
			RuntimeChunkBounds += Chunk.Bounds;
			Chunk.bRuntimeDeferred = bRuntimeInitializationEnabled;
			FDynamicMesh3 Geometry;
			if (!Chunk.bRuntimeDeferred)
				Chunk.InitialMesh->ProcessMesh([&Geometry](const FDynamicMesh3& Source) { Geometry = Source; });
			else ++DeferredRuntimeChunks;
			Chunk.Mesh->SetCollisionEnabled(Chunk.bRuntimeDeferred ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
			Chunk.Mesh->bAutoRegister = !Chunk.bRuntimeDeferred;
			if (Chunk.bRuntimeDeferred && Chunk.Mesh->IsRegistered()) Chunk.Mesh->UnregisterComponent();
			if(GetWorld() && GetWorld()->IsGameWorld())Chunk.Mesh->SetVisibility(false);
			Chunk.Mesh->SetMesh(MoveTemp(Geometry));
			if (!Chunk.bRuntimeDeferred)
			{
				Chunk.Mesh->GetBodySetup()->SetFlags(RF_Transient);
				Chunk.Mesh->UpdateCollision(false);
				if (!Chunk.Mesh->IsRegistered()) Chunk.Mesh->RegisterComponent();
			}
			Chunk.Mesh->SetMaterial(0, SurfaceMaterial);
			Chunk.Cuts = 0;
		}
		if (bRuntimeInitializationEnabled && GetWorld())
		{
			bool bFoundPlayer = false;
			for (auto Controller = GetWorld()->GetPlayerControllerIterator(); Controller; ++Controller)
				if (Controller->IsValid() && Controller->Get()->GetPawn())
				{
					PrepareRuntimeGeometry(Controller->Get()->GetPawn()->GetActorLocation(), 5000.f);
					bFoundPlayer = true;
				}
			if (!bFoundPlayer)
				for (TActorIterator<APlayerStart> Start(GetWorld()); Start; ++Start)
					PrepareRuntimeGeometry(Start->GetActorLocation(), 5000.f);
		}
		UpdateChunkStats();
		UpdateTerrainVisibility(this);
		SuccessfulCuts = 0;
		LastTouchedChunks = 0;
		SimplifiedTriangles = 0;
		LastBooleanMilliseconds = LastCollisionMilliseconds = LastTotalMilliseconds = LastSimplificationMilliseconds = 0;
		LastStatus = TEXT("Chunked tunnel restored");
		OnTerrainCommitted.Broadcast(FBox(ForceInit));
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
	OnTerrainCommitted.Broadcast(FBox(ForceInit));
}

bool AGeneratedMiningRock::MineSphere(FVector WorldCenter, float WorldRadius)
{
    if(ActorHasTag(TEXT("BM_UnmineableBoundary"))){LastStatus=TEXT("房间边界不可挖掘");return false;}
	if (ActorHasTag(TEXT("BM_BombOnly"))) { LastStatus = TEXT("坚硬岩层：只能用炸弹破坏"); return false; }
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

bool AGeneratedMiningRock::MinePickaxe(FTransform WorldFrame, FVector HalfSize, int32 Seed, float Roughness)
{
    if(ActorHasTag(TEXT("BM_UnmineableBoundary"))){LastStatus=TEXT("房间边界不可挖掘");return false;}
	if (ActorHasTag(TEXT("BM_BombOnly"))) { LastStatus = TEXT("坚硬岩层：只能用炸弹破坏"); return false; }
	if (SegmentIndex != INDEX_NONE && SegmentState != EMiningSegmentState::Active)
	{
		LastStatus = TEXT("This segment is not available for mining");
		return false;
	}
	if (!WorldFrame.IsValid() || HalfSize.ContainsNaN() || HalfSize.GetMin() <= 0) return false;
	if (!bChunked) return Super::MinePickaxe(WorldFrame, HalfSize, Seed, Roughness);
	const FDynamicMesh3 Cutter = MiningPickaxe::MakeCutter(WorldFrame, HalfSize, Seed, Roughness);
	const bool bResult = MineChunks(WorldFrame.GetLocation(), HalfSize.Size() * 1.12, &Cutter);
	return bResult;
}

void AGeneratedMiningRock::RecycleGeometry()
{
	if (SegmentState != EMiningSegmentState::Frozen) return;
	GetWorldTimerManager().ClearTimer(BackgroundWorkTimer);
	CutTask = {};
	SimplificationTask = {};
	bSimplificationPending = false;
	++MeshGeneration;
	CutQueue.Reset(); bCutPending=false;
	SetSegmentState(EMiningSegmentState::Recycled);
	RockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CommitMesh(FDynamicMesh3());
	for (FMiningRockChunk& Chunk : Chunks)
	{
		Chunk.bRuntimeDeferred = false;
		if (!Chunk.Mesh) { Chunk.Triangles = 0; continue; }
		Chunk.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Chunk.Mesh->SetMesh(FDynamicMesh3());
		Chunk.Mesh->UpdateCollision(false);
	}
	DeferredRuntimeChunks = 0;
	if (bChunked) UpdateChunkStats();
	SetActorHiddenInGame(true);
	LastStatus = TEXT("Completed excavation recycled behind bedrock seal");
	OnTerrainCommitted.Broadcast(FBox(ForceInit));
}

void AGeneratedMiningRock::ClearChunks()
{
	if (GetWorld()) GetWorldTimerManager().ClearTimer(BackgroundWorkTimer);
	CutTask = {};
	SimplificationTask = {};
	bSimplificationPending = false;
	DeferredRuntimeChunks = 0;
	RuntimeChunkBounds = FBox(ForceInit);
	EditorGrid.Reset();
	++MeshGeneration;
	CutQueue.Reset(); bCutPending=false;
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
	MakeSourceEditable();
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
		Chunk.Mesh = NewObject<UDynamicMeshComponent>(this, NAME_None, RF_Transactional | RF_DuplicateTransient);
		Chunk.Mesh->SetEnableRaytracing(false);
		Chunk.Mesh->SetVisibleInRayTracing(false);
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
	MakeSourceEditable();
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
			Chunk.Mesh = NewObject<UDynamicMeshComponent>(this, NAME_None, RF_Transactional | RF_DuplicateTransient);
			Chunk.Mesh->SetEnableRaytracing(false);
			Chunk.Mesh->SetVisibleInRayTracing(false);
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
		const UDynamicMesh* QueryMesh = GetChunkQueryMesh(Chunk);
		Chunk.Triangles = QueryMesh ? QueryMesh->GetTriangleCount() : 0;
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
		UDynamicMesh* QueryMesh = GetChunkQueryMesh(Chunk);
		if (!QueryMesh) continue;
		QueryMesh->ProcessMesh([&Mesh](const FDynamicMesh3& Source)
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

bool AGeneratedMiningRock::MineBlast(FVector WorldCenter, float WorldRadius, float Roughness, int32 Seed)
{
    if(ActorHasTag(TEXT("BM_UnmineableBoundary"))){LastStatus=TEXT("房间边界不可挖掘");return false;}
	if (SegmentIndex != INDEX_NONE && SegmentState != EMiningSegmentState::Active) return false;
	if (WorldCenter.ContainsNaN() || !FMath::IsFinite(WorldRadius) || WorldRadius <= 0 || !FMath::IsFinite(Roughness)) return false;
	if (!bChunked) return Super::MineBlast(WorldCenter, WorldRadius, Roughness, Seed);
	const FDynamicMesh3 Cutter = MiningBlast::MakeCutter(WorldCenter, WorldRadius, Roughness, Seed);
	const bool bResult = MineChunks(WorldCenter, WorldRadius * 1.45f, &Cutter);
	return bResult;
}

bool AGeneratedMiningRock::MineChunks(FVector WorldCenter, float WorldRadius, const FDynamicMesh3* WorldCutter)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Mining_Cut);
	const double Started = FPlatformTime::Seconds();
	ON_SCOPE_EXIT { LastTotalMilliseconds = (FPlatformTime::Seconds() - Started) * 1000; };
	LastBooleanMilliseconds = LastCollisionMilliseconds = LastSimplificationMilliseconds = 0;
	LastTouchedChunks = 0;
	const FVector Scale = GetActorScale3D();
	if (WorldCenter.ContainsNaN() || !FMath::IsFinite(WorldRadius) || WorldRadius <= 0 ||
		Scale.ContainsNaN() || Scale.GetMin() <= UE_SMALL_NUMBER)
	{
		LastStatus = TEXT("Use positive scale and a valid radius");
		return false;
	}
	const FVector3d Center(GetActorTransform().InverseTransformPosition(WorldCenter));
	const double Radius = WorldRadius / Scale.GetMin();
	PrepareRuntimeGeometry(WorldCenter, WorldRadius + 1.f);
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
	FDynamicMesh3 Cutter = WorldCutter ? *WorldCutter : ChunkCutter;
	for (int32 Vertex : Cutter.VertexIndicesItr()) Cutter.SetVertex(Vertex, WorldCutter
		? FVector3d(GetActorTransform().InverseTransformPosition(FVector(Cutter.GetVertex(Vertex))))
		: FVector3d(GetActorTransform().InverseTransformPosition(WorldCenter + FVector(Cutter.GetVertex(Vertex)) * WorldRadius)));
	if (GetWorld()->IsGameWorld() && HasActorBegunPlay())
	{
		if (!Chunks.ContainsByPredicate([&](const FMiningRockChunk& Chunk)
			{ return Chunk.Triangles > 0 && Chunk.Bounds.ComputeSquaredDistanceToPoint(FVector(Center)) <= Radius * Radius; }))
		{
			LastStatus = TEXT("No intersection");
			return false;
		}
		return QueueDeferredCut(WorldCenter, WorldRadius, Cutter, WorldCutter != nullptr);
	}
	if(bCutPending)return QueueDeferredCut(WorldCenter,WorldRadius,Cutter,WorldCutter!=nullptr);
	TArray<int32> Affected;
	TArray<FDynamicMesh3> Prepared;
	int32 Removed = 0;
	const int32 Budget = FMath::Clamp(MaxTriangles, 1000, MiningChunks::MaxChunkTriangles);
	const float InteractiveBudget = FMath::Clamp(FMath::Min(InteractiveCutBudgetMilliseconds,
		CVarMiningInteractiveBudgetMilliseconds.GetValueOnGameThread()), .01f, 40.f);
	const double Deadline=GetWorld()->IsGameWorld()?Started+InteractiveBudget/1000:0;
	for (int32 Index = 0; Index < Chunks.Num(); ++Index)
	{
		FMiningRockChunk& Chunk = Chunks[Index];
		if (Chunk.Triangles == 0 || Chunk.Bounds.ComputeSquaredDistanceToPoint(FVector(Center)) > Radius * Radius) continue;
		FDynamicMesh3 Source;
		Chunk.Mesh->GetDynamicMesh()->ProcessMesh([&Source](const FDynamicMesh3& Mesh) { Source = Mesh; });
		FDynamicMesh3 Result;
		const double BooleanStart = FPlatformTime::Seconds();
		if(Deadline>0 && FPlatformTime::Seconds()>=Deadline)
		{return QueueDeferredCut(WorldCenter,WorldRadius,Cutter,WorldCutter!=nullptr);}
		const bool bValid = MiningChunks::Subtract(Source, Cutter, Result, WorldCutter != nullptr,
			Deadline>0?FMath::Max(.0001,Deadline-FPlatformTime::Seconds()):0);
		LastBooleanMilliseconds += (FPlatformTime::Seconds() - BooleanStart) * 1000;
		if (!bValid)
		{
			if(GetWorld()->IsGameWorld())return QueueDeferredCut(WorldCenter,WorldRadius,Cutter,WorldCutter!=nullptr);
			LastStatus = TEXT("Boundary cut rejected: ALL chunks preserved");
			return false;
		}
		if (Source.IsSameAs(Result, FDynamicMesh3::FSameAsOptions())) continue;
		MiningTerrainFragments::RemoveSmallFloatingPieces(Result,Chunk.Bounds,true,40.f/Scale.GetMin());
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
			if (GetWorld()->IsGameWorld())return QueueDeferredCut(WorldCenter,WorldRadius,Cutter,WorldCutter!=nullptr);
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
	CommitChunks(Affected,Prepared,FBox::BuildAABB(WorldCenter,FVector(WorldRadius+1.f)));
	SimplifiedTriangles += Removed;
	return true;
}

void AGeneratedMiningRock::CommitChunks(TArray<int32>& Affected,TArray<FDynamicMesh3>& Prepared,const FBox& Dirty)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Mining_Commit);
	const double CommitStart=FPlatformTime::Seconds();
	const int32 Budget=FMath::Clamp(MaxTriangles,1000,MiningChunks::MaxChunkTriangles);
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
	const double NotificationStart = FPlatformTime::Seconds();
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Mining_TerrainListeners);
		OnTerrainCommitted.Broadcast(Dirty);
	}
	LastTerrainNotificationMilliseconds = (FPlatformTime::Seconds() - NotificationStart) * 1000;
	if (GetWorld()->IsGameWorld())
	{
		for (int32 Index : Affected)
		{
			if (Chunks[Index].Cuts % 12 == 0 || Chunks[Index].Triangles > Budget * 0.8) ScheduleSimplification(Index);
		}
	}
	LastStatus = FString::Printf(TEXT("Cut committed across %d chunk(s)"), LastTouchedChunks);
}

bool AGeneratedMiningRock::QueueDeferredCut(FVector Center,float Radius,const FDynamicMesh3& Cutter,bool bPreserveEdges)
{
	auto Request=MakeShared<FMiningDeferredRequest,ESPMode::ThreadSafe>();
	Request->Center=Center;Request->Radius=Radius;Request->Cutter=Cutter;Request->bPreserveEdges=bPreserveEdges;
	CutQueue.Add(Request);
	bCutPending = true;
	EnsureBackgroundPolling();
	StartDeferredCut();
	LastStatus=TEXT("Hit accepted: finishing terrain cut");
	return true;
}

void AGeneratedMiningRock::StartDeferredCut()
{
	if(CutQueue.IsEmpty() || CutTask.IsValid())return;
	auto Permit = AcquireMiningTaskPermit();
	if (!Permit) return;
	auto Request=CutQueue[0];CutQueue.RemoveAt(0);
	TSharedPtr<FMiningDeferredCut,ESPMode::ThreadSafe> Work=MakeShared<FMiningDeferredCut,ESPMode::ThreadSafe>();
	Work->Permit = MoveTemp(Permit);
	Work->Generation=MeshGeneration;
	Work->Dirty=FBox::BuildAABB(Request->Center,FVector(Request->Radius+1.f));
	const FVector Local=GetActorTransform().InverseTransformPosition(Request->Center);
	const double Radius=Request->Radius/GetActorScale3D().GetMin();
	TArray<FDynamicMesh3> Initial;
	TArray<FBox> Bounds;
	for(int32 I=0;I<Chunks.Num();++I)
	{
		const auto& Chunk=Chunks[I];
		if(Chunk.Triangles==0 || Chunk.Bounds.ComputeSquaredDistanceToPoint(Local)>Radius*Radius)continue;
		Work->Indices.Add(I);Work->Cuts.Add(Chunk.Cuts);Bounds.Add(Chunk.Bounds);
		Chunk.Mesh->GetDynamicMesh()->ProcessMesh([&](const FDynamicMesh3& M){Work->Sources.Add(M);});
		Chunk.InitialMesh->ProcessMesh([&](const FDynamicMesh3& M){Initial.Add(M);});
	}
	const int32 Budget=FMath::Clamp(MaxTriangles,1000,MiningChunks::MaxChunkTriangles);
	const double Tolerance=FMath::Clamp<double>(SimplificationTolerance/GetActorScale3D().X,.05,1);
	const float FragmentSize=40.f/GetActorScale3D().GetMin();
	bCutPending=true;
	// Workers own mesh copies only. Neither collision nor crystal instances change
	// until every affected piece can be committed together on the game thread.
	CutTask=Async(EAsyncExecution::ThreadPool,[Work,Request,Initial=MoveTemp(Initial),Bounds=MoveTemp(Bounds),Budget,Tolerance,FragmentSize]()
	{
		const double Started=FPlatformTime::Seconds();
		for(const FVector3d Offset:{FVector3d::Zero(),FVector3d(.013,.021,.017),FVector3d(-.037,.011,-.023),FVector3d(.061,-.043,.029),FVector3d(.23,.17,-.13),FVector3d(-.61,.43,.29)})
		{
			if(FPlatformTime::Seconds()-Started>2)break;
			FDynamicMesh3 Cutter=Request->Cutter;
			for(int V:Cutter.VertexIndicesItr())Cutter.SetVertex(V,Cutter.GetVertex(V)+Offset);
			Work->Results.Reset();Work->bValid=true;
			for(int I=0;I<Work->Sources.Num();++I)
			{
				if(FPlatformTime::Seconds()-Started>2){Work->bValid=false;break;}
				FDynamicMesh3 Result;
				if(!MiningChunks::Subtract(Work->Sources[I],Cutter,Result,Request->bPreserveEdges,.25))
				{Work->Failure=FString::Printf(TEXT("chunk %d boolean, source %d tris closed %d, result %d tris closed %d"),Work->Indices[I],Work->Sources[I].TriangleCount(),Work->Sources[I].IsClosed(),Result.TriangleCount(),Result.IsClosed());Work->bValid=false;break;}
				MiningTerrainFragments::RemoveSmallFloatingPieces(Result,Bounds[I],true,FragmentSize);
				if(Result.TriangleCount()>Budget)MiningChunks::Simplify(Result,Initial[I],Bounds[I],Tolerance);
				if(Result.TriangleCount()>Budget){Work->Failure=FString::Printf(TEXT("chunk %d exceeds triangle budget: %d / %d"),Work->Indices[I],Result.TriangleCount(),Budget);Work->bValid=false;break;}
				MiningChunks::Normals(Result);Work->Results.Add(MoveTemp(Result));
			}
			if(Work->bValid)break;
		}
		Work->Milliseconds=(FPlatformTime::Seconds()-Started)*1000;
		return Work;
	});
}

void AGeneratedMiningRock::PollDeferredCut()
{
	if (!CutTask.IsValid())
	{
		bCutPending = !CutQueue.IsEmpty();
		if (bCutPending) StartDeferredCut();
		return;
	}
	if (!CutTask.IsReady()) return;
	const double Now = FPlatformTime::Seconds();
	if (Now - MiningCommitWindowStart >= 1.0 / 60.0)
	{
		MiningCommitWindowStart = Now;
		MiningCommitWindowSeconds = 0;
	}
	if (HasActorBegunPlay() && MiningCommitWindowSeconds >= .002) return;
	ON_SCOPE_EXIT { MiningCommitWindowSeconds += FPlatformTime::Seconds() - Now; };
	auto Work=CutTask.Get();
	CutTask = {};
	bCutPending = !CutQueue.IsEmpty();
	bool bCurrent=Work->Generation==MeshGeneration && SegmentState==EMiningSegmentState::Active;
	for(int I=0;bCurrent && I<Work->Indices.Num();++I)
		bCurrent=Chunks.IsValidIndex(Work->Indices[I]) && Chunks[Work->Indices[I]].Cuts==Work->Cuts[I];
	if(bCurrent && Work->bValid)
	{
		TArray<int32> Changed;TArray<FDynamicMesh3> Results;
		for(int I=0;I<Work->Indices.Num();++I)if(!Work->Sources[I].IsSameAs(Work->Results[I],FDynamicMesh3::FSameAsOptions()))
		{Changed.Add(Work->Indices[I]);Results.Add(MoveTemp(Work->Results[I]));}
		if(!Changed.IsEmpty())
		{
			const double Started=FPlatformTime::Seconds();
			CommitChunks(Changed,Results,Work->Dirty);++DeferredCutsCompleted;
			LastBooleanMilliseconds=Work->Milliseconds;LastTotalMilliseconds=(FPlatformTime::Seconds()-Started)*1000;
		}
		else LastStatus=TEXT("No new rock in mining sphere");
	}
	else if(bCurrent)
	{
		LastStatus=TEXT("Cut could not form a closed surface; original terrain preserved");
		UE_LOG(LogTemp,Warning,TEXT("Deferred mining failed on %s at %s: %s"),*GetName(),*Work->Dirty.GetCenter().ToString(),*Work->Failure);
	}
	Work.Reset();
	if(!CutQueue.IsEmpty())StartDeferredCut();
}

void AGeneratedMiningRock::ScheduleSimplification(int32 Index)
{
	if (!bSimplifyChunks || bSimplificationPending || !Chunks.IsValidIndex(Index) || Chunks[Index].Triangles < 128) return;
	auto Permit = AcquireMiningTaskPermit();
	if (!Permit) return;
	FMiningRockChunk& Chunk = Chunks[Index];
	TSharedPtr<FMiningSimplificationResult, ESPMode::ThreadSafe> Result = MakeShared<FMiningSimplificationResult, ESPMode::ThreadSafe>();
	Result->Permit = MoveTemp(Permit);
	Result->Generation = MeshGeneration;
	Result->ChunkIndex = Index;
	Result->Cuts = Chunk.Cuts;
	Chunk.Mesh->GetDynamicMesh()->ProcessMesh([&Result](const FDynamicMesh3& Mesh) { Result->Mesh = Mesh; });
	FDynamicMesh3 Initial;
	Chunk.InitialMesh->ProcessMesh([&Initial](const FDynamicMesh3& Mesh) { Initial = Mesh; });
	const FBox Bounds = Chunk.Bounds;
	const double Tolerance = FMath::Clamp<double>(SimplificationTolerance / GetActorScale3D().X, 0.05, 1);
	bSimplificationPending = true;
	EnsureBackgroundPolling();
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
	UpdateTerrainVisibility(this);
	PollBackgroundWork();
}

void AGeneratedMiningRock::EnsureBackgroundPolling()
{
	if (GetWorld() && !GetWorldTimerManager().IsTimerActive(BackgroundWorkTimer))
		GetWorldTimerManager().SetTimer(BackgroundWorkTimer, this, &AGeneratedMiningRock::PollBackgroundWork, .05f, true);
}

void AGeneratedMiningRock::PollBackgroundWork()
{
	ON_SCOPE_EXIT
	{
		if (GetWorld() && !CutTask.IsValid() && CutQueue.IsEmpty() && !SimplificationTask.IsValid())
			GetWorldTimerManager().ClearTimer(BackgroundWorkTimer);
	};
	PollDeferredCut();
	if (!bSimplificationPending || !SimplificationTask.IsReady()) return;
	const TSharedPtr<FMiningSimplificationResult, ESPMode::ThreadSafe> Result = SimplificationTask.Get();
	SimplificationTask = {};
	bSimplificationPending = false;
	if (bCutPending || Result->Generation != MeshGeneration || !Chunks.IsValidIndex(Result->ChunkIndex) ||
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
	OnTerrainCommitted.Broadcast(Chunk.Bounds.TransformBy(GetActorTransform()));
}
