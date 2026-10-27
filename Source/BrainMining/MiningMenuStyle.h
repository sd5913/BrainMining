#pragma once

#include "CoreMinimal.h"
#include "MiningUISounds.h"
#include "BrainMiningFonts.h"
#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Text/STextBlock.h"

namespace MiningMenuStyle
{
    inline const FLinearColor Gold(1.f,.64f,.015f);
    inline const FLinearColor Cream(.94f,.93f,.88f);
    inline const FSlateBrush* Brush(int32 Index)
    {
        struct FArt
        {
            TStrongObjectPtr<UTexture2D> Textures[4];
            FSlateBrush Brushes[4];
            FArt()
            {
                const TCHAR* Names[]={TEXT("Panel"),TEXT("Row"),TEXT("Hover"),TEXT("Pressed")};
                for(int32 I=0;I<4;++I)
                {
                    Textures[I].Reset(LoadObject<UTexture2D>(nullptr,*FString::Printf(TEXT("/Game/UI/MenuStyle/T_Menu%s"),Names[I])));
                    Brushes[I].SetResourceObject(Textures[I].Get());
                    Brushes[I].DrawAs=ESlateBrushDrawType::Box;
                    Brushes[I].ImageSize=I==0?FVector2D(1200,900):FVector2D(800,64);
                    Brushes[I].Margin=I==0?FMargin(.025f,.0334f):FMargin(.02f,.25f);
                }
            }
        };
        static FArt Art;
        return &Art.Brushes[Index];
    }
    inline const FButtonStyle* Button()
    {
        static FButtonStyle Style=[](){FButtonStyle S=FButtonStyle().SetNormal(*Brush(1)).SetHovered(*Brush(2)).SetPressed(*Brush(3)).SetDisabled(*Brush(1))
            .SetNormalForeground(Cream).SetHoveredForeground(FLinearColor(.025f,.02f,.005f)).SetPressedForeground(FLinearColor::Black)
            .SetNormalPadding(FMargin(20,12)).SetPressedPadding(FMargin(20,12));
            MiningUISounds::Apply(S);return S;}();
        return &Style;
    }
    inline const FSliderStyle* Slider()
    {
        static FSliderStyle Style=[]()
        {
            FSliderStyle S=FCoreStyle::Get().GetWidgetStyle<FSliderStyle>("Slider");
            FSlateBrush Bar=*FCoreStyle::Get().GetBrush("WhiteBrush");Bar.ImageSize=FVector2D(100,3);Bar.TintColor=Gold*.6f;
            FSlateBrush Thumb=Bar;Thumb.ImageSize=FVector2D(5,22);Thumb.TintColor=Gold;
            S.SetNormalBarImage(Bar).SetHoveredBarImage(Bar).SetNormalThumbImage(Thumb).SetHoveredThumbImage(Thumb).SetBarThickness(3.f);
            return S;
        }();
        return &Style;
    }
    inline TSharedRef<SWidget> Section(const FText& Label)
    {
        return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Gold).Padding(FMargin(2,0,0,0))
            [SNew(SBorder).BorderImage(Brush(1)).Padding(FMargin(14,8))
                [SNew(STextBlock).Text(Label).Font(BrainMiningFonts::Title(19)).ColorAndOpacity(Gold)]];
    }
    inline TSharedRef<SWidget> Section(const TCHAR* Label) { return Section(FText::FromString(Label)); }
    inline TSharedRef<SWidget> Frame(TSharedRef<SWidget> Content,float Width,float Height)
    {
        return SNew(SOverlay)
            +SOverlay::Slot()[SNew(SBackgroundBlur).BlurStrength(7.f)]
            +SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0,0,0,.48f))]
            +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(28)
                [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                    [SNew(SBox).WidthOverride(Width).HeightOverride(Height>0.f?FOptionalSize(Height):FOptionalSize())
                        [SNew(SBorder).BorderImage(Brush(0)).Padding(38)[Content]]]];
    }
}
