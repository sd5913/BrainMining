#include "SMiningSpatialEditor.h"
#include "SMiningSectionCanvas.h"
#include "MiningSpatialVisualizer.h"
#include "MiningSpatialSafeEditing.h"
#include "MiningSpatialDirectEditing.h"
#include "MiningSpatialEnvelope.h"
#include "MiningTunnelGenerator.h"
#include "MiningLayoutRoom.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "FileHelpers.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

void SMiningSpatialEditor::Open()
{
	MiningSpatialOverview::SetEnabled(true);
	FGlobalTabmanager::Get()->TryInvokeTab(FName("MiningSpatialEditor"));
}

SMiningSpatialEditor::~SMiningSpatialEditor()
{
	if (MiningSpatialOverview::IsEnabled()) MiningSpatialOverview::SetEnabled(false);
}

void SMiningSpatialEditor::Focus(MiningSpatialOverview::EScope Scope, MiningSpatialOverview::EView View)
{
	AActor* Actor = GEditor ? GEditor->GetSelectedActors()->GetTop<AActor>() : nullptr;
	if (Actor != Selection) FollowSelection(Actor);
	if (MiningSpatialOverview::Focus(Scope, Actor, Region.Get(), OverviewStatus, View)) OverviewScope = Scope;
}

TSharedRef<SWidget> SMiningSpatialEditor::Overview()
{
	using namespace MiningSpatialOverview;
	return SNew(SExpandableArea).InitiallyCollapsed(true).AreaTitle(FText::FromString(TEXT("场景总览与视角（展开设置）")))
		.BodyContent()[SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SWrapBox).UseAllottedSize(true)
				+ SWrapBox::Slot().Padding(0, 0, 8, 0)[SNew(SCheckBox)
					.IsEnabled_Lambda([] { return GEditor && !GEditor->PlayWorld; })
					.IsChecked_Lambda([] { return MiningSpatialOverview::IsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([](ECheckBoxState State) { MiningSpatialOverview::SetEnabled(State == ECheckBoxState::Checked); })
					[SNew(STextBlock).Text(FText::FromString(TEXT("全局轮廓")))]]
				+ SWrapBox::Slot().Padding(0, 0, 8, 0)[SNew(SCheckBox)
					.IsChecked_Lambda([] { return Options().bLabels ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([](ECheckBoxState State) { Options().bLabels = State == ECheckBoxState::Checked; Redraw(); })
					[SNew(STextBlock).Text(FText::FromString(TEXT("空间名称")))]]
				+ SWrapBox::Slot()[SNew(SCheckBox)
					.IsChecked_Lambda([] { return Options().bRockBounds ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([](ECheckBoxState State) { Options().bRockBounds = State == ECheckBoxState::Checked; Redraw(); })
					[SNew(STextBlock).Text(FText::FromString(TEXT("岩体总包围框（仅参考）")))]]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4, 2)[SNew(SWrapBox).UseAllottedSize(true)
				+ SWrapBox::Slot()[Button(TEXT("总览全图"), [this] { Focus(EScope::Map); })]
				+ SWrapBox::Slot()[Button(TEXT("聚焦当前矿区"), [this] { Focus(EScope::Region); })]
				+ SWrapBox::Slot()[Button(TEXT("聚焦选中空间"), [this] { Focus(EScope::Selection); })]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4, 2)[SNew(SWrapBox).UseAllottedSize(true)
				+ SWrapBox::Slot()[Button(TEXT("透视"), [this] { Focus(OverviewScope, EView::Perspective); })]
				+ SWrapBox::Slot()[Button(TEXT("顶视 XY"), [this] { Focus(OverviewScope, EView::Top); })]
				+ SWrapBox::Slot()[Button(TEXT("正视 XZ"), [this] { Focus(OverviewScope, EView::Front); })]
				+ SWrapBox::Slot()[Button(TEXT("侧视 YZ"), [this] { Focus(OverviewScope, EView::Side); })]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4, 2)[SNew(STextBlock).AutoWrapText(true)
				.Text(FText::FromString(TEXT("青=主通道 · 碧绿=支线 · 蓝=主房间 · 绿=支线房间 · 紫=空洞 · 橙=堵块 · 灰=未启用 · 红=无效草稿")))]
			+ SVerticalBox::Slot().AutoHeight().Padding(4, 2)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return FText::FromString(OverviewStatus); })]
		];
}
TSharedRef<SWidget> SMiningSpatialEditor::Button(const FString& Name, TFunction<void()> Action)
{
	return SNew(SButton).Text(FText::FromString(Name)).IsEnabled_Lambda([] { return GEditor && !GEditor->PlayWorld; }).OnClicked_Lambda([Action] { Action(); return FReply::Handled(); });
}

TSharedRef<SWidget> SMiningSpatialEditor::Number(const FString& Name, int32 Property)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(Name))]
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(85)
		[SNew(SNumericEntryBox<float>).IsEnabled_Lambda([] { return FMiningSpatialSelection::Editable() && !FMiningSpatialSelection::Tunnel->Knots[FMiningSpatialSelection::Knot].bLocked; })
		.Value_Lambda([Property]() -> TOptional<float>
		{
			if (!FMiningSpatialSelection::Editable()) return TOptional<float>();
			const FMiningSpatialKnot& Knot = FMiningSpatialSelection::Tunnel->Knots[FMiningSpatialSelection::Knot];
			return Property == 0 ? Knot.Roll : Property == 1 ? Knot.SectionScale.X : Knot.SectionScale.Y;
		})
		.OnValueCommitted_Lambda([Property](float Value, ETextCommit::Type)
		{
			if (!FMiningSpatialSelection::Editable() || !FMath::IsFinite(Value)) return;
			FMiningSpatialKnot& Knot = FMiningSpatialSelection::Tunnel->Knots[FMiningSpatialSelection::Knot]; if (Knot.bLocked) return;
			FScopedTransaction Transaction(FText::FromString(TEXT("截面扭转/尺寸"))); FMiningSpatialSelection::Tunnel->Modify();
			if (Property == 0) Knot.Roll = FMath::Clamp(Value, -7200.f, 7200.f);
			else if (Property == 1) Knot.SectionScale.X = FMath::Clamp(Value, 0.05f, 20.f);
			else Knot.SectionScale.Y = FMath::Clamp(Value, 0.05f, 20.f);
			FMiningSpatialSelection::Tunnel->Changed();
		})]];
}

