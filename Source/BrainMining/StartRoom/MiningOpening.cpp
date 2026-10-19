#include "StartRoom/MiningOpening.h"
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
#include "MainGameDoor.h"
#include "Blueprint/WidgetLayoutLibrary.h"

AMiningOpening* AMiningOpening::EnsureMainGameOpening(AMiningExhibitHall* Hall)
{
    if(!Hall || !Hall->bReady || Hall->Areas.IsEmpty())return nullptr;
    UWorld* World=Hall->GetWorld();
    if(!World || !World->GetMapName().EndsWith(TEXT("Lvl_MainGame")))return nullptr;
    // Keep any explicitly authored opening. This also makes repeated calls safe.
    for(TActorIterator<AMiningOpening> It(World);It;++It)return *It;
    auto* Room=AMiningStartRoom::Find(World);if(!Room)return nullptr;
    auto* Title=LoadObject<UTexture2D>(nullptr,TEXT("/Game/StartRoom/UI/T_OpeningTitle.T_OpeningTitle"));
    if(!Title)return nullptr;
    const FTransform RoomTransform=Room->GetActorTransform();
    const FTransform SpawnTransform(RoomTransform.GetRotation(),RoomTransform.GetLocation());
    auto* Opening=World->SpawnActorDeferred<AMiningOpening>(StaticClass(),SpawnTransform,Room,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if(!Opening)return nullptr;
    Opening->Tags.Add(Hall->Areas[0].Tag);
    Opening->TitleTexture=Title;
    Opening->SeatEye=RoomTransform.TransformPosition(FVector(-260,-160,127));
    Opening->StandingLocation=RoomTransform.TransformPosition(FVector(-260,-70,98));
    Opening->Facing=(RoomTransform.GetRotation()*FRotator(0,13,0).Quaternion()).Rotator();
    Opening->DoorTitleLocation=RoomTransform.TransformPosition(FVector(466,0,168));
    Opening->DoorTitleRotation=(RoomTransform.GetRotation()*FRotator(0,180,0).Quaternion()).Rotator();
    Opening->FinishSpawning(SpawnTransform);
    return Opening;
}

TSharedRef<SWidget> UMiningOpeningTitle::RebuildWidget()
{
    auto* Picture=WidgetTree->ConstructWidget<UImage>();WidgetTree->RootWidget=Picture;
    Picture->SetBrushFromTexture(Texture);
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
    auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();
    auto* CanvasSlot=Canvas->AddChildToCanvas(Column);
    CanvasSlot->SetAnchors(FAnchors(.5f,.72f));CanvasSlot->SetAlignment(FVector2D(.5f,0));CanvasSlot->SetAutoSize(true);
    auto AddButton=[&](const TCHAR* Label,bool Start)
    {
        auto* Box=WidgetTree->ConstructWidget<USizeBox>();Box->SetWidthOverride(280);Box->SetHeightOverride(56);
        auto* Row=Column->AddChildToVerticalBox(Box);Row->SetPadding(FMargin(0,6));
        auto* Button=WidgetTree->ConstructWidget<UButton>();Box->AddChild(Button);
        FButtonStyle Style=Button->GetStyle();
        Style.Normal.TintColor=FSlateColor(FLinearColor(.025f,.035f,.035f,.92f));
        Style.Hovered.TintColor=FSlateColor(FLinearColor(.32f,.23f,.09f,1));
        Style.Pressed.TintColor=FSlateColor(FLinearColor(.5f,.35f,.1f,1));Button->SetStyle(Style);
        auto* Text=WidgetTree->ConstructWidget<UTextBlock>();Text->SetText(FText::FromString(Label));
        Text->SetFont(BrainMiningFonts::Body(24));Text->SetColorAndOpacity(FSlateColor(FLinearColor(1,.79f,.4f)));
        Button->AddChild(Text);
        if(Start){StartButton=Button;Button->OnClicked.AddDynamic(this,&UMiningOpeningMenu::StartGame);}
        else Button->OnClicked.AddDynamic(this,&UMiningOpeningMenu::QuitGame);
    };
    AddButton(TEXT("开始游戏"),true);AddButton(TEXT("退出游戏"),false);
    return Super::RebuildWidget();
}
void UMiningOpeningMenu::StartGame(){if(Opening.IsValid())Opening->StartGame();}
void UMiningOpeningMenu::QuitGame(){UKismetSystemLibrary::QuitGame(this,GetOwningPlayer(),EQuitPreference::Quit,false);}

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
    APawn* Pawn=Controller->GetPawn();bOldCollision=Pawn->GetActorEnableCollision();Pawn->SetActorEnableCollision(false);
    if(auto* Character=Cast<ACharacter>(Pawn)){Character->GetCharacterMovement()->StopMovementImmediately();Character->GetCharacterMovement()->DisableMovement();}
    const FVector EyeOffset=Pawn->GetPawnViewLocation()-Pawn->GetActorLocation();
    StandingEye=StandingLocation+EyeOffset;SeatPawnLocation=SeatEye-EyeOffset;
    Pawn->SetActorLocation(SeatPawnLocation);Controller->SetControlRotation(Facing);
    Controller->SetIgnoreMoveInput(true);Controller->SetIgnoreLookInput(true);bInputLocked=true;
    if(auto* HUD=Controller->GetHUD()){bOldHUD=HUD->bShowHUD;HUD->bShowHUD=false;}
    if(auto* MiningPC=Cast<AMiningExhibitController>(Controller))MiningPC->SetMiningGUIEnabled(false);
    Camera=GetWorld()->SpawnActor<ACameraActor>(SeatEye,Facing);
    Camera->GetCameraComponent()->SetFieldOfView(80);Camera->GetCameraComponent()->bConstrainAspectRatio=false;Controller->SetViewTarget(Camera);
    auto* Title=CreateWidget<UMiningOpeningTitle>(Controller);Title->Texture=TitleTexture;WorldTitle->SetWidget(Title);
    InitialTitle=SeatEye+Facing.Vector()*240+Facing.RotateVector(FVector(0,0,40));
    InitialRotation=(Facing+FRotator(0,180,0)).Quaternion();
    WorldTitle->SetWorldLocationAndRotation(InitialTitle,InitialRotation);WorldTitle->SetWorldScale3D(FVector(.19f));WorldTitle->SetVisibility(true);
    Menu=CreateWidget<UMiningOpeningMenu>(Controller);Menu->Opening=this;Menu->SetIsFocusable(true);Menu->AddToViewport(500);
    Controller->bShowMouseCursor=true;FInputModeUIOnly Mode;Mode.SetWidgetToFocus(Menu->StartButton ? Menu->StartButton->TakeWidget() : Menu->TakeWidget());Controller->SetInputMode(Mode);
}
void AMiningOpening::StartGame()
{
    if(!bPrepared || bStarted || bFinished)return;
    bStarted=true;Elapsed=0;if(Menu)Menu->RemoveFromParent();
    CornerTitle=CreateWidget<UMiningOpeningTitle>(Player.Get());CornerTitle->Texture=TitleTexture;CornerTitle->bCornerLayout=true;
    CornerTitle->SetVisibility(ESlateVisibility::HitTestInvisible);CornerTitle->AddToViewport(490);
    WorldTitle->SetVisibility(false);
    if(Player.IsValid()){Player->bShowMouseCursor=false;Player->SetInputMode(FInputModeGameOnly());}
}
void AMiningOpening::Tick(float Dt)
{
    Super::Tick(Dt);
    if(bTitleFlying)
    {
        TitleFlightTime+=Dt;const float A=FMath::SmoothStep(0.f,1.f,FMath::Clamp(TitleFlightTime/.95f,0.f,1.f));
        const FVector Logo=FMath::Lerp(InitialTitle,DoorTitleLocation,A)+FVector(0,0,FMath::Sin(A*PI)*32);
        WorldTitle->SetWorldLocationAndRotation(Logo,FQuat::Slerp(InitialRotation,DoorTitleRotation.Quaternion(),A));
        WorldTitle->SetWorldScale3D(FVector(FMath::Lerp(InitialTitleScale,.205f,A)));
        if(A>=1){EnsureTitleOnWall();bTitleFlying=false;bTitleLanded=true;SetActorTickEnabled(false);}
        return;
    }
    if(bFinished)return;
    if(!bPrepared)
    {
        auto* PC=GetWorld()->GetFirstPlayerController();auto* Hall=AMiningExhibitHall::Find(GetWorld());
        if(PC && PC->GetPawn() && GetWorld()->GetTimeSeconds()>.6f && (!Hall || Hall->bReady))Prepare(PC);
        return;
    }
    if(!bStarted || !Player.IsValid() || !Camera)return;
    Elapsed+=Dt;const float T=FMath::Clamp(Elapsed/Duration,0.f,1.f);const float A=FMath::SmoothStep(0.f,1.f,T);
    const FVector Eye=StandingEye;
    if(APawn* Pawn=Player->GetPawn())Pawn->SetActorLocation(FMath::Lerp(SeatPawnLocation,StandingLocation,A));
    // Lean forward, lift from the mattress, then settle beside the bed.
    FVector P=FMath::Lerp(SeatEye,Eye,A);P.Z-=FMath::Sin(FMath::Min(T*2.f,1.f)*PI)*9.f;
    Camera->SetActorLocation(P);
    const FRotator FinalAim=Player->GetControlRotation();
    Camera->SetActorRotation(FQuat::Slerp(Facing.Quaternion(),FinalAim.Quaternion(),A));
    if(T>=1.f)
    {
        bFinished=true;Release();
    }
}
void AMiningOpening::BeginTitleToWall(AMainGameDoor* Door)
{
    if(!bFinished || bTitleFlying || bTitleLanded || !Player.IsValid() || !Door)return;
    // Start the flight exactly where the upper-left HUD title is painted.
    const float DPI=UWidgetLayoutLibrary::GetViewportScale(Player.Get());
    FVector Origin,Direction,LeftOrigin,LeftDirection,RightOrigin,RightDirection;
    FVector Eye;FRotator View;Player->GetPlayerViewPoint(Eye,View);
    InitialTitle=Eye+View.RotateVector(FVector(240,-160,85));InitialTitleScale=.07f;
    if(Player->DeprojectScreenPositionToWorld(176*DPI,112*DPI,Origin,Direction) &&
       Player->DeprojectScreenPositionToWorld(36*DPI,112*DPI,LeftOrigin,LeftDirection) &&
       Player->DeprojectScreenPositionToWorld(316*DPI,112*DPI,RightOrigin,RightDirection))
    {
        InitialTitle=Origin+Direction*240;
        InitialTitleScale=FVector::Distance(LeftOrigin+LeftDirection*240,RightOrigin+RightDirection*240)/1024.f;
    }
    InitialRotation=(View+FRotator(0,180,0)).Quaternion();
    if(CornerTitle)CornerTitle->RemoveFromParent();
    WorldTitle->SetWorldLocationAndRotation(InitialTitle,InitialRotation);WorldTitle->SetWorldScale3D(FVector(InitialTitleScale));
    WorldTitle->SetVisibility(true);bTitleFlying=true;TitleFlightTime=0;SetActorTickEnabled(true);
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
    Decal->SetFadeScreenSize(0.f);Decal->RegisterComponent();
    // Decal UVs use local Z for horizontal and local Y for vertical. Rotate
    // the projection frame from the widget frame so the projected bitmap is
    // upright instead of rotated 90 degrees on the wall.
    const FRotationMatrix WidgetFrame(DoorTitleRotation);
    const FVector Projection = -WidgetFrame.GetUnitAxis(EAxis::X);
    const FVector Horizontal = -WidgetFrame.GetUnitAxis(EAxis::Y);
    const FRotator DecalRotation = FRotationMatrix::MakeFromXZ(Projection, Horizontal).Rotator();
    Decal->SetWorldLocationAndRotation(DoorTitleLocation+Projection*10.f,DecalRotation);
    WorldTitle->SetVisibility(false);
}
void AMiningOpening::Release()
{
    if(Menu)Menu->RemoveFromParent();
    if(Player.IsValid())
    {
        if(APawn* Pawn=Player->GetPawn())
        {
            Pawn->SetActorEnableCollision(bOldCollision);Player->SetViewTarget(Pawn);
            if(auto* C=Cast<ACharacter>(Pawn))C->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        }
        if(bInputLocked){Player->SetIgnoreMoveInput(false);Player->SetIgnoreLookInput(false);bInputLocked=false;}
        Player->bShowMouseCursor=false;Player->SetInputMode(FInputModeGameOnly());
        if(auto* HUD=Player->GetHUD())HUD->bShowHUD=bOldHUD;
        if(auto* PC=Cast<AMiningExhibitController>(Player.Get()))PC->SetMiningGUIEnabled(true);
    }
    if(Camera){Camera->Destroy();Camera=nullptr;}
}
void AMiningOpening::EndPlay(const EEndPlayReason::Type Reason){if(CornerTitle)CornerTitle->RemoveFromParent();if(bPrepared&&!bFinished)Release();Super::EndPlay(Reason);}
