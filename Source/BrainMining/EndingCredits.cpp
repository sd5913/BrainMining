#include "EndingEmergence.h"
#include "BrainMiningFonts.h"
#include "Engine/Font.h"
#include "UObject/StrongObjectPtr.h"
#include "MiningCollection.h"
#include "MiningExperienceSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

static FSlateFontInfo CreditsFont(float Size, bool bEnglish=false)
{
    static TStrongObjectPtr<UFont> Chinese;
    static TStrongObjectPtr<UFont> English;
    auto& Font=bEnglish?English:Chinese;
    if(!Font.IsValid())Font.Reset(LoadObject<UFont>(nullptr,bEnglish?TEXT("/Game/Fonts/F_CreditsRockwell"):TEXT("/Game/Fonts/F_CreditsHiragino")));
    return Font.IsValid()?FSlateFontInfo(Font.Get(),Size,TEXT("Regular")):BrainMiningFonts::Body(Size);
}

void AEndingEmergence::UpdateCredits()
{
    if(!Player || !GEngine || !GEngine->GameViewport)return;
    const bool NeedFocus=!Credits.IsValid() || Credits->GetVisibility()!=EVisibility::Visible;
    if(!Credits.IsValid())
    {
        const float CreditsStartedAt=Age;
        // Retain the shared catalogue so the pause menu can still inspect the
        // player's collection after the film, without spawning world pickups.
        for(TActorIterator<AMiningCollectionField> It(GetWorld());It;++It)
            if(It->bCatalogueOnly){EndingCatalogue=*It;break;}
        if(!EndingCatalogue)
        {
            EndingCatalogue=GetWorld()->SpawnActorDeferred<AMiningCollectionField>(AMiningCollectionField::StaticClass(),FTransform::Identity);
            if(EndingCatalogue)
            {
                EndingCatalogue->bCatalogueOnly=true;EndingCatalogue->bSharedAbilities=true;
                EndingCatalogue->bUseAuthoredPlacements=true;EndingCatalogue->bShowExplorationHUD=false;
                EndingCatalogue->FinishSpawning(FTransform::Identity);
            }
        }
        auto Layout=SNew(SConstraintCanvas);
        auto Text=[&](const TCHAR* Words,bool English,float Size,float X,float Y,float Width,float Height,ETextJustify::Type Justify)
        {
            Layout->AddSlot().Offset(FMargin(X,Y,Width,Height)).Anchors(FAnchors(0)).Alignment(FVector2D::ZeroVector)
                [SNew(STextBlock).Text(FText::FromString(Words)).Font(CreditsFont(Size,English))
                    .Justification(Justify).ColorAndOpacity(FLinearColor(.96f,.95f,.92f))];
        };
        Text(TEXT("找到出口的最好方式，就是前进。"),false,43,205,222,1190,90,ETextJustify::Center);
        Text(TEXT("The best way out is always through."),true,44,205,312,1190,80,ETextJustify::Center);
        Text(TEXT("罗伯特·弗罗斯特"),false,38,720,451,665,78,ETextJustify::Right);
        Text(TEXT("Robert Frost 1914"),true,39,720,526,665,74,ETextJustify::Right);
        Layout->AddSlot().Offset(FMargin(270,676,1060,60)).Anchors(FAnchors(0)).Alignment(FVector2D::ZeroVector)
            [SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1)
                +SHorizontalBox::Slot().AutoWidth().Padding(0,0,40,0)[SNew(STextBlock).Text(FText::FromString(TEXT("铁皮辣妹"))).Font(CreditsFont(24))]
                +SHorizontalBox::Slot().AutoWidth().Padding(0,0,40,0)[SNew(STextBlock).Text(FText::FromString(TEXT("空空道人"))).Font(CreditsFont(24))]
                +SHorizontalBox::Slot().AutoWidth().Padding(0,0,40,0)[SNew(STextBlock).Text(FText::FromString(TEXT("Seven"))).Font(CreditsFont(26,true))]
                +SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(TEXT("Rasimet"))).Font(CreditsFont(26,true))]
                +SHorizontalBox::Slot().FillWidth(1)];
        auto Prompt=SNew(SConstraintCanvas);
        Prompt->AddSlot().Offset(FMargin(500,812,600,60)).Anchors(FAnchors(0)).Alignment(FVector2D::ZeroVector)
            [SNew(STextBlock).Text(FText::FromString(TEXT("[ESC]"))).Font(CreditsFont(26,true)).Justification(ETextJustify::Center)
                .ColorAndOpacity_Lambda([this,CreditsStartedAt]()
                {
                    // Black fades in for 1.4 s, text for 1.6 s; wait five more
                    // seconds after the text is fully present before breathing.
                    const float T=Age-CreditsStartedAt-8.f;
                    const float Alpha=T<0?0.f:FMath::Min(1.f,T/.8f)*(.35f+.65f*(.5f+.5f*FMath::Cos(T*2.f*PI/3.6f)));
                    return FSlateColor(FLinearColor(1,1,1,Alpha));
                })];
        Credits=SNew(SOverlay)
            +SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor_Lambda([this,CreditsStartedAt](){return FLinearColor(0,0,0,FMath::SmoothStep(0.f,1.f,FMath::Clamp((Age-CreditsStartedAt)/1.4f,0.f,1.f)));})]
            +SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
                [SNew(SBox).WidthOverride(1600).HeightOverride(900)
                    [SNew(SOverlay)
                        +SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).Padding(0).ColorAndOpacity_Lambda([this,CreditsStartedAt](){return FLinearColor(1,1,1,FMath::SmoothStep(0.f,1.f,FMath::Clamp((Age-CreditsStartedAt-1.4f)/1.6f,0.f,1.f)));})[Layout]]
                        +SOverlay::Slot()[Prompt]]]];
        GEngine->GameViewport->AddViewportWidgetContent(Credits.ToSharedRef(),90);
    }
    const bool Open=EndingCatalogue && EndingCatalogue->bCatalogueOpen;
    Credits->SetVisibility(Open?EVisibility::Hidden:EVisibility::Visible);
    if(!Open && NeedFocus)
    {
        Player->bShowMouseCursor=false;
        FInputModeGameOnly Mode;Mode.SetConsumeCaptureMouseDown(false);Player->SetInputMode(Mode);
    }
}
void AEndingEmergence::RemoveCredits()
{
    if(Credits.IsValid() && GEngine && GEngine->GameViewport)GEngine->GameViewport->RemoveViewportWidgetContent(Credits.ToSharedRef());
    Credits.Reset();
}
