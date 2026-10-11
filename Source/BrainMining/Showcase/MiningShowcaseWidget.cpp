#include "Showcase/MiningShowcase.h"
#include "Showcase/MiningTreasure.h"
#include "Fonts/CompositeFont.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace MiningShowcaseUI
{
    FSlateFontInfo Font(int32 Size)
    {
        static TSharedPtr<const FCompositeFont> ChineseFont;
        const FString Path = TEXT("C:/Windows/Fonts/simhei.ttf");
        if (!ChineseFont && FPaths::FileExists(Path)) ChineseFont = MakeShared<FStandaloneCompositeFont>(NAME_None, Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
        return ChineseFont ? FSlateFontInfo(ChineseFont, Size) : FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size);
    }
}

TSharedRef<SWidget> UMiningShowcaseWidget::RebuildWidget()
{
    if (Director.IsValid() && Director->bContinuousLayout)
    {
        return SNew(SOverlay)
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(24)
            [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FLinearColor(.04f,.03f,.015f,.9f)).Padding(16)
                .Visibility_Lambda([this]() { return Director.IsValid() && IsValid(Director->TreasureField) && !Director->IsModalActive() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
                [SNew(STextBlock).Font(MiningShowcaseUI::Font(22)).ColorAndOpacity(FLinearColor(1,.78f,.25f))
                    .Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("金币  %lld"), Director.IsValid() && Director->TreasureField ? Director->TreasureField->Coins : 0LL)); })]]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,76)
            [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FLinearColor(.015f,.025f,.04f,.9f)).Padding(14)
                .Visibility_Lambda([this]() { return Director.IsValid() && Director->TreasureField && Director->TreasureField->IsAreaActive() && !Director->IsModalActive() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
                [SNew(STextBlock).Font(MiningShowcaseUI::Font(18)).Justification(ETextJustify::Center)
                    .Text_Lambda([this]() { return Director.IsValid() && Director->TreasureField ? FText::FromString(Director->TreasureField->GetPrompt()) : FText::GetEmpty(); })]]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
            [SNew(STextBlock).Font(MiningShowcaseUI::Font(22)).Text(FText::FromString(TEXT("+")))
                .Visibility_Lambda([this]() { return Director.IsValid() && Director->TreasureField && Director->TreasureField->IsAreaActive() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })]
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24)
            [
                SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FLinearColor(0.018f, 0.028f, 0.045f, 0.9f)).Padding(16)
                .Visibility_Lambda([this]() { return Director.IsValid() && Director->bGuideVisible && !Director->IsModalActive() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [SNew(STextBlock).Font(MiningShowcaseUI::Font(21)).ColorAndOpacity(FLinearColor(.3f,.95f,.82f))
                        .Text_Lambda([this]() { return Director.IsValid() && Director->Exhibits.IsValidIndex(Director->ActiveIndex) ? Director->Exhibits[Director->ActiveIndex].Title : FText::FromString(TEXT("BRAIN MINING / 连续探索区")); })]
                    + SVerticalBox::Slot().AutoHeight().Padding(0,8)
                    [SNew(STextBlock).Font(MiningShowcaseUI::Font(14)).WrapTextAt(310)
                        .Text_Lambda([this]() { return Director.IsValid() && Director->Exhibits.IsValidIndex(Director->ActiveIndex) ? Director->Exhibits[Director->ActiveIndex].Instructions : FText::FromString(TEXT("沿青色步道直接进入各区，无需传送\n南侧：挖掘 → 宝物支路／回声坡道\n北侧：收集 → 弹窗 → 文思 → 卡牌")); })]
                    + SVerticalBox::Slot().AutoHeight()
                    [SNew(STextBlock).Font(MiningShowcaseUI::Font(12)).ColorAndOpacity(FLinearColor(.6f,.75f,.8f)).WrapTextAt(310)
                        .Text_Lambda([this]() { return Director.IsValid() ? Director->Status : FText::GetEmpty(); })]
                ]
            ]
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(16)
            [SNew(STextBlock).Font(MiningShowcaseUI::Font(12)).ColorAndOpacity(FLinearColor(.5f,.9f,.85f))
                .Visibility(EVisibility::HitTestInvisible).Text(FText::FromString(TEXT("F2 提示  ·  F5 重开本区  ·  F8 退出交互（不传送）")))];
    }
    TSharedRef<SVerticalBox> Left = SNew(SVerticalBox);
    TSharedRef<SVerticalBox> Right = SNew(SVerticalBox);
    if (Director.IsValid())
    {
        for (int32 Index = 0; Index < Director->Exhibits.Num(); ++Index)
        {
            const FMiningShowcaseExhibit Exhibit = Director->Exhibits[Index];
            TSharedRef<SVerticalBox> Column = Index < 6 ? Left : Right;
            Column->AddSlot().AutoHeight().Padding(4, 4)
            [
                SNew(SButton).ContentPadding(8).ButtonColorAndOpacity(Exhibit.Color * 0.22f)
                .IsEnabled_Lambda([this]() { return Director.IsValid() && !Director->bBusy; })
                .OnClicked_Lambda([this, Index]() { if (Director.IsValid()) Director->EnterExhibit(Index); return FReply::Handled(); })
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(STextBlock).Font(MiningShowcaseUI::Font(21)).ColorAndOpacity(Exhibit.Color)
                        .Text_Lambda([this, Index, Exhibit]()
                        {
                            const FString Marker = Director.IsValid() && Director->Completed.Contains(Exhibit.Id) ? TEXT("  [已完成]") :
                                Director.IsValid() && Director->Visited.Contains(Exhibit.Id) ? TEXT("  [已体验]") : TEXT("");
                            return FText::FromString(FString::Printf(TEXT("%02d  %s%s"), Index + 1, *Exhibit.Title.ToString(), *Marker));
                        })
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
                    [SNew(STextBlock).Font(MiningShowcaseUI::Font(14)).Text(Exhibit.Instructions).WrapTextAt(450).ColorAndOpacity(FLinearColor(0.72f, 0.8f, 0.84f))]
                ]
            ];
        }
    }
    return SNew(SOverlay)
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(18)
        [
            SNew(STextBlock).Font(MiningShowcaseUI::Font(13)).Text(FText::FromString(TEXT("F5 重开  ·  F8 返回大厅")))
            .ColorAndOpacity(FLinearColor(0.5f, 0.9f, 0.85f))
            .Visibility_Lambda([this]() { return Director.IsValid() && Director->IsModalActive() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
        ]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(18)
        [
            SNew(SBox).WidthOverride(365)
            .Visibility_Lambda([this]() { return Director.IsValid() && !Director->bMenuOpen && !Director->IsModalActive() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
            [
                SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.02f, 0.035f, 0.055f, 0.9f)).Padding(14)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [SNew(STextBlock).Font(MiningShowcaseUI::Font(19)).ColorAndOpacity(FLinearColor(0.25f, 0.9f, 0.8f)).Text_Lambda([this]()
                    {
                        if (!Director.IsValid()) return FText::GetEmpty();
                        if (Director->Exhibits.IsValidIndex(Director->ActiveIndex)) return Director->Exhibits[Director->ActiveIndex].Title;
                        return FText::FromString(TEXT("BRAIN MINING / 展示大厅"));
                    })]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8)
                    [SNew(STextBlock).Font(MiningShowcaseUI::Font(14)).WrapTextAt(335).Text_Lambda([this]()
                    {
                        if (!Director.IsValid()) return FText::GetEmpty();
                        if (Director->Exhibits.IsValidIndex(Director->ActiveIndex)) return Director->Exhibits[Director->ActiveIndex].Instructions;
                        const int32 Nearby = Director->GetNearbyExhibit();
                        if (Director->Exhibits.IsValidIndex(Nearby)) return FText::Format(FText::FromString(TEXT("按 E 进入：{0}")), Director->Exhibits[Nearby].Title);
                        return FText::FromString(TEXT("WASD 移动 · 走近编号展台按 E\nF2 打开展区目录"));
                    })]
                    + SVerticalBox::Slot().AutoHeight()
                    [SNew(STextBlock).Font(MiningShowcaseUI::Font(13)).ColorAndOpacity(FLinearColor(0.65f, 0.75f, 0.8f)).WrapTextAt(335)
                        .Text_Lambda([this]() { return Director.IsValid() ? Director->Status : FText::GetEmpty(); })]
                ]
            ]
        ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(30)
        [
            SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
            .Visibility_Lambda([this]() { return Director.IsValid() && Director->bMenuOpen ? EVisibility::Visible : EVisibility::Collapsed; })
            [
                SNew(SBox).WidthOverride(1100).HeightOverride(960)
                [
                    SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.018f, 0.028f, 0.045f, 0.98f)).Padding(30)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [SNew(STextBlock).Text(FText::FromString(TEXT("BRAIN MINING  /  功能展示馆"))).Font(MiningShowcaseUI::Font(30)).ColorAndOpacity(FLinearColor(0.3f, 0.95f, 0.82f))]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 14)
                        [SNew(STextBlock).Text(FText::FromString(TEXT("四个分支 · 十一个展区 · 同一世界按需加载\n每次进入重新体验；F5 重开当前展区，F8 返回大厅。"))).Font(MiningShowcaseUI::Font(17)).ColorAndOpacity(FLinearColor(0.7f, 0.8f, 0.86f))]
                        + SVerticalBox::Slot().FillHeight(1)
                        [SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1)[Left] + SHorizontalBox::Slot().FillWidth(1)[Right]]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 12)
                        [SNew(STextBlock).Font(MiningShowcaseUI::Font(15)).WrapTextAt(1000).Text_Lambda([this]() { return Director.IsValid() ? Director->Status : FText::GetEmpty(); })]
                        + SVerticalBox::Slot().AutoHeight()
                        [SNew(SButton).ContentPadding(10).OnClicked_Lambda([this]() { if (Director.IsValid()) Director->SetMenuOpen(false); return FReply::Handled(); })
                            [SNew(STextBlock).Text(FText::FromString(TEXT("关闭目录，自由参观大厅  [F2]"))).Font(MiningShowcaseUI::Font(17)).Justification(ETextJustify::Center)]]
                    ]
                ]
            ]
        ];
}
