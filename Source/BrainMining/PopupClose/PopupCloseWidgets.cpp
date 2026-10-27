#include "PopupClose/PopupCloseWidgets.h"
#include "MiningLocalizationSubsystem.h"
#include "BrainMiningFonts.h"
#include "PopupClose/PopupCloseContent.h"

#include "Blueprint/WidgetLayoutLibrary.h"
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
#include "Widgets/Layout/SScaleBox.h"
#include "Brushes/SlateNoResource.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

namespace
{
	class SPopupCloseComputerCursor : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPopupCloseComputerCursor) {} SLATE_END_ARGS()
		void Construct(const FArguments&) { SetVisibility(EVisibility::HitTestInvisible); }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(64,80); }
		virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool) const override
		{
			// Center is the cursor hotspot. Integer blocks preserve the crisp
			// black-and-white arrow used by early desktop operating systems.
			static const TCHAR* Rows[]={
				TEXT("B"),TEXT("BB"),TEXT("BWB"),TEXT("BWWB"),TEXT("BWWWB"),
				TEXT("BWWWWB"),TEXT("BWWWWWB"),TEXT("BWWWWWWB"),TEXT("BWWWWWWWB"),
				TEXT("BWWWWBBBBB"),TEXT("BWWBWB"),TEXT("BWB BWB"),TEXT("BB  BWB"),
				TEXT("B    BWB"),TEXT("     BWB"),TEXT("      BB")};
			const FVector2D Hotspot=G.GetLocalSize()*.5f;
			const auto* Brush=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
			for(int32 Y=0;Y<UE_ARRAY_COUNT(Rows);++Y)
				for(int32 X=0;Rows[Y][X];++X)
					if(Rows[Y][X]!=TEXT(' '))
						FSlateDrawElement::MakeBox(Out,Layer+1,G.ToPaintGeometry(FVector2f(2,2),
							FSlateLayoutTransform(FVector2f(Hotspot+FVector2D(X*2,Y*2)))),Brush,ESlateDrawEffect::None,
							(Rows[Y][X]==TEXT('W')?FLinearColor::White:FLinearColor::Black)*Style.GetColorAndOpacityTint());
			return Layer+1;
		}
	};
	FSlateFontInfo PopupFont(float Size)
	{
        return BrainMiningFonts::Body(Size);
    }

}