void SMiningSpatialEditor::Construct(const FArguments& Arguments)
{
	FDetailsViewArgs Settings; Settings.bAllowSearch = true; Settings.bHideSelectionTip = true; Settings.bUpdatesFromSelection = false; Settings.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Details = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(Settings);
	Details->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateLambda([] { return GEditor && !GEditor->PlayWorld; }));
	Canvas = SNew(SMiningSectionCanvas);
	ChildSlot[SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(STextBlock).Text(FText::FromString(TEXT("空间搭建 · 直接在场景里拖动")))]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)[DirectTools()]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)[SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot()[SNew(SComboButton).OnGetMenuContent(this, &SMiningSpatialEditor::Objects).ButtonContent()[SNew(STextBlock).Text(FText::FromString(TEXT("选择空间 / 矿区")))]]
			+ SWrapBox::Slot()[Button(TEXT("+通道"), [this] { Create(EMiningManualKind::Passage); })]
			+ SWrapBox::Slot()[Button(TEXT("+可编辑空洞"), [this] { Create(EMiningManualKind::Cavity); })]
			+ SWrapBox::Slot()[Button(TEXT("+支线房间"), [this] { Create(EMiningManualKind::SideRoom); })]
			+ SWrapBox::Slot()[Button(TEXT("选中房间连接主通道"), [this] { ConnectSideRoom(); })]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([] { return FText::FromString(MiningSpatialDirectEditing::Hint()); })]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)[SNew(STextBlock).AutoWrapText(true).ColorAndOpacity_Lambda([] { return MiningSpatialSafeEditing::Issue().Reason.IsEmpty() ? FSlateColor(FLinearColor(0.35f, 1, 0.6f)) : FSlateColor(FLinearColor(1, 0.65f, 0.12f)); }).Text_Lambda([] { return FText::FromString(MiningSpatialSafeEditing::Status()); })]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)[SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot()[Button(TEXT("总览全图"), [this] { Focus(MiningSpatialOverview::EScope::Map); })]
			+ SWrapBox::Slot()[Button(TEXT("聚焦选中"), [this] { Focus(MiningSpatialOverview::EScope::Selection); })]
			+ SWrapBox::Slot()[Button(TEXT("定位生成问题"), [] { MiningSpatialSafeEditing::FocusIssue(); })]
			+ SWrapBox::Slot()[Button(TEXT("检查并重建"), [this] { Rebuild(); })]
		]
		+ SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)
			+ SScrollBox::Slot().Padding(6, 2)[Overview()]
			+ SScrollBox::Slot().Padding(6, 2)[EnvelopeTools()]
			+ SScrollBox::Slot().Padding(6, 2)[SNew(SExpandableArea).InitiallyCollapsed(true).AreaTitle(FText::FromString(TEXT("高级编辑 / 精确参数 / 旧草稿修复"))).BodyContent()[Advanced()]]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot()[Button(TEXT("撤销"), [] { MiningSpatialDirectEditing::End(); GEditor->UndoTransaction(); })]
			+ SWrapBox::Slot()[Button(TEXT("重做"), [] { MiningSpatialDirectEditing::End(); GEditor->RedoTransaction(); })]
			+ SWrapBox::Slot()[Button(TEXT("保存当前关卡"), [] { MiningSpatialDirectEditing::End(); MiningSpatialSafeEditing::Save(); })]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([] { return FText::FromString(MiningSpatialSafeEditing::TerrainStatus()); })]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([] { return FText::FromString(MiningSpatialSafeEditing::SaveStatus()); })]
	];
}

