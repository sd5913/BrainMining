#include "MiningUISounds.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"
#include "UObject/StrongObjectPtr.h"

namespace MiningUISounds
{
    void Apply(FButtonStyle& Style, bool bClick)
    {
        struct FSounds
        {
            TStrongObjectPtr<USoundBase> Hover{LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Pilot/S_UI_Hover.S_UI_Hover"))};
            TStrongObjectPtr<USoundBase> Click{LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Pilot/S_UI_Click.S_UI_Click"))};
        };
        static FSounds Sounds;
        FSlateSound Hover, Click;
        Hover.SetResourceObject(Sounds.Hover.Get());
        if (bClick) Click.SetResourceObject(Sounds.Click.Get());
        // Activate on a confirmed click, not on both mouse-down and mouse-up.
        Style.SetHoveredSound(Hover).SetPressedSound(FSlateSound()).SetClickedSound(Click);
    }

    const FButtonStyle* DefaultButton()
    {
        static FButtonStyle Style = []
        {
            auto S = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
            Apply(S);
            return S;
        }();
        return &Style;
    }
}
