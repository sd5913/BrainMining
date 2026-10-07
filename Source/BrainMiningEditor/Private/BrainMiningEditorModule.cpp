#include "Modules/ModuleManager.h"
#include "MiningDesignerLibrary.h"
#include "SMiningDesignerPanel.h"
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
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBrainMiningEditorModule::RegisterMenus));
	}
	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("MiningDesigner");
	}
private:
	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		Menu->FindOrAddSection("BrainMining").AddMenuEntry("MiningDesigner", FText::FromString(TEXT("矿业策划台")), FText::FromString(TEXT("统一调整角色、挖掘、场景和调试参数")), FSlateIcon(), FUIAction(FExecuteAction::CreateStatic(&UMiningDesignerLibrary::OpenDesigner)));
	}
};

IMPLEMENT_MODULE(FBrainMiningEditorModule, BrainMiningEditor)
