#include "Modules/ModuleManager.h"
#include "MiningDesignerLibrary.h"
#include "SMiningDesignerPanel.h"
#include "SMiningManualEditor.h"
#include "SMiningSpatialEditor.h"
#include "MiningSpatialVisualizer.h"
#include "MiningSpatialOverview.h"
#include "MiningSpatialSafeEditing.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "SSubtitleToolPanel.h"
#include "SExternalMiningPanel.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "MiningTunnelGenerator.h"
#include "MiningManualRegion.h"

class FBrainMiningEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		MiningSpatialOverview::Register();
		MiningSpatialSafeEditing::Register();
		SpatialVisualizer = MakeShared<FMiningSpatialVisualizer>();
		if (GUnrealEd) { GUnrealEd->RegisterComponentVisualizer(UMiningSpatialPathComponent::StaticClass()->GetFName(), SpatialVisualizer); SpatialVisualizer->OnRegister(); }
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner("MiningSpatialEditor", FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
		{
			return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SMiningSpatialEditor)];
		})).SetDisplayName(FText::FromString(TEXT("三维隧道编辑器")));
		DragTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { MiningSpatialSafeEditing::PumpInteractive(); return true; }));
		ManualTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float)
		{
			if (GUnrealEd && !GUnrealEd->FindComponentVisualizer(UMiningSpatialPathComponent::StaticClass()).IsValid())
			{
				GUnrealEd->RegisterComponentVisualizer(UMiningSpatialPathComponent::StaticClass()->GetFName(), SpatialVisualizer); SpatialVisualizer->OnRegister();
			}
			if (!GEditor || GEditor->PlayWorld || IsRunningCommandlet()) return true;
			if (UWorld* World = GEditor->GetEditorWorldContext().World())
			{
				MiningSpatialSafeEditing::Observe(World);
				for (TActorIterator<AMiningTunnelGenerator> Generator(World); Generator; ++Generator)
					if (Generator->bHollowRegionPrototype && Generator->GetHollowRegions().ContainsByPredicate([](AMiningHollowRegion* Region) { return IsValid(Region) && Region->IsA<AMiningManualRegion>(); }))
						Generator->ProcessEditorRebuild();
			}
			return true;
		}), 0.1f);
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner("MiningManualEditor", FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
		{
			return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SMiningManualEditor)];
		})).SetDisplayName(FText::FromString(TEXT("矿道钢笔编辑器")));
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner("ExternalMiningGenerator", FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
		{
			return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SExternalMiningPanel)];
		})).SetDisplayName(FText::FromString(TEXT("外部通路矿区生成器"))).SetTooltipText(FText::FromString(TEXT("Blender 通路切割、可挖岩层和不可挖基岩外壳")));
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner("MiningDesigner", FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
		{
			return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SMiningDesignerPanel)];
		})).SetDisplayName(FText::FromString(TEXT("矿业策划台"))).SetTooltipText(FText::FromString(TEXT("中文分类调参、场景对象编辑和安全预设")));
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner("SubtitleTool", FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
		{
			return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SSubtitleToolPanel)];
		})).SetDisplayName(FText::FromString(TEXT("字幕工具"))).SetTooltipText(FText::FromString(TEXT("创建字幕段、填写文本和音频，并在关卡里放置触发盒")));
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBrainMiningEditorModule::RegisterMenus));
	}
	virtual void ShutdownModule() override
	{
		MiningSpatialSafeEditing::Unregister();
		MiningSpatialOverview::Unregister();
		if (GUnrealEd) GUnrealEd->UnregisterComponentVisualizer(UMiningSpatialPathComponent::StaticClass()->GetFName());
		SpatialVisualizer.Reset();
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("MiningSpatialEditor");
		FTSTicker::GetCoreTicker().RemoveTicker(ManualTicker);
		FTSTicker::GetCoreTicker().RemoveTicker(DragTicker);
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("MiningDesigner");
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("MiningManualEditor");
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("SubtitleTool");
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("ExternalMiningGenerator");
	}
private:
	TSharedPtr<FMiningSpatialVisualizer> SpatialVisualizer;
	FTSTicker::FDelegateHandle ManualTicker;
	FTSTicker::FDelegateHandle DragTicker;
	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		Menu->FindOrAddSection("BrainMining").AddMenuEntry("MiningSpatialEditor", FText::FromString(TEXT("三维隧道编辑器")), FText::FromString(TEXT("三维曲线、逐截面钢笔轮廓和扭转")), FSlateIcon(), FUIAction(FExecuteAction::CreateStatic(&SMiningSpatialEditor::Open)));
		Menu->FindOrAddSection("BrainMining").AddMenuEntry("MiningManualEditor", FText::FromString(TEXT("矿道钢笔编辑器")), FText::FromString(TEXT("手绘通道两侧、独立空洞和支线房间")), FSlateIcon(), FUIAction(FExecuteAction::CreateStatic(&SMiningManualEditor::Open)));
		Menu->FindOrAddSection("BrainMining").AddMenuEntry("MiningDesigner", FText::FromString(TEXT("矿业策划台")), FText::FromString(TEXT("统一调整角色、挖掘、场景和调试参数")), FSlateIcon(), FUIAction(FExecuteAction::CreateStatic(&UMiningDesignerLibrary::OpenDesigner)));
		FToolMenuSection& Section = Menu->FindOrAddSection("BrainMining");
		Section.AddMenuEntry("ExternalMiningGenerator", FText::FromString(TEXT("外部通路矿区生成器")), FText::FromString(TEXT("独立生成可挖岩层、基岩外壳和外部模型通路")), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("ExternalMiningGenerator")));
		})));
		Section.AddMenuEntry("SubtitleTool", FText::FromString(TEXT("字幕工具")), FText::FromString(TEXT("创建字幕段、编辑文本和音频，并放置触发盒")), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("SubtitleTool")));
		})));
	}
};

IMPLEMENT_MODULE(FBrainMiningEditorModule, BrainMiningEditor)
