#pragma once

#include "CoreMinimal.h"
#include "MiningSpatialGeometry.h"

namespace MiningSpatialSafeEditing
{
	struct FIssue
	{
		TWeakObjectPtr<AActor> Actor;
		MiningSpatial::FDiagnostic Geometry;
		FString Reason;
		bool bLimited = false;
	};
	void Register();
	void Unregister();
	void Observe(UWorld* World);
	void Reset();
	bool Handle(AActor* Actor);
	bool Apply(AActor* Actor, bool bForce = false);
	bool IsFreeEditing(AActor* Actor);
	struct FDragStats
	{
		int32 Requests = 0, Coalesced = 0, Committed = 0, Discarded = 0;
		double LastInputMilliseconds = 0, MaxInputMilliseconds = 0;
		double LastComputeMilliseconds = 0, LastCommitMilliseconds = 0, MaxCommitMilliseconds = 0, LastLatencyMilliseconds = 0;
	};
	void BeginInteractive();
	bool IsInteractive();
	bool QueueInteractive(AActor* Actor);
	void PumpInteractive();
	bool FinishInteractive(bool bCancel = false);
	bool HasPendingInteractive();
	const FDragStats& DragStats();
	bool Add(AActor* Actor);
	const FIssue& Issue();
	FString Status();
	FString TerrainStatus();
	FString SaveStatus();
	bool PrepareWorld(UWorld* World);
	bool Save();
	bool IsRepairing();
	bool BeginRepair();
	bool CancelRepair();
	void FocusIssue();
	bool SmoothIssue();
}
