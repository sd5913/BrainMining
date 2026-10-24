#include "StartRoom/MiningOpening.h"
#include "MiningLoadingScreen.h"
#include "MiningMenuStyle.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "TimerManager.h"
#include "MiningSaveSubsystem.h"
#include "MiningExperienceSubsystem.h"
#include "Engine/GameInstance.h"
#include "MiningHapticsSubsystem.h"
#include "BrainMiningFonts.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/WidgetComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Showcase/MiningExhibitHall.h"
#include "StartRoom/MiningStartRoom.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MainGameDoor.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Widgets/SWidget.h"

AMiningOpening* AMiningOpening::EnsureMainGameOpening(AMiningExhibitHall* Hall)
{
    if(!Hall || !Hall->bReady || Hall->Areas.IsEmpty())return nullptr;
    UWorld* World=Hall->GetWorld();
    if(!World || !World->GetMapName().EndsWith(TEXT("Lvl_MainGame")))return nullptr;
    // Keep any explicitly authored opening. This also makes repeated calls safe.
    TActorIterator<AMiningOpening> ExistingOpening(World);
    if(ExistingOpening)return *ExistingOpening;
    auto* Room=AMiningStartRoom::Find(World);if(!Room)return nullptr;
    auto* Title=LoadObject<UTexture2D>(nullptr,TEXT("/Game/StartRoom/UI/T_OpeningTitle.T_OpeningTitle"));
    if(!Title)return nullptr;
    const FTransform RoomTransform=Room->GetActorTransform();
    const FTransform SpawnTransform(RoomTransform.GetRotation(),RoomTransform.GetLocation());
    auto* Opening=World->SpawnActorDeferred<AMiningOpening>(StaticClass(),SpawnTransform,Room,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if(!Opening)return nullptr;
    Opening->Tags.Add(Hall->Areas[0].Tag);
    Opening->TitleTexture=Title;
    Opening->SeatEye=RoomTransform.TransformPosition(FVector(-390,-160,210));
    Opening->StandingLocation=RoomTransform.TransformPosition(FVector(-260,-70,98));
    Opening->Facing=(RoomTransform.GetRotation()*FRotator(-14,24,0).Quaternion()).Rotator();
    Opening->Duration=2.6f;
    Opening->DoorTitleLocation=RoomTransform.TransformPosition(FVector(466,0,168));
    Opening->DoorTitleRotation=(RoomTransform.GetRotation()*FRotator(0,180,0).Quaternion()).Rotator();
    Opening->FinishSpawning(SpawnTransform);
    return Opening;
}

TSharedRef<SWidget> UMiningOpeningTitle::RebuildWidget()
{
    auto* Picture=WidgetTree->ConstructWidget<UImage>();WidgetTree->RootWidget=Picture;
    Picture->SetBrushFromTexture(Texture,true);
    if(bCornerLayout)
    {
        auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Canvas;
        auto* PictureSlot=Canvas->AddChildToCanvas(Picture);PictureSlot->SetPosition(FVector2D(36,30));PictureSlot->SetSize(FVector2D(280,164));
    }
    return Super::RebuildWidget();
}

TSharedRef<SWidget> UMiningOpeningMenu::RebuildWidget()
{
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Canvas;
    auto* Fit=WidgetTree->ConstructWidget<UScaleBox>();Fit->SetStretch(EStretch::ScaleToFit);
    auto* TitleSlot=Canvas->AddChildToCanvas(Fit);TitleSlot->SetAnchors(FAnchors(.045f,.08f,.47f,.53f));TitleSlot->SetOffsets(FMargin(0));
    TitleImage=WidgetTree->ConstructWidget<UImage>();Fit->AddChild(TitleImage);
    if(Opening.IsValid())TitleImage->SetBrushFromTexture(Opening->TitleTexture,true);
    // Only the menu logo turns toward the screen centre. The same supplied
    // artwork remains flat when projected onto the actual mineable wall.
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/StartRoom/UI/M_OpeningTitlePerspective")))
    {
        auto* Perspective=UMaterialInstanceDynamic::Create(Base,TitleImage);
        if(Opening.IsValid())Perspective->SetTextureParameterValue(TEXT("Logo"),Opening->TitleTexture);
        TitleImage->SetBrushFromMaterial(Perspective);
    }
    auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();
    auto* CanvasSlot=Canvas->AddChildToCanvas(Column);
    const auto* Saves=GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>();
    const bool bHasSave=Saves && Saves->HasSave();
    // Keep the labels clear of the chair. Rows stagger toward the left while
    // sharing the room's perspective direction, rather than opposing tilts.
    CanvasSlot->SetAnchors(FAnchors(.865f,.65f-(bHasSave?86.f/900.f:0.f)));CanvasSlot->SetAlignment(FVector2D(.5f,0));CanvasSlot->SetAutoSize(true);
    Column->SetRenderShear(FVector2D(0,0));
    auto AddButton=[&](const TCHAR* Name,const TCHAR* Label,float Angle,float Shear,float Offset)
    {
        auto* Box=WidgetTree->ConstructWidget<USizeBox>();Box->SetWidthOverride(240);Box->SetHeightOverride(62);
        auto* Row=Column->AddChildToVerticalBox(Box);Row->SetPadding(FMargin(0,12));
        auto* Button=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),Name);Box->AddChild(Button);
        FButtonStyle Style;
        FSlateBrush Empty;Empty.DrawAs=ESlateBrushDrawType::NoDrawType;
        Style.SetNormal(Empty).SetHovered(Empty).SetPressed(Empty);
        Style.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));Button->SetStyle(Style);
        Button->SetRenderTransformPivot(FVector2D(.5f,.5f));
        Button->SetRenderTransformAngle(Angle);
        Button->SetRenderShear(FVector2D(Shear,0));
        Button->SetRenderTranslation(FVector2D(Offset,0));
        auto* Text=WidgetTree->ConstructWidget<UTextBlock>();Text->SetText(FText::FromString(Label));
        Text->SetFont(BrainMiningFonts::Title(36));Text->SetColorAndOpacity(FSlateColor(FLinearColor(.94f,.95f,.91f)));
        Text->SetShadowColorAndOpacity(FLinearColor(.035f,.018f,.012f,.65f));Text->SetShadowOffset(FVector2D(2,3));
        Button->AddChild(Text);
        return Button;
    };
    ContinueButton=nullptr;
    if(bHasSave)
    {
        ContinueButton=AddButton(TEXT("OpeningContinueButton"),TEXT("继续游戏"),7.f,10.f,18.f);
        ContinueButton->OnClicked.AddDynamic(this,&UMiningOpeningMenu::ContinueGame);
    }
    StartButton=AddButton(TEXT("OpeningStartButton"),TEXT("开始游戏"),7.f,10.f,4.f);
    StartButton->OnClicked.AddDynamic(this,&UMiningOpeningMenu::StartGame);
    ExitButton=AddButton(TEXT("OpeningExitButton"),TEXT("退出游戏"),7.f,10.f,-12.f);
    ExitButton->OnClicked.AddDynamic(this,&UMiningOpeningMenu::QuitGame);
    auto* DesignSize=WidgetTree->ConstructWidget<USizeBox>();
    DesignSize->SetWidthOverride(1600);DesignSize->SetHeightOverride(900);DesignSize->AddChild(Canvas);
    auto* Responsive=WidgetTree->ConstructWidget<UScaleBox>();Responsive->SetStretch(EStretch::ScaleToFit);
    Responsive->AddChild(DesignSize);WidgetTree->RootWidget=Responsive;

    return Super::RebuildWidget();
}
void UMiningOpeningMenu::NativeTick(const FGeometry& Geometry,float Dt)
{
    Super::NativeTick(Geometry,Dt);
    const bool bStarting=Opening.IsValid()&&Opening->bStarted;
    const auto AnimateButton=[&](UButton* Button,float& Scale)
    {
        if(!Button)return;
        const auto SlateButton=Button->GetCachedWidget();
        const auto FocusCause=SlateButton.IsValid()?SlateButton->HasUserFocus(0):TOptional<EFocusCause>();
        const bool Navigated=FocusCause.IsSet() && FocusCause.GetValue()==EFocusCause::Navigation;
        // SetInputMode gives Continue automatic focus. That must not pre-apply
        // the hover animation before the pointer ever reaches the button.
        const float Target=Button->IsPressed()?.96f:(Button->IsHovered()?1.12f:(Navigated?1.07f:1.f));
        Scale=FMath::FInterpTo(Scale,Target,Dt,Button->IsPressed()?18.f:10.f);
        Button->SetRenderScale(FVector2D(Scale));
        if(auto* Label=Cast<UTextBlock>(Button->GetContent()))
            Label->SetColorAndOpacity(FSlateColor(Button->IsHovered()||Navigated?FLinearColor::White:FLinearColor(.94f,.95f,.91f)));
    };
    AnimateButton(ContinueButton,ContinueScale);
    AnimateButton(StartButton,StartScale);
    AnimateButton(ExitButton,ExitScale);
    if(bStarting)
    {
        if(ContinueButton)ContinueButton->SetVisibility(ESlateVisibility::Collapsed);
        if(StartButton)StartButton->SetVisibility(ESlateVisibility::Collapsed);
        if(ExitButton)ExitButton->SetVisibility(ESlateVisibility::Collapsed);
        TitleFade=FMath::Max(0.f,TitleFade-Dt/.75f);
        if(TitleImage)TitleImage->SetRenderOpacity(FMath::SmoothStep(0.f,1.f,TitleFade));
    }
}
void UMiningOpeningMenu::StartGame(){if(Opening.IsValid())Opening->StartGame();}
void UMiningOpeningMenu::QuitGame(){GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>()->RequestQuitConfirmation();}

