#pragma once

#include "IDetailsView.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

class USubtitleSegment;

struct FSubtitleRow
{
	FName SegmentId;
	FSoftObjectPath AssetPath;
};

class SSubtitleToolPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSubtitleToolPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void RefreshSegments();
	FReply CreateSegment();
	FReply PlaceTrigger();
	FReply SaveGeneralSettings();
	TSharedRef<ITableRow> GenerateRow(TSharedPtr<FSubtitleRow> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnSelected(TSharedPtr<FSubtitleRow> Item, ESelectInfo::Type SelectInfo);
	USubtitleSegment* LoadSelected() const;

	TArray<TSharedPtr<FSubtitleRow>> Rows;
	TSharedPtr<SListView<TSharedPtr<FSubtitleRow>>> ListView;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<IDetailsView> SettingsView;
	TSharedPtr<STextBlock> SettingsStatus;
	TSharedPtr<SEditableTextBox> IdBox;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<FSubtitleRow> Selected;
};