TSharedRef<SWidget> MakePopupCloseComputerCursor() { return SNew(SPopupCloseComputerCursor); }

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
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 28.f))
		[
			SNew(STextBlock)
			.Visibility(EVisibility::HitTestInvisible)
			.Text(MiningText::Get(TEXT("Office.Hint.Leave")))
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
	int32 ContentIndex=NextId<=2 || WorkElapsed<0.f?NextId-1:PopupCloseContent::ForProgress(WorkElapsed,NextId-3);
 if(NextId>2 && WorkElapsed>=32.f && !bReconnectShown){ContentIndex=PopupCloseContent::ReconnectIndex;bReconnectShown=true;}
	const auto& Copy = PopupCloseContent::Get(ContentIndex);
 const bool bIntroduction=PopupCloseContent::IsIntroduction(NextId-1);
 const bool bThemePage=NextId==2;
 const int32 Variant=PopupCloseContent::ArtVariant(ContentIndex);
 const bool bHasArt=ArtTextures.IsValidIndex(Variant) && ArtTextures[Variant]!=nullptr;
 const auto& Skin=PopupCloseContent::GetArt(Variant);
 const FVector2D Size=bHasArt && !bIntroduction?Skin.Size:Copy.Size;
	const float Width = Size.X;
	const float Height = Size.Y;
	const float X = bIntroduction?(ViewportSize.X-Width)*.5f:FMath::FRandRange(24.f, FMath::Max(24.f, ViewportSize.X - Width - 24.f));
	const float Y = bIntroduction?(ViewportSize.Y-Height)*.5f:FMath::FRandRange(24.f, FMath::Max(24.f, ViewportSize.Y - Height - 80.f));
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
						.Text(Copy.Title)
						.Font(BrainMiningFonts::Title(22.f))
						.ColorAndOpacity(FLinearColor(0.08f, 0.08f, 0.1f))
					]

				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.VAlign(bIntroduction?VAlign_Center:VAlign_Top)
			.Padding(FMargin(12.f, 10.f))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,0,0,bThemePage?28.f:0.f)
				[SNew(STextBlock).Visibility(bThemePage?EVisibility::HitTestInvisible:EVisibility::Collapsed)
				 .Text(MiningText::Get(TEXT("Office.Hint.Theme"))).Font(PopupFont(24.f)).ColorAndOpacity(FLinearColor(.25f,.32f,.4f))]
				+SVerticalBox::Slot().AutoHeight()
				[
				SNew(STextBlock)
				.Text(Copy.Body)
				.Font(bThemePage?BrainMiningFonts::Title(80.f):PopupFont(bIntroduction?28.f:20.f))
				.Justification(bIntroduction?ETextJustify::Center:ETextJustify::Left)
				.AutoWrapText(true)
				.ColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.12f))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(12.f,4.f,12.f,8.f)
			[
				SNew(SBox).WidthOverride(160.f).HeightOverride(48.f)
				[
					SNew(SButton).ContentPadding(FMargin(8.f,2.f)).ButtonColorAndOpacity(FLinearColor(.15f,.42f,.72f))
					.IsEnabled_Lambda([WeakThis] { return WeakThis.IsValid() && WeakThis->bRepliesEnabled; })
					.OnClicked_Lambda([WeakThis,Id]()
					{
						if(auto Self=WeakThis.Get();Self && Self->bRepliesEnabled && Self->ClosePopup(Id,true))
						{
							++Self->ManuallyClosedCount;
							if(Self->OnReply)Self->OnReply();
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Text(MiningText::Get(bThemePage?TEXT("Office.Hint.Create"):bIntroduction?TEXT("Office.Hint.ViewTheme"):TEXT("Office.Hint.Reply"))).Font(PopupFont(22.f))
						.ColorAndOpacity(FLinearColor::White).Justification(ETextJustify::Center)
					]
				]
			]
		];

 if(bHasArt)Window=MakeArtPopup(Id,Variant,Size,ContentIndex);
 // Keep both opening pages readable even when the room timer has already
 // accumulated more work. Hidden problems retain their normal accounting.
 Window->SetVisibility(TAttribute<EVisibility>::CreateLambda([WeakThis,Id]
 {
  if(auto Self=WeakThis.Get())
  {
   // Keep the theme hidden until acknowledgement or the long-idle fallback.
   if(Id==2 && Self->IntroductionPriority==1)return EVisibility::Hidden;
   if(Self->IntroductionPriority<=2 && Id>Self->IntroductionPriority && !Self->bIntroductionFallback)return EVisibility::Hidden;
  }
  return EVisibility::Visible;
 }));
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
 const FVector2D ClosePoint=bHasArt?FVector2D((Rect.X+Rect.Z*.5f)*Width,(Rect.Y+Rect.W*.5f)*Height):FVector2D(Width-92.f,Height-32.f);
	Windows.Add({ Id, ContentIndex, Window, FVector2D(X,Y), Size, ClosePoint, Variant==4?PopupCloseContent::UnreadCount(WorkElapsed,Id,ContentIndex):0 });
}

