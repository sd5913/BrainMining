#pragma once

#include "CoreMinimal.h"

class AActor;
class AMiningSpatialRegion;
class UWorld;

namespace MiningSpatialOverview
{
	enum class EScope { Map, Region, Selection };
	enum class EView { Keep, Perspective, Top, Front, Side };

	struct FLine
	{
		FVector Start, End;
	};

	struct FEntry
	{
		TWeakObjectPtr<AActor> Actor;
		uint32 Hash = 0;
		FBox Bounds = FBox(ForceInit);
		TArray<FLine> Lines;
		TArray<FVector> Vertices;
		TArray<FIntVector> Triangles;
		bool bDraft = false;
		void AddLine(const FVector& Start, const FVector& End);
	};

	class FScene
	{
	public:
		void Refresh(UWorld* World);
		void Reset();
		const TArray<FEntry>& GetEntries() const { return Entries; }
		FBox GetBounds(EScope Scope, AActor* Selected, AMiningSpatialRegion* Region, bool bIncludeRockBounds = false) const;
	private:
		TWeakObjectPtr<UWorld> CachedWorld;
		TArray<FEntry> Entries;
	};

	struct FOptions
	{
		bool bLabels = false;
		bool bRockBounds = false;
		bool bSolid = true;
		bool bDirectManipulation = true;
	};

	FOptions& Options();
	void Register();
	void Unregister();
	void SetEnabled(bool bEnabled);
	bool IsEnabled();
	void Redraw();
	void GetPreviewTerrain(UWorld* World, TArray<AActor*>& Actors);
	bool Focus(EScope Scope, AActor* Selected, AMiningSpatialRegion* Region, FString& Status, EView View = EView::Keep);
}
