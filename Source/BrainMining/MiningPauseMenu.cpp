#include "MiningLoadingScreen.h"
#include "MiningExperienceSubsystem.h"
#include "MiningSaveSubsystem.h"
#include "Subtitles/SubtitleSubsystem.h"
#include "BrainMiningFonts.h"
#include "MiningMenuStyle.h"
#include "EndingEmergence.h"
#include "EndingCreditsInput.h"
#include "BrainMiningPlayerController.h"
#include "MiningGamepadSubsystem.h"
#include "StartRoom/MiningOpening.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "MiningCollection.h"
#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningRoomEscape.h"
#include "GeneratedMiningRock.h"
#include "Showcase/MiningExhibitHall.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"

// Preserve keyboard/gamepad focus, using the menu's gold outline instead of
// Slate's default blue focus rectangle.
class SMiningPauseButton : public SButton
{
public:
    virtual TOptional<bool> OnQueryShowFocus(EFocusCause) const override { return false; }
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& WidgetStyle,bool Enabled) const override
    {
        Layer=SButton::OnPaint(Args,G,Clip,Elements,Layer,WidgetStyle,Enabled);
        if(HasKeyboardFocus())
        {
            const auto S=G.GetLocalSize();
            TArray<FVector2D> P={{2,2},{S.X-10,2},{S.X-2,10},{S.X-2,S.Y-2},{10,S.Y-2},{2,S.Y-10},{2,2}};
            FSlateDrawElement::MakeLines(Elements,++Layer,G.ToPaintGeometry(),P,ESlateDrawEffect::None,MiningMenuStyle::Gold,true,2.f);
        }
        return Layer;
    }
};

