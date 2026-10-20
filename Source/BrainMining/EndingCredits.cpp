#include "EndingEmergence.h"
#include "EndingCreditsInput.h"
#include "BrainMiningPlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Styling/SlateStyle.h"
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

bool EndingCreditsInput::Handle(APlayerController* Player,const FKey& Key)
{
    if(!Player || !Player->IsLocalController() || !Player->GetWorld() || (Key!=EKeys::C && Key!=EKeys::Escape))return false;
    auto* World=Player->GetWorld();
    auto* Session=World->GetGameInstance()?World->GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>():nullptr;
    if(Session&&Session->IsMenuOpen())return false;
    if(auto* PC=Cast<ABrainMiningPlayerController>(Player);PC&&PC->IsAudioSettingsOpen())return false;
    bool AtCredits=false;
    for(TActorIterator<AEndingEmergence> It(World);It;++It)
        if(It->bComplete && !It->IsHidden()){AtCredits=true;break;}
    if(!AtCredits)return false;
    AMiningCollectionField* Field=nullptr;
    for(TActorIterator<AMiningCollectionField> It(World);It;++It)
    {
        if(It->bCatalogueOpen){It->ToggleCatalogue();return true;}
        if(It->bCatalogueOnly)Field=*It;
    }
    if(Key==EKeys::Escape)
    {
        UKismetSystemLibrary::QuitGame(Player,Player,EQuitPreference::Quit,false);
        return true;
    }
    if(!Field)
    {
        Field=World->SpawnActorDeferred<AMiningCollectionField>(AMiningCollectionField::StaticClass(),FTransform::Identity);
        if(Field)
        {
            Field->bCatalogueOnly=true;Field->bSharedAbilities=true;
            Field->bUseAuthoredPlacements=true;Field->bShowExplorationHUD=false;
            Field->FinishSpawning(FTransform::Identity);
        }
    }
    if(Field)Field->ToggleCatalogue();
    return true;
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
        // Rich text lays mixed fonts out on one baseline, including CJK and
        // Latin ascenders/descenders; centering four independent widgets cannot.
        static FSlateStyleSet Styles(TEXT("EndingCreditTypography"));
        Styles.Set("cn",FTextBlockStyle().SetFont(CreditsFont(24)).SetColorAndOpacity(FLinearColor(.96f,.95f,.92f)));
        Styles.Set("en",FTextBlockStyle().SetFont(CreditsFont(26,true)).SetColorAndOpacity(FLinearColor(.96f,.95f,.92f)));
        Layout->AddSlot().Offset(FMargin(270,676,1060,65)).Anchors(FAnchors(0)).Alignment(FVector2D::ZeroVector)
            [SNew(SRichTextBlock).DecoratorStyleSet(&Styles).TextStyle(&Styles.GetWidgetStyle<FTextBlockStyle>("cn"))
                .Text(FText::FromString(TEXT("<cn>铁皮辣妹</>    <cn>空空道人</>    <en>Seven</>    <en>Rasimet</>")))
                .Justification(ETextJustify::Center)];
        auto Prompt=SNew(SConstraintCanvas);
        Prompt->AddSlot().Offset(FMargin(240,812,1120,65)).Anchors(FAnchors(0)).Alignment(FVector2D::ZeroVector)
            [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).Padding(0)
                .ColorAndOpacity_Lambda([this,CreditsStartedAt]()
                {
                    const float T=Age-CreditsStartedAt-8.f;
                    const float Alpha=T<0?0.f:FMath::Min(1.f,T/.8f)*(.35f+.65f*(.5f+.5f*FMath::Cos(T*2.f*PI/3.6f)));
                    return FLinearColor(1,1,1,Alpha);
                })
                [SNew(SRichTextBlock).DecoratorStyleSet(&Styles).TextStyle(&Styles.GetWidgetStyle<FTextBlockStyle>("cn"))
                    .Text(FText::FromString(TEXT("<en>[C]</> <cn>收集界面</>     <en>[ESC]</> <cn>退出游戏</>")))
                    .Justification(ETextJustify::Center)]];
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