AMiningOpening::AMiningOpening()
{
    PrimaryActorTick.bCanEverTick=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    WorldTitle=CreateDefaultSubobject<UWidgetComponent>(TEXT("TitleInRoom"));WorldTitle->SetupAttachment(RootComponent);
    WorldTitle->SetWidgetSpace(EWidgetSpace::World);WorldTitle->SetDrawSize(FVector2D(1024,600));
    WorldTitle->SetTwoSided(true);
    // The title is presentation only.  It must never intercept the mining
    // visibility trace; the authored diggable wall behind it remains the
    // actual target after the title settles on the wall.
    WorldTitle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WorldTitle->SetCastShadow(false);WorldTitle->SetVisibility(false);
}
bool AMiningOpening::IsOpening(UWorld* World)
{
    if(World)for(TActorIterator<AMiningOpening> It(World);It;++It)if(!It->bFinished)return true;
    return false;
}
void AMiningOpening::Prepare(APlayerController* Controller)
{
    Player=Controller;bPrepared=true;
    if(TitleTexture){TitleTexture->SetForceMipLevelsToBeResident(60.f);TitleTexture->WaitForStreaming();}
    if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/StartRoom/UI/M_OpeningTitleDecal")))
        Material->EnsureIsComplete();
    APawn* Pawn=Controller->GetPawn();bOldCollision=Pawn->GetActorEnableCollision();Pawn->SetActorEnableCollision(false);
    if(auto* Character=Cast<ACharacter>(Pawn)){Character->GetCharacterMovement()->StopMovementImmediately();Character->GetCharacterMovement()->DisableMovement();}
    FMinimalViewInfo NativeView;Pawn->CalcCamera(0,NativeView);
    FVector EyeOffset=NativeView.Location-Pawn->GetActorLocation();StandingFOV=NativeView.FOV;
    // The first skeletal-camera sample can still contain the spawn pose. Keep
    // the standing eye at adult eye height instead of caching that crouched pose.
    if(const auto* Character=Cast<ACharacter>(Pawn))
        EyeOffset.Z=FMath::Max(EyeOffset.Z,Character->GetSimpleCollisionHalfHeight()*.95f);
    StandingEye=StandingLocation+EyeOffset;
    // Preserve the authored menu composition instead of forcing a low seated eye.
    SeatPawnLocation=SeatEye-EyeOffset;
    Pawn->SetActorLocation(SeatPawnLocation);Controller->SetControlRotation(Facing);
    Controller->SetIgnoreMoveInput(true);Controller->SetIgnoreLookInput(true);bInputLocked=true;
    if(auto* HUD=Controller->GetHUD()){bOldHUD=HUD->bShowHUD;HUD->bShowHUD=false;}
    if(auto* MiningPC=Cast<AMiningExhibitController>(Controller))MiningPC->SetMiningGUIEnabled(false);
    Camera=GetWorld()->SpawnActor<ACameraActor>(SeatEye,Facing);
    Camera->GetCameraComponent()->SetFieldOfView(80);Camera->GetCameraComponent()->bConstrainAspectRatio=false;Controller->SetViewTarget(Camera);
    auto* Title=CreateWidget<UMiningOpeningTitle>(Controller);Title->Texture=TitleTexture;WorldTitle->SetWidget(Title);
    InitialTitle=SeatEye+Facing.Vector()*240+Facing.RotateVector(FVector(0,0,40));
    InitialRotation=(Facing+FRotator(0,180,0)).Quaternion();
    WorldTitle->SetWorldLocationAndRotation(InitialTitle,InitialRotation);WorldTitle->SetWorldScale3D(FVector(.19f));WorldTitle->SetVisibility(false);
    Menu=CreateWidget<UMiningOpeningMenu>(Controller);Menu->Opening=this;Menu->SetIsFocusable(true);Menu->AddToViewport(500);
    Controller->bShowMouseCursor=true;FInputModeUIOnly Mode;
    UButton* DefaultButton=Menu->ContinueButton?Menu->ContinueButton.Get():Menu->StartButton.Get();
    Mode.SetWidgetToFocus(DefaultButton?DefaultButton->TakeWidget():Menu->TakeWidget());Controller->SetInputMode(Mode);
    Controller->PlayerCameraManager->UpdateCamera(0.f);
    if(auto* MiningPC=Cast<AMiningExhibitController>(Controller))MiningPC->RevealOpening();
}
void UMiningOpeningMenu::ContinueGame(){if(Opening.IsValid())Opening->ContinueSavedGame();}
void AMiningOpening::ContinueSavedGame()
{
    if(!bPrepared || bStarted || bFinished || CollectionChoice)return;
    FMiningLoadingScreen Loading;
    auto* Saves=GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>();
    if(!Saves || !Saves->LoadProgress())return;
    bStarted=true;bFinished=true;Release();
    if(Player.IsValid())Player->SetViewTarget(Player->GetPawn());
}
void AMiningOpening::StartGame()
{
    if(!bPrepared || bStarted || bFinished)return;
    if(CollectionChoice)return;
    if(GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>()->HasCollectionProgress())
    {
        auto Choices=SNew(SVerticalBox);
        Choices->AddSlot().AutoHeight().Padding(0,0,0,28)
            [SNew(STextBlock).Text(FText::FromString(TEXT("发现有藏品进度，是否要保留藏品进度")))
                .Font(BrainMiningFonts::Body(24)).ColorAndOpacity(MiningMenuStyle::Gold).AutoWrapText(true)];
        TSharedPtr<SButton> Keep;
        Choices->AddSlot().AutoHeight().Padding(0,8)[SAssignNew(Keep,SButton).ButtonStyle(MiningMenuStyle::Button()).ContentPadding(20)
            .OnClicked_Lambda([this](){ConfirmNewGame(true);return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(TEXT("保留"))).Font(BrainMiningFonts::Body(24))]];
        Choices->AddSlot().AutoHeight().Padding(0,8)[SNew(SButton).ButtonStyle(MiningMenuStyle::Button()).ContentPadding(20)
            .OnClicked_Lambda([this](){ConfirmNewGame(false);return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(TEXT("清除"))).Font(BrainMiningFonts::Body(24)).ColorAndOpacity(FLinearColor(1.f,.16f,.12f,1.f))]];
        CollectionChoice=MiningMenuStyle::Frame(Choices,640,0);
        GetGameInstance()->GetGameViewportClient()->AddViewportWidgetContent(CollectionChoice.ToSharedRef(),2000);
        FInputModeUIOnly Mode;Mode.SetWidgetToFocus(Keep);Player->SetInputMode(Mode);
        return;
    }
    ConfirmNewGame(false);
}
void AMiningOpening::ConfirmNewGame(bool bKeepCollection)
{
    if(!bPrepared || bStarted || bFinished)return;
    if(CollectionChoice){GetGameInstance()->GetGameViewportClient()->RemoveViewportWidgetContent(CollectionChoice.ToSharedRef());CollectionChoice.Reset();}
    GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>()->StartNewGame(bKeepCollection);
    bStarted=true;Elapsed=0;
    if(Menu)Menu->SetVisibility(ESlateVisibility::HitTestInvisible);
    WorldTitle->SetVisibility(false);
    if(Player.IsValid()){Player->bShowMouseCursor=false;Player->SetInputMode(FInputModeGameOnly());}
}
void AMiningOpening::Tick(float Dt)
{
    Super::Tick(Dt);
    auto* Session=GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>();
    if(bFinished){Session->FinishDeveloperTravel();return;}
    if(!bPrepared)
    {
        auto* PC=GetWorld()->GetFirstPlayerController();auto* Hall=AMiningExhibitHall::Find(GetWorld());
        if(PC && PC->GetPawn() && (!Hall || Hall->bReady))Prepare(PC);
        return;
    }
    if(!bStarted && Session->PendingDeveloperArea!=INDEX_NONE)
    {
        auto* Saves=GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>();
        if(Saves->HasSave())
        {
            ContinueSavedGame();
            if(!bStarted)Session->PendingDeveloperArea=INDEX_NONE;
        }
        else ConfirmNewGame(true);
        return;
    }
    if(!bStarted || !Player.IsValid() || !Camera)return;
    Elapsed+=Dt;if(Menu&&Elapsed>=.8f){Menu->RemoveFromParent();Menu=nullptr;}
    const float T=FMath::Clamp(Elapsed/Duration,0.f,1.f);const float A=FMath::SmoothStep(0.f,1.f,T);
    const FVector Eye=StandingEye;
    if(APawn* Pawn=Player->GetPawn())Pawn->SetActorLocation(FMath::Lerp(SeatPawnLocation,StandingLocation,A));
    // Shift weight over the feet, rise, then take a small step beside the bed.
    // The endpoint is cached, so idle head animation cannot bend this camera rail.
    const float Rise=FMath::SmoothStep(.18f,.86f,T),Step=FMath::SmoothStep(.12f,.94f,T);
    FVector P=FMath::Lerp(SeatEye,Eye,Step);P.Z=FMath::Lerp(SeatEye.Z,Eye.Z,Rise);
    const float Lean=FMath::SmoothStep(0.f,.22f,T)*(1-FMath::SmoothStep(.38f,.9f,T));
    P+=Facing.Vector().GetSafeNormal2D()*12.f*Lean;P.Z-=3.f*Lean;
    Camera->SetActorLocation(P);
    Camera->GetCameraComponent()->SetFieldOfView(FMath::Lerp(80.f,StandingFOV,A));
    const FRotator FinalAim=Player->GetControlRotation();
    FRotator Look=FQuat::Slerp(Facing.Quaternion(),FinalAim.Quaternion(),A).Rotator();Look.Pitch-=2.f*Lean;
    Camera->SetActorRotation(Look);
    if(T>=1.f)
    {
        Release();
    }
}
void AMiningOpening::BeginTitleToWall(AMainGameDoor* Door)
{
    if(bTitleLanded)return;
    EnsureTitleOnWall();bTitleLanded=true;
    UMiningHapticsSubsystem::Emit(Player.Get(), EMiningHapticCue::TitleLanded);

}
void AMiningOpening::EnsureTitleOnWall()
{
    if (!GetWorld()) return;
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/StartRoom/UI/M_OpeningTitleDecal.M_OpeningTitleDecal"));
    if(!Material)return;
    // Once the animation settles, draw the logo on the actual excavatable wall.
    // A shallow projection disappears locally with each removed piece of terrain.
    auto* Decal=NewObject<UDecalComponent>(this,TEXT("MineableWallTitle"));
    AddInstanceComponent(Decal);Decal->SetupAttachment(RootComponent);
    Decal->SetDecalMaterial(Material);Decal->DecalSize=FVector(18.f,61.5f,104.96f);
    Decal->SetFadeScreenSize(0.f);
    // Decal UVs use local Z for horizontal and local Y for vertical. Rotate
    // the projection frame from the widget frame so the projected bitmap is
    // upright instead of rotated 90 degrees on the wall.
    const FRotationMatrix WidgetFrame(DoorTitleRotation);
    const FVector Projection = -WidgetFrame.GetUnitAxis(EAxis::X);
    const FVector Horizontal = -WidgetFrame.GetUnitAxis(EAxis::Y);
    const FRotator DecalRotation = FRotationMatrix::MakeFromXZ(Projection, Horizontal).Rotator();
    Decal->SetWorldLocationAndRotation(DoorTitleLocation+Projection*10.f,DecalRotation);
    // Configure the pose and fade before publishing the render proxy.
    Decal->SetFadeIn(0.f,2.4f);Decal->RegisterComponent();
    WorldTitle->SetVisibility(false);
}
void AMiningOpening::Release()
{
    if(Menu)Menu->RemoveFromParent();
    if(Player.IsValid())
    {
        if(APawn* Pawn=Player->GetPawn())
        {
            Pawn->SetActorEnableCollision(bOldCollision);
            if(auto* C=Cast<ACharacter>(Pawn))C->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            Player->SetViewTargetWithBlend(Pawn,.45f,EViewTargetBlendFunction::VTBlend_EaseInOut,2.f,true);
        }
        if(bInputLocked){Player->SetIgnoreMoveInput(false);Player->SetIgnoreLookInput(false);bInputLocked=false;}
        Player->bShowMouseCursor=false;Player->SetInputMode(FInputModeGameOnly());
        if(auto* HUD=Player->GetHUD())HUD->bShowHUD=bOldHUD;
        if(auto* PC=Cast<AMiningExhibitController>(Player.Get()))PC->SetMiningGUIEnabled(true);
    }
    if(Camera)
    {
        const TWeakObjectPtr<ACameraActor> Outgoing=Camera;
        const TWeakObjectPtr<AMiningOpening> Opening=this;
        FTimerHandle Cleanup;
        GetWorldTimerManager().SetTimer(Cleanup,FTimerDelegate::CreateLambda([Outgoing,Opening](){if(Outgoing.IsValid())Outgoing->Destroy();if(Opening.IsValid())Opening->bFinished=true;}),.55f,false);
        Camera=nullptr;
    }
}
void AMiningOpening::EndPlay(const EEndPlayReason::Type Reason){if(CollectionChoice){GetGameInstance()->GetGameViewportClient()->RemoveViewportWidgetContent(CollectionChoice.ToSharedRef());CollectionChoice.Reset();}if(CornerTitle)CornerTitle->RemoveFromParent();if(bPrepared&&!bFinished)Release();Super::EndPlay(Reason);}
