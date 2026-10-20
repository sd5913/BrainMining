#include "DiggableRock.h"
#include "MiningBlastGeometry.h"
#include "MiningPickaxeGeometry.h"
#include "Engine/World.h"
#include "FogChamberComponent.h"
#include "BrainMining.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "Generators/SweepGenerator.h"
#include "Generators/SphereGenerator.h"
#include "Operations/MeshBoolean.h"
#include "Operations/LocalPlanarSimplify.h"
#include "Misc/ScopeExit.h"
#include "PhysicsEngine/BodySetup.h"
#include "UDynamicMesh.h"

using namespace UE::Geometry;

ADiggableRock::ADiggableRock()
{
	PrimaryActorTick.bCanEverTick = false;
	RockMesh = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("RockMesh"));
	SetRootComponent(RockMesh);
	// UE 5.8 dynamic-mesh BLAS can be evicted during topology replacement.
	// Excavatable terrain uses raster lighting; do not submit stale RT geometry.
	RockMesh->SetEnableRaytracing(false);
	RockMesh->SetVisibleInRayTracing(false);
	RockMesh->SetCollisionProfileName(TEXT("BlockAll"));
	RockMesh->SetGenerateOverlapEvents(false);
	RockMesh->SetComplexAsSimpleCollisionEnabled(true, false);
	RockMesh->SetDeferredCollisionUpdatesEnabled(true, false);
	RockMesh->bUseAsyncCooking = false;
	RockMesh->SetCanEverAffectNavigation(false);
	FogChamber = CreateDefaultSubobject<UFogChamberComponent>(TEXT("FogChamber"));
}

void ADiggableRock::BeginPlay()
{
	Super::BeginPlay();
	InitializeForPlay();
}

void ADiggableRock::InitializeForPlay()
{
	ResetRock();
}

void ADiggableRock::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ResetRock();
}

void ADiggableRock::CommitMesh(FDynamicMesh3&& Mesh)
{
	TriangleCount = Mesh.TriangleCount();
	// Also enforce this for components serialized in older maps.
	RockMesh->SetEnableRaytracing(false);
	RockMesh->SetVisibleInRayTracing(false);
	RockMesh->SetMesh(MoveTemp(Mesh));
	RockMesh->GetBodySetup()->SetFlags(RF_Transient);
	const double CollisionStart = FPlatformTime::Seconds();
	RockMesh->UpdateCollision(false);
	LastCollisionMilliseconds = (FPlatformTime::Seconds() - CollisionStart) * 1000.0;
}

void ADiggableRock::BakeAuthoredMesh(UDynamicMesh* Mesh)
{
	if (!Mesh || Mesh->GetMeshRef().TriangleCount()==0 || !Mesh->GetMeshRef().IsClosed()) return;
	Modify();
	AuthoredRestMesh=NewObject<UDynamicMesh>(this);
	FDynamicMesh3 Copy=Mesh->GetMeshRef();
	AuthoredRestMesh->SetMesh(MoveTemp(Copy));
	ResetRock();
}

void ADiggableRock::ResetRock()
{
	if(AuthoredRestMesh && AuthoredRestMesh->GetMeshRef().TriangleCount()>0)
	{
		FDynamicMesh3 Mesh=AuthoredRestMesh->GetMeshRef();
		CommitMesh(MoveTemp(Mesh));
		if(RockMaterial)RockMesh->SetMaterial(0,RockMaterial);
		SuccessfulCuts=0;LastBooleanMilliseconds=LastTotalMilliseconds=0;
		LastStatus=TEXT("Authored mesh ready");
		OnTerrainCommitted.Broadcast(FBox(ForceInit));
		return;
	}
	const FVector SafeSize(FMath::Max(50.0, RockSize.X), FMath::Max(50.0, RockSize.Y), FMath::Max(50.0, RockSize.Z));
	FDynamicMesh3 Mesh;
	if (bUseCircularRockMesh)
	{
		// FCylinderGenerator is vertical by default. Rotate its vertices so the
		// doorway's short depth follows local X and its round face is YZ.
		FCylinderGenerator Generator;
		Generator.Height = SafeSize.X;
		Generator.Radius[0] = Generator.Radius[1] = FMath::Min(SafeSize.Y, SafeSize.Z) * .5f;
		Generator.AngleSamples = 20;
		Generator.bCapped = true;
		Generator.Generate();
		Mesh = FDynamicMesh3(&Generator);
		const FVector3d BoundsCenter = Mesh.GetBounds().Center();
		for (int32 VertexID : Mesh.VertexIndicesItr())
		{
			const FVector3d P = Mesh.GetVertex(VertexID) - BoundsCenter;
			Mesh.SetVertex(VertexID, FVector3d(P.Z, P.X, P.Y));
		}
	}
	else
	{
		FMinimalBoxMeshGenerator Generator;
		Generator.Box = FOrientedBox3d(FVector3d::Zero(), FVector3d(SafeSize * 0.5));
		Generator.Generate();
		Mesh = FDynamicMesh3(&Generator);
	}
	const bool bRoomValid = FogChamber->InitializeRoom(Mesh, SafeSize);
	CommitMesh(MoveTemp(Mesh));
	if (RockMaterial)
	{
		RockMesh->SetMaterial(0, RockMaterial);
	}
	SuccessfulCuts = 0;
	LastBooleanMilliseconds = 0.0f;
	LastTotalMilliseconds = 0.0f;
	LastStatus = bRoomValid ? TEXT("Ready") : TEXT("Invalid room bounds: room must fit inside rock with 10cm walls");
	OnTerrainCommitted.Broadcast(FBox(ForceInit));
}

