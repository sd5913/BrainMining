#include "PopupClose/PopupCloseStage.h"
#include "PopupClose/PopupCloseContent.h"
#include "PopupClose/PopupOfficeDevice.h"
#include "PopupClose/PopupCloseFlyingWindow.h"
#include "PopupClose/PopupCloseGameMode.h"
#include "PopupClose/PopupCloseOffice.h"
#include "PopupClose/PopupCloseWidgets.h"
#include "Camera/CameraComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/ConstructorHelpers.h"
#if WITH_EDITOR
#include "Editor.h"
#include "FileHelpers.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogPopupClose,Log,All);
namespace
{
 constexpr float ScreenEnterSeconds=.25f;
 constexpr float ScreenExitSeconds=.8f;
 constexpr float PanicSeconds=45.f;
 constexpr float WorkSeconds=65.f;
 constexpr float SpraySeconds=8.f;
 constexpr float SprayInterval=.08f;
 float IssueInterval(float Seconds,int32 Index)
 {
  const float InitialInterval=Index>=2?16.f:14.f;
  const float ManageableInterval=Index>=2?12.f:10.f;
  if(Seconds<PanicSeconds)return FMath::Lerp(InitialInterval,ManageableInterval,FMath::Clamp(Seconds/PanicSeconds,0.f,1.f));
  return ManageableInterval*FMath::Pow(.12f/ManageableInterval,FMath::Clamp((Seconds-PanicSeconds)/(WorkSeconds-PanicSeconds),0.f,1.f));
 }
 const FVector DeviceLocations[]={FVector(0,0,0),FVector(-500,350,0),FVector(750,430,0),FVector(-780,-350,0)};
 const float DeviceYaws[]={0,180,0,180};
 struct FPaperLandingStatus
 {
  int32 Count=0;
  int32 BelowFloor=0;
  bool bCCD=true;
  float LowestClearance=FLT_MAX;
 };
 FPaperLandingStatus PaperLandingStatus(UWorld* World,float FloorTop)
 {
  FPaperLandingStatus Status;
  for(TActorIterator<APopupCloseFlyingWindow> It(World);It;++It)
  {
   if(It->IsHidden())continue;
   auto Collision=Cast<UBoxComponent>(It->GetRootComponent());
   if(!Collision || Collision->GetUnscaledBoxExtent().X>=1.f)continue;
   if(auto Mesh=It->FindComponentByClass<UStaticMeshComponent>())
   {
    const float Clearance=Mesh->CalcBounds(Mesh->GetComponentTransform()).GetBox().Min.Z-FloorTop;
    ++Status.Count;Status.BelowFloor+=Clearance<-.25f?1:0;
    Status.bCCD &= Collision->BodyInstance.bUseCCD;
    Status.LowestClearance=FMath::Min(Status.LowestClearance,Clearance);
   }
  }
  return Status;
 }
}

