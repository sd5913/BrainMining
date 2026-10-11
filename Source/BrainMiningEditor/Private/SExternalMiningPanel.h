#pragma once

#include "Widgets/SCompoundWidget.h"

class AMiningExternalRegion;
class IDetailsView;

class SExternalMiningPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExternalMiningPanel) {}
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual void Tick(const FGeometry& Geometry, double Time, float DeltaTime) override;

private:
	TWeakObjectPtr<AMiningExternalRegion> Region;
	TSharedPtr<IDetailsView> Details;
	void UseSelection();
	void SetRegion(AMiningExternalRegion* Target);
	bool CanEdit() const;
	FReply CreateRegion();
	FReply AddCutter();
	FReply AddGuide();
	FReply AddSelectedGuides();
	FReply GenerateGuides();
	FReply ClearGuides();
	FReply Generate();
	FReply Clear();
	FText Status() const;
};
