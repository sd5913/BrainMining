#include "SMiningDesignerPanel.h"
#include "MiningDesignerCatalog.h"
#include "MiningTunnelGenerator.h"
#include "MiningLayoutRoom.h"
#include "MiningHollowRegion.h"
#include "MiningHollowChamber.h"
#include "MiningTunnelRoute.h"
#include "MiningTunnelShapeSection.h"
#include "MiningLayoutSpace.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Selection.h"
#include "FileHelpers.h"
#include "ScopedTransaction.h"
#include "Components/SplineComponent.h"
#include "Components/SceneComponent.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

namespace
{
FText Text(const FString& Value) { return FText::FromString(Value); }

void TranslateHandle(const TSharedPtr<IPropertyHandle>& Handle, EMiningDesignerSection Section)
{
	if (!Handle || !Handle->IsValidHandle() || !Handle->GetProperty()) return;
	const FName Name = Handle->GetProperty()->GetFName();
	Handle->SetPropertyDisplayName(Text(MiningDesigner::Caption(Name)));
	Handle->SetToolTipText(Text(MiningDesigner::Help(Name) + TEXT("\n") + MiningDesigner::Effect(Section)));
	uint32 Count = 0;
	Handle->GetNumChildren(Count);
	for (uint32 Index = 0; Index < Count; ++Index) TranslateHandle(Handle->GetChildHandle(Index), Section);
}

class FMiningDesignerDetails : public IDetailCustomization
{
public:
	explicit FMiningDesignerDetails(EMiningDesignerSection InSection) : Section(InSection) {}
	virtual void CustomizeDetails(IDetailLayoutBuilder& Builder) override
	{
		TArray<TWeakObjectPtr<UObject>> Objects;
		Builder.GetObjectsBeingCustomized(Objects);
		if (Objects.Num() != 1 || !MiningDesigner::Family(Objects[0].Get())) return;
		UObject* Object = Objects[0].Get();
		TArray<FName> Categories;
		Builder.GetCategoryNames(Categories);
		for (FName Name : Categories) Builder.HideCategory(Name);
		Builder.HideCategory("TransformCommon");
		Builder.HideCategory("Selected Points");
		for (TFieldIterator<FObjectPropertyBase> Component(Object->GetClass()); Component; ++Component)
		{
			if (Component->PropertyClass->IsChildOf<UActorComponent>()) Builder.HideCategory(Component->GetFName());
		}
		if (Section == EMiningDesignerSection::Terrain && !Object->IsA<AMiningTunnelGenerator>() && !Object->IsA<AMiningTunnelRoute>())
		{
			AActor* Actor = CastChecked<AActor>(Object);
			if (Actor->GetRootComponent())
			{
				IDetailCategoryBuilder& Transform = Builder.EditCategory("DesignerTransform", Text(TEXT("位置与缩放 · 也可在视口拖动")), ECategoryPriority::Important);
				const TArray<UObject*> Components = {Actor->GetRootComponent()};
				if (IDetailPropertyRow* Row = Transform.AddExternalObjectProperty(Components, "RelativeLocation")) Row->DisplayName(Text(TEXT("位置（相对父级，厘米）"))).ToolTip(Text(TEXT("未挂接的对象即世界坐标。房间和洞室的位置为地板中心。")));
				if (!Object->IsA<AMiningHollowRegion>())
				{
					if (IDetailPropertyRow* Row = Transform.AddExternalObjectProperty(Components, "RelativeScale3D")) Row->DisplayName(Text(TEXT("尺寸缩放（保持正值）"))).ToolTip(Text(TEXT("当前形状不支持旋转。负缩放或超出生成限制时保留旧地形。")));
				}
			}
		}
		for (FName Name : MiningDesigner::Fields(Object, Section))
		{
			TSharedRef<IPropertyHandle> Handle = Builder.GetProperty(Name, MiningDesigner::Family(Object));
			if (!Handle->IsValidHandle()) continue;
			TranslateHandle(Handle, Section);
			const bool bSettings = Name.ToString().EndsWith(TEXT("Settings"));
			IDetailCategoryBuilder& Category = Builder.EditCategory(bSettings ? FName(*(TEXT("Designer_") + Name.ToString())) : FName("DesignerParameters"), Text(bSettings ? MiningDesigner::Caption(Name) : TEXT("参数")), ECategoryPriority::Default);
			if (bSettings)
			{
				uint32 Count = 0;
				Handle->GetNumChildren(Count);
				for (uint32 Index = 0; Index < Count; ++Index) Category.AddProperty(Handle->GetChildHandle(Index));
			}
			else Category.AddProperty(Handle).ShouldAutoExpand(Name == "Plugs");
		}
	}
private:
	EMiningDesignerSection Section;
};
}

