#include "SMiningManualEditor.h"
#include "SMiningPenCanvas.h"
#include "MiningManualRegion.h"
#include "MiningManualShape.h"
#include "MiningSpatialTunnel.h"
#include "MiningLayoutRoom.h"
#include "MiningTunnelGenerator.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "FileHelpers.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

void SMiningManualEditor::Open() { FGlobalTabmanager::Get()->TryInvokeTab(FName("MiningManualEditor")); }

TSharedRef<SWidget> SMiningManualEditor::Button(const FString& Label, TFunction<FReply()> Action)
{
	return SNew(SButton).Text(FText::FromString(Label)).OnClicked_Lambda(MoveTemp(Action)).IsEnabled_Lambda([] { return GEditor && !GEditor->PlayWorld; });
}

void SMiningManualEditor::Construct(const FArguments& Arguments)
{
	FDetailsViewArgs DetailsArgs; DetailsArgs.bAllowSearch = true; DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bLockable = false; DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Details = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsArgs);
	Details->SetEnabled(TAttribute<bool>::CreateLambda([] { return GEditor && !GEditor->PlayWorld; }));
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(6)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1)
			[
				SAssignNew(Picker, SComboBox<TWeakObjectPtr<AMiningManualRegion>>).OptionsSource(&Regions)
				.OnGenerateWidget_Lambda([](TWeakObjectPtr<AMiningManualRegion> Value) { return SNew(STextBlock).Text(FText::FromString(Value.IsValid() ? Value->GetActorNameOrLabel() : TEXT("无效矿区"))); })
				.OnSelectionChanged_Lambda([this](TWeakObjectPtr<AMiningManualRegion> Value, ESelectInfo::Type) { if (Value.IsValid()) SelectRegion(Value.Get()); })
				[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(Region.IsValid() ? Region->GetActorNameOrLabel() : TEXT("选择手绘矿区")); })]
			]
			+ SHorizontalBox::Slot().AutoWidth()[Button(TEXT("新建矿区"), [this] { return NewRegion(); })]
			+ SHorizontalBox::Slot().AutoWidth()[Button(TEXT("初始化主线连接"), [this] { return ConnectMain(); })]
			+ SHorizontalBox::Slot().AutoWidth()[Button(TEXT("检查并构建"), [this] { return Rebuild(); })]
			+ SHorizontalBox::Slot().AutoWidth()[Button(TEXT("适应视图 F"), [this] { Canvas->Fit(); return FReply::Handled(); })]
			+ SHorizontalBox::Slot().AutoWidth()[Button(TEXT("保存地图"), [this] { Message = FEditorFileUtils::SaveCurrentLevel() ? TEXT("当前地图已保存，源控制点/手柄和生成结果一同保存。") : TEXT("地图未保存，请检查保存对话框。"); return FReply::Handled(); })]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)
		[
			SAssignNew(ObjectPicker, SComboBox<TWeakObjectPtr<AActor>>).OptionsSource(&Objects)
			.OnGenerateWidget_Lambda([](TWeakObjectPtr<AActor> Value) { return SNew(STextBlock).Text(FText::FromString(Value.IsValid() ? Value->GetActorNameOrLabel() : TEXT("已删除"))); })
			.OnSelectionChanged_Lambda([this](TWeakObjectPtr<AActor> Value, ESelectInfo::Type) { if (Value.IsValid()) Select(Value.Get()); })
			[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(Selected.IsValid() ? TEXT("当前空间：") + Selected->GetActorNameOrLabel() : TEXT("选择空间（包括无效草稿和禁用对象）")); })]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot()[Button(TEXT("+ 矩形空洞"), [this] { return Add(0); })]
			+ SWrapBox::Slot()[Button(TEXT("+ 椭圆空洞"), [this] { return Add(1); })]
			+ SWrapBox::Slot()[Button(TEXT("+ 手绘空洞"), [this] { return Add(2); })]
			+ SWrapBox::Slot()[Button(TEXT("+ 支线房间"), [this] { return Add(3); })]
			+ SWrapBox::Slot()[Button(TEXT("+ 支线通道"), [this] { return Add(4); })]
			+ SWrapBox::Slot()[Button(TEXT("+ 空白通道"), [this] { return Add(5); })]
			+ SWrapBox::Slot()[Button(TEXT("+ 可挖堵块"), [this] { return Add(6); })]
			+ SWrapBox::Slot()[Button(TEXT("复制空间"), [this] { return Duplicate(); })]
			+ SWrapBox::Slot()[Button(TEXT("选择 / 钢笔追加"), [this] { Canvas->ToggleDraw(); return FReply::Handled(); })]
			+ SWrapBox::Slot()[Button(TEXT("切换 A / B"), [this] { Canvas->ToggleSide(); return FReply::Handled(); })]
			+ SWrapBox::Slot()[Button(TEXT("反转当前边界"), [this] { Canvas->ReverseBoundary(); return FReply::Handled(); })]
			+ SWrapBox::Slot()[Button(TEXT("整体弯曲"), [this] { Canvas->ToggleBend(); return FReply::Handled(); })]
			+ SWrapBox::Slot()[Button(TEXT("重绑弯曲导向"), [this] { Canvas->RebindGuide(); return FReply::Handled(); })]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot()[Button(TEXT("角点"), [this] { Canvas->Mode(EMiningPenMode::Corner); return FReply::Handled(); })]
			+ SWrapBox::Slot()[Button(TEXT("平滑点"), [this] { Canvas->Mode(EMiningPenMode::Smooth); return FReply::Handled(); })]
			+ SWrapBox::Slot()[Button(TEXT("独立手柄"), [this] { Canvas->Mode(EMiningPenMode::Independent); return FReply::Handled(); })]
			+ SWrapBox::Slot()[Button(TEXT("锁定 / 解锁点"), [this] { Canvas->Lock(); return FReply::Handled(); })]
			+ SWrapBox::Slot()[Button(TEXT("删除选中点"), [this] { Canvas->DeletePoints(); return FReply::Handled(); })]
			+ SWrapBox::Slot()[Button(TEXT("吸附开关"), [this] { Canvas->ToggleSnap(); return FReply::Handled(); })]
			+ SWrapBox::Slot()[Button(TEXT("绑定起点房间"), [this] { Canvas->BindPortal(false); return FReply::Handled(); })]
			+ SWrapBox::Slot()[Button(TEXT("绑定终点房间"), [this] { Canvas->BindPortal(true); return FReply::Handled(); })]
			+ SWrapBox::Slot()[Button(TEXT("撤销"), [] { GEditor->UndoTransaction(); return FReply::Handled(); })]
			+ SWrapBox::Slot()[Button(TEXT("重做"), [] { GEditor->RedoTransaction(); return FReply::Handled(); })]
		]
		+ SVerticalBox::Slot().FillHeight(1).Padding(6)
		[
			SNew(SSplitter)
			+ SSplitter::Slot().Value(0.74f)
			[SAssignNew(Canvas, SMiningPenCanvas).OnSelected([this](AActor* Actor) { Select(Actor); })]
			+ SSplitter::Slot().Value(0.26f)[Details.ToSharedRef()]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 2)
		[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return FText::FromString(Canvas->Help()); })]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4)
		[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return FText::FromString(Message + (Region.IsValid() ? TEXT("\n") + Region->RegionStatus : TEXT(""))); })]
	];
	Message = TEXT("蓝/橙为通道 A/B；红色为无效草稿；黄方块调整房间大小。两侧均从起点画向终点。轮廓与 Y 深度共同构成三维空间。");
}

