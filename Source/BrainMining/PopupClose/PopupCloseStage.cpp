#include "PopupClose/PopupCloseStage.h"
#include "MiningRoomAudioSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "PopupClose/PopupCloseContent.h"
#include "PopupClose/PopupOfficeDevice.h"
#include "PopupClose/PopupOfficeFlood.h"
#include "PopupClose/PopupCloseLayout.h"
#include "PopupClose/PopupCloseFlyingWindow.h"
#include "PopupClose/PopupCloseGameMode.h"
#include "PopupClose/PopupCloseOffice.h"
#include "PopupClose/PopupCloseWidgets.h"
#include "MiningWalletSubsystem.h"
#include "MiningGUIWidgets.h"
#include "BrainMiningPlayerController.h"
#include "Engine/GameInstance.h"
#include "Components/TextBlock.h"
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
#include "MiningInteractionHUDWidget.h"
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
 float IssueInterval(float Seconds,int32 Index)
 {
  const float InitialInterval=Index>=2?16.f:14.f;
  const float ManageableInterval=Index>=2?12.f:10.f;
  if(Seconds<PanicSeconds)return FMath::Lerp(InitialInterval,ManageableInterval,FMath::Clamp(Seconds/PanicSeconds,0.f,1.f));
  return ManageableInterval*FMath::Pow(.12f/ManageableInterval,FMath::Clamp((Seconds-PanicSeconds)/(WorkSeconds-PanicSeconds),0.f,1.f));
 }
 const FVector DeviceLocations[]={PopupCloseLayout::DevicePosition(0),PopupCloseLayout::DevicePosition(1),PopupCloseLayout::DevicePosition(2),PopupCloseLayout::DevicePosition(3)};
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
 ExitDoorModel=MakeBox(TEXT("OfficeBlastDoor"),FVector(1000,0,145),FVector(.22f,1.96f,2.9f));
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
 Floor->SetRelativeScale3D(FVector(20,15,.2f));
 Ceiling->SetRelativeLocation(FVector(0,0,370));Ceiling->SetRelativeScale3D(FVector(20,15,.2f));
 WallPositiveY->SetRelativeLocation(FVector(0,750,180));WallNegativeY->SetRelativeLocation(FVector(0,-750,180));
 WallPositiveY->SetRelativeScale3D(FVector(20,.2f,3.6f));WallNegativeY->SetRelativeScale3D(FVector(20,.2f,3.6f));
 ExitDoorModel->SetRelativeLocation(FVector(1000,0,145));ExitDoorModel->SetRelativeScale3D(FVector(.22f,1.96f,2.9f));
 SpawnPoint->SetRelativeLocation(FVector(-790,0,92));ScreenCamera->SetRelativeLocation(FVector(-75,0,124));
 RoomLight->SetRelativeLocation(FVector(0,0,285));RoomLight->SetIntensityUnits(bInteriorLighting?ELightUnits::Lumens:ELightUnits::Unitless);RoomLight->SetIntensity(bInteriorLighting?InteriorFillIntensity:15000);RoomLight->SetAttenuationRadius(1700);
 Sun->SetIntensity(3);Sky->bRealTimeCapture=false;Sky->SetIntensity(.5f);
 Sun->SetVisibility(!bAreaManaged && !bInteriorLighting);Sky->SetVisibility(!bAreaManaged && !bInteriorLighting);Atmosphere->SetVisibility(!bAreaManaged && !bInteriorLighting);
 ScreenCamera->bOverrideAspectRatioAxisConstraint=true;
 ScreenCamera->AspectRatioAxisConstraint=EAspectRatioAxisConstraint::AspectRatio_MaintainXFOV;
 ScreenCamera->PostProcessBlendWeight=0;
 TableTop->SetRelativeScale3D(PopupCloseLayout::Compress(FVector(1.9f,1.f,.06f)));
 TableTop->SetVisibility(!bReplaceCentralDesk);TableTop->SetCollisionEnabled(bReplaceCentralDesk?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryAndPhysics);
 Keyboard->SetVisibility(!bReplaceCentralDesk);Keyboard->SetCollisionEnabled(bReplaceCentralDesk?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryAndPhysics);
 Keyboard->SetRelativeLocation(PopupCloseLayout::Compress(FVector(-10,0,80)));
 Keyboard->SetRelativeScale3D(PopupCloseLayout::Compress(FVector(.16f,.36f,.03f)));
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
 Paint(ExitDoorModel,FLinearColor(.23f,.27f,.29f));
 Paint(Screen,FLinearColor(.035f,.08f,.16f));Paint(Stand,FLinearColor(.065f,.08f,.1f));Paint(Keyboard,FLinearColor(.1f,.12f,.15f));
 if(FloorArtMaterial)Floor->SetMaterial(0,FloorArtMaterial);
 if(CeilingArtMaterial)Ceiling->SetMaterial(0,CeilingArtMaterial);
 if(WallArtMaterial){WallPositiveY->SetMaterial(0,WallArtMaterial);WallNegativeY->SetMaterial(0,WallArtMaterial);}
 if(DesktopArtMaterial)TableTop->SetMaterial(0,DesktopArtMaterial);
}
void APopupCloseStage::Paint(UStaticMeshComponent* Mesh,const FLinearColor& Color) const
{
 if(!Mesh || !BaseMaterial)return;
 auto Material=UMaterialInstanceDynamic::Create(BaseMaterial,const_cast<APopupCloseStage*>(this));
 Material->SetVectorParameterValue(TEXT("Color"),Color);Mesh->SetMaterial(0,Material);
}