void SMiningDesignerPanel::Construct(const FArguments& Arguments)
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs Args;
	Args.bAllowSearch = true;
	Args.bHideSelectionTip = true;
	Args.bLockable = false;
	Args.bUpdatesFromSelection = false;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Details = PropertyEditor.CreateDetailView(Args);
	TWeakPtr<SMiningDesignerPanel> WeakThis = SharedThis(this);
	Details->GetGenericLayoutDetailsDelegate() = FOnGetDetailCustomizationInstance::CreateLambda([WeakThis]()
	{
		const auto Panel = WeakThis.Pin();
		return MakeShared<FMiningDesignerDetails>(Panel ? Panel->Section : EMiningDesignerSection::Gameplay);
	});
	Details->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateSP(this, &SMiningDesignerPanel::Editable));
	Details->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateLambda([WeakThis](const FPropertyAndParent&)
	{
		const auto Panel = WeakThis.Pin();
		return !Panel || !Panel->Editable();
	}));
	Details->OnFinishedChangingProperties().AddLambda([WeakThis](const FPropertyChangedEvent&)
	{
		if (const auto Panel = WeakThis.Pin()) Panel->Message = TEXT("参数已修改，尚未保存关卡。可撤销；请留意下方生成状态。");
	});
	TSharedRef<SWrapBox> Tabs = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(5, 5));
	for (EMiningDesignerSection Category : {EMiningDesignerSection::Gameplay, EMiningDesignerSection::Mining, EMiningDesignerSection::Terrain, EMiningDesignerSection::Visual, EMiningDesignerSection::Advanced})
	{
		Tabs->AddSlot()[Button(MiningDesigner::SectionName(Category), [this, Category] { return SwitchSection(Category); }, [] { return true; })];
	}
	TSharedRef<SWrapBox> Actions = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4, 4));
	Actions->AddSlot()[Button(TEXT("定位对象"), [this] { return FocusTarget(); }, [this] { return UMiningDesignerLibrary::CanEdit(Target.Get()); })];
	Actions->AddSlot()[Button(TEXT("编辑路径点"), [this] { return EditPath(); })];
	Actions->AddSlot()[Button(TEXT("直线支路控制点"), [this] { return StraightBranch(); }, [this] { return Editable() && Target.IsValid() && Target->IsA<AMiningHollowChamber>(); })];
	Actions->AddSlot()[Button(TEXT("检查并重新生成"), [this] { return Rebuild(); }, [this] { return Editable() && Generator(); })];
	Actions->AddSlot()[Button(TEXT("撤销"), [this] { GEditor->UndoTransaction(); Details->ForceRefresh(); return FReply::Handled(); }, [] { return GEditor && !GEditor->IsPlaySessionInProgress(); })];
	Actions->AddSlot()[Button(TEXT("重做"), [this] { GEditor->RedoTransaction(); Details->ForceRefresh(); return FReply::Handled(); }, [] { return GEditor && !GEditor->IsPlaySessionInProgress(); })];
	Actions->AddSlot()[Button(TEXT("保存当前关卡"), [this] { return FinishOperation(FEditorFileUtils::SaveCurrentLevel(), TEXT("未保存，请检查文件权限或保存对话框。"), TEXT("当前关卡已保存。预设需用下方按钮单独保存。")); }, [] { return GEditor && !GEditor->IsPlaySessionInProgress(); })];
	TSharedRef<SWrapBox> Presets = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4, 4));
	Presets->AddSlot()[SNew(SBox).WidthOverride(190)[SAssignNew(PresetName, SEditableTextBox).HintText(Text(TEXT("输入预设名称"))).IsEnabled_Lambda([this] { return Editable(); })]];
	Presets->AddSlot()[SNew(SBox).WidthOverride(210)[SAssignNew(PresetPicker, SComboBox<TSharedPtr<FString>>).OptionsSource(&PresetOptions)
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> Name) { return SNew(STextBlock).Text(Text(*Name)); })
		.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Name, ESelectInfo::Type) { if (Name) PresetName->SetText(Text(*Name)); })
		[SNew(STextBlock).Text(Text(TEXT("选择同类型／同分类预设")))]]];
	Presets->AddSlot()[Button(TEXT("保存预设"), [this] { return SavePreset(); })];
	Presets->AddSlot()[Button(TEXT("应用预设"), [this] { return LoadPreset(); })];
	Presets->AddSlot()[Button(TEXT("复制参数"), [this] { return CopyPreset(); })];
	Presets->AddSlot()[Button(TEXT("粘贴参数"), [this] { return PastePreset(); })];
	Presets->AddSlot()[Button(TEXT("恢复本页默认"), [this] { return ResetDefaults(); })];
	ChildSlot
	[
		SNew(SBorder).Padding(14)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[SNew(STextBlock).Text(Text(TEXT("矿业策划台"))).Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]
			{
				if (!GEditor || GEditor->IsPlaySessionInProgress()) return Text(TEXT("试玩中：编辑已锁定。停止试玩后再调参，避免修改丢失或影响正在挖掘的地形。"));
				const FString Map = GEditor->GetEditorWorldContext().World() ? GEditor->GetEditorWorldContext().World()->GetName() : TEXT("无关卡");
				return Text(TEXT("当前关卡：") + Map + TEXT("   ｜   当前分类：") + MiningDesigner::SectionName(Section));
			})]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[Tabs]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
			[SAssignNew(ActorPicker, SComboBox<TSharedPtr<FDesignerActorItem>>).OptionsSource(&ActorOptions)
				.OnGenerateWidget_Lambda([](TSharedPtr<FDesignerActorItem> Item) { return SNew(STextBlock).Text(Text(Item->Actor.IsValid() ? Item->Actor->GetActorLabel() : TEXT("对象已删除"))); })
				.OnSelectionChanged_Lambda([this](TSharedPtr<FDesignerActorItem> Item, ESelectInfo::Type) { if (!bUpdatingPicker && Item) SelectTarget(Item->Actor.Get(), true); })
				[SNew(STextBlock).Text_Lambda([this] { return Text(Target.IsValid() ? Target->GetActorLabel() : TEXT("请选择生成器或场景对象；也可在视口点击对象")); })]]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return Text(MiningDesigner::Effect(Section)); }).ColorAndOpacity(FLinearColor(0.42f, 0.82f, 1.f))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[Actions]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
			[SNew(SCheckBox).Visibility_Lambda([this] { return Section == EMiningDesignerSection::Advanced ? EVisibility::Visible : EVisibility::Collapsed; })
				.IsChecked_Lambda([this] { return bAdvancedUnlocked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bAdvancedUnlocked = State == ECheckBoxState::Checked; RefreshDetails(); })
				[SNew(STextBlock).Text(Text(TEXT("解锁高级参数（预算上限仍为30000）")))]]
			+ SVerticalBox::Slot().FillHeight(1).MinHeight(160)[Details.ToSharedRef()]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 5)[SNew(STextBlock).Text(Text(TEXT("预设仅保存当前分类的数值与开关，不复制位置、所属对象、房间顺序或路径点。"))).AutoWrapText(true)]
			+ SVerticalBox::Slot().AutoHeight()[Presets]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return Text(Message); }).ColorAndOpacity(FLinearColor(1.f, 0.77f, 0.33f))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)[SNew(SBox).MaxDesiredHeight(85)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]
			{
				FString Result = MiningDesigner::Status(Target.Get());
				if (AMiningTunnelGenerator* Layout = Generator(); Layout && Layout != Target.Get()) Result += TEXT("\n矿区生成：") + Layout->LastBuildStatus;
				int32 Count = 0;
				UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
				if (World) for (TActorIterator<AMiningTunnelGenerator> Actor(World); Actor; ++Actor) ++Count;
				if (Count > 1) Result = TEXT("警告：多个生成器会导致角色参数来源不唯一，请检查关卡配置。\n") + Result;
				return Text(Result);
			})]]
		]
	];
	RefreshActors();
	AActor* Selected = GEditor ? Cast<AActor>(GEditor->GetSelectedActors()->GetTop(AActor::StaticClass())) : nullptr;
	if (MiningDesigner::Family(Selected)) SelectTarget(Selected, false);
	else if (ActorOptions.Num() == 1) SelectTarget(ActorOptions[0]->Actor.Get(), false);
}