void SMiningManualEditor::Select(AActor* Actor)
{
	Selected = Actor; Details->SetObject(Actor); Canvas->SetSelected(Actor);
	if (GEditor && Actor && !GEditor->PlayWorld)
	{
		GEditor->SelectNone(false, true, false); GEditor->SelectActor(Actor, true, true);
	}
}

void SMiningManualEditor::SelectRegion(AMiningManualRegion* Actor)
{
	if (Region == Actor) return;
	Region = Actor; Canvas->SetRegion(Actor); Select(Actor);
}

void SMiningManualEditor::Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime)
{
	SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
	if (!GEditor || GEditor->PlayWorld || CurrentTime < NextScan) return;
	NextScan = CurrentTime + 0.5;
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return;
	TArray<TWeakObjectPtr<AMiningManualRegion>> Found;
	for (TActorIterator<AMiningManualRegion> Actor(World); Actor; ++Actor)
		if (!Actor->IsA<AMiningSpatialRegion>()) Found.Add(*Actor);
	if (Found != Regions) { Regions = MoveTemp(Found); Picker->RefreshOptions(); }
	AActor* Actor = GEditor->GetSelectedActors()->GetTop<AActor>();
	if (Region.IsValid() && Region->IsA<AMiningSpatialRegion>()) SelectRegion(nullptr);
	if (Actor && (Actor->IsA<AMiningSpatialRegion>() || Actor->IsA<AMiningSpatialTunnel>())) return;
	AMiningManualRegion* Wanted = Cast<AMiningManualRegion>(Actor);
	if (AMiningManualShape* Shape = Cast<AMiningManualShape>(Actor)) Wanted = Shape->Region;
	if (Wanted && Wanted != Region.Get()) SelectRegion(Wanted);
	if (!Region.IsValid() && !Regions.IsEmpty()) SelectRegion(Regions[0].Get());
	TArray<TWeakObjectPtr<AActor>> FoundObjects;
	if (Region.IsValid())
	{
		FoundObjects.Add(Region.Get());
		if (Region->FromRoom) FoundObjects.Add(Region->FromRoom.Get());
		if (Region->ToRoom) FoundObjects.Add(Region->ToRoom.Get());
		for (AMiningManualShape* Shape : Region->GetShapes()) FoundObjects.Add(Shape);
	}
	if (FoundObjects != Objects) { Objects = MoveTemp(FoundObjects); ObjectPicker->RefreshOptions(); }
	if (Actor && Actor != Selected.Get() && (Wanted || (Region.IsValid() && (Actor == Region->FromRoom || Actor == Region->ToRoom))))
	{
		Select(Actor);
	}
}

