#include "Subtitles/SubtitleWidget.h"

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
			SLATE_ARGUMENT(FSlateFontInfo, Font)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Font = Args._Font;
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

			// A few pixels of vertical motion makes the line feel buoyant while
			// the tiny rotation prevents it from reading as a rigid HUD element.
			const float MainX = FMath::Sin(AnimationTime * 1.35f) * 2.1f;
			const float MainY = FMath::Sin(AnimationTime * 1.9f) * 5.5f;
			const float MainAngle = FMath::DegreesToRadians(FMath::Sin(AnimationTime * 1.2f) * 0.9f);
			const float CosAngle = FMath::Cos(MainAngle);
			const float SinAngle = FMath::Sin(MainAngle);
			const FMatrix2x2 MainMatrix(CosAngle, SinAngle, -SinAngle, CosAngle);

			SetTransform(TextBlock, MainMatrix, FVector2D(MainX, MainY));
			SetTransform(
				ChromaticRedTextBlock,
				MainMatrix,
				FVector2D(MainX + 1.8f + FMath::Sin(AnimationTime * 1.7f + 0.7f) * 0.5f, MainY - 0.45f));
			SetTransform(
				ChromaticCyanTextBlock,
				MainMatrix,
				FVector2D(MainX - 1.8f + FMath::Sin(AnimationTime * 1.55f + 2.1f) * 0.5f, MainY + 0.45f));
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
				.Font(SubtitleFont(22.f))
			]
		];
}
