#include "MiningLocalizationSubsystem.h"
#include "MiningCollection.h"
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