FReply SMiningManualEditor::Add(int32 Kind)
{
	if (!Region.IsValid()) { Message = TEXT("先选择或新建一个手绘矿区。"); return FReply::Handled(); }
	const FScopedTransaction Transaction(FText::FromString(TEXT("手绘：新增空间")));
	FActorSpawnParameters Spawn; Spawn.OverrideLevel = Region->GetLevel(); Spawn.ObjectFlags |= RF_Transactional;
	const FVector2D Position = Canvas->ViewCenter();
	AMiningManualShape* Shape = Region->GetWorld()->SpawnActor<AMiningManualShape>(FVector(Position.X, Region->GetActorLocation().Y, Position.Y), FRotator::ZeroRotator, Spawn);
	if (!Shape) return FReply::Handled();
	Shape->Region = Region.Get(); Shape->Kind = Kind == 6 ? EMiningManualKind::RockBlock : Kind >= 4 ? EMiningManualKind::Passage : Kind == 3 ? EMiningManualKind::SideRoom : EMiningManualKind::Cavity;
	Shape->SetActorLabel(Kind == 6 ? TEXT("手绘_可挖岩石保留区") : Kind >= 4 ? TEXT("手绘_支线通道") : Kind == 3 ? TEXT("手绘_支线房间") : TEXT("手绘_空洞"));
	Shape->SetFolderPath(FName(TEXT("ManualMine/Spaces")));
	if (Kind == 1) Shape->ResetEllipse();
	if (Kind == 2 || Kind == 4 || Kind == 5) { Shape->BoundaryA.Reset(); Shape->BoundaryB.Reset(); }
	if (Kind == 4)
	{
		for (int32 Side = 0; Side < 2; ++Side)
			for (double PositionX : {-600.0, 0.0, 600.0})
			{
				FMiningPenPoint Point; Point.Position = FVector2D(PositionX, Side == 0 ? 250 : -250);
				(Side == 0 ? Shape->BoundaryA : Shape->BoundaryB).Add(Point);
			}
	}
	Shape->OnConstruction(Shape->GetActorTransform()); Shape->MarkPackageDirty(); Select(Shape);
	Message = Kind == 2 || Kind == 5 ? TEXT("新空间是未完成草稿。切换到钢笔追加，点击/拖动绘制；通道 A/B 都按同方向绘制，完成后切回选择。") : TEXT("空间已添加。拖内部移动；支线房间黄柄调尺寸；选曲线节点拖动。隐藏空洞只需与主路留出岩层。入口绑定可在右侧设置。");
	return FReply::Handled();
}

FReply SMiningManualEditor::Duplicate()
{
	AMiningManualShape* Source = Cast<AMiningManualShape>(Selected.Get());
	if (!Source || !Region.IsValid()) { Message = TEXT("选中空洞、支线房间或通道后复制。"); return FReply::Handled(); }
	const FScopedTransaction Transaction(FText::FromString(TEXT("手绘：复制空间")));
	FActorSpawnParameters Spawn; Spawn.Template = Source; Spawn.OverrideLevel = Source->GetLevel(); Spawn.ObjectFlags |= RF_Transactional;
	AMiningManualShape* Copy = Source->GetWorld()->SpawnActor<AMiningManualShape>(Source->GetActorLocation() + FVector(200, 0, -200), FRotator::ZeroRotator, Spawn);
	if (Copy)
	{
		Copy->SetActorLabel(Source->GetActorNameOrLabel() + TEXT("_副本"));
		if (Copy->Kind == EMiningManualKind::Passage) { Copy->StartPortal.Room = nullptr; Copy->EndPortal.Room = nullptr; }
		Copy->OnConstruction(Copy->GetActorTransform()); Copy->MarkPackageDirty(); Select(Copy);
		Message = TEXT("已复制独立空间。通道副本不继承房间绑定，避免意外多开入口；可重新设置。");
	}
	return FReply::Handled();
}