TSharedRef<SWidget> SMiningSpatialEditor::DirectTools()
{
	using namespace MiningSpatialDirectEditing;
	const auto ToolButton = [this](const FString& Name, ETool Value) -> TSharedRef<SWidget>
	{
		return SNew(SButton).Text(FText::FromString(Name)).IsEnabled_Lambda([] { return GEditor && !GEditor->PlayWorld; })
			.ButtonColorAndOpacity_Lambda([Value] { return MiningSpatialOverview::Options().bDirectManipulation && Tool() == Value ? FLinearColor(0.08f, 0.5f, 0.7f) : FLinearColor(0.2f, 0.2f, 0.2f); })
			.OnClicked_Lambda([Value] { MiningSpatialOverview::Options().bDirectManipulation = true; MiningSpatialOverview::SetEnabled(true); SetTool(Value); return FReply::Handled(); });
	};
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot()[ToolButton(TEXT("拉通道走向"), ETool::Path)]
			+ SWrapBox::Slot()[ToolButton(TEXT("勾画边界"), ETool::Boundary)]
			+ SWrapBox::Slot()[ToolButton(TEXT("移动空间"), ETool::Move)]
			+ SWrapBox::Slot()[Button(TEXT("正对截面"), [] { FaceSection(); })]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5)[SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0, 0, 12, 0)[SNew(SCheckBox).IsChecked_Lambda([] { return AutoSmooth() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([](ECheckBoxState State) { AutoSmooth() = State == ECheckBoxState::Checked; })[SNew(STextBlock).Text(FText::FromString(TEXT("拖路径时自动拉顺邻近弯道")))]]
			+ SWrapBox::Slot()[SNew(SCheckBox).IsChecked_Lambda([] { return MiningSpatialOverview::Options().bSolid ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([](ECheckBoxState State) { MiningSpatialOverview::Options().bSolid = State == ECheckBoxState::Checked; MiningSpatialOverview::Redraw(); })[SNew(STextBlock).Text(FText::FromString(TEXT("空间灰模预览（不改游戏）")))]]
		];
}

TSharedRef<SWidget> SMiningSpatialEditor::EnvelopeTools()
{
	const auto Parameter = [this](const TCHAR* Label, bool bThickness)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(Label))]
			+ SHorizontalBox::Slot().FillWidth(1).Padding(6, 0)[SNew(SNumericEntryBox<float>)
				.IsEnabled_Lambda([this] { return Region.IsValid() && Region->bAutomaticBedrock && GEditor && !GEditor->PlayWorld; })
				.Value_Lambda([this, bThickness]() -> TOptional<float>
				{
					if (!Region.IsValid()) return {};
					float Value = bThickness ? Region->AutomaticBedrockThickness : Region->ExcavationMargin;
					for (const AMiningSpatialRegion* Item : MiningSpatial::SceneRegions(*Region)) Value = FMath::Max(Value, bThickness ? Item->AutomaticBedrockThickness : Item->ExcavationMargin);
					return Value;
				})
				.OnValueCommitted_Lambda([this, bThickness](float Value, ETextCommit::Type)
				{
					if (!Region.IsValid() || !FMath::IsFinite(Value) || !GEditor || GEditor->PlayWorld) return;
					MiningSpatialDirectEditing::End(); FScopedTransaction Transaction(FText::FromString(TEXT("调整全场景外围")));
					for (AMiningSpatialRegion* Item : MiningSpatial::SceneRegions(*Region))
					{
						Item->Modify();
						if (bThickness) Item->AutomaticBedrockThickness = FMath::Clamp(Value, 30.f, 500.f); else Item->ExcavationMargin = FMath::Max(10.f, Value);
					}
					MiningSpatialSafeEditing::Apply(Region.Get(), true);
				})];
	};
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[SNew(STextBlock).Text(FText::FromString(TEXT("全场景岩体与统一基岩（厘米）")))]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SCheckBox)
			.IsEnabled_Lambda([this] { return Region.IsValid() && GEditor && !GEditor->PlayWorld; })
			.IsChecked_Lambda([this] { return Region.IsValid() && Region->bAutomaticBedrock ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
			{
				if (!Region.IsValid() || !GEditor || GEditor->PlayWorld) return; MiningSpatialDirectEditing::End(); FScopedTransaction Transaction(FText::FromString(TEXT("切换场景统一外壳")));
				for (AMiningSpatialRegion* Item : MiningSpatial::SceneRegions(*Region)) { Item->Modify(); Item->bAutomaticBedrock = State == ECheckBoxState::Checked; }
				MiningSpatialSafeEditing::Apply(Region.Get(), true);
			})[SNew(STextBlock).Text(FText::FromString(TEXT("整个场景共用基岩外壳（房间之间可挖）")))]]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 3)[Parameter(TEXT("场景外扩余量"), false)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 3)[Parameter(TEXT("基岩厚度"), true)]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SCheckBox)
			.IsEnabled_Lambda([this] { return Region.IsValid() && GEditor && !GEditor->PlayWorld; })
			.IsChecked_Lambda([this] { return Region.IsValid() && Region->bFreeEditing ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
			{
				if (!Region.IsValid() || !GEditor || GEditor->PlayWorld) return; MiningSpatialDirectEditing::End(); FScopedTransaction Transaction(FText::FromString(TEXT("切换自由草稿"))); Region->Modify();
				Region->bFreeEditing = State == ECheckBoxState::Checked; MiningSpatialSafeEditing::Apply(Region.Get(), true);
			})[SNew(STextBlock).Text(FText::FromString(TEXT("自由编辑：生成失败不回退我的造型")))]]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("一整片可挖岩体，只有场景最外侧有基岩；房间和通道是内部空洞，不单独包壳。边界按全场景总范围自动生成，以上参数作用于整个场景。关闭灰模预览查看实体。原分块预算不变。")))];
}