APopupCloseStage::APopupCloseStage()
{
 PrimaryActorTick.bCanEverTick=true;
 SceneRoot=CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));SetRootComponent(SceneRoot);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
 BaseMaterial=Material.Object;
 auto MakeBox=[&](const TCHAR* Name,FVector P,FVector Scale)
 {
  auto Mesh=CreateDefaultSubobject<UStaticMeshComponent>(Name);Mesh->SetupAttachment(SceneRoot);
  Mesh->SetStaticMesh(Cube.Object);Mesh->SetRelativeLocation(P);Mesh->SetRelativeScale3D(Scale);
  Mesh->SetCollisionProfileName(TEXT("BlockAll"));return Mesh;
 };
 Floor=MakeBox(TEXT("Floor"),FVector(0,0,-10),FVector(24,18,.2f));
 Ceiling=MakeBox(TEXT("Ceiling"),FVector(0,0,370),FVector(24,18,.2f));
 WallPositiveY=MakeBox(TEXT("WallPositiveY"),FVector(0,900,180),FVector(24,.2f,3.6f));
 WallNegativeY=MakeBox(TEXT("WallNegativeY"),FVector(0,-900,180),FVector(24,.2f,3.6f));
 TableTop=MakeBox(TEXT("TableTop"),FVector(0,0,74),FVector(1.9f,1.f,.06f));
 for(int32 I=0;I<4;++I)MakeBox(*FString::Printf(TEXT("Leg%d"),I+1),FVector(I<2?60:-60,I%2?28:-28,36),FVector(.08f,.08f,.72f));
 Monitor=MakeBox(TEXT("Monitor"),FVector(48,0,122),FVector(.08f,.72f,.5f));
 Screen=MakeBox(TEXT("Screen"),FVector(42,0,124),FVector(.02f,.62f,.4f));
 Stand=MakeBox(TEXT("Stand"),FVector(48,0,92),FVector(.08f,.08f,.22f));
 MakeBox(TEXT("StandBase"),FVector(40,0,80),FVector(.28f,.16f,.03f));
 Keyboard=MakeBox(TEXT("Keyboard"),FVector(-10,0,80),FVector(.34f,.14f,.03f));
 AimVolume=CreateDefaultSubobject<UBoxComponent>(TEXT("AimVolume"));AimVolume->SetupAttachment(SceneRoot);
 AimVolume->SetRelativeLocation(FVector(42,0,124));AimVolume->SetBoxExtent(FVector(36,48,36));
 AimVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);AimVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
 AimVolume->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);AimVolume->SetHiddenInGame(true);
 ScreenCamera=CreateDefaultSubobject<UCameraComponent>(TEXT("ScreenCamera"));ScreenCamera->SetupAttachment(SceneRoot);
 ScreenCamera->SetRelativeLocation(FVector(-75,0,124));ScreenCamera->SetFieldOfView(55);
 ScreenCamera->bUsePawnControlRotation=false;
 DesktopSurface=CreateDefaultSubobject<UWidgetComponent>(TEXT("DesktopSurface"));DesktopSurface->SetupAttachment(SceneRoot);
 DesktopSurface->SetWidgetSpace(EWidgetSpace::World);
 DesktopSurface->SetDrawSize(FVector2D(PopupCloseContent::DesktopWidth,PopupCloseContent::DesktopHeight));
 DesktopSurface->SetRelativeLocation(FVector(39.5f,0,124));DesktopSurface->SetRelativeRotation(FRotator(0,180,0));
 DesktopSurface->SetRelativeScale3D(FVector(.0484375f));DesktopSurface->SetBlendMode(EWidgetBlendMode::Opaque);
 DesktopSurface->SetTwoSided(false);DesktopSurface->SetWindowFocusable(false);
 DesktopSurface->SetCollisionProfileName(TEXT("UI"));DesktopSurface->SetCastShadow(false);
 DesktopInteraction=CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("DesktopInteraction"));
 DesktopInteraction->SetupAttachment(SceneRoot);DesktopInteraction->InteractionSource=EWidgetInteractionSource::Mouse;
 DesktopInteraction->InteractionDistance=1000;DesktopInteraction->VirtualUserIndex=1;DesktopInteraction->bEnableHitTesting=false;
 DesktopInteraction->SetComponentTickEnabled(false);
 SpawnPoint=CreateDefaultSubobject<USceneComponent>(TEXT("SpawnPoint"));SpawnPoint->SetupAttachment(SceneRoot);
 SpawnPoint->SetRelativeLocation(FVector(-950,0,92));
 SpawnArrow=CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnArrow"));SpawnArrow->SetupAttachment(SpawnPoint);
 Sun=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));Sun->SetupAttachment(SceneRoot);
 Sun->SetMobility(EComponentMobility::Movable);Sun->SetRelativeRotation(FRotator(-48,25,0));Sun->SetIntensity(3);Sun->SetAtmosphereSunLight(true);
 Sky=CreateDefaultSubobject<USkyLightComponent>(TEXT("Sky"));Sky->SetupAttachment(SceneRoot);
 Sky->SetMobility(EComponentMobility::Movable);Sky->bRealTimeCapture=false;Sky->SetIntensity(.5f);
 Atmosphere=CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));Atmosphere->SetupAttachment(SceneRoot);
 RoomLight=CreateDefaultSubobject<UPointLightComponent>(TEXT("RoomLight"));RoomLight->SetupAttachment(SceneRoot);
 RoomLight->SetMobility(EComponentMobility::Movable);RoomLight->SetRelativeLocation(FVector(0,0,285));
 RoomLight->SetIntensity(15000);RoomLight->SetAttenuationRadius(1700);RoomLight->SetCastShadows(false);
}