FReply SMiningManualEditor::NewRegion()
{
	UWorld* World = GEditor->GetEditorWorldContext().World(); if (!World) return FReply::Handled();
	const FScopedTransaction Transaction(FText::FromString(TEXT("手绘：新建矿区")));
	FActorSpawnParameters Spawn; Spawn.ObjectFlags |= RF_Transactional;
	AMiningManualRegion* Created = World->SpawnActor<AMiningManualRegion>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	if (Created) { Created->SetActorLabel(TEXT("手绘矿区_设置两端房间")); Created->OnConstruction(Created->GetActorTransform()); SelectRegion(Created); Created->MarkPackageDirty(); }
	Message = TEXT("新矿区不替换已有地图。先在右侧设置 FromRoom / ToRoom 和岩石范围，然后初始化主线连接，并加入生成器 HollowRegions 列表。");
	return FReply::Handled();
}

FReply SMiningManualEditor::ConnectMain()
{
	if (!Region.IsValid() || !Region->FromRoom || !Region->ToRoom) { Message = TEXT("先为矿区指定两个主房间。"); return FReply::Handled(); }
	if (Region->MainPassage) { Select(Region->MainPassage); Message = TEXT("主线已有手绘数据，未覆盖。请直接编辑；确需重置时使用右侧的初始化入口连接按钮。"); return FReply::Handled(); }
	const FScopedTransaction Transaction(FText::FromString(TEXT("手绘：创建主线通道")));
	Region->Modify();
	FActorSpawnParameters Spawn; Spawn.OverrideLevel = Region->GetLevel(); Spawn.ObjectFlags |= RF_Transactional;
	const FVector First = Region->FromRoom->GetRoomBounds().GetCenter(), Last = Region->ToRoom->GetRoomBounds().GetCenter();
	AMiningManualShape* Passage = Region->GetWorld()->SpawnActor<AMiningManualShape>(FVector(First.X, First.Y, First.Z), FRotator::ZeroRotator, Spawn);
	if (!Passage) return FReply::Handled();
	Passage->Region = Region.Get(); Passage->Kind = EMiningManualKind::Passage;
	Passage->StartPortal.Room = Region->FromRoom; Passage->EndPortal.Room = Region->ToRoom;
	const bool bVertical = FMath::Abs(Last.Z - First.Z) > FMath::Abs(Last.X - First.X);
	Passage->StartPortal.Wall = bVertical ? (Last.Z > First.Z ? EMiningPortalWall::PositiveZ : EMiningPortalWall::NegativeZ) : (Last.X > First.X ? EMiningPortalWall::PositiveX : EMiningPortalWall::NegativeX);
	Passage->EndPortal.Wall = bVertical ? (Last.Z > First.Z ? EMiningPortalWall::NegativeZ : EMiningPortalWall::PositiveZ) : (Last.X > First.X ? EMiningPortalWall::NegativeX : EMiningPortalWall::PositiveX);
	Passage->Depth = FMath::Min3(700.0, Region->FromRoom->GetRoomBounds().GetSize().Y * 0.7, Region->ToRoom->GetRoomBounds().GetSize().Y * 0.7);
	Passage->EndPortal.DepthOffset = First.Y - Last.Y;
	Passage->StartPortal.Aperture = FMath::Min(500.0, Region->FromRoom->GetRoomBounds().GetSize()[bVertical ? 0 : 2] * 0.7);
	Passage->EndPortal.Aperture = FMath::Min(500.0, Region->ToRoom->GetRoomBounds().GetSize()[bVertical ? 0 : 2] * 0.7);
	Passage->SetActorLabel(TEXT("手绘_主线通道")); Region->MainPassage = Passage;
	if (!Passage->FitPortals()) Message = Passage->ShapeStatus; else Message = TEXT("已建立入口连接。只初始化这一次，之后房间移动仅跟随两端，不重新布置中段。请检查矿区范围是否包住全部轮廓。");
	Region->MarkPackageDirty(); Select(Passage); return FReply::Handled();
}

FReply SMiningManualEditor::Rebuild()
{
	if (!Region.IsValid()) return FReply::Handled();
	bool bFound = false;
	for (TActorIterator<AMiningTunnelGenerator> Generator(Region->GetWorld()); Generator; ++Generator)
		if (Generator->UsesHollowRegion(Region.Get())) { bFound = true; Generator->RebuildTunnels(); Message = Generator->LastBuildStatus; }
	if (!bFound) Message = TEXT("该矿区尚未加入生成器。将它加入对应生成器的 HollowRegions 列表；需启用 Hollow Region Prototype / Direct Room Faces / Automatic Chunks。");
	return FReply::Handled();
}