APopupCloseStage::~APopupCloseStage()=default;
void APopupCloseStage::BeginPlay()
{
 Super::BeginPlay();ConfigureOffice();ApplyColors();GatherDevices();
 Flood=MakeUnique<FPopupOfficeFlood>(*this);
 Exit=MakeUnique<FPopupOfficeExit>(*this);
 if (bAreaManaged) SetAreaActive(false);
 else ConfigureArtRendering(bAreaActive);
}
void APopupCloseStage::ConfigureArtRendering(bool bActive)
{
 // The mining project deliberately disables AA; textured office furniture benefits from temporal AA.
 // Restore both the previous value and its existing priority when leaving this art room.
 if(!GetWorld() || !GetWorld()->IsGameWorld())return;
 auto AA=IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"));
 if(!AA)return;
 if(bActive && bInteriorLighting)
 {
  if(!SavedOfficeAntiAliasing.IsSet()){SavedOfficeAntiAliasing=AA->GetInt();AA->SetWithCurrentPriority(2);}
 }
 else if(SavedOfficeAntiAliasing.IsSet())
 {
  if(AA->GetInt()==2)AA->SetWithCurrentPriority(SavedOfficeAntiAliasing.GetValue());
  SavedOfficeAntiAliasing.Reset();
 }
}
void APopupCloseStage::SetAreaActive(bool bActive)
{
 const bool bEntering=bActive && !bAreaActive;
 if (!bActive){CancelSession();if(Flood)Flood->Pause();UMiningRoomAudioSubsystem::StopSource(this);}
 bAreaActive = bActive;
 ConfigureArtRendering(bActive);
 if (bEntering && bStartOnAreaEntry) StartOfficeRound();
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
 for (auto Device : Devices) if (IsValid(Device)){Device->ResetSession();Device->SetScreenInteractive(false);}
 if(Flood)Flood->Reset();
 if(Exit)Exit->Reset();
 Phase = EPhase::Idle;
 UMiningRoomAudioSubsystem::StopSource(this);
 RoundTime = ViewTime = SprayTime = SprayDebt = SlowPhaseScheduledIssues = FirstBurstPeakFrame = 0;
 LastTickTime = 0;
 bPanic = bForcedExit = false;
 bAccountingValid = bWarningsValid = bIndependentTimersValid = true;
}
void APopupCloseStage::EndPlay(const EEndPlayReason::Type Reason)
{
 ConfigureArtRendering(false);
 Exit.Reset();
 Flood.Reset();
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
 // Also update copies of the office used in the integrated showcase levels.
 // Authored device transforms belong to the level. Only missing devices use the
 // fallback spawn positions above; moving existing devices here creates overlaps.
}
APopupOfficeDevice* APopupCloseStage::FocusedDevice(APlayerController* PC) const
{
 if(Phase==EPhase::Spraying || Phase==EPhase::Complete || View!=EView::Free)return nullptr;
 for(auto Device:Devices)if(Device && Device->CanInteract(PC))return Device;
 return nullptr;
}
FString APopupCloseStage::GetInteractionAction(APlayerController* PC) const
{
 if (!bAreaActive) return FString();
 auto* Device = FocusedDevice(PC);
 if (!Device) return FString();
 if (Phase == EPhase::Idle) return TEXT("开始办公");
 return Device->IsPrinter() ? FString::Printf(TEXT("处理文件 · %d 张"), Device->Pending()) : TEXT("使用电脑");
}

