#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "MiningSpatialTunnel.h"
#include "MiningSpatialOverview.h"

class IDetailsView;
class SMiningSectionCanvas;

class SMiningSpatialEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMiningSpatialEditor) {} SLATE_END_ARGS()
	void Construct(const FArguments& Arguments);
	virtual ~SMiningSpatialEditor() override;
	virtual void Tick(const FGeometry& Geometry, double Time, float Delta) override;
	static void Open();
private:
	TSharedPtr<IDetailsView> Details;
	TSharedPtr<SMiningSectionCanvas> Canvas;
	TWeakObjectPtr<AActor> Selection;
	TWeakObjectPtr<AMiningSpatialRegion> Region;
	TWeakObjectPtr<AMiningSpatialTunnel> LastTunnel;
	int32 LastKnot = INDEX_NONE;
	bool bInitialFrame = true;
	MiningSpatialOverview::EScope OverviewScope = MiningSpatialOverview::EScope::Map;
	FString OverviewStatus;
	TSharedRef<SWidget> Overview();
	TSharedRef<SWidget> DirectTools();
	TSharedRef<SWidget> EnvelopeTools();
	TSharedRef<SWidget> Advanced();
	void Focus(MiningSpatialOverview::EScope Scope, MiningSpatialOverview::EView View = MiningSpatialOverview::EView::Keep);
	void FollowSelection(AActor* Actor);
	TSharedRef<SWidget> Button(const FString& Name, TFunction<void()> Action);
	TSharedRef<SWidget> Objects();
	TSharedRef<SWidget> Number(const FString& Name, int32 Property);
	void Select(AActor* Actor);
	void Create(EMiningManualKind Kind);
	void ConnectSideRoom();
	void Rebuild();
	void Preset(bool bEllipse);
};
