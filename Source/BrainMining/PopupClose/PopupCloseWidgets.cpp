#include "PopupClose/PopupCloseWidgets.h"
#include "PopupClose/PopupCloseContent.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Fonts/CompositeFont.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

namespace
{
	FSlateFontInfo PopupFont(float Size)
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

		if (ChineseFont)
		{
			return FSlateFontInfo(ChineseFont, Size);
		}
		return FCoreStyle::GetDefaultFontStyle("Regular", Size);
	}

}

TSharedRef<SWidget> UPopupClosePromptWidget::RebuildWidget()
{
	return SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 72.f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.72f))
			.Padding(FMargin(18.f, 10.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("按 E 使用电脑")))
				.Font(PopupFont(22.f))
				.ColorAndOpacity(FLinearColor::White)
			]
		];
}

TSharedRef<SWidget> UPopupCloseDesktopWidget::RebuildWidget()
{
	Canvas = SNew(SConstraintCanvas);

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.Visibility(EVisibility::HitTestInvisible)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.04f, 0.1f, 0.24f, 1.f))
		]
		+ SOverlay::Slot()
		[
			Canvas.ToSharedRef()
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(32.f)
		[
			SNew(STextBlock).Visibility(EVisibility::HitTestInvisible)
			.Text(FText::FromString(TEXT("工作台")))
			.Font(PopupFont(32.f)).ColorAndOpacity(FLinearColor(.6f,.76f,.88f))
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 28.f))
		[
			SNew(STextBlock)
			.Visibility(EVisibility::HitTestInvisible)
			.Text(FText::FromString(TEXT("待办    /    工作消息    /    项目排期")))
			.Font(PopupFont(20.f))
			.ColorAndOpacity(FLinearColor(0.8f, 0.88f, 1.f))
		];
}

void UPopupCloseDesktopWidget::SpawnPopup()
{
	if (!Canvas.IsValid())
	{
		return;
	}

	const FVector2D ViewportSize(PopupCloseContent::DesktopWidth,PopupCloseContent::DesktopHeight);
	const auto& Copy = PopupCloseContent::Get(NextId-1);
	const float Width = Copy.Size.X;
	const float Height = Copy.Size.Y;
	const float X = FMath::FRandRange(24.f, FMath::Max(24.f, ViewportSize.X - Width - 24.f));
	const float Y = FMath::FRandRange(24.f, FMath::Max(24.f, ViewportSize.Y - Height - 80.f));
	const int32 Id = NextId++;

	TWeakObjectPtr<UPopupCloseDesktopWidget> WeakThis(this);
	TSharedRef<SWidget> Window = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(Copy.Color)
		.Padding(0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(Copy.Color * 0.72f)
				.Padding(FMargin(10.f, 4.f, 4.f, 4.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Copy.Title))
						.Font(PopupFont(22.f))
						.ColorAndOpacity(FLinearColor(0.08f, 0.08f, 0.1f))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(48.f)
						.HeightOverride(40.f)
						[
							SNew(SButton)
							.ButtonColorAndOpacity(FLinearColor(0.78f, 0.16f, 0.14f))
							.OnClicked_Lambda([WeakThis, Id]()
							{
								if (UPopupCloseDesktopWidget* Self = WeakThis.Get())
								{
									Self->ClosePopup(Id);
									++Self->ManuallyClosedCount;
								}
								return FReply::Handled();
							})
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("×")))
								.Font(PopupFont(24.f))
								.Justification(ETextJustify::Center)
								.ColorAndOpacity(FLinearColor::White)
							]
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(FMargin(12.f, 10.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Copy.Body))
				.Font(PopupFont(20.f))
				.AutoWrapText(true)
				.ColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.12f))
			]
		];

	Canvas->AddSlot()
		.Offset(FMargin(X, Y, Width, Height))
		.Anchors(FAnchors(0.f, 0.f))
		.Alignment(FVector2D(0.f, 0.f))
		.AutoSize(false)
		.ZOrder(static_cast<float>(Id))
		[
			Window
		];

	Windows.Add({ Id, Window, FVector2D(X,Y), Copy.Size });
}

void UPopupCloseDesktopWidget::ClosePopup(int32 Id)
{
	const int32 Index = Windows.IndexOfByPredicate([Id](const FWindow& Window) { return Window.Id == Id; });
	if (Index == INDEX_NONE || !Canvas.IsValid() || !Windows[Index].Root.IsValid())
	{
		return;
	}

	Canvas->RemoveSlot(Windows[Index].Root.ToSharedRef());
	Windows.RemoveAt(Index);
}

void UPopupCloseDesktopWidget::ClearOldestPopup()
{
 if(Windows.Num())ClosePopup(Windows[0].Id);
}

void UPopupCloseDesktopWidget::ClearPopups()
{
 while(Windows.Num())ClearOldestPopup();
 ManuallyClosedCount=0;
}

FVector2D UPopupCloseDesktopWidget::GetLatestClosePoint() const
{
 if(!Windows.Num())return FVector2D::ZeroVector;
 const auto& Window=Windows.Last();
 return Window.Position+FVector2D(Window.Size.X-28.f,24.f);
}

bool UPopupCloseDesktopWidget::HasVariedWindowSizes() const
{
 for(const auto& Window:Windows)if(!Window.Size.Equals(Windows[0].Size))return true;
 return false;
}

void UPopupCloseDesktopWidget::ReleaseSlateResources(bool bReleaseChildren)
{
 Super::ReleaseSlateResources(bReleaseChildren);Windows.Reset();Canvas.Reset();
}
