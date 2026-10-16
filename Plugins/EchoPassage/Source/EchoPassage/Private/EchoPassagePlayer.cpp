#include "EchoPassagePlayer.h"
#include "EchoPassageStage.h"
#include "SEchoPassageView.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"

UEchoPassageInteractionComponent::UEchoPassageInteractionComponent() { PrimaryComponentTick.bCanEverTick=true; }
APlayerController* UEchoPassageInteractionComponent::Player() const { return Cast<APlayerController>(GetOwner()); }
APawn* UEchoPassageInteractionComponent::GetPawn() const { return Player()?Player()->GetPawn():nullptr; }
void UEchoPassageInteractionComponent::BeginPlay()
{
    Super::BeginPlay();
    for(TActorIterator<AEchoPassageStage> It(GetWorld());It;++It) {Stage=*It;break;}
    if(!Stage || !Player() || !Player()->IsLocalController()) {SetComponentTickEnabled(false);return;}
    if(GEngine && GEngine->GameViewport)
    {
        SAssignNew(View,SEchoPassageView).Owner(this);GEngine->GameViewport->AddViewportWidgetContent(View.ToSharedRef(),20);
        Player()->SetInputMode(FInputModeGameOnly());Player()->bShowMouseCursor=false;
    }
    RefreshCharacter();
}
void UEchoPassageInteractionComponent::LockMovement(bool bLock)
{
    if(Player() && bMovementLocked!=bLock) {Player()->SetIgnoreMoveInput(bLock);bMovementLocked=bLock;}
}
void UEchoPassageInteractionComponent::ReleaseCharacter()
{
    if(Character.IsValid())Character->LandedDelegate.RemoveDynamic(this,&UEchoPassageInteractionComponent::OnLanded);
    if(Pickaxe)
    {
        TArray<USceneComponent*> Children;Pickaxe->GetChildrenComponents(true,Children);
        for(auto* Part:Children)Part->DestroyComponent();
        Pickaxe->DestroyComponent();Pickaxe=nullptr;
    }
    Character.Reset();
}
void UEchoPassageInteractionComponent::RefreshCharacter()
{
    auto* P=Cast<ACharacter>(GetPawn());
    if(Character.Get()==P)return;
    ReleaseCharacter();Character=P;
    if(!P)return;
    P->LandedDelegate.AddDynamic(this,&UEchoPassageInteractionComponent::OnLanded);
    if(bUseSharedPickaxe)return;
    auto* Camera=P->FindComponentByClass<UCameraComponent>();if(!Camera)return;
    Pickaxe=NewObject<USceneComponent>(P);Pickaxe->SetupAttachment(Camera);Pickaxe->RegisterComponent();
    auto Make=[&](FVector Location,FVector Size,const TCHAR* Material)
    {
        auto* Part=NewObject<UStaticMeshComponent>(P);Part->SetupAttachment(Pickaxe);
        Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Part->SetRelativeLocation(Location);Part->SetRelativeScale3D(Size/100);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetCastShadow(false);
        Part->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,Material));Part->RegisterComponent();return Part;
    };
    Make(FVector::ZeroVector,FVector(3.4,3.4,51),TEXT("/Game/Passages/Echo/Materials/M_PickWood.M_PickWood"));
    Make(FVector(0,0,22),FVector(8,33,7),TEXT("/Game/Passages/Echo/Materials/M_PickIron.M_PickIron"))->SetRelativeRotation(FRotator(0,0,-12));
}
void UEchoPassageInteractionComponent::OnLanded(const FHitResult& Hit)
{
    if(Stage && Stage->bCollapsed && !Stage->bLanded)
    {
        Stage->EmitEcho(Hit.ImpactPoint,true);LockMovement(false);
    }
}
void UEchoPassageInteractionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    LockMovement(false);
    if(bLookLocked && Player())Player()->SetIgnoreLookInput(false);
    ReleaseCharacter();
    if(View && GEngine && GEngine->GameViewport)GEngine->GameViewport->RemoveViewportWidgetContent(View.ToSharedRef());
    GetWorld()->GetTimerManager().ClearTimer(TravelTimer);View.Reset();Super::EndPlay(Reason);
}
void UEchoPassageInteractionComponent::BindInput(UInputComponent* Input)
{
    Input->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&UEchoPassageInteractionComponent::Knock).bConsumeInput=false;
    Input->BindKey(EKeys::R,IE_Pressed,this,&UEchoPassageInteractionComponent::Restart);
}
void UEchoPassageInteractionComponent::TickComponent(float DeltaSeconds,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaSeconds,TickType,TickFunction);
    if(!Stage || !Player())return;
    RefreshCharacter();auto* P=Character.Get();if(!P || bChangingLevel)return;
    SwingTime+=DeltaSeconds;
    if(Pickaxe)
    {
        Pickaxe->SetVisibility(bExplorationActive,true);
        const float Swing=SwingTime<.48f?FMath::Sin(FMath::Clamp(SwingTime/.48f,0.f,1.f)*PI):0;
        const float Bob=P->GetVelocity().Size2D()>20 && !P->GetCharacterMovement()->IsFalling()?FMath::Sin(GetWorld()->GetTimeSeconds()*10)*.6f:0;
        Pickaxe->SetRelativeRotation(FRotator(-15-Swing*(bWallStrike?65:100),-12+Swing*12,12));
        Pickaxe->SetRelativeLocation(FVector(52+Swing*9,28-Swing*12,-35-Swing*12+Bob));
    }
    const FVector Local=Stage->GetActorTransform().InverseTransformPosition(P->GetActorLocation());
    if(bExplorationActive && !Stage->bCollapsed && Local.X>-90 && Local.X<200 && FMath::Abs(Local.Y)<200 && Local.Z>700)
    {P->GetCharacterMovement()->StopMovementImmediately();LockMovement(true);Stage->Collapse();}
    if(!Stage->bRoomInSameLevel && Stage->bLanded && !Stage->bCompleted && Stage->IsAtExit(P->GetActorLocation()))
    {
        Stage->Complete();
        if(!Stage->DestinationLevel.IsNull())
        {
            const FString Package=Stage->DestinationLevel.ToSoftObjectPath().GetLongPackageName();
            if(FPackageName::DoesPackageExist(Package))
            {
                bChangingLevel=true;bPendingStrike=false;P->GetCharacterMovement()->StopMovementImmediately();LockMovement(true);
                Player()->SetIgnoreLookInput(true);bLookLocked=true;
                if(Player()->PlayerCameraManager)Player()->PlayerCameraManager->StartCameraFade(0,1,.35f,FLinearColor::Black,false,true);
                GetWorld()->GetTimerManager().SetTimer(TravelTimer,this,&UEchoPassageInteractionComponent::TravelToDestination,.4f,false);
            }
            else UE_LOG(LogTemp,Error,TEXT("Echo passage destination missing: %s"),*Package);
        }
    }
    if(Local.Z< -600)Restart();
    if(bPendingStrike && GetWorld()->GetTimeSeconds()>=StrikeAt) {bPendingStrike=false;Stage->EmitEcho(PendingPoint);}
}
void UEchoPassageInteractionComponent::Knock()
{
    auto* P=Cast<ACharacter>(GetPawn());
    if(!bExplorationActive || !Stage || !P || !Stage->bLanded || (Stage->bCompleted && !Stage->bRoomInSameLevel) || bPendingStrike || SwingTime<Stage->KnockCooldown || GetWorld()->GetTimeSeconds()-Stage->LastEcho<Stage->KnockCooldown || P->GetCharacterMovement()->IsFalling())return;
    FVector Eye;FRotator Aim;Player()->GetPlayerViewPoint(Eye,Aim);
    FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(EchoStrike),false,P);
    bool Found=GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+Aim.Vector()*245,ECC_Visibility,Query);
    if(!Found)
    {
        const FVector Base=P->GetActorLocation()+FRotator(0,Player()->GetControlRotation().Yaw,0).Vector()*75;
        const float ReachDown=P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+100;
        Found=GetWorld()->LineTraceSingleByChannel(Hit,Base,Base-FVector(0,0,ReachDown),ECC_Visibility,Query);
    }
    if(!Found) {Feedback=TEXT("靠近地面或墙壁，再敲一下");FeedbackUntil=GetWorld()->GetTimeSeconds()+1.5f;return;}
    bWallStrike=FMath::Abs(Hit.ImpactNormal.Z)<.5f;SwingTime=0;
    OnKnock.Broadcast();
    PendingPoint=Hit.ImpactPoint;StrikeAt=GetWorld()->GetTimeSeconds()+.18f;bPendingStrike=true;
    Feedback=bWallStrike?TEXT("石壁传来回声"):TEXT("敲击地面，听见前路");FeedbackUntil=GetWorld()->GetTimeSeconds()+1.4f;
}
void UEchoPassageInteractionComponent::Restart()
{
    if(RestartOverride.IsBound() && RestartOverride.Execute())return;
    // An always-loaded passage must not steal the shared miner's R key while
    // the player is in another room or excavation area.
    if(!bExplorationActive)return;
    if(!Stage || !GetPawn() || bChangingLevel)return;
    bPendingStrike=false;FeedbackUntil=0;Stage->ResetPassage();LockMovement(false);SwingTime=10;
    auto* P=Cast<ACharacter>(GetPawn());
    if(P) {P->StopJumping();P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);}
    // Entry floor is at local Z=900. Account for the actual host capsule.
    FVector Entry=Stage->GetEntry();if(P)Entry.Z+=P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-88;
    OnRestarted.Broadcast();
    GetPawn()->SetActorLocation(Entry,false,nullptr,ETeleportType::TeleportPhysics);
    Player()->SetControlRotation(FRotator(0,Stage->GetActorRotation().Yaw,0));
}
void UEchoPassageInteractionComponent::TravelToDestination()
{
    if(Stage && bChangingLevel && !Stage->DestinationLevel.IsNull())
        UGameplayStatics::OpenLevelBySoftObjectPtr(this,Stage->DestinationLevel,true,TEXT("FromEcho=1"));
}

void UEchoPassageInteractionComponent::SetExplorationActive(bool bActive)
{
    bExplorationActive=bActive;
    if(!bActive)bPendingStrike=false;
    if(View)View->SetVisibility(bActive?EVisibility::HitTestInvisible:EVisibility::Collapsed);
    if(Pickaxe)Pickaxe->SetVisibility(bActive,true);
}