TSharedRef<SWidget> SMiningSpatialEditor::Advanced()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)[SNew(SCheckBox)
			.IsChecked_Lambda([] { return !MiningSpatialOverview::Options().bDirectManipulation ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([](ECheckBoxState State) { MiningSpatialDirectEditing::Reset(); MiningSpatialOverview::Options().bDirectManipulation = State != ECheckBoxState::Checked; FMiningSpatialSelection::Activate(); MiningSpatialOverview::Redraw(); })
			[SNew(STextBlock).Text(FText::FromString(TEXT("显示原切线操作柄（高级模式）")))]]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)[SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot()[Button(TEXT("上一节点"), [] { if (FMiningSpatialSelection::Editable()) FMiningSpatialSelection::Select(FMiningSpatialSelection::Tunnel.Get(), FMiningSpatialSelection::Knot - 1); })]
			+ SWrapBox::Slot()[Button(TEXT("下一节点"), [] { if (FMiningSpatialSelection::Editable()) FMiningSpatialSelection::Select(FMiningSpatialSelection::Tunnel.Get(), FMiningSpatialSelection::Knot + 1); })]
			+ SWrapBox::Slot()[Button(TEXT("插入节点"), [] { FMiningSpatialSelection::Insert(); })]
			+ SWrapBox::Slot()[Button(TEXT("删除节点"), [] { FMiningSpatialSelection::Delete(); })]
			+ SWrapBox::Slot()[Button(TEXT("锁定/解锁节点"), []
			{
				if (!FMiningSpatialSelection::Editable()) return; FScopedTransaction Transaction(FText::FromString(TEXT("锁定三维节点"))); FMiningSpatialSelection::Tunnel->Modify();
				FMiningSpatialKnot& Knot = FMiningSpatialSelection::Tunnel->Knots[FMiningSpatialSelection::Knot]; Knot.bLocked = !Knot.bLocked;
			})]
			+ SWrapBox::Slot()[Number(TEXT("扭转° "), 0)] + SWrapBox::Slot()[Number(TEXT("宽× "), 1)] + SWrapBox::Slot()[Number(TEXT("高× "), 2)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([]
		{
			if (!FMiningSpatialSelection::Editable()) return FText::FromString(TEXT("在场景大纲或空间菜单选择空间。"));
			if (FMiningSpatialSelection::Tunnel->Kind == EMiningManualKind::SideRoom) return FText::FromString(TEXT("当前编辑支线房间整体：W移动、E旋转，房间尺寸在右侧设置。"));
			return FText::FromString(FString::Printf(TEXT("当前：%s · 节点 %d/%d · %s"), *FMiningSpatialSelection::Tunnel->GetActorNameOrLabel(), FMiningSpatialSelection::Knot + 1, FMiningSpatialSelection::Tunnel->Knots.Num(), FMiningSpatialSelection::Part == 1 ? TEXT("入切线柄") : FMiningSpatialSelection::Part == 2 ? TEXT("出切线柄") : TEXT("节点位置")));
		})]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("拖编号节点上的箭头改变路径，拖橙色小点调整弯曲。黄色角标只是问题范围，不是可拖动盒子。")))]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)[SNew(STextBlock).AutoWrapText(true).ColorAndOpacity_Lambda([] { return MiningSpatialSafeEditing::Issue().Reason.IsEmpty() ? FSlateColor(FLinearColor(0.35f, 1, 0.6f)) : FSlateColor(FLinearColor(1, 0.65f, 0.12f)); }).Text_Lambda([] { return FText::FromString(MiningSpatialSafeEditing::Status()); })]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)[SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FLinearColor(1, 0.7f, 0.25f)).Text_Lambda([]
		{
			const auto& Issue = MiningSpatialSafeEditing::Issue();
			return FText::FromString(Issue.Reason.IsEmpty() ? FString() : (Issue.Actor.IsValid() ? Issue.Actor->GetActorNameOrLabel() + TEXT("：") : FString()) + Issue.Reason);
		})]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)[SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot()[Button(TEXT("定位限制 / 问题"), [] { MiningSpatialSafeEditing::FocusIssue(); })]
			+ SWrapBox::Slot()[Button(TEXT("平滑问题弯道"), [] { MiningSpatialSafeEditing::SmoothIssue(); })]
			+ SWrapBox::Slot()[Button(TEXT("修复旧草稿"), [] { MiningSpatialSafeEditing::BeginRepair(); })]
			+ SWrapBox::Slot()[Button(TEXT("取消修复"), [] { MiningSpatialSafeEditing::CancelRepair(); })]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)[SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FLinearColor(1, 0.65f, 0.12f)).Visibility_Lambda([] { return MiningSpatialSafeEditing::IsRepairing() ? EVisibility::Visible : EVisibility::Collapsed; }).Text(FText::FromString(TEXT("旧草稿修复模式：允许暂时无效的轮廓；地形未生效，不能开玩。完整合法后自动退出。")))]
		+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(400)[SNew(SSplitter)
			+ SSplitter::Slot().Value(0.55f)[SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(1)[Canvas.ToSharedRef()]
				+ SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SWrapBox).UseAllottedSize(true)
					+ SWrapBox::Slot()[Button(TEXT("适配截面"), [this] { Canvas->Fit(); })]
					+ SWrapBox::Slot()[Button(TEXT("角点"), [this] { Canvas->Mode(EMiningPenMode::Corner); })]
					+ SWrapBox::Slot()[Button(TEXT("平滑点"), [this] { Canvas->Mode(EMiningPenMode::Smooth); })]
					+ SWrapBox::Slot()[Button(TEXT("独立手柄"), [this] { Canvas->Mode(EMiningPenMode::Independent); })]
					+ SWrapBox::Slot()[Button(TEXT("锁定点"), [this] { Canvas->Lock(); })]
					+ SWrapBox::Slot()[Button(TEXT("重置矩形截面"), [this] { Preset(false); })]
					+ SWrapBox::Slot()[Button(TEXT("重置椭圆截面"), [this] { Preset(true); })]
				]
			]
			+ SSplitter::Slot().Value(0.45f)[Details.ToSharedRef()]
		]]
		+ SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot()[Button(TEXT("检查并重建"), [this] { Rebuild(); })]
			+ SWrapBox::Slot()[Button(TEXT("显示岩石"), [this] { if (Region.IsValid()) for (TActorIterator<AMiningTunnelGenerator> Item(Region->GetWorld()); Item; ++Item) Item->ShowGameGeometry(); })]
			+ SWrapBox::Slot()[Button(TEXT("隐藏岩石看路径"), [this] { if (Region.IsValid()) for (TActorIterator<AMiningTunnelGenerator> Item(Region->GetWorld()); Item; ++Item) Item->ShowLayout(); })]
			+ SWrapBox::Slot()[Button(TEXT("+可挖堵块"), [this] { Create(EMiningManualKind::RockBlock); })]
			+ SWrapBox::Slot()[Button(TEXT("新建三维矿区"), [this]
			{
				FScopedTransaction Transaction(FText::FromString(TEXT("新建三维矿区"))); UWorld* World = GEditor->GetEditorWorldContext().World();
				FActorSpawnParameters Parameters; Parameters.ObjectFlags |= RF_Transactional;
				AMiningSpatialRegion* Created = World->SpawnActor<AMiningSpatialRegion>(Parameters); Created->SetActorLabel(TEXT("三维矿区_设置房间与岩石范围")); Select(Created);
			})]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([] { return FText::FromString(MiningSpatialSafeEditing::TerrainStatus()); })]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([] { return FText::FromString(MiningSpatialSafeEditing::SaveStatus()); })]
	;
}

