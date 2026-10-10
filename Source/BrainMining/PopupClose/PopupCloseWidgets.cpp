#include "PopupClose/PopupCloseWidgets.h"

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

	struct FPopupCopy
	{
		const TCHAR* Title;
		const TCHAR* Body;
		FLinearColor Color;
	};

	const FPopupCopy& RandomCopy()
	{
		static const FPopupCopy Copies[] = {
			{ TEXT("系统更新"), TEXT("发现重要更新。是否立即重启并安装？"), FLinearColor(0.93f, 0.94f, 0.96f) },
			{ TEXT("内存不足"), TEXT("可用内存即将耗尽。请关闭一些窗口。"), FLinearColor(1.f, 0.95f, 0.82f) },
			{ TEXT("磁盘错误"), TEXT("驱动器 C: 报告了无法读取的扇区。"), FLinearColor(1.f, 0.86f, 0.84f) },
			{ TEXT("是否保存"), TEXT("未保存的更改将会丢失。要保存吗？"), FLinearColor(0.86f, 0.93f, 1.f) },
			{ TEXT("广告"), TEXT("恭喜获得免费加速！点击领取，不要关闭。"), FLinearColor(0.95f, 0.9f, 1.f) },
			{ TEXT("驱动安装"), TEXT("新的显示驱动已就绪，需要立刻安装。"), FLinearColor(0.9f, 0.96f, 0.9f) },
			{ TEXT("网络中断"), TEXT("与服务器的连接已断开，正在重试。"), FLinearColor(0.95f, 0.95f, 0.95f) },
			{ TEXT("确认删除"), TEXT("这些文件将永久删除。此操作无法撤销。"), FLinearColor(1.f, 0.9f, 0.78f) }
		};
		return Copies[FMath::RandRange(0, UE_ARRAY_COUNT(Copies) - 1)];
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
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 28.f))
		[
			SNew(STextBlock)
			.Visibility(EVisibility::HitTestInvisible)
			.Text(FText::FromString(TEXT("点击窗口右上角关闭")))
			.Font(PopupFont(16.f))
			.ColorAndOpacity(FLinearColor(0.8f, 0.88f, 1.f))
		];
}

void UPopupCloseDesktopWidget::SpawnPopup()
{
	if (!Canvas.IsValid())
	{
		return;
	}

	FVector2D ViewportSize(1280.f, 720.f);
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D PixelSize;
		GEngine->GameViewport->GetViewportSize(PixelSize);
		const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
		ViewportSize = PixelSize / Scale;
	}

	const FPopupCopy& Copy = RandomCopy();
	const float Width = 340.f;
	const float Height = 176.f;
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
						.Font(PopupFont(16.f))
						.ColorAndOpacity(FLinearColor(0.08f, 0.08f, 0.1f))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(32.f)
						.HeightOverride(28.f)
						[
							SNew(SButton)
							.ButtonColorAndOpacity(FLinearColor(0.78f, 0.16f, 0.14f))
							.OnClicked_Lambda([WeakThis, Id]()
							{
								if (UPopupCloseDesktopWidget* Self = WeakThis.Get())
								{
									Self->ClosePopup(Id);
								}
								return FReply::Handled();
							})
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("×")))
								.Font(PopupFont(16.f))
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
				.Font(PopupFont(14.f))
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

	Windows.Add({ Id, Window });
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
