#include "MiningWalletSubsystem.h"
#include "EndingEmergence.h"
#include "MiningCollection.h"
#include "MiningExperienceSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

void AEndingEmergence::UpdateCredits()
{
    if(!Player || !GEngine || !GEngine->GameViewport)return;
    const bool NeedFocus=!Credits.IsValid() || Credits->GetVisibility()!=EVisibility::Visible;
    if(!Credits.IsValid())
    {
        // Reuse the catalogue previously opened from the ending's pause menu.
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
        auto Rows=SNew(SVerticalBox);
        Rows->AddSlot().AutoHeight().Padding(0,0,0,30)[SNew(STextBlock).Text(FText::FromString(TEXT("感谢游玩"))).Font(FCoreStyle::GetDefaultFontStyle("Bold",48)).Justification(ETextJustify::Center)];
        Rows->AddSlot().AutoHeight().Padding(0,0,0,48)[SNew(STextBlock).Text(FText::FromString(TEXT("开发者\n\nSeven   ·   Rasimet   ·   空空道人   ·   铁皮辣妹"))).Font(FCoreStyle::GetDefaultFontStyle("Regular",22)).Justification(ETextJustify::Center)];
        auto Button=[&](const TCHAR* Text,TFunction<FReply()> Action)
        {Rows->AddSlot().AutoHeight().Padding(40,8)[SNew(SBox).HeightOverride(58)[SNew(SButton).HAlign(HAlign_Center).VAlign(VAlign_Center).OnClicked_Lambda(MoveTemp(Action))[SNew(STextBlock).Text(FText::FromString(Text)).Font(FCoreStyle::GetDefaultFontStyle("Regular",24))]]];};
        Button(TEXT("收集"),[this](){if(EndingCatalogue){EndingCatalogue->ToggleCatalogue();if(EndingCatalogue->bCatalogueOpen)Credits->SetVisibility(EVisibility::Hidden);}return FReply::Handled();});
        Button(TEXT("再玩一次"),[this](){if(auto* Session=GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>())Session->CampaignCollection.Reset();if(auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>()){Wallet->Balance=0;Wallet->Minerals.Reset();Wallet->UpgradeLevels={0,0,0,0};Wallet->MineralDoubleChance=0;Wallet->bBombSkillUnlocked=false;Wallet->bDrillSkillUnlocked=false;}UGameplayStatics::OpenLevel(this,TEXT("/Game/MainGame/Maps/Lvl_MainGame"));return FReply::Handled();});
        Button(TEXT("退出游戏"),[this](){UKismetSystemLibrary::QuitGame(this,Player,EQuitPreference::Quit,false);return FReply::Handled();});
        Credits=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.007f,.009f,.015f,1)).HAlign(HAlign_Center).VAlign(VAlign_Center)[Rows];
        GEngine->GameViewport->AddViewportWidgetContent(Credits.ToSharedRef(),90);
    }
    // The existing catalogue temporarily owns focus and pauses the world.
    const bool Open=EndingCatalogue && EndingCatalogue->bCatalogueOpen;
    Credits->SetVisibility(Open?EVisibility::Hidden:EVisibility::Visible);
    if(!Open && NeedFocus){Player->bShowMouseCursor=true;FInputModeGameAndUI Mode;Mode.SetWidgetToFocus(Credits);Mode.SetHideCursorDuringCapture(false);Player->SetInputMode(Mode);}
}
void AEndingEmergence::RemoveCredits()
{
    if(Credits.IsValid() && GEngine && GEngine->GameViewport)GEngine->GameViewport->RemoveViewportWidgetContent(Credits.ToSharedRef());
    Credits.Reset();
}
