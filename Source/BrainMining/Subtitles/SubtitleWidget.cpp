#include "Subtitles/SubtitleWidget.h"

#include "Fonts/CompositeFont.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
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

void USubtitleWidget::SetLine(const FText& Line)
{
	CurrentLine = Line;
	if (TextBlock.IsValid())
	{
		TextBlock->SetText(CurrentLine);
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
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.68f))
			.Padding(FMargin(18.f, 10.f))
			[
				SAssignNew(TextBlock, STextBlock)
				.Text(CurrentLine)
				.Font(SubtitleFont(22.f))
				.Justification(ETextJustify::Center)
				.AutoWrapText(false)
				.ColorAndOpacity(FLinearColor::White)
				.ShadowOffset(FVector2D(1.f, 1.f))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f))
			]
		];
}
