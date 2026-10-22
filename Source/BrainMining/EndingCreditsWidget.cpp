#include "EndingCreditsWidget.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void UEndingCreditsWidget::NativeConstruct()
{
    Super::NativeConstruct();Elapsed=0;
    if(Contributors)
    {
        // Keep the authored bilingual credits font, without relying on the
        // embedded rich-text style table for the developer line.
        FString Names;bool bTag=false;
        for(TCHAR C:Contributors->GetText().ToString())
            if(C==TEXT('<'))bTag=true;
            else if(C==TEXT('>'))bTag=false;
            else if(!bTag)Names.AppendChar(C);
        if(Names.TrimStartAndEnd().IsEmpty())Names=TEXT("铁皮辣妹    空空道人    Seven    Rasimet");
        Contributors->SetTextStyleSet(nullptr);
        Contributors->SetDefaultColorAndOpacity(FSlateColor(FLinearColor(.96f,.95f,.92f)));
        Contributors->SetText(FText::FromString(Names));
        Contributors->SetVisibility(ESlateVisibility::HitTestInvisible);
        Contributors->SetRenderOpacity(0);
    }
    if(ThankYou){ThankYou->SetText(FText::FromString(TEXT("恭喜你挖出来了！《挖哦》也因此诞生！感谢游玩！")));ThankYou->SetRenderOpacity(0);}
    if(Prompt)Prompt->SetRenderOpacity(0);
}

void UEndingCreditsWidget::NativeTick(const FGeometry& G,float Dt)
{
    Super::NativeTick(G,Dt);Elapsed+=Dt;
    if(BlackBackground)BlackBackground->SetRenderOpacity(FMath::SmoothStep(0.f,FMath::Max(.01f,BlackFadeSeconds),Elapsed));
    if(TextGroup)TextGroup->SetRenderOpacity(FMath::SmoothStep(0.f,FMath::Max(.01f,TextFadeSeconds),Elapsed-BlackFadeSeconds));
    const float ThanksFade=FMath::SmoothStep(0.f,.8f,Elapsed-PromptDelay);
    if(Contributors)Contributors->SetRenderOpacity(ThanksFade);
    if(ThankYou)ThankYou->SetRenderOpacity(ThanksFade);
    if(Prompt)
    {
        const float T=Elapsed-PromptDelay;
        Prompt->SetRenderOpacity(T<0.f?0.f:FMath::Min(1.f,T/.8f)*(.35f+.65f*(.5f+.5f*FMath::Cos(T*2.f*PI/3.6f))));
    }
}

int32 UEndingCreditsWidget::NativePaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& C,FSlateWindowElementList& Out,int32 L,const FWidgetStyle& S,bool Enabled) const
{
    L=Super::NativePaint(A,G,C,Out,L,S,Enabled);
    if(CRTStrength<=0)return L;
    const FVector2D Size=G.GetLocalSize();
    const float Scale=FMath::Max(.5f,Size.Y/900.f),Spacing=FMath::Max(2.f,ScanlineSpacing)*Scale;
    const float Alpha=CRTStrength*(.94f+.06f*FMath::Sin(Elapsed*47.f));
    const auto* Brush=FCoreStyle::Get().GetBrush("WhiteBrush");
    for(float Y=0;Y<Size.Y;Y+=Spacing)
        FSlateDrawElement::MakeBox(Out,L+1,G.ToPaintGeometry(FVector2f(Size.X,Scale),FSlateLayoutTransform(FVector2f(0,Y))),Brush,ESlateDrawEffect::None,FLinearColor(0,0,0,Alpha));
    return L+1;
}