bool ADiggableRock::MineSphere(FVector WorldCenter, float WorldRadius)
{
    if(ActorHasTag(TEXT("BM_UnmineableBoundary"))){LastStatus=TEXT("房间边界不可挖掘");return false;}
	if (ActorHasTag(TEXT("BM_BombOnly"))) { LastStatus = TEXT("坚硬岩层：只能用炸弹破坏"); return false; }
	const bool bResult = MineCutter(WorldCenter, WorldRadius, nullptr);
	return bResult;
}

bool ADiggableRock::MinePickaxe(FTransform WorldFrame, FVector HalfSize, int32 Seed, float Roughness)
{
    if(ActorHasTag(TEXT("BM_UnmineableBoundary"))){LastStatus=TEXT("房间边界不可挖掘");return false;}
	if (ActorHasTag(TEXT("BM_BombOnly"))) { LastStatus = TEXT("坚硬岩层：只能用炸弹破坏"); return false; }
	if (!WorldFrame.IsValid() || HalfSize.ContainsNaN() || HalfSize.GetMin() <= 0) return false;
	const FDynamicMesh3 Cutter = MiningPickaxe::MakeCutter(WorldFrame, HalfSize, Seed, Roughness);
	const bool bResult = MineCutter(WorldFrame.GetLocation(), HalfSize.Size() * 1.12, &Cutter);
	return bResult;
}

bool ADiggableRock::MineBlast(FVector WorldCenter, float WorldRadius, float Roughness, int32 Seed)
{
    if(ActorHasTag(TEXT("BM_UnmineableBoundary"))){LastStatus=TEXT("房间边界不可挖掘");return false;}
	if (WorldCenter.ContainsNaN() || !FMath::IsFinite(WorldRadius) || WorldRadius <= 0 || !FMath::IsFinite(Roughness)) return false;
	const FDynamicMesh3 Cutter = MiningBlast::MakeCutter(WorldCenter, WorldRadius, Roughness, Seed);
	const bool bResult = MineCutter(WorldCenter, WorldRadius * 1.45f, &Cutter);
	return bResult;
}

