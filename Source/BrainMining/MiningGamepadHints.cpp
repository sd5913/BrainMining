#include "MiningGamepadHints.h"
#include "MiningGamepadSubsystem.h"
#include "BrainMiningFonts.h"
#include "Subtitles/SubtitleSettings.h"
#include "Brushes/SlateImageBrush.h"
#include "Misc/Paths.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

const TCHAR* MiningGamepadGlyphName(EMiningGamepadGlyph Glyph)
{
    static const TCHAR* Names[] = {TEXT(""), TEXT("A"), TEXT("B"), TEXT("X"), TEXT("Y"), TEXT("LT"), TEXT("RT"), TEXT("LB"), TEXT("RB"), TEXT("RightStick"), TEXT("DPad"), TEXT("DPadUp"), TEXT("Menu")};
    const int32 Index = static_cast<int32>(Glyph);
    return Index >= 0 && Index < UE_ARRAY_COUNT(Names) ? Names[Index] : TEXT("");
}

namespace
{
    class SMiningGamepadHints : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SMiningGamepadHints) {}
            SLATE_ARGUMENT(TWeakObjectPtr<UMiningGamepadSubsystem>, Input)
        SLATE_END_ARGS()

        void Construct(const FArguments& Args)
        {
            Input = Args._Input;
            for (int32 Index = 1; Index < static_cast<int32>(EMiningGamepadGlyph::Count); ++Index)
            {
                const auto Glyph = static_cast<EMiningGamepadGlyph>(Index);
                const FString Path = FPaths::ProjectContentDir() / TEXT("UI/Gamepad") / (FString(MiningGamepadGlyphName(Glyph)) + TEXT(".svg"));
                Brushes.Add(Glyph, MakeShared<FSlateVectorImageBrush>(Path, FVector2D(30, 24)));
            }
            ChildSlot.Padding(FMargin(18, 10))[SAssignNew(Row, SHorizontalBox)];
            Refresh();
        }

        virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override
        {
            SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
            Refresh();
            AnimationTime += FMath::Max(0.f, DeltaTime);
            const float Strength = FMath::Clamp(GetDefault<USubtitleSettings>()->FloatStrength, 0.f, 3.f) * .5f;
            Row->SetRenderTransform(FSlateRenderTransform(FVector2D(FMath::Sin(AnimationTime * 1.35f) * Strength, FMath::Sin(AnimationTime * 1.9f) * 2.f * Strength)));
        }

    private:
        TSharedRef<SWidget> Label(const FString& Text) const
        {
            const auto Font = BrainMiningFonts::Title(14);
            const float Strength = FMath::Clamp(GetDefault<USubtitleSettings>()->ChromaticStrength, 0.f, 3.f) * .5f;
            const auto Content = FText::FromString(Text);
            return SNew(SOverlay)
                + SOverlay::Slot()[SNew(STextBlock).Text(Content).Font(Font)
                    .ColorAndOpacity(FLinearColor(1.f, .12f, .12f, .2f * Strength))
                    .RenderTransform(FSlateRenderTransform(FVector2D(1.8f * Strength, -.45f * Strength)))]
                + SOverlay::Slot()[SNew(STextBlock).Text(Content).Font(Font)
                    .ColorAndOpacity(FLinearColor(.12f, .85f, 1.f, .2f * Strength))
                    .RenderTransform(FSlateRenderTransform(FVector2D(-1.8f * Strength, .45f * Strength)))]
                + SOverlay::Slot()[SNew(STextBlock).Text(Content).Font(Font).ColorAndOpacity(FLinearColor::White)
                    .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, .72f))];
        }

        void AddIcon(EMiningGamepadGlyph Glyph)
        {
            if (const auto* Brush = Brushes.Find(Glyph))
                Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
                    [SNew(SBox).WidthOverride(28).HeightOverride(22)[SNew(SImage).Image(Brush->Get())]];
        }

        void Refresh()
        {
            const auto Current = Input.IsValid() ? Input->GetHintEntries() : TArray<FMiningGamepadHint>();
            if (Current == Entries) return;
            Entries = Current;
            Row->ClearChildren();
            for (int32 Index = 0; Index < Entries.Num(); ++Index)
            {
                const auto& Entry = Entries[Index];
                AddIcon(Entry.Key);
                if (Entry.AlternateKey != EMiningGamepadGlyph::None)
                {
                    Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)[Label(TEXT("/"))];
                    AddIcon(Entry.AlternateKey);
                }
                Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, Index + 1 == Entries.Num() ? 0 : 14, 0)[Label(Entry.Label)];
            }
        }

        TWeakObjectPtr<UMiningGamepadSubsystem> Input;
        TArray<FMiningGamepadHint> Entries;
        TMap<EMiningGamepadGlyph, TSharedPtr<FSlateVectorImageBrush>> Brushes;
        TSharedPtr<SHorizontalBox> Row;
        float AnimationTime = 0;
    };
}

TSharedRef<SWidget> MakeMiningGamepadHints(UMiningGamepadSubsystem* Input)
{
    return SNew(SMiningGamepadHints).Input(Input);
}
