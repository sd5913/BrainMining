#include "MiningGUIWidgets.h"
#include "MiningLocalizationSubsystem.h"
#include "MiningProgressionComponent.h"
#include "MiningPickaxeSettings.h"
#include "MiningWalletSubsystem.h"
#include "StartRoom/MiningWallSign.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Internationalization/TextLocalizationManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "Serialization/BufferArchive.h"
#include "Slate/WidgetRenderer.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningHUDLanguageTest,"BrainMining.Localization.MineralHUDAndFlightSign",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningHUDLanguageTest::RunTest(const FString&)
{
    auto& Manager=FTextLocalizationManager::Get();
    const bool PreviousPreview=Manager.IsGameLocalizationPreviewEnabled();
    const FString PreviousLanguage=Manager.GetConfiguredGameLocalizationPreviewLanguage();
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto* Instance=NewObject<UGameInstance>(GEngine);
    auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(World);Context.OwningGameInstance=Instance;
    World->SetGameInstance(Instance);Instance->Init();
    FWidgetRenderer* Renderer=GUsingNullRHI?nullptr:new FWidgetRenderer(false);
    ON_SCOPE_EXIT
    {
        if(Renderer)BeginCleanup(Renderer);
        FlushRenderingCommands();
        Instance->Shutdown();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);
        if(PreviousPreview)Manager.EnableGameLocalizationPreview(PreviousLanguage);
        else Manager.DisableGameLocalizationPreview();
        Manager.WaitForAsyncTasks();
    };
    Manager.EnableGameLocalizationPreview(TEXT("zh-Hans"));Manager.WaitForAsyncTasks();
    auto* Wallet=Instance->GetSubsystem<UMiningWalletSubsystem>();
    Wallet->bUseMineralInventory=true;
    auto* Controller=World->SpawnActor<APlayerController>();
    World->AddController(Controller);
    Controller->SetPlayer(NewObject<ULocalPlayer>(GEngine));
    auto* Pawn=World->SpawnActor<APawn>();
    Pawn->AddInstanceComponent(NewObject<UMiningProgressionComponent>(Pawn));
    Controller->Possess(Pawn);
    auto* HUDClass=LoadClass<UMiningGUIHUDWidget>(nullptr,TEXT("/Game/UI/MiningGUI/WBP_MiningHUD.WBP_MiningHUD_C"));
    if(!TestNotNull(TEXT("Published HUD class loads"),HUDClass))return false;
    auto* HUD=CreateWidget<UMiningGUIHUDWidget>(Controller,HUDClass);
    if(!TestNotNull(TEXT("Published HUD creates"),HUD))return false;
    if(!TestTrue(TEXT("HUD resolves its player's pawn"),HUD->GetOwningPlayerPawn()==Pawn))return false;
    const auto HUDWidget=HUD->TakeWidget();
    HUD->RefreshMineralProgression();
    auto* Sign=World->SpawnActor<AMiningWallSign>();
    Sign->Words=FText::FromString(TEXT("按住空格起飞"));
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("Validation/LocalizationAchievements/HUDRenders");
    if(Renderer)IFileManager::Get().MakeDirectory(*Directory,true);
    const auto Capture=[&](const TSharedRef<SWidget>& Widget,const FString& Name,FVector2D Size)
    {
        if(!Renderer)return;
        auto* Target=FWidgetRenderer::CreateTargetFor(Size,TF_Bilinear,false);
        Renderer->DrawWidget(Target,Widget,1.f,Size,0.f);
        FlushRenderingCommands();
        FBufferArchive Image;
        TestTrue(TEXT("Localized widget exports PNG"),FImageUtils::ExportRenderTarget2DAsPNG(Target,Image));
        TestTrue(TEXT("Localized widget contains pixels"),Image.Num()>1000);
        TestTrue(TEXT("Localized preview saves"),FFileHelper::SaveArrayToFile(Image,*(Directory/(Name+TEXT(".png")))));
    };
    const TCHAR* Cultures[]={TEXT("zh-Hans"),TEXT("en"),TEXT("zh-Hant")};
    const TCHAR* ExpectedNames[][4]={{TEXT("铜矿"),TEXT("铁矿"),TEXT("孔雀石"),TEXT("钻石")},{TEXT("Copper"),TEXT("Iron"),TEXT("Malachite"),TEXT("Diamond")},{TEXT("銅礦"),TEXT("鐵礦"),TEXT("孔雀石"),TEXT("鑽石")}};
    const TCHAR* ExpectedSigns[]={TEXT("按住空格键起飞"),TEXT("Hold Space to fly"),TEXT("按住空白鍵起飛")};
    for(int32 CultureIndex=0;CultureIndex<UE_ARRAY_COUNT(Cultures);++CultureIndex)
    {
        Manager.EnableGameLocalizationPreview(Cultures[CultureIndex]);Manager.WaitForAsyncTasks();
        for(int32 MineralIndex=0;MineralIndex<4;++MineralIndex)
        {
            auto* Label=Cast<UTextBlock>(HUD->GetWidgetFromName(FName(*FString::Printf(TEXT("InventoryMineralName%d"),MineralIndex))));
            if(TestNotNull(TEXT("Inventory mineral label exists"),Label))
                TestEqual(TEXT("Existing HUD label updates without rebuilding"),Label->GetText().ToString(),FString(ExpectedNames[CultureIndex][MineralIndex]));
        }
        TestEqual(TEXT("Authored wall sign changes language"),Sign->GetDisplayText().ToString(),FString(ExpectedSigns[CultureIndex]));
        for(const int32 Level:{0,3})
        {
            Wallet->UpgradeLevels[0]=Level;HUD->RefreshMineralProgression();
            auto* Requirement=Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("PickaxeRequirementName0")));
            const auto Costs=UMiningPickaxeSettings::ResolveLevel(Level+1).MaterialCosts;
            if(TestNotNull(TEXT("Upgrade requirement exists"),Requirement) && !Costs.IsEmpty())
                TestEqual(TEXT("Upgrade uses capitalized mineral label"),Requirement->GetText().ToString(),FMiningMineralCost::LabelText(Costs[0].Mineral).ToString());
            for(const FVector2D Size:{FVector2D(1280,720),FVector2D(1920,1080)})
                Capture(HUDWidget,FString::Printf(TEXT("%s-level%d-%d"),Cultures[CultureIndex],Level,int32(Size.X)),Size);
        }
        if(Sign->Surface->GetSlateWidget().IsValid())Capture(Sign->Surface->GetSlateWidget().ToSharedRef(),FString(Cultures[CultureIndex])+TEXT("-flight-sign"),FVector2D(1024,256));
    }
    Sign->Words=FText::FromString(TEXT("按住空格键起飞"));
    TestEqual(TEXT("Alternate saved flight prompt localizes"),Sign->GetDisplayText().ToString(),FString(ExpectedSigns[2]));
    Sign->Words=FText::FromString(TEXT("Custom wall text"));
    TestEqual(TEXT("Unrelated wall sign preserved"),Sign->GetDisplayText().ToString(),FString(TEXT("Custom wall text")));
    Sign->LocalizationKey=TEXT("UI.World.HoldSpaceToFly");
    TestEqual(TEXT("Explicit key supports newly authored signs"),Sign->GetDisplayText().ToString(),FString(ExpectedSigns[2]));
    return true;
}
#endif