TSharedRef<SWidget> SMiningSpatialEditor::Objects()
{
	FMenuBuilder Menu(true, nullptr);
	if (GEditor && !GEditor->PlayWorld)
		for (TActorIterator<AActor> Item(GEditor->GetEditorWorldContext().World()); Item; ++Item)
			if (Item->IsA<AMiningSpatialTunnel>() || Item->IsA<AMiningSpatialRegion>() || Item->IsA<AMiningLayoutRoom>())
			{
				TWeakObjectPtr<AActor> Actor(*Item);
				Menu.AddMenuEntry(FText::FromString(Item->GetActorNameOrLabel()), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([this, Actor] { if (Actor.IsValid()) Select(Actor.Get()); })));
			}
	return Menu.MakeWidget();
}

void SMiningSpatialEditor::Select(AActor* Actor)
{
	FollowSelection(Actor);
	GEditor->SelectNone(false, true); GEditor->SelectActor(Actor, true, true); FMiningSpatialSelection::Activate(); Canvas->Fit();
}

void SMiningSpatialEditor::FollowSelection(AActor* Actor)
{
	Selection = Actor;
	if (Region.IsValid() && (!GEditor || Region->GetWorld() != GEditor->GetEditorWorldContext().World())) Region.Reset();
	const bool bSupported = IsValid(Actor) && (Actor->IsA<AMiningSpatialTunnel>() || Actor->IsA<AMiningSpatialRegion>() || Actor->IsA<AMiningLayoutRoom>());
	Details->SetObject(bSupported ? Actor : nullptr);
	if (AMiningSpatialTunnel* Tunnel = Cast<AMiningSpatialTunnel>(Actor)) { Region = Tunnel->Region; if (FMiningSpatialSelection::Tunnel != Tunnel) FMiningSpatialSelection::Select(Tunnel); }
	else
	{
		FMiningSpatialSelection::Select(nullptr);
		if (AMiningSpatialRegion* SelectedRegion = Cast<AMiningSpatialRegion>(Actor)) Region = SelectedRegion;
		else if (AMiningLayoutRoom* Room = Cast<AMiningLayoutRoom>(Actor))
		{
			if (!Region.IsValid() || (Region->FromRoom != Room && Region->ToRoom != Room))
			{
				Region.Reset();
				for (TActorIterator<AMiningSpatialRegion> Candidate(Room->GetWorld()); Candidate; ++Candidate)
					if (Candidate->FromRoom == Room || Candidate->ToRoom == Room) { Region = *Candidate; break; }
			}
		}
	}
	Canvas->Fit();
}

