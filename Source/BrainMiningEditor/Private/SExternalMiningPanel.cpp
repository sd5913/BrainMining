#include "SExternalMiningPanel.h"
#include "MiningExternalRegion.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/StaticMesh.h"
#include "ScopedTransaction.h"
#include "LevelEditorViewport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SExternalMiningPanel::Construct(const FArguments& Args)
{
	FDetailsViewArgs ViewArgs;
	ViewArgs.bAllowSearch = true;
	ViewArgs.bHideSelectionTip = true;
	ViewArgs.NameAreaSettings = FDetailsViewArgs::ObjectsUseNameArea;
	Details = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateDetailView(ViewArgs);
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(12)
		[
			SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("外部通路矿区生成器\n通路切割体生成空腔；阻塞模型生成引导可挖层。阻塞 FBX 导入时关闭合并网格、保留共同原点，然后在内容浏览器选中模型并批量添加。引导层默认跟随第一个通路的变换。")))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(12, 0, 12, 8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[
				SNew(SButton).Text(FText::FromString(TEXT("新建矿区"))).IsEnabled_Lambda([] { return GEditor && !GEditor->PlayWorld; }).OnClicked(this, &SExternalMiningPanel::CreateRegion)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[
				SNew(SButton).Text(FText::FromString(TEXT("使用场景所选矿区"))).OnClicked_Lambda([this] { UseSelection(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(FText::FromString(TEXT("添加通路模型"))).IsEnabled(this, &SExternalMiningPanel::CanEdit).OnClicked(this, &SExternalMiningPanel::AddCutter)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(12, 0, 12, 8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[
				SNew(SButton).Text(FText::FromString(TEXT("添加引导模型"))).IsEnabled(this, &SExternalMiningPanel::CanEdit).OnClicked(this, &SExternalMiningPanel::AddGuide)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(FText::FromString(TEXT("批量添加所选阻塞模型"))).IsEnabled(this, &SExternalMiningPanel::CanEdit).OnClicked(this, &SExternalMiningPanel::AddSelectedGuides)
			]
		]
		+ SVerticalBox::Slot().FillHeight(1).Padding(8, 0)
		[
			Details.ToSharedRef()
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(12, 8)
		[
			SNew(STextBlock).AutoWrapText(true).Text(this, &SExternalMiningPanel::Status)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(12, 0, 12, 12)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[
				SNew(SButton).Text(FText::FromString(TEXT("生成 / 重新生成引导可挖层"))).IsEnabled(this, &SExternalMiningPanel::CanEdit).OnClicked(this, &SExternalMiningPanel::GenerateGuides)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[
				SNew(SButton).Text(FText::FromString(TEXT("恢复引导层"))).IsEnabled(this, &SExternalMiningPanel::CanEdit).OnClicked_Lambda([this] { if (CanEdit()) Region->ResetGuideExcavation(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(FText::FromString(TEXT("清除引导可挖层"))).IsEnabled(this, &SExternalMiningPanel::CanEdit).OnClicked(this, &SExternalMiningPanel::ClearGuides)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(12, 0, 12, 12)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[
				SNew(SButton).Text(FText::FromString(TEXT("生成 / 重新生成矿区"))).IsEnabled(this, &SExternalMiningPanel::CanEdit).OnClicked(this, &SExternalMiningPanel::Generate)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(FText::FromString(TEXT("清除本矿区生成结果"))).IsEnabled(this, &SExternalMiningPanel::CanEdit).OnClicked(this, &SExternalMiningPanel::Clear)
			]
		]
	];
	UseSelection();
}

bool SExternalMiningPanel::CanEdit() const
{
	return Region.IsValid() && GEditor && !GEditor->PlayWorld && Region->GetWorld() == GEditor->GetEditorWorldContext().World();
}

void SExternalMiningPanel::SetRegion(AMiningExternalRegion* Target)
{
	if (Region.Get() == Target) return;
	Region = Target;
	Details->SetObject(Target);
}

void SExternalMiningPanel::UseSelection()
{
	if (!GEditor || GEditor->PlayWorld) return;
	for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
	{
		AActor* Actor = Cast<AActor>(*It);
		if (!Actor) continue;
		AMiningExternalRegion* Target = Cast<AMiningExternalRegion>(Actor);
		if (!Target) Target = Cast<AMiningExternalRegion>(Actor->GetOwner());
		if (Target && Target->GetWorld() == GEditor->GetEditorWorldContext().World()) { SetRegion(Target); return; }
	}
}

void SExternalMiningPanel::Tick(const FGeometry& Geometry, double Time, float DeltaTime)
{
	SCompoundWidget::Tick(Geometry, Time, DeltaTime);
	if (GEditor && !GEditor->PlayWorld)
	{
		if (!Region.IsValid() || Region->GetWorld() != GEditor->GetEditorWorldContext().World()) { Region.Reset(); Details->SetObject(nullptr); }
		UseSelection();
	}
}

FReply SExternalMiningPanel::CreateRegion()
{
	if (!GEditor || GEditor->PlayWorld) return FReply::Handled();
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return FReply::Handled();
	const FScopedTransaction Transaction(NSLOCTEXT("ExternalMining", "Create", "新建外部通路矿区"));
	World->GetCurrentLevel()->Modify();
	FVector Location = FVector::ZeroVector;
	if (GCurrentLevelEditingViewportClient) Location = GCurrentLevelEditingViewportClient->GetViewLocation() + GCurrentLevelEditingViewportClient->GetViewRotation().Vector() * 3000;
	FActorSpawnParameters Spawn;
	Spawn.OverrideLevel = World->GetCurrentLevel();
	Spawn.ObjectFlags |= RF_Transactional;
	AMiningExternalRegion* Target = World->SpawnActor<AMiningExternalRegion>(Location, FRotator::ZeroRotator, Spawn);
	if (Target)
	{
		Target->SetActorLabel(TEXT("外部通路矿区"));
		Target->SetFolderPath(FName(TEXT("ExternalMining/Edit")));
		GEditor->SelectNone(false, true);
		GEditor->SelectActor(Target, true, true);
		SetRegion(Target);
		Target->MarkPackageDirty();
	}
	return FReply::Handled();
}

FReply SExternalMiningPanel::AddCutter()
{
	if (!CanEdit()) return FReply::Handled();
	const FScopedTransaction Transaction(NSLOCTEXT("ExternalMining", "AddCutter", "添加外部通路模型"));
	Region->Modify();
	Region->Cutters.AddDefaulted();
	Region->BuildStatus = TEXT("已添加切割体：展开通路数组选择模型并设置变换，然后重新生成。");
	Region->MarkPackageDirty();
	Details->ForceRefresh();
	return FReply::Handled();
}

FReply SExternalMiningPanel::Generate()
{
	if (CanEdit()) Region->GenerateRegion();
	return FReply::Handled();
}

FReply SExternalMiningPanel::AddGuide()
{
	if (!CanEdit()) return FReply::Handled();
	const FScopedTransaction Transaction(NSLOCTEXT("ExternalMining", "AddGuide", "添加引导层模型"));
	Region->Modify(); Region->GuideLayers.AddDefaulted();
	Region->GuideBuildStatus = TEXT("已添加引导模型：选择阻塞块模型后生成引导可挖层。");
	Region->MarkPackageDirty(); Details->ForceRefresh(); return FReply::Handled();
}

FReply SExternalMiningPanel::AddSelectedGuides()
{
	if (!CanEdit()) return FReply::Handled();
	TArray<FAssetData> Selected;
	FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().GetSelectedAssets(Selected);
	Selected.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
	const FScopedTransaction Transaction(NSLOCTEXT("ExternalMining", "AddSelectedGuides", "批量添加引导模型"));
	Region->Modify();
	int32 Added = 0;
	for (const FAssetData& Asset : Selected)
	{
		UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset());
		if (!Mesh || Region->GuideLayers.ContainsByPredicate([Mesh](const FMiningExternalGuideLayer& Layer) { return Layer.Mesh == Mesh; })) continue;
		if (Region->GuideLayers.Num() >= 128) break;
		Region->GuideLayers.AddDefaulted_GetRef().Mesh = Mesh; ++Added;
	}
	Region->GuideBuildStatus = Added > 0 ? FString::Printf(TEXT("已添加 %d 个阻塞模型；确认对应通路后生成引导可挖层。"), Added) : TEXT("请先在内容浏览器选中尚未添加的阻塞块 Static Mesh。");
	Region->MarkPackageDirty(); Details->ForceRefresh(); return FReply::Handled();
}

FReply SExternalMiningPanel::GenerateGuides()
{
	if (CanEdit()) Region->GenerateGuideLayers();
	return FReply::Handled();
}

FReply SExternalMiningPanel::ClearGuides()
{
	if (CanEdit()) Region->ClearGeneratedGuideLayers();
	return FReply::Handled();
}

FReply SExternalMiningPanel::Clear()
{
	if (CanEdit()) Region->ClearGeneratedRegion();
	return FReply::Handled();
}

FText SExternalMiningPanel::Status() const
{
	if (GEditor && GEditor->PlayWorld) return FText::FromString(TEXT("运行中：请退出 Play 后编辑并生成矿区。"));
	if (!Region.IsValid()) return FText::FromString(TEXT("新建矿区，或在 Outliner 中选择一个外部通路矿区。"));
	return FText::FromString(Region->BuildStatus + TEXT("\n") + Region->GuideBuildStatus);
}
