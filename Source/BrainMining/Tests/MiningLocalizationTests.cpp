#include "MiningLocalizationSubsystem.h"
#include "MiningCollection.h"
#include "MiningSaveSubsystem.h"
#include "Showcase/MiningTreasure.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "WenSi/WenSiThoughtBubble.h"
#include "Subtitles/SubtitleVoice.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningLanguageResolutionTest,"BrainMining.Localization.CultureResolution",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningLanguageResolutionTest::RunTest(const FString&)
{
    TestEqual(TEXT("Steam simplified"),UMiningLocalizationSubsystem::ResolveCulture(TEXT("schinese")),FString(TEXT("zh-Hans")));
    TestEqual(TEXT("Steam traditional"),UMiningLocalizationSubsystem::ResolveCulture(TEXT("tchinese")),FString(TEXT("zh-Hant")));
    TestEqual(TEXT("Hong Kong"),UMiningLocalizationSubsystem::ResolveCulture(TEXT("zh_HK")),FString(TEXT("zh-Hant")));
    TestEqual(TEXT("Taiwan"),UMiningLocalizationSubsystem::ResolveCulture(TEXT("zh-TW")),FString(TEXT("zh-Hant")));
    TestEqual(TEXT("Traditional script"),UMiningLocalizationSubsystem::ResolveCulture(TEXT("zh-Hant-HK")),FString(TEXT("zh-Hant")));
    TestEqual(TEXT("Singapore"),UMiningLocalizationSubsystem::ResolveCulture(TEXT("zh-SG")),FString(TEXT("zh-Hans")));
    TestEqual(TEXT("English region"),UMiningLocalizationSubsystem::ResolveCulture(TEXT("en-GB")),FString(TEXT("en")));
    TestEqual(TEXT("Unsupported language"),UMiningLocalizationSubsystem::ResolveCulture(TEXT("fr")),FString(TEXT("en")));
    TestFalse(TEXT("Unknown settings rejected"),UMiningLocalizationSubsystem::IsSupportedPreference(TEXT("../invalid")));
    TestTrue(TEXT("Auto preference supported"),UMiningLocalizationSubsystem::IsSupportedPreference(TEXT("auto")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningLanguageResourcesTest,"BrainMining.Localization.CompiledResourcesAndLiveText",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningLanguageResourcesTest::RunTest(const FString&)
{
    const FString PreviousLanguage=FInternationalization::Get().GetCurrentLanguage()->GetName();
    const FString PreviousLocale=FInternationalization::Get().GetCurrentLocale()->GetName();
    ON_SCOPE_EXIT
    {
        UKismetInternationalizationLibrary::SetCurrentLanguage(PreviousLanguage,false);
        UKismetInternationalizationLibrary::SetCurrentLocale(PreviousLocale,false);
        FTextLocalizationManager::Get().DisableGameLocalizationPreview();
        FTextLocalizationManager::Get().WaitForAsyncTasks();
    };
    MiningText::RegisterTables();
    const FText PersistentLabel=MiningText::Get(TEXT("UI.Settings.Title"));
    UKismetInternationalizationLibrary::SetCurrentLanguageAndLocale(TEXT("en"),false);
    FTextLocalizationManager::Get().EnableGameLocalizationPreview(TEXT("en"));
    FTextLocalizationManager::Get().WaitForAsyncTasks();
    TestEqual(TEXT("Existing text switches to English"),PersistentLabel.ToString(),FString(TEXT("Settings")));
    UKismetInternationalizationLibrary::SetCurrentLanguageAndLocale(TEXT("zh-Hant"),false);
    FTextLocalizationManager::Get().EnableGameLocalizationPreview(TEXT("zh-Hant"));
    FTextLocalizationManager::Get().WaitForAsyncTasks();
    TestEqual(TEXT("Existing text switches to traditional Chinese"),PersistentLabel.ToString(),FString(TEXT("設定")));
    UKismetInternationalizationLibrary::SetCurrentLanguageAndLocale(TEXT("zh-Hans"),false);
    FTextLocalizationManager::Get().EnableGameLocalizationPreview(TEXT("zh-Hans"));
    FTextLocalizationManager::Get().WaitForAsyncTasks();
    TestEqual(TEXT("Existing text switches to simplified Chinese"),PersistentLabel.ToString(),FString(TEXT("设置")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningLanguageContentTest,"BrainMining.Localization.PlayableContentAndGrammar",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningLanguageContentTest::RunTest(const FString&)
{
    const FString PreviousLanguage=FInternationalization::Get().GetCurrentLanguage()->GetName();
    const FString PreviousLocale=FInternationalization::Get().GetCurrentLocale()->GetName();
    ON_SCOPE_EXIT
    {
        UKismetInternationalizationLibrary::SetCurrentLanguageAndLocale(PreviousLanguage,false);
        UKismetInternationalizationLibrary::SetCurrentLocale(PreviousLocale,false);
        FTextLocalizationManager::Get().DisableGameLocalizationPreview();
        FTextLocalizationManager::Get().WaitForAsyncTasks();
    };
    const FText PersistentName=AMiningCollectible::ItemNameText(0);
    const TCHAR* Cultures[]={TEXT("zh-Hans"),TEXT("en"),TEXT("zh-Hant")};
    const TCHAR* ExpectedNames[]={TEXT("来自天堂的马桶"),TEXT("The Heavenly Toilet"),TEXT("來自天堂的馬桶")};
    const TCHAR* BeforeProperty[]={TEXT("这是一个"),TEXT("This rock is "),TEXT("這是一個")};
    for(int32 CultureIndex=0;CultureIndex<UE_ARRAY_COUNT(Cultures);++CultureIndex)
    {
        UKismetInternationalizationLibrary::SetCurrentLanguageAndLocale(Cultures[CultureIndex],false);
        FTextLocalizationManager::Get().EnableGameLocalizationPreview(Cultures[CultureIndex]);
        FTextLocalizationManager::Get().WaitForAsyncTasks();
        TestEqual(TEXT("Collection identity survives language changes"),PersistentName.ToString(),FString(ExpectedNames[CultureIndex]));
        auto Row=SNew(SHorizontalBox);
        auto Property=SNew(STextBlock).Text(MiningText::Get(TEXT("WenSi.Word.Heavy")));
        WenSiThoughtBubble::AddStatement(Row,MiningText::Get(TEXT("WenSi.Noun.Rock")).ToString(),Property,FSlateFontInfo());
        TestEqual(TEXT("Statement has two text segments and one interactive property"),Row->GetChildren()->Num(),3);
        TestEqual(TEXT("Translated grammar precedes the draggable word"),StaticCastSharedRef<STextBlock>(Row->GetChildren()->GetChildAt(0))->GetText().ToString(),FString(BeforeProperty[CultureIndex]));
        TestTrue(TEXT("The same interactive widget is preserved"),Row->GetChildren()->GetChildAt(1)==Property);
        TestFalse(TEXT("Narrative subtitle is available"),MiningText::Get(TEXT("Subtitle.StartRoom.001")).IsEmpty());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningQueuedLanguageTest,"BrainMining.Localization.QueuedNoticesAndAuthoredMinerals",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningQueuedLanguageTest::RunTest(const FString&)
{
    const FString PreviousLanguage=FInternationalization::Get().GetCurrentLanguage()->GetName();
    const FString PreviousLocale=FInternationalization::Get().GetCurrentLocale()->GetName();
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto* Instance=NewObject<UGameInstance>(GEngine);
    auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(World);Context.OwningGameInstance=Instance;
    World->SetGameInstance(Instance);Instance->Init();
    ON_SCOPE_EXIT
    {
        Instance->Shutdown();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);
        UKismetInternationalizationLibrary::SetCurrentLanguageAndLocale(PreviousLanguage,false);
        UKismetInternationalizationLibrary::SetCurrentLocale(PreviousLocale,false);
    };
    auto* Saves=Instance->GetSubsystem<UMiningSaveSubsystem>();
    Saves->TutorialActive=true;Saves->TutorialSoilPassed=true;
    Saves->RecordMineral(EMiningMineral::Copper);
    Saves->RecordMineral(EMiningMineral::Iron);
    if(!TestEqual(TEXT("Actual mineral pickups queue two tutorial notices"),Saves->CompletionNotices.Num(),2))return false;
    const FText VisibleNotice=FText::Format(FText::AsCultureInvariant(TEXT("✓  {0}")),Saves->CompletionNotices[0]);
    FMiningTreasureDefinition Authored;
    Authored.Id=TEXT("2");Authored.Name=FText::FromString(TEXT("铜矿"));
    const FText ExistingMineral=Authored.GetDisplayName();
    const FText ExistingMapTitle=MiningText::Get(TEXT("UI.Map.Title"));
    const TCHAR* Cultures[]={TEXT("en"),TEXT("zh-Hant"),TEXT("zh-Hans")};
    const TCHAR* ExpectedMinerals[]={TEXT("copper"),TEXT("銅礦"),TEXT("铜矿")};
    const TCHAR* ExpectedNotices[]={TEXT("✓  Collect 1 copper deposit"),TEXT("✓  收集 1 個銅礦"),TEXT("✓  收集1个铜矿")};
    const TCHAR* ExpectedMaps[]={TEXT("Mine map"),TEXT("礦道地圖"),TEXT("矿道地图")};
    for(int32 CultureIndex=0;CultureIndex<UE_ARRAY_COUNT(Cultures);++CultureIndex)
    {
        UKismetInternationalizationLibrary::SetCurrentLanguageAndLocale(Cultures[CultureIndex],false);
        FTextLocalizationManager::Get().EnableGameLocalizationPreview(Cultures[CultureIndex]);
        FTextLocalizationManager::Get().WaitForAsyncTasks();
        TestEqual(TEXT("Already displayed completion notice changes language"),VisibleNotice.ToString(),FString(ExpectedNotices[CultureIndex]));
        TestEqual(TEXT("Authored Chinese mineral name uses live translation"),ExistingMineral.ToString(),FString(ExpectedMinerals[CultureIndex]));
        TestEqual(TEXT("Map resource changes language"),ExistingMapTitle.ToString(),FString(ExpectedMaps[CultureIndex]));
    }
    Authored.Id=TEXT("custom");Authored.Name=FText::FromString(TEXT("Custom collectible"));
    TestEqual(TEXT("Custom definitions retain their authored name"),Authored.GetDisplayName().ToString(),Authored.Name.ToString());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiningSubtitlePacingTest,"BrainMining.Localization.SubtitleWordPacing",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMiningSubtitlePacingTest::RunTest(const FString&)
{
    const FString English=TEXT("Inspiration emerges when simple ideas come together and interact with each other.");
    const float Duration=SubtitleVoice::TextDuration(English);
    TestTrue(TEXT("English is timed by words instead of every letter"),Duration>2.f && Duration<8.f);
    TestTrue(TEXT("Sentence punctuation leaves reading time"),SubtitleVoice::TextDuration(English+TEXT("..."))>Duration);
    TestEqual(TEXT("Chinese retains character pacing"),SubtitleVoice::TextDuration(TEXT("一二三四五六七八九十")),2.f);
    const auto Clip=SubtitleVoice::Generate(English,123);
    TestEqual(TEXT("Audio and subtitle use the same duration"),Clip.Duration,Duration);
    TestFalse(TEXT("Word timing still produces voiced samples"),Clip.Samples.IsEmpty());
    TestTrue(TEXT("Whitespace alone does not produce voice"),SubtitleVoice::Generate(TEXT(" \n "),123).Samples.IsEmpty());
    return true;
}
#endif