TSharedRef<SWidget> SMiningDesignerPanel::Button(const FString& Label, TFunction<FReply()> Action, TFunction<bool()> Enabled)
{
	return SNew(SButton).Text(Text(Label)).OnClicked_Lambda(MoveTemp(Action))
		.IsEnabled_Lambda([this, Enabled = MoveTemp(Enabled)] { return Enabled ? Enabled() : Editable(); });
}

bool SMiningDesignerPanel::Editable() const
{
	return UMiningDesignerLibrary::CanEdit(Target.Get()) && (Section != EMiningDesignerSection::Advanced || bAdvancedUnlocked);
}

void SMiningDesignerPanel::Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime)
{
	SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
	if (!GEditor || CurrentTime < NextRefresh) return;
	NextRefresh = CurrentTime + 0.3;
	const bool bPlaying = GEditor->IsPlaySessionInProgress();
	if (bPlaying)
	{
		if (!bWasPlaying) { bWasPlaying = true; RefreshDetails(); }
		return;
	}
	RefreshActors();
	if (bWasPlaying) { bWasPlaying = false; RefreshDetails(); }
	AActor* Selected = Cast<AActor>(GEditor->GetSelectedActors()->GetTop(AActor::StaticClass()));
	if (Selected != LastSelection.Get())
	{
		LastSelection = Selected;
		if (MiningDesigner::Family(Selected)) SelectTarget(Selected, false);
	}
}