TSharedRef<SWidget> UPopupCloseDesktopWidget::MakeArtPopup(int32 Id,int32 Variant,FVector2D Size,int32 ContentIndex)
{
 const auto& Skin=PopupCloseContent::GetArt(Variant);const auto& Copy=PopupCloseContent::Get(ContentIndex);
 const bool bIntroduction=PopupCloseContent::IsIntroduction(Id-1),bThemePage=Id==2;
 auto Panel=SNew(SConstraintCanvas);const float W=Size.X,H=Size.Y;
 Panel->AddSlot().Offset(FMargin(0,0,W,H)).Alignment(FVector2D(0,0)).ZOrder(0)
 [SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
  [SNew(SImage).Image(ArtBrushes[Variant].Get()).Visibility(EVisibility::HitTestInvisible)]];
 // Localize only the baked message body. The patch shares the original
 // neutral grey, with an explicit top-left anchor inside the supplied frame.
 if(Variant==4)
 {
  Panel->AddSlot().Offset(FMargin(W*.10f,H*.24f,W*.80f,H*.50f)).Alignment(FVector2D(0,0)).ZOrder(1)
   [SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
    .BorderBackgroundColor(FLinearColor(FColor(189,189,189))).Visibility(EVisibility::HitTestInvisible)
    .HAlign(HAlign_Center).VAlign(VAlign_Center)
    [SNew(STextBlock).Text(FText::Format(MiningText::Get(TEXT("Office.Popup.Unread")), FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(PopupCloseContent::UnreadCount(WorkElapsed,Id,ContentIndex))}})).Font(PopupFont(24))
     .Justification(ETextJustify::Center).ColorAndOpacity(FLinearColor(.015f,.015f,.015f))]];
 }
 if(Skin.bUseWorkText)
 {
  int32 LineBreaks=0;for(TCHAR Character:Copy.Body.ToString())if(Character=='\n')++LineBreaks;
  const int32 BodyFont=bThemePage?80:bIntroduction?26:Variant==2?(LineBreaks>2?11:15):16;
  const FVector4 R=Skin.TextRect;
  Panel->AddSlot().Offset(FMargin(R.X*W,R.Y*H,R.Z*W,R.W*H)).Alignment(FVector2D(0,0)).ZOrder(1)
  [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly).HAlign(HAlign_Left).VAlign(VAlign_Top)
   [SNew(SVerticalBox).Visibility(EVisibility::HitTestInvisible)
   +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,5)
    [SNew(STextBlock).Text(Copy.Title).WrapTextAt(R.Z*W).Font(BrainMiningFonts::Title(bIntroduction?24:Variant==2?(LineBreaks>2?13:16):18)).ColorAndOpacity(FLinearColor(.035f,.035f,.04f))]
   +SVerticalBox::Slot().FillHeight(1).VAlign(bIntroduction?VAlign_Center:VAlign_Top)
    [SNew(STextBlock).Text(Copy.Body).Font(bThemePage?BrainMiningFonts::Title(BodyFont):PopupFont(BodyFont))
     .Justification(bIntroduction?ETextJustify::Center:ETextJustify::Left).WrapTextAt(R.Z*W).ColorAndOpacity(FLinearColor(.035f,.035f,.04f))]]];
 }
 static const FButtonStyle HitStyle=FButtonStyle().SetNormal(FSlateNoResource()).SetHovered(FSlateNoResource()).SetPressed(FSlateNoResource()).SetDisabled(FSlateNoResource()).SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
 TWeakObjectPtr<UPopupCloseDesktopWidget> WeakThis(this);
 const auto AddCloseHit=[&](FVector4 R)
 {
 Panel->AddSlot().Offset(FMargin(R.X*W,R.Y*H,R.Z*W,R.W*H)).Alignment(FVector2D(0,0)).ZOrder(2)
 [SNew(SButton).ButtonStyle(&HitStyle).ContentPadding(0).IsEnabled_Lambda([WeakThis]{return WeakThis.IsValid() && WeakThis->bRepliesEnabled;})
  .OnClicked_Lambda([WeakThis,Id]
  {
   if(auto Self=WeakThis.Get();Self && Self->bRepliesEnabled && Self->ClosePopup(Id,true))
   {++Self->ManuallyClosedCount;if(Self->OnReply)Self->OnReply();}
   return FReply::Handled();
  })];
 };
 AddCloseHit(Skin.CloseRect);
 if(bIntroduction)
 {
  AddCloseHit(FVector4(.516,.725,.303,.15)); // Existing Cancel button.
  AddCloseHit(FVector4(.88,.04,.076,.12)); // Existing title-bar X.
 }
 return Panel;
}

