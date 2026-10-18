#include "Subtitles/SubtitleWidget.h"
#include "Subtitles/SubtitleSettings.h"

#include "Fonts/CompositeFont.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FSlateFontInfo SubtitleFont(float Size)
	{
		static TSharedPtr<const FCompositeFont> ChineseFont;
		if (!ChineseFont)
		{
			const TCHAR* Candidates[] = {
				TEXT("C:/Windows/Fonts/simhei.ttf"),
				TEXT("C:/Windows/Fonts/msyh.ttc"),
				TEXT("C:/Windows/Fonts/msyh.ttf")
			};
			for (const TCHAR* Path : Candidates)
			{
				if (FPaths::FileExists(Path))
				{
					ChineseFont = MakeShared<FStandaloneCompositeFont>(NAME_None, FString(Path), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
					break;
				}
			}
		}
		return ChineseFont ? FSlateFontInfo(ChineseFont, Size) : FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size);
	}
}

	/**
	 * The animation lives in Slate rather than UUserWidget::NativeTick. This
	 * keeps the effect active in packaged builds even when the UMG tick flag is
	 * disabled by the owning HUD or by a widget blueprint.
	 */
	class SSubtitleVisual : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SSubtitleVisual) {}
			SLATE_ARGUMENT(FText, Text)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Font = SubtitleFont(FMath::Clamp(GetDefault<USubtitleSettings>()->FontSize, 12, 72));
			CurrentText = Args._Text;
			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SAssignNew(ChromaticRedTextBlock, STextBlock)
					.Text(CurrentText)
					.Font(Font)
					.Justification(ETextJustify::Center)
					.AutoWrapText(false)
					.ColorAndOpacity(FLinearColor(1.f, 0.12f, 0.12f, 0.2f))
				]
				+ SOverlay::Slot()
				[
					SAssignNew(ChromaticCyanTextBlock, STextBlock)
					.Text(CurrentText)
					.Font(Font)
					.Justification(ETextJustify::Center)
					.AutoWrapText(false)
					.ColorAndOpacity(FLinearColor(0.12f, 0.85f, 1.f, 0.2f))
				]
				+ SOverlay::Slot()
				[
					SAssignNew(TextBlock, STextBlock)
					.Text(CurrentText)
					.Font(Font)
					.Justification(ETextJustify::Center)
					.AutoWrapText(false)
					.ColorAndOpacity(FLinearColor::White)
					.ShadowOffset(FVector2D(1.f, 1.f))
					.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.72f))
				]
			];
		}

		void SetText(const FText& InText)
		{
			CurrentText = InText;
			AnimationTime = 0.f;
			if (TextBlock.IsValid()) TextBlock->SetText(CurrentText);
			if (ChromaticRedTextBlock.IsValid()) ChromaticRedTextBlock->SetText(CurrentText);
			if (ChromaticCyanTextBlock.IsValid()) ChromaticCyanTextBlock->SetText(CurrentText);
		}

		virtual void Tick(const FGeometry& AllottedGeometry, const double CurrentTime, const float InDeltaTime) override
		{
			SCompoundWidget::Tick(AllottedGeometry, CurrentTime, InDeltaTime);
			AnimationTime += FMath::Max(0.f, InDeltaTime);
			const auto* Settings = GetDefault<USubtitleSettings>();
			const int32 Size = FMath::Clamp(Settings->FontSize, 12, 72);
			if (Font.Size != Size)
			{
				Font.Size = Size;
				TextBlock->SetFont(Font);
				ChromaticRedTextBlock->SetFont(Font);
				ChromaticCyanTextBlock->SetFont(Font);
			}
			const float Float = FMath::IsFinite(Settings->FloatStrength) ? FMath::Clamp(Settings->FloatStrength, 0.f, 3.f) : 1.4f;
			const float Chromatic = FMath::IsFinite(Settings->ChromaticStrength) ? FMath::Clamp(Settings->ChromaticStrength, 0.f, 3.f) : 1.4f;
			const float Alpha = FMath::Min(0.2f * Chromatic, 0.6f);
			ChromaticRedTextBlock->SetColorAndOpacity(FLinearColor(1.f, 0.12f, 0.12f, Alpha));
			ChromaticCyanTextBlock->SetColorAndOpacity(FLinearColor(0.12f, 0.85f, 1.f, Alpha));

			// A few pixels of vertical motion makes the line feel buoyant while
			// the tiny rotation prevents it from reading as a rigid HUD element.
			const float MainX = FMath::Sin(AnimationTime * 1.35f) * 2.1f * Float;
			const float MainY = FMath::Sin(AnimationTime * 1.9f) * 5.5f * Float;
			const float MainAngle = FMath::DegreesToRadians(FMath::Sin(AnimationTime * 1.2f) * 0.9f * Float);
			const float CosAngle = FMath::Cos(MainAngle);
			const float SinAngle = FMath::Sin(MainAngle);
			const FMatrix2x2 MainMatrix(CosAngle, SinAngle, -SinAngle, CosAngle);

			SetTransform(TextBlock, MainMatrix, FVector2D(MainX, MainY));
			SetTransform(
				ChromaticRedTextBlock,
				MainMatrix,
				FVector2D(MainX + (1.8f + FMath::Sin(AnimationTime * 1.7f + 0.7f) * 0.5f) * Chromatic, MainY - 0.45f * Chromatic));
			SetTransform(
				ChromaticCyanTextBlock,
				MainMatrix,
				FVector2D(MainX + (-1.8f + FMath::Sin(AnimationTime * 1.55f + 2.1f) * 0.5f) * Chromatic, MainY + 0.45f * Chromatic));
		}

	private:
		static void SetTransform(const TSharedPtr<STextBlock>& Block, const FMatrix2x2& Matrix, const FVector2D& Offset)
		{
			if (Block.IsValid())
			{
				Block->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
				Block->SetRenderTransform(FSlateRenderTransform(Matrix, Offset));
			}
		}

		FSlateFontInfo Font;
		FText CurrentText;
		float AnimationTime = 0.f;
		TSharedPtr<STextBlock> TextBlock;
		TSharedPtr<STextBlock> ChromaticRedTextBlock;
		TSharedPtr<STextBlock> ChromaticCyanTextBlock;
	};

void USubtitleWidget::SetLine(const FText& Line)
{
	CurrentLine = Line;
	if (SubtitleVisual.IsValid())
	{
		SubtitleVisual->SetText(CurrentLine);
	}
}

TSharedRef<SWidget> USubtitleWidget::CreateStylePreview(const FText& Text)
{
	return SNew(SSubtitleVisual).Text(Text);
}

TSharedRef<SWidget> USubtitleWidget::RebuildWidget()
{
	return SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(48.f, 0.f, 48.f, 72.f))
		[
			SNew(SBox)
			.Padding(FMargin(18.f, 10.f))
			[
				SAssignNew(SubtitleVisual, SSubtitleVisual)
				.Text(CurrentLine)
			]
		];
}
