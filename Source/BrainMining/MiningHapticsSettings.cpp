#include "MiningHapticsSubsystem.h"
#include "MiningLocalizationSubsystem.h"
#include "BrainMiningFonts.h"
#include "MiningMenuStyle.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Text/STextBlock.h"

void UMiningHapticsSubsystem::LoadPreferences(const FString& Filename)
{
    FConfigFile Config;
    if (!IFileManager::Get().FileExists(*Filename)) return;
    Config.Read(Filename);
    Config.GetBool(TEXT("BrainMining.Haptics"), TEXT("Enabled"), bEnabled);
    Config.GetBool(TEXT("BrainMining.Haptics"), TEXT("Continuous"), bContinuous);
    Config.GetBool(TEXT("BrainMining.Haptics"), TEXT("Optional"), bOptional);
    float Loaded = Strength;
    if (Config.GetFloat(TEXT("BrainMining.Haptics"), TEXT("Strength"), Loaded)) Strength = FMath::IsFinite(Loaded) ? FMath::Clamp(Loaded, 0.f, 1.f) : .75f;
    Stop();
}

void UMiningHapticsSubsystem::SavePreferences(const FString& Filename) const
{
    FConfigFile Config;
    Config.SetBool(TEXT("BrainMining.Haptics"), TEXT("Enabled"), bEnabled);
    Config.SetBool(TEXT("BrainMining.Haptics"), TEXT("Continuous"), bContinuous);
    Config.SetBool(TEXT("BrainMining.Haptics"), TEXT("Optional"), bOptional);
    Config.SetFloat(TEXT("BrainMining.Haptics"), TEXT("Strength"), Strength);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    if (!Config.Write(Filename)) UE_LOG(LogTemp, Warning, TEXT("Could not save vibration settings: %s"), *Filename);
}

void UMiningHapticsSubsystem::Save() const { SavePreferences(FPaths::ProjectSavedDir() / TEXT("Config/BrainMiningHaptics.ini")); }
void UMiningHapticsSubsystem::SetEnabled(bool Enabled) { bEnabled = Enabled; if (!Enabled) Stop(); Save(); }
void UMiningHapticsSubsystem::SetStrength(float Value) { Strength = FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, 1.f) : .75f; if (Strength <= 0) Stop(); Save(); }
void UMiningHapticsSubsystem::SetContinuousEnabled(bool Enabled) { bContinuous = Enabled; Continuous = FVector2D::ZeroVector; Save(); }
void UMiningHapticsSubsystem::SetOptionalEnabled(bool Enabled) { bOptional = Enabled; Save(); }

TSharedRef<SWidget> UMiningHapticsSubsystem::MakeSettingsWidget()
{
    TWeakObjectPtr<UMiningHapticsSubsystem> Weak(this);
    auto Rows = SNew(SVerticalBox);
    Rows->AddSlot().AutoHeight().Padding(0,18,0,8)[MiningMenuStyle::Section(MiningText::Get(TEXT("UI.Settings.Haptics")))];
    for (int32 Option = 0; Option < 3; ++Option)
    {
        Rows->AddSlot().AutoHeight().Padding(0, 4)[SNew(SButton).ButtonStyle(MiningMenuStyle::Button()).ContentPadding(FMargin(18,10))
            .OnClicked_Lambda([Weak, Option]()
            {
                if (Weak.IsValid())
                {
                    if (Option == 0) Weak->SetEnabled(!Weak->bEnabled);
                    else if (Option == 1) Weak->SetContinuousEnabled(!Weak->bContinuous);
                    else Weak->SetOptionalEnabled(!Weak->bOptional);
                }
                return FReply::Handled();
            })[SNew(STextBlock).Font(BrainMiningFonts::Body(17)).ColorAndOpacity(FSlateColor::UseForeground()).Text_Lambda([Weak, Option]()
            {
                const bool Enabled = Weak.IsValid() && (Option == 0 ? Weak->bEnabled : Option == 1 ? Weak->bContinuous : Weak->bOptional);
                const TCHAR* Key = Option == 0 ? TEXT("UI.Settings.HapticsEnabled") : Option == 1 ? TEXT("UI.Settings.HapticsContinuous") : TEXT("UI.Settings.HapticsOptional");
                return FText::Format(FText::AsCultureInvariant(TEXT("{0}     [ {1} ]")), MiningText::Get(Key), FText::AsCultureInvariant(Enabled ? TEXT("×") : TEXT(" ")));
            })]];
    }
    Rows->AddSlot().AutoHeight().Padding(0, 8)[SNew(STextBlock).Font(BrainMiningFonts::Style("Regular", 16)).Text_Lambda([Weak]()
    {
        return FText::FormatNamed(MiningText::Get(TEXT("UI.Settings.HapticsStrength")), TEXT("Strength"), FText::AsPercent(Weak.IsValid() ? Weak->Strength : 0.f));
    })];
    Rows->AddSlot().AutoHeight()[SNew(SSlider).Style(MiningMenuStyle::Slider()).StepSize(.05f).Value_Lambda([Weak]() { return Weak.IsValid() ? Weak->Strength : 0.f; })
        .OnValueChanged_Lambda([Weak](float Value) { if (Weak.IsValid()) Weak->SetStrength(Value); })];
    return Rows;
}