void APopupCloseStage::ShowPrompt(APlayerController* PC,const FString& Message)
{
 if (UMiningInteractionHUDWidget::IsActiveFor(PC) && !GetInteractionAction(PC).IsEmpty())
 {
  if (Prompt) Prompt->SetVisibility(ESlateVisibility::Collapsed);
  return;
 }
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
 UE_LOG(LogPopupClose,Display,TEXT("Office round started: independent randomized devices, 45s manageable + 20s acceleration, then continuous office-wide eruption"));
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
 UMiningRoomAudioSubsystem::Emit(this,TEXT("S_Open"),.25f);
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
 UMiningRoomAudioSubsystem::Emit(this,TEXT("S_Close"),.25f);
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
 if(!IsOfficeEscaped()) UMiningRoomAudioSubsystem::Loop(this,TEXT("A_Office_Loop"),.18f);
 if((Phase==EPhase::Spraying || Phase==EPhase::Complete) && Exit && !Exit->HasOfferedBomb())
  UMiningRoomAudioSubsystem::Loop(this,TEXT("S_OfficeBurst_Loop"),.32f);
 if(auto GameInstance=GetGameInstance())if(auto Wallet=GameInstance->GetSubsystem<UMiningWalletSubsystem>())
  if(auto Controller=Cast<ABrainMiningPlayerController>(GetWorld()->GetFirstPlayerController());Controller && Controller->MiningGUI && Controller->MiningGUI->CoinText)
  {
   const FText Coins=FText::AsNumber(Wallet->Balance);
   if(!Controller->MiningGUI->CoinText->GetText().EqualTo(Coins))Controller->MiningGUI->SetCoinDisplay(Coins);
  }
 if(Flood && (Phase==EPhase::Idle || Phase==EPhase::Running))Flood->Warm(Dt);
 if(Phase==EPhase::Running)TickIssues(Dt);
 if(Phase==EPhase::Spraying || Phase==EPhase::Complete)
 {
  if(SprayTime<1.f)FirstBurstPeakFrame=FMath::Max(FirstBurstPeakFrame,Wall);
  TickSpray(Dt);
 }
 TickView(Dt);
 auto PC=GetWorld()->GetFirstPlayerController();
 if(Exit && Exit->IsWaitingForBomb())
 {
  ShowPrompt(PC,Exit->CanCollect(PC)?TEXT("E 获取炸弹技能"):Exit->NeedsDoorBreach()?TEXT("G 装备炸弹 · 左键投掷"):TEXT(""));
  return;
 }
 if(auto Device=FocusedDevice(PC))
 {
  ShowPrompt(PC,Phase==EPhase::Idle?TEXT("按 E 开始办公"):
   Device->IsPrinter()?FString::Printf(TEXT("按 E 处理文件  ·  %d 张"),Device->Pending()):TEXT("按 E 使用电脑"));
  if(PC && PC->WasInputKeyJustPressed(EKeys::E))HandleInteraction(PC,Device);
 }
 else ShowPrompt(PC,bStartOnAreaEntry && Phase==EPhase::Running && View==EView::Free && RoundTime<10.f
  ?TEXT("办公已开始 · 靠近电脑或打印机，瞄准后按 E 处理") :TEXT(""));
}
void APopupCloseStage::TickIssues(float Dt)
{
 RoundTime+=Dt;
 if(RoundTime>=PanicSeconds && !bPanic)
 {
  bPanic=true;
  UMiningRoomAudioSubsystem::Emit(this,TEXT("S_Warning"),.35f);
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
 UMiningRoomAudioSubsystem::Emit(this,TEXT("S_OfficePanic"),.65f);
 bForcedExit=View==EView::Entering || View==EView::InScreen;
 // Disable every reply before releasing any shared Slate pointer capture.
 for(auto Device:Devices)if(auto UI=Device->GetDesktop())UI->SetRepliesEnabled(false);
 for(auto Device:Devices)Device->Lock();
 ExitScreen();
 if(Flood)Flood->Start();
 UE_LOG(LogPopupClose,Display,TEXT("Four-device eruption started immediately at %.2f s; force_exit=%d"),RoundTime,bForcedExit);
}
void APopupCloseStage::TickSpray(float Dt)
{
 SprayTime+=Dt;if(Flood)Flood->Tick(Dt);
 if(Exit && !Exit->HasOfferedBomb() && SprayTime>=15.f)
 {
  if(Flood)Flood->Stop();Exit->OfferBomb();
 }
 if(SprayTime>.5f)for(auto Device:Devices)Device->CleanupOnePopup();
 if(Phase==EPhase::Spraying && SprayTime>=15.f && Flood)
 {
  Phase=EPhase::Complete;for(auto Device:Devices)Device->HideWarning();
  WriteSessionReport();
 }
}
FString APopupCloseStage::GetSessionStatus() const
{
 FString Status=FString::Printf(TEXT("phase=%d view=%d elapsed=%.2f spray=%.2f"),int32(Phase),int32(View),RoundTime,SprayTime);
 for(auto Device:Devices)if(Device)Status+=FString::Printf(TEXT("\nDevice%d %s pending=%d created=%d handled=%d warning=%d locked=%d emitted=%d"),
 Device->DeviceIndex,Device->IsPrinter()?TEXT("printer"):TEXT("computer"),Device->Pending(),Device->Created(),Device->Handled(),Device->WarningVisible(),Device->IsLocked(),Flood?Flood->Emitted(Device->DeviceIndex):0);
 const auto Paper=PaperLandingStatus(GetWorld(),Floor->Bounds.GetBox().Max.Z);
 Status+=FString::Printf(TEXT("\nEmitted papers=%d below_floor=%d CCD=%d lowest_visual_clearance=%.3f cm"),Paper.Count,Paper.BelowFloor,Paper.bCCD,Paper.Count?Paper.LowestClearance:0.f);
 if(Flood)Status+=TEXT("\n")+Flood->Describe();
 if(Exit)Status+=TEXT("\n")+Exit->Describe();
 if(auto GameInstance=GetGameInstance())if(auto Wallet=GameInstance->GetSubsystem<UMiningWalletSubsystem>())Status+=FString::Printf(TEXT("\nShared wallet balance=%lld"),Wallet->Balance);
 UE_LOG(LogPopupClose,Display,TEXT("%s"),*Status);return Status;
}
void APopupCloseStage::OnPlayerBombExploded(FVector Location){if(Exit)Exit->HandleExplosion(Location);}
FString APopupCloseStage::GetExitStatus() const
{
 const FString Status=Exit?Exit->Describe():TEXT("Exit not initialized");UE_LOG(LogPopupClose,Display,TEXT("%s"),*Status);return Status;
}
void APopupCloseStage::WriteSessionReport()
{
 auto PC=GetWorld()->GetFirstPlayerController();
 const bool bInput=PC && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && !PC->bShowMouseCursor && PC->GetViewTarget()==PC->GetPawn() && View==EView::Free;
 bool bLocked=true,bEmitted=true;for(auto Device:Devices){bLocked &= Device->IsLocked() && !Device->CanInteract(PC);bEmitted &= Flood && Flood->Emitted(Device->DeviceIndex)>=95;}
 const auto Paper=PaperLandingStatus(GetWorld(),Floor->Bounds.GetBox().Max.Z);
 const bool bPaperSafe=Paper.BelowFloor==0 && Paper.bCCD && Flood && Flood->IsHealthy();
 const bool bPass=bInput && bLocked && bEmitted && bPaperSafe && bAccountingValid && bWarningsValid && bIndependentTimersValid && SlowPhaseScheduledIssues<=24.f && Flood->IsFloorCovered();
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
 auto Wallet=GetGameInstance()?GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>():nullptr;
 auto Balance=[Wallet]()->int64{return Wallet?Wallet->Balance:0;};
 const int64 ReplyReward=Wallet && Wallet->UsesMineralInventory()?0:20;
 const int64 PaperReward=Wallet && Wallet->UsesMineralInventory()?0:10;
 Check(Wallet!=nullptr,TEXT("office uses the shared game wallet"));
 Check(Devices.Num()==4 && !Devices[0]->IsPrinter() && !Devices[1]->IsPrinter() && Devices[2]->IsPrinter() && Devices[3]->IsPrinter(),TEXT("two computers and two printers"));
 float MinDistance=FLT_MAX;for(int32 I=0;I<4;++I)for(int32 J=I+1;J<4;++J)MinDistance=FMath::Min(MinDistance,float(FVector::Dist(Devices[I]->GetActorLocation(),Devices[J]->GetActorLocation())));
 Check(MinDistance>=500,TEXT("devices remain separated by at least five meters in the smaller office"));
 Check(Devices[0]->GetActorLocation().Equals(GetActorLocation()),TEXT("original computer remains at room center"));
 Check(ActiveComputer->GetSurface()->GetWidgetSpace()==EWidgetSpace::World && PC->GetViewTarget()==ActiveComputer,TEXT("physical monitor and bezel used for computer view"));
 if(ActiveComputer->Pending()==0)ActiveComputer->AddProblem();
 const int64 BeforeReply=Balance();
 Check(ActiveComputer->ClickReplyForValidation(),TEXT("actual reply button removes exactly one message"));
 Check(Wallet && Balance()-BeforeReply==ReplyReward,TEXT("popup reply follows the active economy; MainGame awards no coins"));
 ActiveComputer->AddProblem();ActiveComputer->AddProblem();const int32 PendingBefore=ActiveComputer->Pending();
 Check(Balance()==BeforeReply+ReplyReward,TEXT("new pending messages award no coins"));
 auto First=ActiveComputer.Get();ExitScreen();
 // Finish the same exit path immediately for deterministic checks in this one-frame validation.
 if(PC->PlayerCameraManager)PC->PlayerCameraManager->UpdateCamera(ScreenExitSeconds+.01f);
 RestorePlayerControl();PC->SetViewTarget(Pawn);View=EView::Free;ActiveComputer=nullptr;
 Check(First->Pending()==PendingBefore && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && !PC->bShowMouseCursor,TEXT("exiting preserves unresolved messages and restores controls"));
 Check(Balance()==BeforeReply+ReplyReward,TEXT("exiting a computer awards no coins"));
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
  const int32 Before=Printer->Pending();const int64 BeforePaper=Balance();HandleInteraction(PC,Printer);
  Check(Printer->Pending()==Before-1 && Printer->WarningVisible(),TEXT("one interaction processes one sheet; warning remains for backlog"));
  Check(Wallet && Balance()-BeforePaper==PaperReward,TEXT("printer follows the active economy; MainGame awards no coins"));
  for(int32 Remaining=Printer->Pending();Remaining>0 && Printer->CanInteract(PC);--Remaining)HandleInteraction(PC,Printer);
  Check(Printer->Pending()==0 && !Printer->WarningVisible() && Printer->AccountingValid(),TEXT("paper stack and warning clear only after every sheet is processed"));
  Check(Balance()-BeforePaper==Before*PaperReward,TEXT("processing all sheets follows the active economy"));
  const int64 BeforeEmpty=Balance();
  Check(!Printer->ProcessPaper() && Balance()==BeforeEmpty,TEXT("empty printer interaction awards no coins"));
 }
 Pawn->SetActorLocation(Devices[1]->GetActorLocation()+FVector(1000,0,98),false,nullptr,ETeleportType::TeleportPhysics);
 Check(!Devices[1]->CanInteract(PC),TEXT("remote interaction rejected"));
 AimAt(Devices[1]);Check(Devices[1]->CanInteract(PC),TEXT("second computer can be approached and aimed at"));
 const int64 BeforeEnter=Balance();HandleInteraction(PC,Devices[1]);
 Check(ActiveComputer==Devices[1] && View==EView::Entering && First->Pending()==PendingBefore,TEXT("switching computer preserves the first computer's backlog"));
 Check(Balance()==BeforeEnter,TEXT("entering another computer awards no coins"));
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
static FAutoConsoleCommand ResetPopupOffice(TEXT("PopupClose.Reset"),TEXT("Reset the active office, including its persistent flood layer"),FConsoleCommandDelegate::CreateLambda([]()
{
 if(!GEditor || !GEditor->PlayWorld)return;
 for(TActorIterator<APopupCloseStage> It(GEditor->PlayWorld);It;++It){It->ResetCompletedSessions();It->GetSessionStatus();}
}));
#endif