void SMiningSpatialEditor::Tick(const FGeometry& Geometry, double Time, float Delta)
{
	SCompoundWidget::Tick(Geometry, Time, Delta);
	if (!GEditor || GEditor->PlayWorld) return;
	MiningSpatialSafeEditing::Observe(GEditor->GetEditorWorldContext().World());
	AActor* Actor = GEditor->GetSelectedActors()->GetTop<AActor>();
	if (Actor != Selection || (!Actor && FMiningSpatialSelection::Tunnel.IsValid())) FollowSelection(Actor);
	if (Actor && Actor == FMiningSpatialSelection::Tunnel) FMiningSpatialSelection::Activate();
	if (Region.IsValid() && Region->GetWorld() != GEditor->GetEditorWorldContext().World()) Region.Reset();
	if (bInitialFrame && GCurrentLevelEditingViewportClient) { bInitialFrame = false; Focus(MiningSpatialOverview::EScope::Map); }
	if (LastTunnel != FMiningSpatialSelection::Tunnel || LastKnot != FMiningSpatialSelection::Knot) { LastTunnel = FMiningSpatialSelection::Tunnel; LastKnot = FMiningSpatialSelection::Knot; Canvas->Fit(); }
}

void SMiningSpatialEditor::Preset(bool bEllipse)
{
	if (!FMiningSpatialSelection::Editable()) return;
	FMiningSpatialKnot& Knot = FMiningSpatialSelection::Tunnel->Knots[FMiningSpatialSelection::Knot]; if (Knot.bLocked) return;
	FScopedTransaction Transaction(FText::FromString(TEXT("替换当前截面预设"))); FMiningSpatialSelection::Tunnel->Modify();
	Knot.Profile = bEllipse ? MiningSpatial::Ellipse() : MiningSpatial::Rectangle(); FMiningSpatialSelection::Tunnel->Changed(); Canvas->Fit();
}