void SMiningDesignerPanel::RefreshActors()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) { Target.Reset(); if (Details) Details->SetObject(nullptr); return; }
	TArray<TWeakObjectPtr<AActor>> Actors;
	for (TActorIterator<AActor> Actor(World); Actor; ++Actor)
	{
		if (MiningDesigner::Family(*Actor) && (Section == EMiningDesignerSection::Terrain || Actor->IsA<AMiningTunnelGenerator>())) Actors.Add(*Actor);
	}
	if (Actors != KnownActors)
	{
		KnownActors = Actors;
		ActorOptions.Reset();
		for (const auto& Actor : Actors) { auto Item = MakeShared<FDesignerActorItem>(); Item->Actor = Actor; ActorOptions.Add(Item); }
		ActorOptions.Sort([](const auto& Left, const auto& Right) { return Left->Actor->GetActorLabel() < Right->Actor->GetActorLabel(); });
		ActorPicker->RefreshOptions();
	}
	if (!Target.IsValid() || Target->GetWorld() != World)
	{
		Target.Reset();
		Details->SetObject(nullptr);
		if (ActorOptions.Num() == 1) SelectTarget(ActorOptions[0]->Actor.Get(), false);
	}
}

void SMiningDesignerPanel::SelectTarget(AActor* Actor, bool bSelectInLevel)
{
	if (!Actor || !MiningDesigner::Family(Actor) || !GEditor || GEditor->IsPlaySessionInProgress()) return;
	if (bSelectInLevel)
	{
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(Actor, true, true);
	}
	if (Target == Actor && Details->GetSelectedObjects().Num() > 0) return;
	Target = Actor;
	LastSelection = Actor;
	if (!Actor->IsA<AMiningTunnelGenerator>()) Section = EMiningDesignerSection::Terrain;
	Message.Reset();
	RefreshActors();
	TGuardValue<bool> PickerGuard(bUpdatingPicker, true);
	for (const auto& Item : ActorOptions) if (Item->Actor == Actor) { ActorPicker->SetSelectedItem(Item); break; }
	RefreshDetails();
	RefreshPresets();
}