bool UPopupCloseDesktopWidget::ClosePopup(int32 Id,bool bAcknowledge)
{
	const int32 Index = Windows.IndexOfByPredicate([Id](const FWindow& Window) { return Window.Id == Id; });
	if (Index == INDEX_NONE || !Canvas.IsValid() || !Windows[Index].Root.IsValid())
	{
		return false;
	}

 // A notification opens its actual message immediately. It remains one
 // unresolved task until the player acknowledges the message contents.
 if(bAcknowledge && Windows[Index].UnreadMessages>0)
 {
  auto& Window=Windows[Index];Canvas->RemoveSlot(Window.Root.ToSharedRef());
  Window.UnreadMessages=0;
  Window.Size=PopupCloseContent::GetArt(1).Size;
  Window.Position.X=FMath::Min(Window.Position.X,PopupCloseContent::DesktopWidth-Window.Size.X-16.f);
  Window.Position.Y=FMath::Min(Window.Position.Y,PopupCloseContent::DesktopHeight-Window.Size.Y-56.f);
  Window.Root=MakeArtPopup(Id,1,Window.Size,Window.ContentIndex);
  const auto R=PopupCloseContent::GetArt(1).CloseRect;
  Window.ClosePoint=FVector2D((R.X+R.Z*.5f)*Window.Size.X,(R.Y+R.W*.5f)*Window.Size.Y);
  Canvas->AddSlot().Offset(FMargin(Window.Position.X,Window.Position.Y,Window.Size.X,Window.Size.Y))
   .Alignment(FVector2D(0,0)).ZOrder(float(Id))[Window.Root.ToSharedRef()];
  return false;
 }
	Canvas->RemoveSlot(Windows[Index].Root.ToSharedRef());
	Windows.RemoveAt(Index);
 if(Id==IntroductionPriority){++IntroductionPriority;IntroductionAge=0.f;bIntroductionFallback=false;}
 return true;
}

void UPopupCloseDesktopWidget::ClearOldestPopup()
{
 if(Windows.Num())ClosePopup(Windows[0].Id);
}

void UPopupCloseDesktopWidget::ClearPopups()
{
 while(Windows.Num())ClearOldestPopup();
 ManuallyClosedCount=0;NextId=1;IntroductionPriority=1;IntroductionAge=0.f;bIntroductionFallback=false;WorkElapsed=-1.f;bReconnectShown=false;
}

void UPopupCloseDesktopWidget::AdvanceIntroduction(float DeltaSeconds)
{
 // Protect the normal announcement -> manual theme flow throughout the slow phase.
 // A long-idle announcement reveals the theme first, before releasing later work.
 if(IntroductionPriority>2 || Windows.IsEmpty())return;
 if(!Windows.ContainsByPredicate([this](const FWindow& W){return W.Id==IntroductionPriority;}))return;
 IntroductionAge+=FMath::Max(0.f,DeltaSeconds);
 if(IntroductionPriority==1 && IntroductionAge>=45.f)
 {IntroductionPriority=2;IntroductionAge=0.f;}
 else if(IntroductionPriority==2 && (IntroductionAge>=20.f || (WorkElapsed>=55.f && IntroductionAge>=5.f)))bIntroductionFallback=true;
}

int32 UPopupCloseDesktopWidget::FrontWindowIndex() const
{
 const int32 Introduction=Windows.IndexOfByPredicate([this](const FWindow& Window){return !bIntroductionFallback && IntroductionPriority<=2 && Window.Id==IntroductionPriority;});
 if(Introduction!=INDEX_NONE)return Introduction;
 const bool bAnnouncementOpen=IntroductionPriority==1;
 for(int32 I=Windows.Num()-1;I>=0;--I)if(Windows[I].Id!=2 || !bAnnouncementOpen)return I;
 return INDEX_NONE;
}

int32 UPopupCloseDesktopWidget::GetFrontUnreadCount() const
{
 const int32 Index=FrontWindowIndex();
 return Windows.IsValidIndex(Index)?Windows[Index].UnreadMessages:0;
}

int32 UPopupCloseDesktopWidget::GetFrontPopupPhase() const
{
 const int32 Index=FrontWindowIndex();
 return Windows.IsValidIndex(Index)?PopupCloseContent::PhaseForContent(Windows[Index].ContentIndex):-1;
}

FString UPopupCloseDesktopWidget::GetFrontPopupTitle() const
{
 const int32 Index=FrontWindowIndex();
 return Windows.IsValidIndex(Index)?PopupCloseContent::Get(Windows[Index].ContentIndex).Title.ToString():FString();
}

FString UPopupCloseDesktopWidget::GetFrontPopupBody() const
{
 const int32 Index=FrontWindowIndex();
 return Windows.IsValidIndex(Index)?PopupCloseContent::Get(Windows[Index].ContentIndex).Body.ToString():FString();
}

FVector2D UPopupCloseDesktopWidget::GetLatestClosePoint() const
{
 if(!Windows.Num())return FVector2D::ZeroVector;
 const auto& Window=Windows[FrontWindowIndex()];
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
 return SNew(SBox).WidthOverride(24).HeightOverride(76).HAlign(HAlign_Center).VAlign(VAlign_Center)
 [SNew(SImage).Image(WarningBrush.Get())];
}
