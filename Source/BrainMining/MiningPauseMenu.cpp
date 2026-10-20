#include "MiningExperienceSubsystem.h"
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

bool UMiningExperienceSubsystem::TogglePauseMenu()
{
    auto* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;
    if(auto* SettingsPC=Cast<ABrainMiningPlayerController>(PC);SettingsPC&&SettingsPC->IsAudioSettingsOpen()){SettingsPC->ToggleAudioSettings();return true;}
    if(Menu){CloseMenu();return true;}
    if(auto* Mobile=Cast<ABrainMiningPlayerController>(PC);Mobile&&Mobile->IsUsingTouchControls())return ToggleMobileMenu();
    if(EndingCreditsInput::Handle(PC,EKeys::Escape))return true;
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
        .Text_Lambda([this](){return FText::FromString(bQuitConfirmation?TEXT("确定退出游戏？"):TEXT("暂停"));}).Font(BrainMiningFonts::Title(32)).ColorAndOpacity(MiningMenuStyle::Gold)];
    TSharedPtr<SButton> FirstButton;
    auto Button=[&](const TCHAR* Text,TFunction<void()> Action,bool Confirm)
    {
        auto Choice=SNew(SButton).ButtonStyle(MiningMenuStyle::Button()).HAlign(HAlign_Left).ContentPadding(FMargin(28,16))
            .Visibility_Lambda([this,Confirm](){return bQuitConfirmation==Confirm?EVisibility::Visible:EVisibility::Collapsed;})
            .OnClicked_Lambda([Action](){Action();return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(Text)).Font(BrainMiningFonts::Body(22)).ColorAndOpacity(FSlateColor::UseForeground())];
        Choices->AddSlot().AutoHeight().Padding(0,7)[Choice];
        if(!FirstButton)FirstButton=Choice;
        return Choice;
    };
    ResumeButton=Button(TEXT("继续游戏"),[this](){CloseMenu();},false);
    auto CollectionButton=Button(TEXT("收集界面"),[this](){OpenPauseCollection();},false);
    CollectionButton->SetEnabled(bFromGameplay);
    auto AudioButton=Button(TEXT("设置"),[this]()
    {
        auto* Controller=Cast<ABrainMiningPlayerController>(MenuController.Get());
        CloseMenu();
        if(Controller)Controller->ToggleAudioSettings();
    },false);
    AudioButton->SetEnabled(bFromGameplay&&Cast<ABrainMiningPlayerController>(PC)!=nullptr);
    Button(TEXT("退出游戏"),[this](){RequestQuitConfirmation();},false);
    CancelQuitButton=Button(TEXT("取消"),[this](){CancelQuitConfirmation();},true);
    Button(TEXT("确认退出"),[this](){auto* PC=MenuController.Get();CloseMenu();UKismetSystemLibrary::QuitGame(this,PC,EQuitPreference::Quit,false);},true);
    Menu=MiningMenuStyle::Frame(Choices,650,0);
    PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);bPausedByMenu=!PC->IsPaused()&&PC->SetPause(true);
    Viewport->AddViewportWidgetContent(Menu.ToSharedRef(),1500);
    FInputModeUIOnly Mode;Mode.SetWidgetToFocus(FirstButton);PC->SetInputMode(Mode);PC->bShowMouseCursor=true;return true;
}
void UMiningExperienceSubsystem::RequestQuitConfirmation()
{
    if(!bPauseMenu)return;
    bQuitConfirmation=true;
    auto* PC=MenuController.Get();
    if(PC&&PC->GetLocalPlayer()&&CancelQuitButton&&FSlateApplication::IsInitialized())
        FSlateApplication::Get().SetUserFocus(PC->GetLocalPlayer()->GetControllerId(),CancelQuitButton,EFocusCause::Navigation);
}
void UMiningExperienceSubsystem::CancelQuitConfirmation()
{
    bQuitConfirmation=false;
    auto* PC=MenuController.Get();
    if(PC&&PC->GetLocalPlayer()&&ResumeButton&&FSlateApplication::IsInitialized())
        FSlateApplication::Get().SetUserFocus(PC->GetLocalPlayer()->GetControllerId(),ResumeButton,EFocusCause::Navigation);
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
    PC->GetPawn()->SetActorLocation(Destination,false,nullptr,ETeleportType::TeleportPhysics);PC->SetControlRotation(Facing);
    if(Id==11)for(TActorIterator<AMiningRoomEscape> Room(GetWorld());Room;++Room)if(Room->StageRibbon)Room->Phase=EMiningEscapePhase::Escaped;
    CloseMenu();return true;
}