void APopupCloseStage::ConfigureOffice()
{
 // Older saved component overrides must not keep the original small room.
 Floor->SetRelativeScale3D(FVector(24,18,.2f));
 Ceiling->SetRelativeLocation(FVector(0,0,370));Ceiling->SetRelativeScale3D(FVector(24,18,.2f));
 WallPositiveY->SetRelativeLocation(FVector(0,900,180));WallNegativeY->SetRelativeLocation(FVector(0,-900,180));
 WallPositiveY->SetRelativeScale3D(FVector(24,.2f,3.6f));WallNegativeY->SetRelativeScale3D(FVector(24,.2f,3.6f));
 SpawnPoint->SetRelativeLocation(FVector(-950,0,92));ScreenCamera->SetRelativeLocation(FVector(-75,0,124));
 RoomLight->SetRelativeLocation(FVector(0,0,285));RoomLight->SetIntensity(15000);RoomLight->SetAttenuationRadius(1700);
 Sun->SetIntensity(3);Sky->bRealTimeCapture=false;Sky->SetIntensity(.5f);
 Sun->SetVisibility(!bAreaManaged);Sky->SetVisibility(!bAreaManaged);Atmosphere->SetVisibility(!bAreaManaged);
 ScreenCamera->bOverrideAspectRatioAxisConstraint=true;
 ScreenCamera->AspectRatioAxisConstraint=EAspectRatioAxisConstraint::AspectRatio_MaintainXFOV;
 ScreenCamera->PostProcessBlendWeight=0;
 TableTop->SetRelativeScale3D(FVector(1.9f,1.f,.06f));
 Keyboard->SetRelativeScale3D(FVector(.16f,.36f,.03f));
 // Replacement metal frames are part of the office's instanced geometry.
 TArray<UStaticMeshComponent*> Meshes;GetComponents(Meshes);
 for(auto Mesh:Meshes)if(Mesh->GetName().StartsWith(TEXT("Leg")) || Mesh==Monitor || Mesh==Screen || Mesh==Stand || Mesh->GetName()==TEXT("StandBase"))
 {
  Mesh->SetVisibility(false);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 }
 AimVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 DesktopSurface->SetVisibility(false);DesktopSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 DesktopInteraction->bEnableHitTesting=false;
}

void APopupCloseStage::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);ConfigureOffice();ApplyColors();
}

void APopupCloseStage::ApplyColors()
{
 Paint(Floor,FLinearColor(.14f,.18f,.23f));Paint(Ceiling,FLinearColor(.82f,.84f,.8f));
 Paint(WallPositiveY,FLinearColor(.8f,.82f,.79f));Paint(WallNegativeY,FLinearColor(.8f,.82f,.79f));
 Paint(TableTop,FLinearColor(.38f,.25f,.14f));Paint(Monitor,FLinearColor(.035f,.04f,.05f));
 Paint(Screen,FLinearColor(.035f,.08f,.16f));Paint(Stand,FLinearColor(.065f,.08f,.1f));Paint(Keyboard,FLinearColor(.1f,.12f,.15f));
}
void APopupCloseStage::Paint(UStaticMeshComponent* Mesh,const FLinearColor& Color) const
{
 if(!Mesh || !BaseMaterial)return;
 auto Material=UMaterialInstanceDynamic::Create(BaseMaterial,const_cast<APopupCloseStage*>(this));
 Material->SetVectorParameterValue(TEXT("Color"),Color);Mesh->SetMaterial(0,Material);
}


