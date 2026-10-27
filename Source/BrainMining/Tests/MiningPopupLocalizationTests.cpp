#include "PopupClose/PopupLocalizedArtSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "RenderingThread.h"
#include "Serialization/BufferArchive.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningPopupArtworkTest, "BrainMining.Rendering.LocalizedPopupArtwork", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FMiningPopupArtworkTest::RunTest(const FString&)
{
    auto* Instance = NewObject<UGameInstance>();
    auto* System = NewObject<UPopupLocalizedArtSubsystem>(Instance);
    System->AddToRoot();
    auto& Manager = FTextLocalizationManager::Get();
    const bool PreviousPreview = Manager.IsGameLocalizationPreviewEnabled();
    const FString PreviousLanguage = Manager.GetConfiguredGameLocalizationPreviewLanguage();
    ON_SCOPE_EXIT
    {
        System->Deinitialize();
        System->RemoveFromRoot();
        if (PreviousPreview) Manager.EnableGameLocalizationPreview(PreviousLanguage);
        else Manager.DisableGameLocalizationPreview();
        Manager.WaitForAsyncTasks();
    };
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Validation/LocalizationAchievements/PopupRenders");
    IFileManager::Get().MakeDirectory(*Directory, true);
    TArray<UMaterialInterface*> Originals;
    for (const FString Culture : {FString(TEXT("zh-Hans")), FString(TEXT("en")), FString(TEXT("zh-Hant"))})
    {
        Manager.EnableGameLocalizationPreview(Culture);
        Manager.WaitForAsyncTasks();
        for (int32 Variant = 0; Variant < 3; ++Variant)
        {
            auto* Material = System->GetArtwork(Variant);
            TestNotNull(TEXT("Localized material exists"), Material);
            if (Originals.Num() < 3) Originals.Add(Material);
            else TestTrue(TEXT("Language changes reuse shared materials"), Material == Originals[Variant]);
        }
        System->RefreshLanguage(Culture);
        FlushRenderingCommands();
        TestEqual(TEXT("Three shared render targets cover every physical popup"), System->Targets.Num(), 3);
        for (const auto& Target : System->Targets)
        {
            FBufferArchive Image;
            TestTrue(TEXT("Artwork exports as PNG"), FImageUtils::ExportRenderTarget2DAsPNG(Target.Value, Image));
            TestTrue(TEXT("Artwork contains visible pixels"), Image.Num() > 5000);
            const FString Path = Directory / FString::Printf(TEXT("%s-%d.png"), *Culture, Target.Key);
            TestTrue(TEXT("Artwork preview saved"), FFileHelper::SaveArrayToFile(Image, *Path));
        }
    }
    return true;
}
#endif
