#include "MiningLoadingScreen.h"
#include "BrainMiningFonts.h"
#include "MoviePlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

FMiningLoadingScreen::FMiningLoadingScreen()
{
    if(!IsMoviePlayerEnabled() || !GetMoviePlayer())return;
    const auto Font=BrainMiningFonts::Body(32);
    const double Started=FPlatformTime::Seconds();
    const float Width=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(TEXT("加载中..."),Font).X;
    FLoadingScreenAttributes Screen;
    Screen.MinimumLoadingScreenDisplayTime=1.2f;
    Screen.bAutoCompleteWhenLoadingCompletes=true;
    Screen.bMoviesAreSkippable=false;
    Screen.WidgetLoadingScreen=SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor::Black)
        .HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SBox).WidthOverride(Width)
        [SNew(STextBlock).Font(Font).ColorAndOpacity(FLinearColor::White).Justification(ETextJustify::Center)
            .Text_Lambda([Started](){const int32 Dots=1+int32((FPlatformTime::Seconds()-Started)/.35)%3;
                return FText::FromString(FString(TEXT("加载中"))+FString::ChrN(Dots,TCHAR('.')));})]];
    GetMoviePlayer()->SetupLoadingScreen(Screen);
    bPlaying=GetMoviePlayer()->PlayMovie();
}
FMiningLoadingScreen::~FMiningLoadingScreen()
{
    if(bPlaying)GetMoviePlayer()->WaitForMovieToFinish(false);
}
