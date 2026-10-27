#include "MiningLocalizationSubsystem.h"
#include "MiningAchievementProvider.h"
#include "BrainMiningFonts.h"
#include "MiningMenuStyle.h"
#include "HAL/PlatformMisc.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/StringTableRegistry.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScaleBox.h"

#if WITH_EDITOR
namespace
{
    int32 EditorPreviewUsers = 0;
    bool PreviousPreviewEnabled = false;
    FString PreviousPreviewLanguage;
}
#endif

namespace MiningText
{
    void RegisterTables()
    {
        if (!FStringTableRegistry::Get().FindStringTable(TEXT("BrainMining.Game")).IsValid())
        {
            LOCTABLE_FROMFILE_GAME("BrainMining.Game", "BrainMining.Game", "Localization/Tables/Game.csv");
        }
    }

    FText Get(const TCHAR* Key)
    {
        RegisterTables();
        return FText::FromStringTable(TEXT("BrainMining.Game"), Key);
    }
}

FString UMiningLocalizationSubsystem::ResolveCulture(const FString& Language)
{
    const FString Normalized = Language.ToLower().Replace(TEXT("_"), TEXT("-"));
    if (Normalized == TEXT("tchinese") || Normalized.StartsWith(TEXT("zh-hant")) ||
        Normalized == TEXT("zh-tw") || Normalized == TEXT("zh-hk") || Normalized == TEXT("zh-mo"))
        return TEXT("zh-Hant");
    if (Normalized == TEXT("schinese") || Normalized == TEXT("zh") || Normalized.StartsWith(TEXT("zh-")))
        return TEXT("zh-Hans");
    return TEXT("en");
}

bool UMiningLocalizationSubsystem::IsSupportedPreference(const FString& Preference)
{
    return Preference == TEXT("auto") || Preference == TEXT("zh-Hans") ||
        Preference == TEXT("zh-Hant") || Preference == TEXT("en");
}

void UMiningLocalizationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
#if WITH_EDITOR
    if (GIsEditor)
    {
        auto& Manager = FTextLocalizationManager::Get();
        if (EditorPreviewUsers++ == 0)
        {
            PreviousPreviewEnabled = Manager.IsGameLocalizationPreviewEnabled();
            PreviousPreviewLanguage = Manager.GetConfiguredGameLocalizationPreviewLanguage();
        }
        bOwnsEditorPreview = true;
    }
#endif
    MiningText::RegisterTables();
    const FString PlatformLanguage = IsRunningCommandlet() ? FString() : MiningPlatform::GameLanguage();
    AutomaticCulture = ResolveCulture(PlatformLanguage.IsEmpty() ? FPlatformMisc::GetDefaultLanguage() : PlatformLanguage);
    FString SavedPreference;
    if (GConfig->GetString(TEXT("BrainMining.Localization"), TEXT("LanguagePreference"), SavedPreference, GGameUserSettingsIni) &&
        IsSupportedPreference(SavedPreference)) LanguagePreference = SavedPreference;
    ApplyLanguage();
}

FString UMiningLocalizationSubsystem::GetLanguage() const
{
    return LanguagePreference == TEXT("auto") ? AutomaticCulture : LanguagePreference;
}

void UMiningLocalizationSubsystem::Deinitialize()
{
#if WITH_EDITOR
    if (bOwnsEditorPreview && --EditorPreviewUsers == 0)
    {
        auto& Manager = FTextLocalizationManager::Get();
        if (PreviousPreviewEnabled) Manager.EnableGameLocalizationPreview(PreviousPreviewLanguage);
        else Manager.DisableGameLocalizationPreview();
        Manager.WaitForAsyncTasks();
    }
#endif
    bOwnsEditorPreview = false;
    OnLanguageChanged.Clear();
    Super::Deinitialize();
}

bool UMiningLocalizationSubsystem::ApplyLanguage()
{
    const FString Culture = GetLanguage();
#if WITH_EDITOR
    if (GIsEditor) FTextLocalizationManager::Get().EnableGameLocalizationPreview(Culture);
    else
#endif
    if (!UKismetInternationalizationLibrary::SetCurrentLanguageAndLocale(Culture, false)) return false;
    FTextLocalizationManager::Get().WaitForAsyncTasks();
    OnLanguageChanged.Broadcast(Culture);
    return true;
}

bool UMiningLocalizationSubsystem::SetLanguagePreference(const FString& Preference)
{
    if (!IsSupportedPreference(Preference)) return false;
    const FString Previous = LanguagePreference;
    LanguagePreference = Preference;
    if (!ApplyLanguage())
    {
        LanguagePreference = Previous;
        return false;
    }
    GConfig->SetString(TEXT("BrainMining.Localization"), TEXT("LanguagePreference"), *LanguagePreference, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
    return true;
}

TSharedRef<SWidget> UMiningLocalizationSubsystem::MakeLanguageSelector()
{
    auto Choices = SNew(SHorizontalBox);
    const TCHAR* Preferences[] = {TEXT("auto"), TEXT("zh-Hans"), TEXT("en"), TEXT("zh-Hant")};
    const TCHAR* Labels[] = {TEXT("UI.Language.Auto"), TEXT("UI.Language.Simplified"), TEXT("UI.Language.English"), TEXT("UI.Language.Traditional")};
    TWeakObjectPtr<UMiningLocalizationSubsystem> WeakThis(this);
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Preferences); ++Index)
    {
        const FString Preference(Preferences[Index]);
        auto ChoiceButton = SNew(SButton).ButtonStyle(MiningMenuStyle::Button()).HAlign(HAlign_Center).ContentPadding(FMargin(6, 10))
            .OnClicked_Lambda([WeakThis, Preference]()
            {
                if (WeakThis.IsValid()) WeakThis->SetLanguagePreference(Preference);
                return FReply::Handled();
            });
        const TWeakPtr<SButton> WeakButton(ChoiceButton);
        ChoiceButton->SetContent(
            SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
            [SNew(STextBlock).Text(MiningText::Get(Labels[Index])).Font(BrainMiningFonts::Body(17))
                .ColorAndOpacity_Lambda([WeakThis, Preference, WeakButton]()
                {
                    const auto Button = WeakButton.Pin();
                    if (Button.IsValid() && (Button->IsHovered() || Button->IsPressed())) return FSlateColor::UseForeground();
                    return FSlateColor(WeakThis.IsValid() && WeakThis->GetLanguagePreference() == Preference ?
                        MiningMenuStyle::Gold : MiningMenuStyle::Cream);
                })]);
        Choices->AddSlot().FillWidth(1).Padding(3, 0)[ChoiceButton];
    }
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0, 8)
            [SNew(STextBlock).Text(MiningText::Get(TEXT("UI.Settings.Language"))).Font(BrainMiningFonts::Title(19)).ColorAndOpacity(MiningMenuStyle::Gold)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[Choices];
}

TSharedRef<SWidget> UMiningLanguageWidget::RebuildWidget()
{
    if (auto* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
        if (auto* Languages = Instance->GetSubsystem<UMiningLocalizationSubsystem>()) return Languages->MakeLanguageSelector();
    return SNullWidget::NullWidget;
}