bool UMiningExperienceSubsystem::TogglePauseMenu(bool bFromCredits)
{
    auto* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;
    if(auto* SettingsPC=Cast<ABrainMiningPlayerController>(PC);SettingsPC&&SettingsPC->IsAudioSettingsOpen()){SettingsPC->ToggleAudioSettings();return true;}
    if(Menu){CloseMenu();return true;}
    if(auto* Mobile=Cast<ABrainMiningPlayerController>(PC);Mobile&&Mobile->IsUsingTouchControls())return ToggleMobileMenu();
    if(!bFromCredits && EndingCreditsInput::Handle(PC,EKeys::Escape))return true;
    auto* Viewport=GetGameInstance()->GetGameViewportClient();
    if(!PC||!PC->IsLocalController()||!PC->GetPawn()||!Viewport||AMiningOpening::IsOpening(GetWorld()))return false;
    if(auto* Input=UMiningGamepadSubsystem::Find(PC);Input&&Input->GetContext()==EMiningInputContext::Blocked)return false;
    bool bAtCredits=false;for(TActorIterator<AEndingEmergence> It(GetWorld());It;++It)if(It->bComplete){bAtCredits=true;break;}
    const bool bFromGameplay=bAtCredits||(!PC->IsPaused()&&!PC->bShowMouseCursor&&!PC->IsMoveInputIgnored()&&!PC->IsLookInputIgnored());
    CaptureMenuInput(PC);
    MenuController=PC;MenuWorld=GetWorld();bPauseMenu=true;bQuitConfirmation=false;
    MenuMining=PC->GetPawn()->FindComponentByClass<UMiningComponent>();
    if(MenuMining.IsValid()){bPreviousMiningSuspended=MenuMining->IsInputSuspended();MenuMining->SetInputSuspended(true);}
    if(auto* Move=PC->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>()){Move->ResetFlightInput();Move->StopMovementImmediately();}
    auto Choices=SNew(SVerticalBox);
    Choices->AddSlot().AutoHeight().Padding(0,0,0,32)[SNew(STextBlock).Justification(ETextJustify::Center)
        .Text(FText::FromString(TEXT("暂停"))).Font(BrainMiningFonts::Title(32)).ColorAndOpacity(MiningMenuStyle::Gold)];
    TSharedPtr<SButton> FirstButton;
    auto Button=[&](const TCHAR* Text,TFunction<void()> Action)
    {
        auto Choice=SNew(SMiningPauseButton).Tag(FName(*(FString(TEXT("PauseAction_"))+Text))).ButtonStyle(MiningMenuStyle::Button()).HAlign(HAlign_Left).ContentPadding(FMargin(28,16))
            .OnClicked_Lambda([Action](){Action();return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(Text)).Font(BrainMiningFonts::Body(22)).ColorAndOpacity(FSlateColor::UseForeground())];
        Choices->AddSlot().AutoHeight().Padding(0,7)[Choice];
        if(!FirstButton)FirstButton=Choice;
        return Choice;
    };
    ResumeButton=Button(TEXT("继续游戏"),[this](){CloseMenu();});
    auto CollectionButton=Button(TEXT("查看藏品"),[this](){OpenPauseCollection();});
    CollectionButton->SetEnabled(bFromGameplay);
    auto AudioButton=Button(TEXT("设置"),[this]()
    {
        auto* Controller=Cast<ABrainMiningPlayerController>(MenuController.Get());
        CloseMenu();
        if(Controller)Controller->ToggleAudioSettings();
    });
    AudioButton->SetEnabled(bFromGameplay&&Cast<ABrainMiningPlayerController>(PC)!=nullptr);
    auto SaveButton=Button(TEXT("保存进度"),[this](){
        const bool Saved=GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>()->SaveProgress();
        CloseMenu();
        if(auto* Subtitles=USubtitleSubsystem::Get(this))Subtitles->PlayInteractionLines(TEXT("SaveResult"),{FText::FromString(Saved?TEXT("进度已保存"):TEXT("当前暂时无法保存，请回到探索状态后重试"))});
    });
    SaveButton->SetEnabled(bFromGameplay && GetWorld()->GetMapName().EndsWith(TEXT("Lvl_MainGame")));
    Button(TEXT("返回主界面"),[this](){ReturnToMainMenu();});
    Menu=MiningMenuStyle::Frame(Choices,650,0);
    PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);bPausedByMenu=!PC->IsPaused()&&PC->SetPause(true);
    Viewport->AddViewportWidgetContent(Menu.ToSharedRef(),1500);
    FInputModeUIOnly Mode;Mode.SetWidgetToFocus(FirstButton);PC->SetInputMode(Mode);PC->bShowMouseCursor=true;return true;
}
void UMiningExperienceSubsystem::RequestQuitConfirmation()
{
    auto* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;
    if(bPauseMenu)if(auto* Mobile=Cast<ABrainMiningPlayerController>(PC);Mobile&&Mobile->IsUsingTouchControls()){bQuitConfirmation=true;return;}
    auto* Viewport=GetGameInstance()->GetGameViewportClient();
    if(Menu || !PC || !Viewport || !AMiningOpening::IsOpening(GetWorld()))return;
    CaptureMenuInput(PC);MenuController=PC;MenuWorld=GetWorld();bQuitConfirmation=true;
    // Balance CloseMenu's input locks while preserving the opening's own locks.
    PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);
    auto Choices=SNew(SVerticalBox);
    Choices->AddSlot().AutoHeight().Padding(0,0,0,28)
        [SNew(STextBlock).Text(FText::FromString(TEXT("确定退出游戏？"))).Justification(ETextJustify::Center)
            .Font(BrainMiningFonts::Title(32)).ColorAndOpacity(MiningMenuStyle::Gold)];
    Choices->AddSlot().AutoHeight().Padding(0,7)
        [SAssignNew(CancelQuitButton,SMiningPauseButton).Tag(TEXT("OpeningQuitCancel")).ButtonStyle(MiningMenuStyle::Button()).ContentPadding(FMargin(28,16))
            .OnClicked_Lambda([this](){CancelQuitConfirmation();return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(TEXT("取消"))).Font(BrainMiningFonts::Body(22)).ColorAndOpacity(FSlateColor::UseForeground())]];
    Choices->AddSlot().AutoHeight().Padding(0,7)
        [SNew(SMiningPauseButton).Tag(TEXT("OpeningQuitConfirm")).ButtonStyle(MiningMenuStyle::Button()).ContentPadding(FMargin(28,16))
            .OnClicked_Lambda([this](){auto* Controller=MenuController.Get();CloseMenu();UKismetSystemLibrary::QuitGame(this,Controller,EQuitPreference::Quit,false);return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(TEXT("确认退出"))).Font(BrainMiningFonts::Body(22)).ColorAndOpacity(FSlateColor::UseForeground())]];
    Menu=MiningMenuStyle::Frame(Choices,560,0);Viewport->AddViewportWidgetContent(Menu.ToSharedRef(),2000);
    FInputModeUIOnly Mode;Mode.SetWidgetToFocus(CancelQuitButton);PC->SetInputMode(Mode);PC->bShowMouseCursor=true;
}
void UMiningExperienceSubsystem::CancelQuitConfirmation()
{
    if(bPauseMenu){bQuitConfirmation=false;return;}
    if(bQuitConfirmation)CloseMenu();
}
void UMiningExperienceSubsystem::OpenPauseCollection()
{
    if(!bPauseMenu)return;
    AMiningCollectionField* Field=nullptr;
    for(TActorIterator<AMiningCollectionField> It(GetWorld());It;++It)if(It->bUseAuthoredPlacements){Field=*It;break;}
    if(!Field){TActorIterator<AMiningCollectionField> FirstField(GetWorld());if(FirstField)Field=*FirstField;}
    // The ending has no placed collection field before its credits begin.
    // Create a catalogue without spawning collectibles or resetting the player.
    if(!Field)
    {
        Field=GetWorld()->SpawnActorDeferred<AMiningCollectionField>(AMiningCollectionField::StaticClass(),FTransform::Identity);
        if(Field)
        {
            Field->bCatalogueOnly=true;Field->bSharedAbilities=true;
            Field->bUseAuthoredPlacements=true;Field->bShowExplorationHUD=false;
            Field->FinishSpawning(FTransform::Identity);
        }
    }
    if(!Field)return;
    CloseMenu();Field->ToggleCatalogue();
}
bool UMiningExperienceSubsystem::JumpToCollectible(int32 Id)
{
    if(!Menu||!bDeveloperMenu||Id<0||Id>=AMiningCollectible::ItemTypeCount)return false;
    auto* PC=MenuController.Get();auto* Hall=AMiningExhibitHall::Find(GetWorld());
    if(!PC||!PC->GetPawn())return false;
    FVector Destination,Look;FName Area;bool Found=false;
    for(TActorIterator<AMiningCollectionField> Field(GetWorld());Field&&!Found;++Field)
        for(const auto& P:Field->AuthoredPlacements)if(P.ItemId==Id)
        {
            Look=P.ClueTransform.GetLocation();Area=P.AreaTag;
            Destination=P.ApproachTransform.GetLocation();
            if(Destination.IsNearlyZero())Destination=Look-P.ClueTransform.GetRotation().GetForwardVector()*220+FVector(0,0,90);
            Found=true;break;
        }
    if(Id==11)for(TActorIterator<AMiningRoomEscape> Room(GetWorld());Room;++Room)if(Room->StageRibbon)
    {Look=Room->StageRibbon->GetActorLocation();Destination=Look+FVector(-180,0,100);Area=TEXT("BM_Campaign_MainGameJourney");Found=true;break;}
    if(!Found){MenuError=TEXT("当前地图中没有这个收集物。");return false;}
    FRotator Facing=(Look-Destination).Rotation();
    if(Hall)
    {
        const int Index=Hall->Areas.IndexOfByPredicate([Area](const auto& A){return A.Tag==Area;});
        if(Index!=INDEX_NONE&&!Hall->EnterDeveloperArea(Index,MenuError))return false;
    }
    AGeneratedMiningRock::PrepareTerrainAt(GetWorld(),Destination);
    if(!GetWorld()->FindTeleportSpot(PC->GetPawn(),Destination,Facing))
    {
        const FVector Approach=Destination;
        const auto* Character=Cast<ACharacter>(PC->GetPawn());
        const float Height=Character?Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight():90.f;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(CollectionJumpFloor),true,PC->GetPawn());
        bool Safe=false;
        // Ground-level clues can sit on sloped soil. Keep the jump nearby, but
        // place the whole capsule above an actual walkable surface.
        for(int Ring=0;Ring<=5&&!Safe;++Ring)for(int Direction=0;Direction<(Ring?8:1)&&!Safe;++Direction)
        {
            const float Angle=Direction*PI/4;
            const FVector Probe=Approach+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*(Ring*100);
            FHitResult Ground;
            if(!GetWorld()->LineTraceSingleByChannel(Ground,Probe+FVector(0,0,450),Probe-FVector(0,0,1000),ECC_Visibility,Query)||Ground.ImpactNormal.Z<.55f)continue;
            FVector Candidate=Ground.ImpactPoint+FVector(0,0,Height+6);
            if(GetWorld()->FindTeleportSpot(PC->GetPawn(),Candidate,Facing)){Destination=Candidate;Safe=true;}
        }
        if(!Safe){MenuError=TEXT("附近暂时没有安全落脚点。");return false;}
    }
    Facing=(Look-Destination).Rotation();
    if(auto* Movement=PC->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>())Movement->StopMovementImmediately();
    PC->GetPawn()->SetActorLocation(Destination,false,nullptr,ETeleportType::TeleportPhysics);
    if(Id==3)Facing=(Look-PC->GetPawn()->GetPawnViewLocation()).Rotation();
    PC->SetControlRotation(Facing);
    if(Id==11)for(TActorIterator<AMiningRoomEscape> Room(GetWorld());Room;++Room)if(Room->StageRibbon)Room->Phase=EMiningEscapePhase::Escaped;
    CloseMenu();return true;
}

void UMiningExperienceSubsystem::ReturnToMainMenu()
{
    FMiningLoadingScreen Saving(TEXT("保存中"));
    auto* Saves=GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>();
    if(GetWorld()->GetMapName().EndsWith(TEXT("Lvl_MainGame")) && !Saves->SaveProgress())
    {
        CloseMenu();
        if(auto* Subtitles=USubtitleSubsystem::Get(this))Subtitles->PlayStatusLine(FText::FromString(TEXT("地形还在更新，稍等片刻后再返回主界面吧")),4.f);
        return;
    }
    CloseMenu();bAllUIHidden=false;Saves->EndSession();
    Saving.ContinueThroughLevelTravel();
    UGameplayStatics::OpenLevel(this,TEXT("/Game/MainGame/Maps/Lvl_MainGame"));
}
