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
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Engine/Texture2D.h"
#include "Brushes/SlateNoResource.h"

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
				.Text_Lambda([this] { return FText::FromString(Message); })
				.Font(PopupFont(22.f))
				.ColorAndOpacity(FLinearColor::White)
			]
		];
}

TSharedRef<SWidget> UPopupCloseDesktopWidget::RebuildWidget()
{
 Windows.Reset();ArtBrushes.Reset();ArtTextures.Reset();
 for(int32 I=0;I<PopupCloseContent::ArtCount;++I)
 {
  const auto& Skin=PopupCloseContent::GetArt(I);
  auto Texture=LoadObject<UTexture2D>(nullptr,*(FString(TEXT("/Game/PopupClose/PopupArt/T_Popup"))+Skin.AssetName));
  ArtTextures.Add(Texture);auto Brush=MakeShared<FSlateBrush>();Brush->DrawAs=ESlateBrushDrawType::Image;
  Brush->SetResourceObject(Texture);Brush->ImageSize=Skin.Size;ArtBrushes.Add(Brush);
 }
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
			.Text(FText::FromString(TEXT("按 E 退出电脑")))
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
 const int32 Variant=(NextId-1)%PopupCloseContent::ArtCount;
 const bool bHasArt=ArtTextures.IsValidIndex(Variant) && ArtTextures[Variant]!=nullptr;
 const auto& Skin=PopupCloseContent::GetArt(Variant);
 const FVector2D Size=bHasArt?Skin.Size:Copy.Size;
	const float Width = Size.X;
	const float Height = Size.Y;
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
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(12.f,4.f,12.f,8.f)
			[
				SNew(SBox).WidthOverride(110.f).HeightOverride(40.f)
				[
					SNew(SButton).ButtonColorAndOpacity(FLinearColor(.15f,.42f,.72f))
					.IsEnabled_Lambda([WeakThis] { return WeakThis.IsValid() && WeakThis->bRepliesEnabled; })
					.OnClicked_Lambda([WeakThis,Id]()
					{
						if(auto Self=WeakThis.Get();Self && Self->bRepliesEnabled && Self->ClosePopup(Id))
						{
							++Self->ManuallyClosedCount;
							if(Self->OnReply)Self->OnReply();
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("回复"))).Font(PopupFont(22.f))
						.ColorAndOpacity(FLinearColor::White).Justification(ETextJustify::Center)
					]
				]
			]
		];

 if(bHasArt)Window=MakeArtPopup(Id,Variant);
	Canvas->AddSlot()
		.Offset(FMargin(X, Y, Width, Height))
		.Anchors(FAnchors(0.f, 0.f))
		.Alignment(FVector2D(0.f, 0.f))
		.AutoSize(false)
		.ZOrder(static_cast<float>(Id))
		[
			Window
		];

 const FVector4 Rect=Skin.CloseRect;
 const FVector2D ClosePoint=bHasArt?FVector2D((Rect.X+Rect.Z*.5f)*Width,(Rect.Y+Rect.W*.5f)*Height):FVector2D(Width-67.f,Height-28.f);
	Windows.Add({ Id, Window, FVector2D(X,Y), Size, ClosePoint });
}