bool ADiggableRock::MineCutter(FVector WorldCenter, float WorldRadius, const FDynamicMesh3* WorldCutter)
{
	const double OperationStart = FPlatformTime::Seconds();
	LastBooleanMilliseconds = 0.0f;
	LastCollisionMilliseconds = 0.0f;
	ON_SCOPE_EXIT { LastTotalMilliseconds = (FPlatformTime::Seconds() - OperationStart) * 1000.0; };
	const FVector Scale = GetActorScale3D();
	if (WorldCenter.ContainsNaN() || !FMath::IsFinite(WorldRadius) || WorldRadius <= 0.0f ||
		Scale.ContainsNaN() || Scale.GetMin() <= UE_SMALL_NUMBER)
	{
		LastStatus = TEXT("Use positive scale and a valid radius");
		return false;
	}
	if (TriangleCount == 0)
	{
		LastStatus = TEXT("Rock depleted");
		return false;
	}
	const FVector LocalCenter = GetActorTransform().InverseTransformPosition(WorldCenter);
	const double LocalRadius = WorldRadius / Scale.GetMin();
	FDynamicMesh3 SourceMesh;
	RockMesh->GetDynamicMesh()->ProcessMesh([&SourceMesh](const FDynamicMesh3& Mesh) { SourceMesh = Mesh; });
	if (SourceMesh.GetBounds().DistanceSquared(FVector3d(LocalCenter)) > LocalRadius * LocalRadius)
	{
		LastStatus = TEXT("No intersection");
		return false;
	}
	if (UnitCutter.TriangleCount() == 0)
	{
		FSphereGenerator Generator;
		Generator.Radius = 1.0;
		Generator.NumPhi = 8;
		Generator.NumTheta = 16;
		Generator.Generate();
		UnitCutter = FDynamicMesh3(&Generator);
	}
	FDynamicMesh3 Cutter = WorldCutter ? *WorldCutter : UnitCutter;
	if (bUseProceduralCutFallback)
	{
		SourceMesh.DiscardAttributes();
		Cutter.DiscardAttributes();
		if (!SourceMesh.HasTriangleGroups()) SourceMesh.EnableTriangleGroups(0);
		if (!Cutter.HasTriangleGroups()) Cutter.EnableTriangleGroups(0);
	}
	for (int32 VertexID : Cutter.VertexIndicesItr())
	{
		Cutter.SetVertex(VertexID, WorldCutter ? FVector3d(GetActorTransform().InverseTransformPosition(FVector(Cutter.GetVertex(VertexID))))
			: FVector3d(GetActorTransform().InverseTransformPosition(WorldCenter + FVector(Cutter.GetVertex(VertexID)) * WorldRadius)));
	}
	FDynamicMesh3 Result;
	FMeshBoolean Operation(&SourceMesh, &Cutter, &Result, FMeshBoolean::EBooleanOp::Difference);
	// Interactive edits must never run the editor's unbounded retry sequence.
	const bool bInteractive = GetWorld() && GetWorld()->IsGameWorld();
	const double Deadline = bInteractive ? FPlatformTime::Seconds() + .035 : 0;
	FProgressCancel Progress;
	Progress.CancelF = [Deadline](){ return Deadline > 0 && FPlatformTime::Seconds() >= Deadline; };
	Operation.Progress = &Progress;
	Operation.bSimplifyAlongNewEdges = WorldCutter == nullptr;
	Operation.bPreserveTriangleGroups = !bUseProceduralCutFallback;
	const double BooleanStart = FPlatformTime::Seconds();
	const auto IsValidResult = [&Result, &Operation]()
	{
		return Operation.CreatedBoundaryEdges.Num() == 0 &&
			Result.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly) &&
			(Result.TriangleCount() == 0 || Result.IsClosed());
	};
	bool bSucceeded = Operation.Compute() && IsValidResult();
	if (!bSucceeded && !Progress.Cancelled())
	{
		Result.Clear();
		Operation.bSimplifyAlongNewEdges = false;
		Operation.CreatedBoundaryEdges.Reset();
		bSucceeded = Operation.Compute() && IsValidResult();
	}
	if (!bSucceeded && (bUseProceduralCutFallback || WorldCutter) && !Progress.Cancelled())
	{
		SourceMesh.DiscardAttributes();
		Cutter.DiscardAttributes();
		if (!SourceMesh.HasTriangleGroups()) SourceMesh.EnableTriangleGroups(0);
		if (!Cutter.HasTriangleGroups()) Cutter.EnableTriangleGroups(0);
		for (double Tolerance : {0.0000001, 0.000001, 0.00001})
		{
			if (Progress.Cancelled()) break;
			Result.Clear();
			FMeshBoolean Retry(&SourceMesh, &Cutter, &Result, FMeshBoolean::EBooleanOp::Difference);
			Retry.Progress = &Progress;
			Retry.SnapTolerance = Tolerance;
			Retry.bSimplifyAlongNewEdges = false;
			bSucceeded = Retry.Compute() && Retry.CreatedBoundaryEdges.IsEmpty() &&
				(Result.TriangleCount() == 0 || Result.IsClosed()) &&
				Result.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly);
			if (bSucceeded) break;
		}
	}
	LastBooleanMilliseconds = (FPlatformTime::Seconds() - BooleanStart) * 1000.0;
	if (!bSucceeded && (bUseProceduralCutFallback || WorldCutter) && !Progress.Cancelled())
	{
		// Faceted terrain can put a cutter vertex exactly on an existing edge.
		// A sub-millimetre translation resolves that coincidence without changing
		// the player's cut shape perceptibly. Keep the same strict validity gate.
		for (int32 Attempt = 0; Attempt < 4 && !bSucceeded; ++Attempt)
		{
			if (Progress.Cancelled()) break;
			for (int32 V : Cutter.VertexIndicesItr()) Cutter.SetVertex(V, Cutter.GetVertex(V) + FVector3d(.013,.021,.017));
			Result.Clear();
			FMeshBoolean Retry(&SourceMesh, &Cutter, &Result, FMeshBoolean::EBooleanOp::Difference);
			Retry.Progress = &Progress;
			// FMeshBoolean normalizes the mesh: .1 here was ~10% of the rock,
			// merging unrelated edges and taking 12.99 seconds on TunnelSoilRight.
			const double Tolerances[]={.0000001,.000001,.000001,.00001};
			Retry.SnapTolerance = Tolerances[Attempt]; Retry.bSimplifyAlongNewEdges = false;
			bSucceeded = Retry.Compute() && Retry.CreatedBoundaryEdges.IsEmpty() &&
				(Result.TriangleCount() == 0 || Result.IsClosed()) &&
				Result.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly);
		}
		LastBooleanMilliseconds = (FPlatformTime::Seconds() - BooleanStart) * 1000.0;
	}
	if (!bSucceeded)
	{
		LastStatus = TEXT("Cut rejected: original rock preserved");
		UE_LOG(LogBrainMining, Warning, TEXT("Mining rejected invalid boolean result on %s"), *GetName());
		return false;
	}
	if (WorldCutter && SourceMesh.IsSameAs(Result, FDynamicMesh3::FSameAsOptions()))
	{
		LastStatus = TEXT("No new rock in blast volume");
		return false;
	}
	const int32 TriangleBudget = FMath::Clamp(MaxTriangles, 1000, 100000);
	if (bUseProceduralCutFallback && Result.TriangleCount() > 0 &&
		((SuccessfulCuts + 1) % 8 == 0 || Result.TriangleCount() > TriangleBudget * 0.8))
	{
		FDynamicMesh3 Simplified = Result;
		Simplified.DiscardAttributes();
		TSet<int32> Edges;
		for (int32 EdgeID : Simplified.EdgeIndicesItr()) Edges.Add(EdgeID);
		FLocalPlanarSimplify::CollapseDegenerateEdges(Simplified, Edges, true);
		FLocalPlanarSimplify Simplifier;
		Simplifier.SimplificationAngleTolerance = 0.001;
		Simplifier.bPreserveTriangleGroups = false;
		Simplifier.SimplifyAlongEdges(Simplified, Edges);
		if (Simplified.IsClosed() && Simplified.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly))
		{
			Simplified.CompactInPlace();
			Result = MoveTemp(Simplified);
		}
	}
	if (Result.TriangleCount() > TriangleBudget)
	{
		LastStatus = TEXT("Triangle budget reached - press R to reset");
		return false;
	}
	if (Result.TriangleCount() > 0)
	{
		Result.EnableAttributes();
		FMeshNormals::InitializeOverlayToPerTriangleNormals(Result.Attributes()->PrimaryNormals());
	}
	CommitMesh(MoveTemp(Result));
	RockMesh->GetDynamicMesh()->ProcessMesh([this, LocalCenter, LocalRadius](const FDynamicMesh3& Mesh)
	{
		FogChamber->ObserveCut(Mesh, LocalCenter, LocalRadius);
	});
	++SuccessfulCuts;
	OnTerrainCommitted.Broadcast(FBox::BuildAABB(WorldCenter, FVector(WorldRadius + 1.0f)));
	LastTotalMilliseconds = (FPlatformTime::Seconds() - OperationStart) * 1000.0;
	LastStatus = TriangleCount > 0 ? TEXT("Cut committed") : TEXT("Rock depleted");
	UE_LOG(LogBrainMining, Display, TEXT("Mining cut %d: triangles=%d boolean=%.2fms collision=%.2fms total=%.2fms"),
		SuccessfulCuts, TriangleCount, LastBooleanMilliseconds, LastCollisionMilliseconds, LastTotalMilliseconds);
	return true;
}