void APopupCloseStage::BeginPlay()
{
 Super::BeginPlay();ConfigureOffice();ApplyColors();GatherDevices();
 if (bAreaManaged) SetAreaActive(false);
}
void APopupCloseStage::SetAreaActive(bool bActive)
{
 if (!bActive) CancelSession();
 bAreaActive = bActive;
 if (!bActive && Prompt) Prompt->SetVisibility(ESlateVisibility::Collapsed);
}
void APopupCloseStage::CancelSession()
{
 if (ActiveComputer) ActiveComputer->SetScreenInteractive(false);
 if (View != EView::Free)
  if (auto Controller = LockedController.Get())
  {
   Controller->SetShowMouseCursor(false);
   Controller->SetInputMode(FInputModeGameOnly());
   if (auto Pawn = LockedPawn.Get()) Controller->SetViewTarget(Pawn);
  }
 RestorePlayerControl();
 View = EView::Free;
 ActiveComputer = nullptr;
 LockedPawn.Reset();
 LockedController.Reset();
}
void APopupCloseStage::ResetCompletedSessions()
{
 CancelSession();
 for (auto Device : Devices) if (IsValid(Device)) Device->ResetSession();
 Phase = EPhase::Idle;
 RoundTime = ViewTime = SprayTime = SprayDebt = SlowPhaseScheduledIssues = FirstBurstPeakFrame = 0;
 LastTickTime = 0;
 bPanic = bForcedExit = false;
 bAccountingValid = bWarningsValid = bIndependentTimersValid = true;
}
void APopupCloseStage::EndPlay(const EEndPlayReason::Type Reason)
{
 if(ActiveComputer)ActiveComputer->SetScreenInteractive(false);
 if(auto PC=LockedController.Get())
 {
  PC->SetShowMouseCursor(false);PC->SetInputMode(FInputModeGameOnly());
  if(auto Pawn=LockedPawn.Get())PC->SetViewTarget(Pawn);
 }
 RestorePlayerControl();
 if (Prompt) { Prompt->RemoveFromParent(); Prompt = nullptr; }
 for (auto Device : Devices) if (IsValid(Device) && Device->GetOwner() == this) Device->Destroy();
 Super::EndPlay(Reason);
}
FTransform APopupCloseStage::GetPlayerSpawnTransform() const { return SpawnPoint->GetComponentTransform(); }
void APopupCloseStage::GatherDevices()
{
 Devices.SetNum(4);
 for(TActorIterator<APopupOfficeDevice> It(GetWorld());It;++It)
  if(It->DeviceIndex>=0 && It->DeviceIndex<4)Devices[It->DeviceIndex]=*It;
 for(int32 I=0;I<4;++I)if(!Devices[I])
 {
  const FTransform Transform(GetActorRotation()+FRotator(0,DeviceYaws[I],0),GetActorTransform().TransformPosition(DeviceLocations[I]));
  auto Device=GetWorld()->SpawnActorDeferred<APopupOfficeDevice>(APopupOfficeDevice::StaticClass(),Transform,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if(Device){Device->DeviceIndex=I;Device->FinishSpawning(Transform);Devices[I]=Device;}
 }
}
APopupOfficeDevice* APopupCloseStage::FocusedDevice(APlayerController* PC) const
{
 if(Phase==EPhase::Spraying || Phase==EPhase::Complete || View!=EView::Free)return nullptr;
 for(auto Device:Devices)if(Device && Device->CanInteract(PC))return Device;
 return nullptr;
}
void APopupCloseStage::ShowPrompt(APlayerController* PC,const FString& Message)
{
 if(Message.IsEmpty()){if(Prompt)Prompt->SetVisibility(ESlateVisibility::Collapsed);return;}
 if(!PC)return;
 if(!Prompt){Prompt=CreateWidget<UPopupClosePromptWidget>(PC);Prompt->AddToViewport(8);}
 Prompt->SetMessage(Message);Prompt->SetVisibility(ESlateVisibility::HitTestInvisible);
}
void APopupCloseStage::StartOfficeRound()
{
 if(Phase!=EPhase::Idle)return;
 Phase=EPhase::Running;RoundTime=0;SlowPhaseScheduledIssues=0;
 // Randomize the initial order, with separated time slots instead of four immediate problems.
 int32 Order[]={0,1,2,3};
 for(int32 I=3;I>0;--I)Swap(Order[I],Order[FMath::RandRange(0,I)]);
 for(int32 Slot=0;Slot<4;++Slot)
 {
  const int32 I=Order[Slot];
  IssueCountdown[I]=(2.f+Slot*1.75f+FMath::FRandRange(0.f,1.5f))/IssueInterval(0.f,I);
 }
 UE_LOG(LogPopupClose,Display,TEXT("Office round started: independent randomized devices, 45s manageable + 20s acceleration + 8s eruption"));
}
void APopupCloseStage::HandleInteraction(APlayerController* PC,APopupOfficeDevice* Device)
{
 if(!Device || View!=EView::Free || Phase==EPhase::Spraying || Phase==EPhase::Complete || !Device->CanInteract(PC))return;
 StartOfficeRound();
 if(Device->IsPrinter())Device->ProcessPaper();else BeginScreenSession(PC,Device);
}
void APopupCloseStage::BeginScreenSession(APlayerController* PC,APopupOfficeDevice* Device)
{
 if(!PC || !PC->GetPawn() || !Device || Device->IsPrinter() || Device->IsLocked() || View!=EView::Free)return;
 ActiveComputer=Device;LockedController=PC;LockedPawn=PC->GetPawn();View=EView::Entering;ViewTime=0;
 if(auto Character=Cast<ACharacter>(PC->GetPawn()))
 {
  SavedMovementMode=Character->GetCharacterMovement()->MovementMode;
  Character->GetCharacterMovement()->StopMovementImmediately();Character->GetCharacterMovement()->DisableMovement();
 }
 PC->SetIgnoreMoveInput(true);bMoveSuppressed=true;PC->SetIgnoreLookInput(true);bLookSuppressed=true;
 Device->FitCamera();PC->SetViewTargetWithBlend(Device,ScreenEnterSeconds,EViewTargetBlendFunction::VTBlend_EaseInOut,2.f,true);
 UE_LOG(LogPopupClose,Display,TEXT("Enter computer %d; pending=%d"),Device->DeviceIndex,Device->Pending());
}
void APopupCloseStage::ExitScreen()
{
 if(View==EView::Free || View==EView::Exiting)return;
 if(ActiveComputer)ActiveComputer->SetScreenInteractive(false);
 View=EView::Exiting;ViewTime=0;
 if(auto PC=LockedController.Get())
 {
  PC->SetShowMouseCursor(false);PC->SetInputMode(FInputModeGameOnly());
  if(auto Pawn=LockedPawn.Get())PC->SetViewTargetWithBlend(Pawn,ScreenExitSeconds,EViewTargetBlendFunction::VTBlend_EaseInOut,2.f,true);
 }
 UE_LOG(LogPopupClose,Display,TEXT("Exit computer; pending messages preserved"));
}
void APopupCloseStage::RestorePlayerControl()
{
 if(bMoveSuppressed)
 {
  if(auto Character=Cast<ACharacter>(LockedPawn.Get()))Character->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(SavedMovementMode));
  if(auto PC=LockedController.Get())PC->SetIgnoreMoveInput(false);
  bMoveSuppressed=false;
 }
 if(bLookSuppressed){if(auto PC=LockedController.Get())PC->SetIgnoreLookInput(false);bLookSuppressed=false;}
}
void APopupCloseStage::TickView(float Dt)
{
 if(View==EView::Free)return;
 ViewTime+=Dt;auto PC=LockedController.Get();
 if(View==EView::Entering && ViewTime>=ScreenEnterSeconds)
 {
  UE_LOG(LogPopupClose,Display,TEXT("Computer view ready after %.3f s"),ViewTime);
  View=EView::InScreen;ViewTime=0;
  ActiveComputer->SetScreenInteractive(true);
  if(PC){FInputModeGameOnly Mode;Mode.SetConsumeCaptureMouseDown(false);PC->SetInputMode(Mode);PC->SetShowMouseCursor(true);}
 }
 else if(View==EView::InScreen)
 {
  if(PC && PC->WasInputKeyJustPressed(EKeys::E))ExitScreen();
  else if(ActiveComputer)ActiveComputer->TickScreen(Dt,PC);
 }
 else if(View==EView::Exiting && ViewTime>=ScreenExitSeconds)
 {
  RestorePlayerControl();View=EView::Free;ActiveComputer=nullptr;
 }
}
void APopupCloseStage::Tick(float Dt)
{
 const double Now=FPlatformTime::Seconds();const float Wall=LastTickTime>0?float(Now-LastTickTime):Dt;LastTickTime=Now;
 Super::Tick(Dt);
 if (bAreaManaged && !bAreaActive) return;
 if(Phase==EPhase::Idle || Phase==EPhase::Running)for(auto Device:Devices)if(Device)Device->WarmPool(Dt);
 if(Phase==EPhase::Running)TickIssues(Dt);
 if(Phase==EPhase::Spraying)
 {
  if(SprayTime<1.f)FirstBurstPeakFrame=FMath::Max(FirstBurstPeakFrame,Wall);
  TickSpray(Dt);
 }
 TickView(Dt);
 auto PC=GetWorld()->GetFirstPlayerController();
 if(auto Device=FocusedDevice(PC))
 {
  ShowPrompt(PC,Phase==EPhase::Idle?TEXT("按 E 开始办公"):
   Device->IsPrinter()?FString::Printf(TEXT("按 E 处理文件  ·  %d 张"),Device->Pending()):TEXT("按 E 使用电脑"));
  if(PC && PC->WasInputKeyJustPressed(EKeys::E))HandleInteraction(PC,Device);
 }
 else ShowPrompt(PC,TEXT(""));
}
void APopupCloseStage::TickIssues(float Dt)
{
 RoundTime+=Dt;
 if(RoundTime>=PanicSeconds && !bPanic)
 {
  bPanic=true;
  UE_LOG(LogPopupClose,Display,TEXT("Rapid acceleration begins at %.2f s; independent schedules preserved, slow-phase issues=%.0f"),RoundTime,SlowPhaseScheduledIssues);
 }
 for(int32 I=0;I<4;++I)
 {
  // Advancing normalized countdowns makes the tempo change smoothly, including at 45 seconds.
  // Each device samples its own next gap; there is no common timer or synchronized phase reset.
  IssueCountdown[I]-=Dt/IssueInterval(RoundTime,I);
  int32 Added=0;
  while(IssueCountdown[I]<=0.f && Added++<3)
  {
   Devices[I]->AddProblem();
   if(RoundTime<PanicSeconds)++SlowPhaseScheduledIssues;
   const float NextGap=FMath::FRandRange(.75f,1.25f);
   IssueCountdown[I]+=NextGap;
   UE_LOG(LogPopupClose,Log,TEXT("Office issue: device=%d t=%.3f next_gap=%.3f"),I,RoundTime,NextGap);
  }
  // Avoid catch-up floods after a long frame or debugger pause.
  if(IssueCountdown[I]<=0.f)IssueCountdown[I]=FMath::FRandRange(.75f,1.25f);
  bIndependentTimersValid &= FMath::IsFinite(IssueCountdown[I]) && IssueCountdown[I]>0.f && IssueCountdown[I]<=1.25f;
 }
 for(auto Device:Devices)
 {
  bAccountingValid &= Device->AccountingValid();
  bWarningsValid &= Device->WarningVisible()==(Device->Pending()>0);
 }
 if(RoundTime>=WorkSeconds)BeginBurst();
}
void APopupCloseStage::BeginBurst()
{
 if(Phase!=EPhase::Running)return;
 Phase=EPhase::Spraying;SprayTime=0;SprayDebt=0;
 bForcedExit=View==EView::Entering || View==EView::InScreen;
 // Disable every reply before releasing any shared Slate pointer capture.
 for(auto Device:Devices)if(auto UI=Device->GetDesktop())UI->SetRepliesEnabled(false);
 for(auto Device:Devices)Device->Lock();
 ExitScreen();
 for(auto Device:Devices)Device->Emit();
 UE_LOG(LogPopupClose,Display,TEXT("Four-device eruption started immediately at %.2f s; force_exit=%d"),RoundTime,bForcedExit);
}
void APopupCloseStage::TickSpray(float Dt)
{
 SprayTime+=Dt;SprayDebt=FMath::Min(SprayDebt+Dt,SprayInterval*3);
 int32 Count=0;
 while(SprayDebt>=SprayInterval && SprayTime<=SpraySeconds && Count++<3)
 {
  SprayDebt-=SprayInterval;for(auto Device:Devices)Device->Emit();
 }
 if(SprayTime>.5f)for(auto Device:Devices)Device->CleanupOnePopup();
 if(SprayTime>=SpraySeconds)
 {
  Phase=EPhase::Complete;for(auto Device:Devices)Device->HideWarning();
  WriteSessionReport();
 }
}
FString APopupCloseStage::GetSessionStatus() const
{
 FString Status=FString::Printf(TEXT("phase=%d view=%d elapsed=%.2f spray=%.2f"),int32(Phase),int32(View),RoundTime,SprayTime);
 for(auto Device:Devices)if(Device)Status+=FString::Printf(TEXT("\nDevice%d %s pending=%d created=%d handled=%d warning=%d locked=%d emitted=%d"),
 Device->DeviceIndex,Device->IsPrinter()?TEXT("printer"):TEXT("computer"),Device->Pending(),Device->Created(),Device->Handled(),Device->WarningVisible(),Device->IsLocked(),Device->Emitted());
 const auto Paper=PaperLandingStatus(GetWorld(),Floor->Bounds.GetBox().Max.Z);
 Status+=FString::Printf(TEXT("\nEmitted papers=%d below_floor=%d CCD=%d lowest_visual_clearance=%.3f cm"),Paper.Count,Paper.BelowFloor,Paper.bCCD,Paper.Count?Paper.LowestClearance:0.f);
 UE_LOG(LogPopupClose,Display,TEXT("%s"),*Status);return Status;
}
void APopupCloseStage::WriteSessionReport()
{
 auto PC=GetWorld()->GetFirstPlayerController();
 const bool bInput=PC && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && !PC->bShowMouseCursor && PC->GetViewTarget()==PC->GetPawn() && View==EView::Free;
 bool bLocked=true,bEmitted=true;for(auto Device:Devices){bLocked &= Device->IsLocked() && !Device->CanInteract(PC);bEmitted &= Device->Emitted()>=95;}
 const auto Paper=PaperLandingStatus(GetWorld(),Floor->Bounds.GetBox().Max.Z);
 const bool bPaperSafe=Paper.Count==Devices[2]->Emitted()+Devices[3]->Emitted() && Paper.Count>=190 && Paper.BelowFloor==0 && Paper.bCCD;
 const bool bPass=bInput && bLocked && bEmitted && bPaperSafe && bAccountingValid && bWarningsValid && bIndependentTimersValid && SlowPhaseScheduledIssues<=24.f && RoundTime+SprayTime<WorkSeconds+SpraySeconds+1.f;
 FString Report=FString::Printf(TEXT("%s\nTotal time: %.2f s\nManageable phase: %.0f s, %.0f automatically scheduled issues\nRapid acceleration: %.0f s\nAll unresolved problems accumulated: %d\nWarning icons track pending counts: %d\nIndependent randomized timers remain valid: %d\nEruption forced camera exit: %d\nCamera and movement restored: %d\nAll four devices permanently locked: %d\nLargest frame in first eruption second: %.2f ms\n"),
 bPass?TEXT("PASS"):TEXT("FAIL"),RoundTime+SprayTime,PanicSeconds,SlowPhaseScheduledIssues,WorkSeconds-PanicSeconds,bAccountingValid,bWarningsValid,bIndependentTimersValid,bForcedExit,bInput,bLocked,FirstBurstPeakFrame*1000);
 Report+=GetSessionStatus()+TEXT("\n");
 IFileManager::Get().MakeDirectory(*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Validation")),true);
 FFileHelper::SaveStringToFile(Report,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Validation/popup_four_device_session.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
 UE_LOG(LogPopupClose,Display,TEXT("%s"),*Report);
}
FString APopupCloseStage::ValidateOffice()
{
 if(!GetWorld()->IsGameWorld() || Phase!=EPhase::Running || View!=EView::InScreen)return TEXT("FAIL: enter a computer first");
 FString Report;int32 Checks=0,Failures=0;
 auto Check=[&](bool Pass,const TCHAR* Label){++Checks;if(!Pass)++Failures;Report+=FString::Printf(TEXT("%s: %s\n"),Pass?TEXT("PASS"):TEXT("FAIL"),Label);};
 auto PC=LockedController.Get();auto Pawn=LockedPawn.Get();
 Check(Devices.Num()==4 && !Devices[0]->IsPrinter() && !Devices[1]->IsPrinter() && Devices[2]->IsPrinter() && Devices[3]->IsPrinter(),TEXT("two computers and two printers"));
 float MinDistance=FLT_MAX;for(int32 I=0;I<4;++I)for(int32 J=I+1;J<4;++J)MinDistance=FMath::Min(MinDistance,float(FVector::Dist(Devices[I]->GetActorLocation(),Devices[J]->GetActorLocation())));
 Check(MinDistance>=600,TEXT("devices separated by at least six meters"));
 Check(Devices[0]->GetActorLocation().Equals(GetActorLocation()),TEXT("original computer remains at room center"));
 Check(ActiveComputer->GetSurface()->GetWidgetSpace()==EWidgetSpace::World && PC->GetViewTarget()==ActiveComputer,TEXT("physical monitor and bezel used for computer view"));
 if(ActiveComputer->Pending()==0)ActiveComputer->AddProblem();
 Check(ActiveComputer->ClickReplyForValidation(),TEXT("actual reply button removes exactly one message"));
 ActiveComputer->AddProblem();ActiveComputer->AddProblem();const int32 PendingBefore=ActiveComputer->Pending();
 auto First=ActiveComputer.Get();ExitScreen();
 // Finish the same exit path immediately for deterministic checks in this one-frame validation.
 if(PC->PlayerCameraManager)PC->PlayerCameraManager->UpdateCamera(ScreenExitSeconds+.01f);
 RestorePlayerControl();PC->SetViewTarget(Pawn);View=EView::Free;ActiveComputer=nullptr;
 Check(First->Pending()==PendingBefore && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && !PC->bShowMouseCursor,TEXT("exiting preserves unresolved messages and restores controls"));
 auto AimAt=[&](APopupOfficeDevice* Device)
 {
  const FVector P=Device->GetActorLocation()-Device->GetActorForwardVector()*215+FVector(0,0,98);
  Pawn->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);
  PC->SetControlRotation((Device->InteractionPoint()-(P+FVector(0,0,60))).Rotation());
  if(PC->PlayerCameraManager)PC->PlayerCameraManager->UpdateCamera(0.f);
 };
 for(int32 I=2;I<4;++I)
 {
  auto Printer=Devices[I];while(Printer->Pending()<2)Printer->AddProblem();
  AimAt(Printer);Check(Printer->CanInteract(PC),TEXT("printer requires approaching and aiming"));
  const int32 Before=Printer->Pending();HandleInteraction(PC,Printer);
  Check(Printer->Pending()==Before-1 && Printer->WarningVisible(),TEXT("one interaction processes one sheet; warning remains for backlog"));
  for(int32 Remaining=Printer->Pending();Remaining>0 && Printer->CanInteract(PC);--Remaining)HandleInteraction(PC,Printer);
  Check(Printer->Pending()==0 && !Printer->WarningVisible() && Printer->AccountingValid(),TEXT("paper stack and warning clear only after every sheet is processed"));
 }
 Pawn->SetActorLocation(Devices[1]->GetActorLocation()+FVector(1000,0,98),false,nullptr,ETeleportType::TeleportPhysics);
 Check(!Devices[1]->CanInteract(PC),TEXT("remote interaction rejected"));
 AimAt(Devices[1]);Check(Devices[1]->CanInteract(PC),TEXT("second computer can be approached and aimed at"));
 HandleInteraction(PC,Devices[1]);
 Check(ActiveComputer==Devices[1] && View==EView::Entering && First->Pending()==PendingBefore,TEXT("switching computer preserves the first computer's backlog"));
 // The second computer remains active so the end-of-round test exercises forced ejection.
 bool Faster=true;for(int32 I=1;I<=100;++I)Faster &= IssueInterval(WorkSeconds*I/100.f,0)<IssueInterval(WorkSeconds*(I-1)/100.f,0);
 Check(Faster,TEXT("problem frequency continuously accelerates with no peak hold"));
 Check(PanicSeconds==45.f && IssueInterval(0.f,0)==14.f && IssueInterval(PanicSeconds,0)==10.f && IssueInterval(PanicSeconds,2)==12.f,TEXT("first 45 seconds only gently accelerate with manageable gaps"));
 Check(IssueInterval(55.f,0)<IssueInterval(45.f,0)*.2f && IssueInterval(WorkSeconds,0)<=.121f,TEXT("acceleration becomes much faster after 45 seconds"));
 Check(bIndependentTimersValid,TEXT("every device maintains its own randomized countdown"));
 Check(bAccountingValid && bWarningsValid,TEXT("pending counts and warning icons stay consistent"));
 const FString Result=FString::Printf(TEXT("%s: %d checks (%d failures)"),Failures?TEXT("FAIL"):TEXT("PASS"),Checks,Failures);
 Report+=Result+TEXT("\n");IFileManager::Get().MakeDirectory(*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Validation")),true);
 FFileHelper::SaveStringToFile(Report,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Validation/popup_four_device_runtime.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
 UE_LOG(LogPopupClose,Display,TEXT("%s"),*Report);return Result;
}

#if WITH_EDITOR
void APopupCloseStage::UpgradeOffice()
{
 if(!GEditor || GEditor->PlayWorld)return;
 auto World=GEditor->GetEditorWorldContext().World();
 if(!World || World->GetOutermost()->GetName()!=TEXT("/Game/PopupClose/Maps/Lvl_PopupClose"))return;
 APopupCloseStage* Stage=nullptr;for(TActorIterator<APopupCloseStage> It(World);It;++It){Stage=*It;break;}if(!Stage)return;
 Stage->Modify();Stage->ConfigureOffice();Stage->ApplyColors();Stage->SetActorLabel(TEXT("办公室 · 玩法与中央桌"));Stage->SetFolderPath(TEXT("PopupOffice/Workstation"));
 APopupCloseOffice* Office=nullptr;for(TActorIterator<APopupCloseOffice> It(World);It;++It){Office=*It;break;}
 if(!Office){FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;Office=World->SpawnActor<APopupCloseOffice>(Stage->GetActorLocation(),Stage->GetActorRotation(),Params);}
 if(Office){Office->OnConstruction(Office->GetActorTransform());Office->SetActorLabel(TEXT("办公室 · 工位与陈设"));Office->SetFolderPath(TEXT("PopupOffice/Environment"));}
 Stage->GatherDevices();
 for(int32 I=0;I<4;++I)
 {
  auto Device=Stage->Devices[I];Device->Modify();Device->DeviceIndex=I;
  Device->SetActorLocationAndRotation(Stage->GetActorTransform().TransformPosition(DeviceLocations[I]),Stage->GetActorRotation()+FRotator(0,DeviceYaws[I],0));
  Device->OnConstruction(Device->GetActorTransform());
  Device->SetActorLabel(I==0?TEXT("电脑 · 中央工位"):I==1?TEXT("电脑 · 西北工位"):I==2?TEXT("打印机 · 东北"):TEXT("打印机 · 西南"));
  Device->SetFolderPath(TEXT("PopupOffice/InteractiveDevices"));
 }
 World->GetWorldSettings()->DefaultGameMode=APopupCloseGameMode::StaticClass();World->MarkPackageDirty();
 const bool Saved=UEditorLoadingAndSavingUtils::SaveMap(World,TEXT("/Game/PopupClose/Maps/Lvl_PopupClose"));
 UE_LOG(LogPopupClose,Display,TEXT("Four-device office upgrade saved=%d"),Saved);
}
void APopupCloseStage::CreateLevel()
{
 if(!GEditor || GEditor->PlayWorld)return;
 auto World=UEditorLoadingAndSavingUtils::NewBlankMap(false);if(!World)return;
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 World->SpawnActor<APopupCloseStage>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
 World->SpawnActor<APopupCloseOffice>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
 UEditorLoadingAndSavingUtils::SaveMap(World,TEXT("/Game/PopupClose/Maps/Lvl_PopupClose"));UpgradeOffice();
}
static FAutoConsoleCommand CreatePopupLevel(TEXT("PopupClose.CreateLevel"),TEXT("Create office level"),FConsoleCommandDelegate::CreateStatic(&APopupCloseStage::CreateLevel));
static FAutoConsoleCommand UpgradePopupOffice(TEXT("PopupClose.UpgradeOffice"),TEXT("Upgrade office with two computers and two printers"),FConsoleCommandDelegate::CreateStatic(&APopupCloseStage::UpgradeOffice));
#endif
