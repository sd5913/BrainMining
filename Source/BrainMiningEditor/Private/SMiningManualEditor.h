#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"

class AMiningManualRegion;
class AMiningManualShape;
class IDetailsView;
class SMiningPenCanvas;

class SMiningManualEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMiningManualEditor) {}
	SLATE_END_ARGS()
	void Construct(const FArguments& Arguments);
	virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override;
	static void Open();
private:
	TWeakObjectPtr<AMiningManualRegion> Region;
	TWeakObjectPtr<AActor> Selected;
	TSharedPtr<IDetailsView> Details;
	TSharedPtr<SMiningPenCanvas> Canvas;
	TSharedPtr<SComboBox<TWeakObjectPtr<AMiningManualRegion>>> Picker;
	TArray<TWeakObjectPtr<AMiningManualRegion>> Regions;
	TSharedPtr<SComboBox<TWeakObjectPtr<AActor>>> ObjectPicker;
	TArray<TWeakObjectPtr<AActor>> Objects;
	double NextScan = 0;
	FString Message;
	void Select(AActor* Actor);
	void SelectRegion(AMiningManualRegion* Actor);
	FReply Add(int32 Kind);
	FReply Rebuild();
	FReply Duplicate();
	FReply ConnectMain();
	FReply NewRegion();
	TSharedRef<SWidget> Button(const FString& Label, TFunction<FReply()> Action);
};
