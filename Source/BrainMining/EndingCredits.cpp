#include "EndingEmergence.h"
#include "EndingCreditsWidget.h"
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
        if(Session && Session->TogglePauseMenu(true))Session->RequestQuitConfirmation();
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
        const auto Class=LoadClass<UEndingCreditsWidget>(nullptr,TEXT("/Game/EndingEmergence/UI/WBP_EndingCredits.WBP_EndingCredits_C"));
        if(!Class){UE_LOG(LogTemp,Error,TEXT("Missing editable ending credits widget"));return;}
        CreditsWidget=CreateWidget<UEndingCreditsWidget>(Player,Class);
        if(const auto* Mobile=Cast<ABrainMiningPlayerController>(Player);Mobile && Mobile->IsUsingTouchControls())
            if(auto* Prompt=CreditsWidget->GetWidgetFromName(TEXT("Prompt")))Prompt->SetVisibility(ESlateVisibility::Collapsed);
        CreditsWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
        Credits=CreditsWidget->TakeWidget();
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
    Credits.Reset();CreditsWidget=nullptr;
}