TSharedRef<SWidget> UPopupCloseDesktopWidget::MakeArtPopup(int32 Id,int32 Variant)
{
 const auto& Skin=PopupCloseContent::GetArt(Variant);const auto& Copy=PopupCloseContent::Get(Id-1);
 auto Panel=SNew(SConstraintCanvas);const float W=Skin.Size.X,H=Skin.Size.Y;
 Panel->AddSlot().Offset(FMargin(0,0,W,H)).Alignment(FVector2D(0,0)).ZOrder(0)
 [SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
  [SNew(SImage).Image(ArtBrushes[Variant].Get()).Visibility(EVisibility::HitTestInvisible)]];
 if(Skin.bUseWorkText)
 {
  int32 LineBreaks=0;for(const TCHAR* P=Copy.Body;*P;++P)if(*P=='\n')++LineBreaks;
  const int32 BodyFont=Variant==2?(LineBreaks>2?11:15):16;
  const FVector4 R=Skin.TextRect;
  Panel->AddSlot().Offset(FMargin(R.X*W,R.Y*H,R.Z*W,R.W*H)).Alignment(FVector2D(0,0)).ZOrder(1)
  [SNew(SVerticalBox).Visibility(EVisibility::HitTestInvisible)
   +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,5)
    [SNew(STextBlock).Text(FText::FromString(Copy.Title)).Font(PopupFont(Variant==2?(LineBreaks>2?13:16):18)).ColorAndOpacity(FLinearColor(.035f,.035f,.04f))]
   +SVerticalBox::Slot().FillHeight(1)
    [SNew(STextBlock).Text(FText::FromString(Copy.Body)).Font(PopupFont(BodyFont)).AutoWrapText(true).ColorAndOpacity(FLinearColor(.035f,.035f,.04f))]];
 }
 static const FButtonStyle HitStyle=FButtonStyle().SetNormal(FSlateNoResource()).SetHovered(FSlateNoResource()).SetPressed(FSlateNoResource()).SetDisabled(FSlateNoResource()).SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
 const FVector4 R=Skin.CloseRect;TWeakObjectPtr<UPopupCloseDesktopWidget> WeakThis(this);
 Panel->AddSlot().Offset(FMargin(R.X*W,R.Y*H,R.Z*W,R.W*H)).Alignment(FVector2D(0,0)).ZOrder(2)
 [SNew(SButton).ButtonStyle(&HitStyle).ContentPadding(0).IsEnabled_Lambda([WeakThis]{return WeakThis.IsValid() && WeakThis->bRepliesEnabled;})
  .OnClicked_Lambda([WeakThis,Id]
  {
   if(auto Self=WeakThis.Get();Self && Self->bRepliesEnabled && Self->ClosePopup(Id))
   {++Self->ManuallyClosedCount;if(Self->OnReply)Self->OnReply();}
   return FReply::Handled();
  })];
 return Panel;
}

bool UPopupCloseDesktopWidget::ClosePopup(int32 Id)
{
	const int32 Index = Windows.IndexOfByPredicate([Id](const FWindow& Window) { return Window.Id == Id; });
	if (Index == INDEX_NONE || !Canvas.IsValid() || !Windows[Index].Root.IsValid())
	{
		return false;
	}

	Canvas->RemoveSlot(Windows[Index].Root.ToSharedRef());
	Windows.RemoveAt(Index);
 return true;
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
 return Window.Position+Window.ClosePoint;
}

bool UPopupCloseDesktopWidget::HasVariedWindowSizes() const
{
 for(const auto& Window:Windows)if(!Window.Size.Equals(Windows[0].Size))return true;
 return false;
}

void UPopupCloseDesktopWidget::ReleaseSlateResources(bool bReleaseChildren)
{
 Super::ReleaseSlateResources(bReleaseChildren);Windows.Reset();Canvas.Reset();
 ArtBrushes.Reset();ArtTextures.Reset();
}


TSharedRef<SWidget> UPopupCloseWarningWidget::RebuildWidget()
{
 WarningTexture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/PopupClose/PopupArt/T_PendingWarning.T_PendingWarning"));
 WarningBrush=MakeShared<FSlateBrush>();WarningBrush->SetResourceObject(WarningTexture);
 WarningBrush->DrawAs=ESlateBrushDrawType::Image;WarningBrush->ImageSize=FVector2D(24,76);
 return SNew(SVerticalBox)
 + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
 [SNew(SBox).WidthOverride(24).HeightOverride(76)
 [SNew(SImage).Image(WarningBrush.Get())]]
 + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
 [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.08f,.025f,.01f,.9f)).Padding(FMargin(8.f,2.f))
 [SNew(STextBlock).Text_Lambda([this] { return FText::AsNumber(PendingCount); }).Font(PopupFont(20.f)).ColorAndOpacity(FLinearColor(1.f,.85f,.3f))]];
}
