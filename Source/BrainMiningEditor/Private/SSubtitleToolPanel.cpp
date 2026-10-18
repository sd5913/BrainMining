#include "SSubtitleToolPanel.h"

#include "Subtitles/SubtitleSegment.h"
#include "Subtitles/SubtitleSettings.h"
#include "Subtitles/SubtitleWidget.h"
#include "Subtitles/SubtitleTrigger.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "IDetailsView.h"
#include "LevelEditorViewport.h"
#include "Misc/PackageName.h"
#include "PropertyEditorModule.h"
#include "UObject/SavePackage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

void SSubtitleToolPanel::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs Args;
	Args.bAllowSearch = true;
	Args.bHideSelectionTip = true;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditor.CreateDetailView(Args);
	SettingsView = PropertyEditor.CreateDetailView(Args);
	SettingsView->SetObject(GetMutableDefault<USubtitleSettings>());

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f)
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(false)
			.HeaderContent()
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("通用设置 · 统一调整所有字幕")))
			]
			.BodyContent()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).HeightOverride(170.f)[SettingsView.ToSharedRef()]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).HeightOverride(85.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						USubtitleWidget::CreateStylePreview(FText::FromString(TEXT("字幕效果预览 · 让思绪轻轻漂浮")))
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton).Text(FText::FromString(TEXT("保存通用设置")))
						.OnClicked(this, &SSubtitleToolPanel::SaveGeneralSettings)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(12.f, 0.f).VAlign(VAlign_Center)
					[
						SAssignNew(SettingsStatus, STextBlock).AutoWrapText(true)
						.Text(FText::FromString(TEXT("调整即时预览，也会应用到当前试玩。保存后重启与打包仍生效。")))
					]
				]
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
		SNew(SSplitter)
		+ SSplitter::Slot()
		.Value(0.38f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(FText::FromString(TEXT("新建字幕段后，在右侧填写文本并选择音频。音频留空时会自动生成随文字变调的复古说话音效。将触发盒放进当前关卡，玩家首次经过时播放。")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(0.f, 0.f, 8.f, 0.f)
				[
					SAssignNew(IdBox, SEditableTextBox)
					.HintText(FText::FromString(TEXT("字幕段ID")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("新建字幕段")))
					.OnClicked(this, &SSubtitleToolPanel::CreateSegment)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.f, 8.f, 8.f, 4.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("已有字幕段")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("刷新")))
					.OnClicked_Lambda([this]()
					{
						RefreshSegments();
						return FReply::Handled();
					})
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(8.f, 0.f)
			[
				SAssignNew(ListView, SListView<TSharedPtr<FSubtitleRow>>)
				.ListItemsSource(&Rows)
				.OnGenerateRow(this, &SSubtitleToolPanel::GenerateRow)
				.OnSelectionChanged(this, &SSubtitleToolPanel::OnSelected)
				.SelectionMode(ESelectionMode::Single)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("在场景中放置触发盒")))
				.OnClicked(this, &SSubtitleToolPanel::PlaceTrigger)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.f, 0.f, 8.f, 8.f)
			[
				SAssignNew(StatusText, STextBlock)
				.AutoWrapText(true)
			]
		]
		+ SSplitter::Slot()
		.Value(0.62f)
		[
			DetailsView.ToSharedRef()
		]
		]
	];
	SettingsView->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent&)
	{
		SettingsStatus->SetText(FText::FromString(TEXT("通用设置已修改，预览与当前试玩已更新；点击保存后保留。")));
	});

	RefreshSegments();
}

FReply SSubtitleToolPanel::SaveGeneralSettings()
{
	const bool bSaved = GetMutableDefault<USubtitleSettings>()->TryUpdateDefaultConfigFile();
	SettingsStatus->SetText(FText::FromString(bSaved
		? TEXT("已保存通用设置，所有字幕统一生效；重启与打包后保留。")
		: TEXT("保存失败：请检查 Config/DefaultGame.ini 是否可写。当前效果尚未保存。")));
	return FReply::Handled();
}

