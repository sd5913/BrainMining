#include "Modules/ModuleManager.h"
#include "MiningDesignerLibrary.h"
#include "SMiningDesignerPanel.h"
#include "SSubtitleToolPanel.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"

class FBrainMiningEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
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
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("MiningDesigner");
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("SubtitleTool");
	}
private:
	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		FToolMenuSection& Section = Menu->FindOrAddSection("BrainMining");
		Section.AddMenuEntry("MiningDesigner", FText::FromString(TEXT("矿业策划台")), FText::FromString(TEXT("统一调整角色、挖掘、场景和调试参数")), FSlateIcon(), FUIAction(FExecuteAction::CreateStatic(&UMiningDesignerLibrary::OpenDesigner)));
		Section.AddMenuEntry("SubtitleTool", FText::FromString(TEXT("字幕工具")), FText::FromString(TEXT("创建字幕段、编辑文本和音频，并放置触发盒")), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("SubtitleTool")));
		})));
	}
};

IMPLEMENT_MODULE(FBrainMiningEditorModule, BrainMiningEditor)