void SMiningSpatialEditor::Create(EMiningManualKind Kind)
{
	if (!Region.IsValid() || !GEditor || GEditor->PlayWorld) { OverviewStatus = TEXT("请先在场景里选中一个通道或房间，确定新空间属于哪个矿区。"); return; }
	FScopedTransaction Transaction(FText::FromString(TEXT("新建三维空间")));
	FActorSpawnParameters Parameters; Parameters.ObjectFlags |= RF_Transactional;
	AMiningSpatialTunnel* Created = Region->GetWorld()->SpawnActor<AMiningSpatialTunnel>(Parameters);
	Created->Region = Region.Get(); Created->Kind = Kind;
	FVector Position = Region->GetActorLocation();
	if (FMiningSpatialSelection::Editable())
	{
		TArray<FMiningSpatialKnot> Resolved; FString Error;
		if (FMiningSpatialSelection::Tunnel->ResolveKnots(Resolved, Error)) Position = Resolved[FMiningSpatialSelection::Knot].Position;
	}
	Created->SetActorLocation(Position);
	Created->SetActorLabel(Kind == EMiningManualKind::Passage ? TEXT("三维通道_编辑节点与入口") : Kind == EMiningManualKind::Cavity ? TEXT("三维空洞_独立扫掠轮廓") : Kind == EMiningManualKind::SideRoom ? TEXT("三维支线房间_中心锚点") : TEXT("三维可挖堵块"));
	Created->SetFolderPath(TEXT("SpatialMine/Authoring"));
	if (Kind == EMiningManualKind::Passage && !Region->MainTunnel)
	{
		Region->Modify(); Region->MainTunnel = Created;
		Created->StartPortal.Room = Region->FromRoom; Created->EndPortal.Room = Region->ToRoom;
		if (Region->FromRoom && Region->ToRoom) Created->FitPortals();
	}
	if (Kind == EMiningManualKind::Cavity || Kind == EMiningManualKind::RockBlock)
	{
		Created->Knots[0].Position = FVector(-150, 0, 0); Created->Knots[0].Leave = FVector(100, 0, 0);
		Created->Knots[1].Position = FVector(150, 0, 0); Created->Knots[1].Arrive = FVector(-100, 0, 0);
		for (FMiningSpatialKnot& Knot : Created->Knots) Knot.Profile = MiningSpatial::Ellipse(800, 800);
	}
	if (!MiningSpatialSafeEditing::Add(Created)) { if (Region->MainTunnel == Created) Region->MainTunnel = nullptr; Created->Destroy(); Transaction.Cancel(); return; }
	Select(Created);
	if (Kind == EMiningManualKind::Cavity || Kind == EMiningManualKind::SideRoom) MiningSpatialDirectEditing::SetTool(MiningSpatialDirectEditing::ETool::Move);
}