void SMiningDesignerPanel::RefreshDetails()
{
	Details->SetObject(Target.Get(), true);
}

void SMiningDesignerPanel::RefreshPresets()
{
	PresetOptions.Reset();
	for (const FString& Name : UMiningDesignerLibrary::ListPresets(Target.Get(), Section)) PresetOptions.Add(MakeShared<FString>(Name));
	PresetPicker->RefreshOptions();
}

AMiningTunnelGenerator* SMiningDesignerPanel::Generator() const
{
	if (auto* Layout = Cast<AMiningTunnelGenerator>(Target.Get())) return Layout;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return nullptr;
	const AMiningHollowRegion* Owner = Cast<AMiningHollowRegion>(Target.Get());
	if (auto* Chamber = Cast<AMiningHollowChamber>(Target.Get())) Owner = Chamber->Region;
	AMiningTunnelGenerator* Only = nullptr;
	int32 Count = 0;
	for (TActorIterator<AMiningTunnelGenerator> Layout(World); Layout; ++Layout)
	{
		if (Owner && Layout->HollowRegion == Owner) return *Layout;
		Only = *Layout;
		++Count;
	}
	return Count == 1 ? Only : nullptr;
}

FReply SMiningDesignerPanel::SwitchSection(EMiningDesignerSection NewSection)
{
	AMiningTunnelGenerator* Layout = Generator();
	Section = NewSection;
	bAdvancedUnlocked = false;
	if (Section != EMiningDesignerSection::Terrain) Target = Layout;
	KnownActors.Reset();
	RefreshActors();
	RefreshDetails();
	RefreshPresets();
	Message.Reset();
	return FReply::Handled();
}

FReply SMiningDesignerPanel::FinishOperation(bool bSuccess, const FString& Error, const FString& Success)
{
	Message = bSuccess ? Success : TEXT("未完成：") + Error;
	RefreshDetails();
	return FReply::Handled();
}

FReply SMiningDesignerPanel::Rebuild()
{
	AMiningTunnelGenerator* Layout = Generator();
	if (!Editable() || !Layout) return FReply::Handled();
	const bool bSuccess = Layout->RebuildTunnels();
	return FinishOperation(bSuccess, TEXT("生成失败，保留上次成功地形。可撤销参数，或检查下方原始诊断。\n") + Layout->LastBuildStatus, TEXT("地形生成成功；尚未保存关卡。"));
}

FReply SMiningDesignerPanel::CopyPreset()
{
	const FString Json = UMiningDesignerLibrary::CapturePreset(Target.Get(), Section);
	if (!Json.IsEmpty()) FPlatformApplicationMisc::ClipboardCopy(*Json);
	return FinishOperation(!Json.IsEmpty(), TEXT("当前没有可复制的参数。"), TEXT("已复制本页参数；位置、归属和路径点不会被复制。"));
}

FReply SMiningDesignerPanel::PastePreset()
{
	if (!Editable()) return FReply::Handled();
	FString Json, Error;
	FPlatformApplicationMisc::ClipboardPaste(Json);
	const bool bSuccess = UMiningDesignerLibrary::ApplyPreset(Target.Get(), Section, Json, Error);
	return FinishOperation(bSuccess, Error, TEXT("本页参数已粘贴，可一次撤销。请检查生成结果后保存关卡。"));
}