void SSubtitleToolPanel::RefreshSegments()
{
	Rows.Reset();
	Selected.Reset();

	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FARFilter Filter;
	Filter.ClassPaths.Add(USubtitleSegment::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	TArray<FAssetData> Assets;
	AssetRegistry.Get().GetAssets(Filter, Assets);
	for (const FAssetData& Asset : Assets)
	{
		TSharedPtr<FSubtitleRow> Row = MakeShared<FSubtitleRow>();
		Row->AssetPath = Asset.GetSoftObjectPath();
		FString TagValue;
		Asset.GetTagValue(FName(TEXT("SegmentId")), TagValue);
		TagValue.TrimQuotesInline();
		if (!TagValue.IsEmpty())
		{
			Row->SegmentId = FName(*TagValue);
		}
		else if (const USubtitleSegment* Segment = Cast<USubtitleSegment>(Asset.GetAsset()))
		{
			Row->SegmentId = Segment->SegmentId;
		}
		else
		{
			Row->SegmentId = Asset.AssetName;
		}
		Rows.Add(Row);
	}

	Rows.Sort([](const TSharedPtr<FSubtitleRow>& A, const TSharedPtr<FSubtitleRow>& B)
	{
		return A->SegmentId.LexicalLess(B->SegmentId);
	});

	if (ListView)
	{
		ListView->RequestListRefresh();
		ListView->ClearSelection();
	}
	if (DetailsView)
	{
		DetailsView->SetObject(nullptr);
	}
}

FReply SSubtitleToolPanel::CreateSegment()
{
	const FString IdText = IdBox ? IdBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (IdText.IsEmpty())
	{
		StatusText->SetText(FText::FromString(TEXT("先填写字幕段ID。")));
		return FReply::Handled();
	}

	for (const TSharedPtr<FSubtitleRow>& Row : Rows)
	{
		if (Row->SegmentId.ToString() == IdText)
		{
			StatusText->SetText(FText::FromString(TEXT("这个字幕段ID已经存在。")));
			return FReply::Handled();
		}
	}

	FString AssetName = IdText;
	AssetName.ReplaceInline(TEXT(" "), TEXT("_"));
	const FString PackageName = TEXT("/Game/Subtitles/DA_") + AssetName;
	if (FPackageName::DoesPackageExist(PackageName))
	{
		StatusText->SetText(FText::FromString(TEXT("同名字幕资源已存在。")));
		return FReply::Handled();
	}

	UPackage* Package = CreatePackage(*PackageName);
	USubtitleSegment* Segment = NewObject<USubtitleSegment>(Package, *FString(TEXT("DA_") + AssetName), RF_Public | RF_Standalone);
	Segment->SegmentId = FName(*IdText);
	FAssetRegistryModule::AssetCreated(Segment);
	Package->MarkPackageDirty();

	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(Package, Segment, *Filename, SaveArgs))
	{
		StatusText->SetText(FText::FromString(TEXT("字幕段保存失败。")));
		return FReply::Handled();
	}

	IdBox->SetText(FText::GetEmpty());
	RefreshSegments();
	for (const TSharedPtr<FSubtitleRow>& Row : Rows)
	{
		if (Row->SegmentId.ToString() == IdText && ListView)
		{
			ListView->SetSelection(Row);
			break;
		}
	}
	StatusText->SetText(FText::FromString(FString::Printf(TEXT("已创建字幕段 %s。在右侧添加字幕文本和音频。"), *IdText)));
	return FReply::Handled();
}

FReply SSubtitleToolPanel::PlaceTrigger()
{
	USubtitleSegment* Segment = LoadSelected();
	if (!Segment)
	{
		StatusText->SetText(FText::FromString(TEXT("先在列表里选中一个字幕段。")));
		return FReply::Handled();
	}
	if (Segment->SegmentId.IsNone())
	{
		StatusText->SetText(FText::FromString(TEXT("这个字幕段还没有ID。")));
		return FReply::Handled();
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		StatusText->SetText(FText::FromString(TEXT("当前没有打开的关卡。")));
		return FReply::Handled();
	}

	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	if (GCurrentLevelEditingViewportClient)
	{
		Rotation = GCurrentLevelEditingViewportClient->GetViewRotation();
		Location = GCurrentLevelEditingViewportClient->GetViewLocation() + Rotation.Vector() * 400.f;
		Rotation.Pitch = 0.f;
		Rotation.Roll = 0.f;
	}

	ASubtitleTrigger* Trigger = World->SpawnActor<ASubtitleTrigger>(Location, Rotation);
	if (!Trigger)
	{
		StatusText->SetText(FText::FromString(TEXT("触发盒创建失败。")));
		return FReply::Handled();
	}

	Trigger->SetSegmentId(Segment->SegmentId);
	Trigger->SetActorLabel(FString::Printf(TEXT("字幕触发_%s"), *Segment->SegmentId.ToString()));
	Trigger->SetFolderPath(TEXT("Subtitles"));
	GEditor->SelectNone(false, true, false);
	GEditor->SelectActor(Trigger, true, true, true);
	StatusText->SetText(FText::FromString(FString::Printf(TEXT("已把字幕段 %s 的触发盒放进当前关卡。玩家第一次经过时播放。"), *Segment->SegmentId.ToString())));
	return FReply::Handled();
}

TSharedRef<ITableRow> SSubtitleToolPanel::GenerateRow(TSharedPtr<FSubtitleRow> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FString Label = Item.IsValid() ? Item->SegmentId.ToString() : FString();
	return SNew(STableRow<TSharedPtr<FSubtitleRow>>, OwnerTable)
		[
			SNew(SBox)
			.Padding(FMargin(6.f, 3.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
			]
		];
}

void SSubtitleToolPanel::OnSelected(TSharedPtr<FSubtitleRow> Item, ESelectInfo::Type SelectInfo)
{
	Selected = Item;
	if (DetailsView)
	{
		DetailsView->SetObject(LoadSelected());
	}
}

USubtitleSegment* SSubtitleToolPanel::LoadSelected() const
{
	if (!Selected.IsValid() || !Selected->AssetPath.IsValid())
	{
		return nullptr;
	}
	return Cast<USubtitleSegment>(Selected->AssetPath.TryLoad());
}