void SMiningSpatialEditor::Rebuild()
{
	if (!Region.IsValid() || !GEditor || GEditor->PlayWorld) return;
	MiningSpatialSafeEditing::Apply(Region.Get(), true);
}

void SMiningSpatialEditor::ConnectSideRoom()
{
	AMiningSpatialTunnel* Room = Cast<AMiningSpatialTunnel>(Selection.Get());
	if (!Room || Room->Kind != EMiningManualKind::SideRoom || !Region.IsValid() || !IsValid(Region->MainTunnel) || !GEditor || GEditor->PlayWorld) return;
	TArray<FMiningSpatialKnot> Resolved; TArray<MiningSpatial::FRing> Rings; FString Error;
	if (!Region->MainTunnel->ResolveKnots(Resolved, Error) || !MiningSpatial::Sample(Resolved, Region->MainTunnel->GetActorTransform().TransformVectorNoScale(Region->MainTunnel->InitialUp), Region->CurveTolerance, Rings, Error, nullptr, Region->bAutomaticBedrock)) return;
	FVector Start = Rings[0].Center;
	for (const MiningSpatial::FRing& Ring : Rings) if (FVector::DistSquared(Ring.Center, Room->GetActorLocation()) < FVector::DistSquared(Start, Room->GetActorLocation())) Start = Ring.Center;
	const FVector TowardStart = Room->GetActorTransform().InverseTransformPosition(Start);
	const FVector Absolute = TowardStart.GetAbs();
	const int32 Axis = Absolute.X >= Absolute.Y && Absolute.X >= Absolute.Z ? 0 : Absolute.Y >= Absolute.Z ? 1 : 2;
	FScopedTransaction Transaction(FText::FromString(TEXT("主通道与支线房间直连")));
	FActorSpawnParameters Parameters; Parameters.ObjectFlags |= RF_Transactional;
	AMiningSpatialTunnel* Created = Region->GetWorld()->SpawnActor<AMiningSpatialTunnel>(Parameters);
	Created->Region = Region.Get(); Created->EndPortal.Room = Room;
	Created->EndPortal.Wall = static_cast<EMiningSpatialWall>(Axis * 2 + (TowardStart[Axis] < 0 ? 1 : 0));
	FTransform Transform; FVector HalfSize, Finish, Normal;
	if (!Created->ResolvePortal(Created->EndPortal, Transform, HalfSize, Finish, Normal, Error)) { MiningSpatialSafeEditing::Add(Created); Created->Destroy(); Transaction.Cancel(); return; }
	const double Length = FVector::Distance(Start, Finish);
	Created->Knots[0].Position = Start; Created->Knots[0].Leave = (Finish - Start) / 3;
	Created->Knots[0].Arrive = -Created->Knots[0].Leave;
	Created->Knots[1].Position = Finish; Created->Knots[1].Arrive = Normal * Length / 3; Created->Knots[1].Leave = -Created->Knots[1].Arrive;
	for (FMiningSpatialKnot& Knot : Created->Knots) Knot.Profile = MiningSpatial::Rectangle(300, 300);
	Created->SetActorLabel(TEXT("三维支线_直连_可继续编辑")); Created->SetFolderPath(TEXT("SpatialMine/Authoring"));
	if (!MiningSpatialSafeEditing::Add(Created)) { Created->Destroy(); Transaction.Cancel(); return; }
	Select(Created);
}
