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
#include "UObject/UObjectGlobals.h"

FMiningLoadingScreen::FMiningLoadingScreen(const TCHAR* Label)
{
    if(!IsMoviePlayerEnabled() || !GetMoviePlayer())return;
    const auto Font=BrainMiningFonts::Body(32);
    const double Started=FPlatformTime::Seconds();
    const FString Caption(Label);
    const float Width=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Caption+TEXT("..."),Font).X;
    FLoadingScreenAttributes Screen;
    Screen.MinimumLoadingScreenDisplayTime=1.2f;
    Screen.bAutoCompleteWhenLoadingCompletes=true;
    Screen.bMoviesAreSkippable=false;
    Screen.WidgetLoadingScreen=SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor::Black)
        .HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SBox).WidthOverride(Width)
        [SNew(STextBlock).Font(Font).ColorAndOpacity(FLinearColor::White).Justification(ETextJustify::Center)
            .Text_Lambda([Started,Caption](){const int32 Dots=1+int32((FPlatformTime::Seconds()-Started)/.35)%3;
                return FText::FromString(Caption+FString::ChrN(Dots,TCHAR('.')));})]];
    GetMoviePlayer()->SetupLoadingScreen(Screen);
    bPlaying=GetMoviePlayer()->PlayMovie();
}
FMiningLoadingScreen::~FMiningLoadingScreen()
{
    if(bPlaying)GetMoviePlayer()->WaitForMovieToFinish(false);
}

void FMiningLoadingScreen::ContinueThroughLevelTravel()
{
    if(!bPlaying)return;
    // OpenLevel schedules travel for the next tick; keep the Slate loading thread alive until it finishes.
    auto Handle=MakeShared<FDelegateHandle>();
    *Handle=FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda([Handle](UWorld*)
    {
        FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(*Handle);
        if(GetMoviePlayer())GetMoviePlayer()->WaitForMovieToFinish(false);
    });
    bPlaying=false;
}
