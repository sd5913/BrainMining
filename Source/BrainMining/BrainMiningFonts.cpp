
#include "BrainMiningFonts.h"
#include "Engine/Font.h"
#include "Styling/CoreStyle.h"
#include "UObject/StrongObjectPtr.h"

UFont* BrainMiningFonts::Asset(bool bTitle)
{
    // Slate-created text is not always held by a UPROPERTY. Retain the font so
    // its embedded face data survives garbage collection and map transitions.
    static TStrongObjectPtr<UFont> Medium;
    static TStrongObjectPtr<UFont> Bold;
    auto& Font = bTitle ? Bold : Medium;
    if (!Font.IsValid())
        Font.Reset(LoadObject<UFont>(nullptr, bTitle
            ? TEXT("/Game/Fonts/F_SourceHanSansCN_Bold.F_SourceHanSansCN_Bold")
            : TEXT("/Game/Fonts/F_SourceHanSansCN_Medium.F_SourceHanSansCN_Medium")));
    return Font.Get();
}

FSlateFontInfo BrainMiningFonts::Body(float Size)
{
    if (auto* Font = Asset()) return FSlateFontInfo(Font, Size, TEXT("Medium"));
    return FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size);
}

FSlateFontInfo BrainMiningFonts::Title(float Size)
{
    if (auto* Font = Asset(true)) return FSlateFontInfo(Font, Size, TEXT("Bold"));
    return FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size);
}

FSlateFontInfo BrainMiningFonts::Style(const FName& Weight, float Size)
{
    return Weight == TEXT("Bold") ? Title(Size) : Body(Size);
}

FSlateFontInfo BrainMiningFonts::Restyle(FSlateFontInfo Font, bool bTitle)
{
    if (auto* Family = Asset(bTitle))
    {
        Font.FontObject = Family;
        Font.CompositeFont.Reset();
        Font.TypefaceFontName = bTitle ? TEXT("Bold") : TEXT("Medium");
    }
    return Font;
}