FReply SMiningDesignerPanel::SavePreset()
{
	if (!Editable()) return FReply::Handled();
	const FString Name = PresetName->GetText().ToString();
	const bool bExists = UMiningDesignerLibrary::ListPresets(Target.Get(), Section).Contains(Name);
	if (bExists && FMessageDialog::Open(EAppMsgType::YesNo, Text(TEXT("覆盖同名预设“") + Name + TEXT("”？原预设文件会被替换。"))) != EAppReturnType::Yes) return FReply::Handled();
	FString Error;
	const bool bSuccess = UMiningDesignerLibrary::SavePreset(Target.Get(), Section, Name, bExists, Error);
	RefreshPresets();
	return FinishOperation(bSuccess, Error, TEXT("预设已保存到 Config/MiningDesignerPresets；没有自动保存关卡。"));
}

FReply SMiningDesignerPanel::LoadPreset()
{
	if (!Editable()) return FReply::Handled();
	FString Error;
	const bool bSuccess = UMiningDesignerLibrary::LoadPreset(Target.Get(), Section, PresetName->GetText().ToString(), Error);
	return FinishOperation(bSuccess, Error, TEXT("预设已应用，可一次撤销。请检查结果后保存关卡。"));
}

FReply SMiningDesignerPanel::ResetDefaults()
{
	if (!Editable() || FMessageDialog::Open(EAppMsgType::YesNo, Text(TEXT("恢复当前分类的蓝图／类默认参数？不会改变位置、归属、房间顺序和路径点。支持撤销。"))) != EAppReturnType::Yes) return FReply::Handled();
	FString Error;
	const bool bSuccess = UMiningDesignerLibrary::ResetSection(Target.Get(), Section, Error);
	return FinishOperation(bSuccess, Error, TEXT("本页参数已恢复默认，可撤销；尚未保存关卡。"));
}

FReply SMiningDesignerPanel::FocusTarget()
{
	if (UMiningDesignerLibrary::CanEdit(Target.Get())) GEditor->MoveViewportCamerasToActor(*Target.Get(), false);
	return FReply::Handled();
}

FReply SMiningDesignerPanel::EditPath()
{
	if (!Editable()) return FReply::Handled();
	USplineComponent* Path = nullptr;
	if (const auto* Region = Cast<AMiningHollowRegion>(Target.Get())) Path = Region->Path;
	if (const auto* Chamber = Cast<AMiningHollowChamber>(Target.Get())) Path = Chamber->Path;
	if (const auto* Route = Cast<AMiningTunnelRoute>(Target.Get())) Path = Route->Path;
	if (!Path) return FinishOperation(false, TEXT("请选择矿区、洞室或主通道对象。"), {});
	GEditor->SelectNone(false, true, false);
	GEditor->SelectActor(Target.Get(), true, false);
	GEditor->SelectComponent(Path, true, true);
	GEditor->MoveViewportCamerasToActor(*Target.Get(), false);
	Message = TEXT("已选中路径组件。在视口点击并拖动控制点；手动支路需启用手动路径。原有端点和几何限制保持不变。");
	return FReply::Handled();
}

FReply SMiningDesignerPanel::StraightBranch()
{
	auto* Chamber = Cast<AMiningHollowChamber>(Target.Get());
	if (!Editable() || !Chamber || FMessageDialog::Open(EAppMsgType::YesNo, Text(TEXT("将此支路恢复为三个直线控制点，并开启手动编辑？现有弯折会被替换，支持撤销。"))) != EAppReturnType::Yes) return FReply::Handled();
	Chamber->ResetBranchPath();
	RefreshDetails();
	Message = TEXT("已请求直线支路控制点。请检查生成状态，再使用“编辑路径点”。");
	return FReply::Handled();
}
