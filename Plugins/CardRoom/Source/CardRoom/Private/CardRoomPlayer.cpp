#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "SCardRoomView.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "Framework/Application/SlateApplication.h"

UCardRoomInteractionComponent::UCardRoomInteractionComponent() {PrimaryComponentTick.bCanEverTick=true;}
APlayerController* UCardRoomInteractionComponent::Player() const {return Cast<APlayerController>(GetOwner());}
APawn* UCardRoomInteractionComponent::GetPawn() const {return Player()?Player()->GetPawn():nullptr;}
void UCardRoomInteractionComponent::BeginPlay()
{
    Super::BeginPlay();
    for(TActorIterator<ACardRoomStage> It(GetWorld());It;++It) { Stage=*It; break; }
    if(!Stage || !Player() || !Player()->IsLocalController()) {SetComponentTickEnabled(false);return;}
    if(GetWorld()->URL.HasOption(TEXT("FromEcho=")) && Player()->PlayerCameraManager)
        Player()->PlayerCameraManager->StartCameraFade(1,0,.45f,FLinearColor::Black,false);
    if(Player()->IsLocalController() && GEngine && GEngine->GameViewport)
    {
        SAssignNew(View,SCardRoomView).Owner(this);
        GEngine->GameViewport->AddViewportWidgetContent(View.ToSharedRef(),20);
        Player()->SetInputMode(FInputModeGameOnly()); Player()->bShowMouseCursor=false;
    }
}
void UCardRoomInteractionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if(View && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(View.ToSharedRef());
    RestoreSeat(); View.Reset(); Super::EndPlay(Reason);
}
void UCardRoomInteractionComponent::BindInput(UInputComponent* InputComponent)
{
    InputComponent->BindKey(EKeys::E,IE_Pressed,this,&UCardRoomInteractionComponent::Interact);
    InputComponent->BindKey(EKeys::Q,IE_Pressed,this,&UCardRoomInteractionComponent::LeaveSeat);
    InputComponent->BindKey(EKeys::SpaceBar,IE_Pressed,this,&UCardRoomInteractionComponent::ConfirmCards).bConsumeInput=false;
}
void UCardRoomInteractionComponent::TickComponent(float DeltaSeconds,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaSeconds,TickType,TickFunction);
    if(bSeated && SeatedPawn.Get()!=GetPawn()) {LeaveSeat();return;}
    if(!bRoomActive || !Stage || !GetPawn()) return;
    if(Stage->Run.Phase!=ECardRoomPhase::Exploring)
    {
        float Kick=0;
        if(Stage->Run.Phase==ECardRoomPhase::Resolving)
        {
            const float T=Stage->Run.PhaseTime;
            if(T>=1 && T<1.25f) Kick=(1-(T-1)/.25f)*(Stage->Run.Round==4?2.f:.35f);
            if((Stage->Run.Round==1||Stage->Run.Round==3)&&T>=2.35f&&T<2.65f) Kick=(1-(T-2.35f)/.3f)*(Stage->Run.Round==3?1.5f:.6f);
            Player()->SetControlRotation(FRotator(Stage->SeatPitch+FMath::Sin(T*70)*Kick,Stage->GetActorRotation().Yaw+FMath::Sin(T*49)*Kick*.6f,0));
        }
        else Player()->SetControlRotation(FRotator(Stage->SeatPitch,Stage->GetActorRotation().Yaw,0));
        return;
    }

}
void UCardRoomInteractionComponent::Interact()
{
    if(!bRoomActive || !Stage || !GetPawn()) return;
    if(Stage->Run.Phase==ECardRoomPhase::Victory) { Stage->CollectReward(); return; }
    if(!Stage->IsPlayerNear(GetPawn()->GetActorLocation()) || !Stage->BeginEncounter()) return;
    EntryLocation=GetPawn()->GetActorLocation(); EntryRotation=Player()->GetControlRotation();
    bSeated=true;SeatedPawn=GetPawn();bEntryCollision=GetPawn()->GetActorEnableCollision();
    Player()->SetIgnoreMoveInput(true);Player()->SetIgnoreLookInput(true);
    if(ACharacter* C=Cast<ACharacter>(GetPawn()))
    {
        EntryMovementMode=C->GetCharacterMovement()->MovementMode;EntryCustomMode=C->GetCharacterMovement()->CustomMovementMode;
        C->StopJumping();C->GetCharacterMovement()->StopMovementImmediately();C->GetCharacterMovement()->DisableMovement();
    }
    GetPawn()->SetActorEnableCollision(false);
    GetPawn()->SetActorRotation(FRotator(0,Stage->GetActorRotation().Yaw,0));
    FVector EyeOffset(0,0,64);
    if(auto* Camera=GetPawn()->FindComponentByClass<UCameraComponent>())EyeOffset=Camera->GetComponentLocation()-GetPawn()->GetActorLocation();
    GetPawn()->SetActorLocation(Stage->GetSeatEyeLocation()-EyeOffset);
    Player()->SetControlRotation(FRotator(Stage->SeatPitch,Stage->GetActorRotation().Yaw,0));
    Player()->bShowMouseCursor=true;
    FInputModeGameAndUI Mode; Mode.SetWidgetToFocus(View); Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); Player()->SetInputMode(Mode);
    Player()->SetMouseLocation(int32(GEngine->GameViewport->Viewport->GetSizeXY().X*.5f),int32(GEngine->GameViewport->Viewport->GetSizeXY().Y*.6f));
}
void UCardRoomInteractionComponent::LeaveSeat()
{
    if((!bRoomActive && !bSeated) || !Stage || Stage->Run.Phase==ECardRoomPhase::Exploring) return;
    Stage->Run=FCardRoomRun(); Stage->RestartEncounter(); Stage->Run=FCardRoomRun();
    RestoreSeat();
    Player()->bShowMouseCursor=false;Player()->SetInputMode(FInputModeGameOnly());
    if(FSlateApplication::IsInitialized())FSlateApplication::Get().SetAllUserFocusToGameViewport();
}
void UCardRoomInteractionComponent::ConfirmCards() { if(Stage) Stage->Run.Confirm(); }
void UCardRoomInteractionComponent::RestoreSeat()
{
    if(!bSeated)return;
    if(SeatedPawn.IsValid())
    {
        SeatedPawn->SetActorLocation(EntryLocation,false,nullptr,ETeleportType::TeleportPhysics);
        SeatedPawn->SetActorEnableCollision(bEntryCollision);
        if(auto* C=Cast<ACharacter>(SeatedPawn.Get()))C->GetCharacterMovement()->SetMovementMode(EMovementMode(EntryMovementMode),EntryCustomMode);
    }
    if(Player()) {Player()->SetControlRotation(EntryRotation);Player()->SetIgnoreMoveInput(false);Player()->SetIgnoreLookInput(false);}
    bSeated=false;SeatedPawn.Reset();
}

void UCardRoomInteractionComponent::SetRoomActive(bool bActive)
{
    bRoomActive=bActive;
    if(View)View->SetVisibility(bActive?EVisibility::Visible:EVisibility::Collapsed);
}

void UCardRoomInteractionComponent::ReleaseForExploration()
{
    RestoreSeat();SetRoomActive(false);
    if(Player()){Player()->bShowMouseCursor=false;Player()->SetInputMode(FInputModeGameOnly());}
    if(FSlateApplication::IsInitialized())FSlateApplication::Get().SetAllUserFocusToGameViewport();
}
