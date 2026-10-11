#include "PopupClose/PopupCloseStage.h"
#include "PopupClose/PopupCloseContent.h"
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
 constexpr float ScreenEnterSeconds=1.2f;
 constexpr float ScreenRampSeconds=31.f;
 constexpr float SlowInterval=1.8f;
 constexpr float FastInterval=.035f;
 constexpr float ScreenExitSeconds=.8f;
 constexpr float SpraySeconds=8.f;
 constexpr float SprayInterval=.06f;
 constexpr int32 PoolCapacity=168;

 float SpawnIntervalAt(float Seconds)
 {
  const float Alpha=FMath::Clamp(Seconds/ScreenRampSeconds,0.f,1.f);
  return SlowInterval*FMath::Pow(FastInterval/SlowInterval,FMath::Pow(Alpha,1.6f));
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
 ScreenCamera->bOverrideAspectRatioAxisConstraint=true;
 ScreenCamera->AspectRatioAxisConstraint=EAspectRatioAxisConstraint::AspectRatio_MaintainXFOV;
 ScreenCamera->PostProcessBlendWeight=0;
 TableTop->SetRelativeScale3D(FVector(1.9f,1.f,.06f));
 Keyboard->SetRelativeScale3D(FVector(.16f,.36f,.03f));
 // Replacement metal frames are part of the office's instanced geometry.
 TArray<UStaticMeshComponent*> Meshes;GetComponents(Meshes);
 for(auto Mesh:Meshes)if(Mesh->GetName().StartsWith(TEXT("Leg")))
 {
  Mesh->SetVisibility(false);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 }
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
 Super::BeginPlay();ConfigureOffice();ApplyColors();WindowPool.Reserve(PoolCapacity);
 if(auto PC=GetWorld()->GetFirstPlayerController())
 {
  Desktop=CreateWidget<UPopupCloseDesktopWidget>(PC);DesktopSurface->SetWidget(Desktop);
  // Allocate the screen render target before interaction or the eruption.
  DesktopSurface->RequestRedraw();
 }
}
void APopupCloseStage::EndPlay(const EEndPlayReason::Type Reason)
{
 RestorePlayerControl(true);Super::EndPlay(Reason);
}
FTransform APopupCloseStage::GetPlayerSpawnTransform() const
{
 return SpawnPoint?SpawnPoint->GetComponentTransform():GetActorTransform();
}
bool APopupCloseStage::IsComputerComponent(const UPrimitiveComponent* Component) const
{
 return Component==Screen || Component==Monitor || Component==Stand || Component==Keyboard || Component==AimVolume || Component==DesktopSurface;
}
bool APopupCloseStage::CanInteract(APlayerController* PC) const
{
 if(Phase!=EPhase::Idle || !PC || !PC->GetPawn())return false;
 if(FVector::Dist(PC->GetPawn()->GetActorLocation(),Screen->GetComponentLocation())>320)return false;
 FVector P;FRotator R;PC->GetPlayerViewPoint(P,R);
 FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(PopupCloseAim),false,PC->GetPawn());
 return GetWorld()->LineTraceSingleByChannel(Hit,P,P+R.Vector()*450,ECC_Visibility,Query)
  && Hit.GetActor()==this && IsComputerComponent(Hit.GetComponent());
}
void APopupCloseStage::SetPromptVisible(APlayerController* PC,bool bVisible)
{
 if(!bVisible){if(Prompt)Prompt->SetVisibility(ESlateVisibility::Collapsed);return;}
 if(!PC)return;
 if(!Prompt){Prompt=CreateWidget<UPopupClosePromptWidget>(PC);Prompt->AddToViewport(8);}
 Prompt->SetVisibility(ESlateVisibility::HitTestInvisible);
}
void APopupCloseStage::FitScreenCamera()
{
 FVector2D Size(1280,800);if(GEngine && GEngine->GameViewport)GEngine->GameViewport->GetViewportSize(Size);
 const float Aspect=Size.Y>0?Size.X/Size.Y:1.6f;
 // Fit the actual 72 x 50 cm casing, including on wide editor viewports.
 const float RequiredWidth=FMath::Max(94.f,50.f*Aspect/.78f);
 const float FOV=FMath::RadiansToDegrees(2.f*FMath::Atan(RequiredWidth/(2.f*114.5f)));
 ScreenCamera->SetFieldOfView(FOV);
}
void APopupCloseStage::WarmWindowPool(float Dt)
{
 if(WindowPool.Num()>=PoolCapacity || Phase==EPhase::Spraying || Phase==EPhase::Complete)return;
 WarmDebt+=Dt;if(WarmDebt<.06f)return;WarmDebt=0;
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto Window=GetWorld()->SpawnActor<APopupCloseFlyingWindow>(FVector(0,0,-10000-WindowPool.Num()*80),FRotator::ZeroRotator,Params);
 if(Window){Window->PrepareForPool(WindowPool.Num());WindowPool.Add(Window);}
}
void APopupCloseStage::Tick(float Dt)
{
	const double Now=FPlatformTime::Seconds();
	WallFrameSeconds=LastTickTime>0?float(Now-LastTickTime):Dt;LastTickTime=Now;
 Super::Tick(Dt);WarmWindowPool(Dt);
 auto PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;
 if(Phase==EPhase::Idle)
 {
  const bool bCanUse=CanInteract(PC);SetPromptVisible(PC,bCanUse);
  if(bCanUse && PC->WasInputKeyJustPressed(EKeys::E))BeginScreenSession(PC,PC->GetPawn());
  return;
 }
 SetPromptVisible(PC,false);
 if(Phase==EPhase::Entering)
 {
  EnterTime+=Dt;
  if(EnterTime>=ScreenEnterSeconds)
  {
   Phase=EPhase::InScreen;ScreenTime=0;SpawnDebt=SlowInterval;
   DesktopInteraction->bEnableHitTesting=true;
   if(auto Locked=LockedController.Get())
   {
    FInputModeGameOnly Mode;Mode.SetConsumeCaptureMouseDown(false);Locked->SetInputMode(Mode);
    Locked->SetShowMouseCursor(true);
   }
   UE_LOG(LogPopupClose,Display,TEXT("Screen entered: continuous 31s acceleration, no peak hold; warmed=%d"),WindowPool.Num());
  }
  return;
 }
 if(Phase==EPhase::InScreen){FitScreenCamera();TickScreen(Dt);return;}
 if(Phase==EPhase::Spraying)TickSpray(Dt);
}
void APopupCloseStage::BeginScreenSession(APlayerController* PC,APawn* Pawn)
{
 if(Phase!=EPhase::Idle || !PC || !Pawn || !Desktop)return;
 LockedController=PC;LockedPawn=Pawn;Phase=EPhase::Entering;EnterTime=0;
 PoolCursor=0;LaunchedWindows=0;TotalPopups=0;FirstSprayDelay=-1;SprayPeakFrame=0;EjectionPeakFrame=0;
 bPopupAccountingValid=true;
 Desktop->ClearPopups();AimVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 if(auto Character=Cast<ACharacter>(Pawn))
 {
  SavedMovementMode=Character->GetCharacterMovement()->MovementMode;
  Character->GetCharacterMovement()->StopMovementImmediately();Character->GetCharacterMovement()->DisableMovement();
 }
 PC->SetIgnoreMoveInput(true);bMoveSuppressed=true;PC->SetIgnoreLookInput(true);bLookSuppressed=true;
 FitScreenCamera();
 PC->SetViewTargetWithBlend(this,ScreenEnterSeconds,EViewTargetBlendFunction::VTBlend_EaseInOut,2.f,true);
 UE_LOG(LogPopupClose,Display,TEXT("Screen approach started: smooth 1.2s camera translation; desktop remains on monitor"));
}
float APopupCloseStage::CurrentSpawnInterval() const
{
 return SpawnIntervalAt(ScreenTime);
}
void APopupCloseStage::TickScreen(float Dt)
{
 DesktopInteraction->TickComponent(Dt,LEVELTICK_All,nullptr);
 if(auto PC=LockedController.Get())
 {
  if(PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))DesktopInteraction->PressPointerKey(EKeys::LeftMouseButton);
  if(PC->WasInputKeyJustReleased(EKeys::LeftMouseButton))DesktopInteraction->ReleasePointerKey(EKeys::LeftMouseButton);
 }
 ScreenTime+=Dt;const float Interval=CurrentSpawnInterval();SpawnDebt=FMath::Min(SpawnDebt+Dt,Interval*3);
 int32 Spawned=0;
 while(Desktop && SpawnDebt>=Interval && Spawned<3)
 {
  SpawnDebt-=Interval;
  Desktop->SpawnPopup();++Spawned;++TotalPopups;
 }
 // Every popup remains until its own close button is clicked or play ends.
 if(Desktop)bPopupAccountingValid &= Desktop->GetOpenCount()+Desktop->GetManuallyClosedCount()==TotalPopups;
 if(ScreenTime>=ScreenRampSeconds)EjectFromScreen();
}
void APopupCloseStage::EjectFromScreen()
{
 DesktopInteraction->ReleasePointerKey(EKeys::LeftMouseButton);DesktopInteraction->bEnableHitTesting=false;
 Phase=EPhase::Spraying;SprayTime=0;SprayDebt=0;
 if(auto PC=LockedController.Get())
 {
  PC->SetShowMouseCursor(false);PC->SetInputMode(FInputModeGameOnly());
  if(auto Pawn=LockedPawn.Get())PC->SetViewTargetWithBlend(Pawn,ScreenExitSeconds,EViewTargetBlendFunction::VTBlend_EaseInOut,2.f,true);
 }
 // Only already-created windows are launched. No class, material or physics-body allocation here.
 SpawnFlyingWindow();
 UE_LOG(LogPopupClose,Display,TEXT("Eruption started immediately: screen_seconds=%.2f warmed=%d popups=%d"),ScreenTime,WindowPool.Num(),TotalPopups);
}
void APopupCloseStage::TickSpray(float Dt)
{
 SprayTime+=Dt;SprayPeakFrame=FMath::Max(SprayPeakFrame,WallFrameSeconds);
 if(SprayTime<=1.f)EjectionPeakFrame=FMath::Max(EjectionPeakFrame,WallFrameSeconds);
 SprayDebt=FMath::Min(SprayDebt+Dt,SprayInterval*3);
 int32 Launched=0;
 while(SprayDebt>=SprayInterval && SprayTime<=SpraySeconds && Launched<3)
 {
  SprayDebt-=SprayInterval;SpawnFlyingWindow();++Launched;
 }
 // Spread UI cleanup over frames, rather than tearing down the entire desktop at peak load.
 if(Desktop && SprayTime>.5f)Desktop->ClearOldestPopup();
 if(SprayTime>=ScreenExitSeconds)RestorePlayerControl(true);
 if(SprayTime>=SpraySeconds)
 {
  Phase=EPhase::Complete;AimVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  RestorePlayerControl(true);WriteSessionReport();
 }
}
void APopupCloseStage::SpawnFlyingWindow()
{
 if(PoolCursor>=WindowPool.Num())return;
 auto Window=WindowPool[PoolCursor++];const FVector Out=-GetActorForwardVector();
 const FVector Origin=Screen->GetComponentLocation()+Out*18+GetActorRightVector()*FMath::FRandRange(-24.f,24.f)+FVector::UpVector*FMath::FRandRange(-12.f,12.f);
 const FRotator R(FMath::FRandRange(-18.f,24.f),FMath::FRandRange(-35.f,35.f),FMath::FRandRange(-25.f,25.f));
 Window->SetActorLocationAndRotation(Origin,R,false,nullptr,ETeleportType::TeleportPhysics);
 const FVector Velocity=Out*FMath::FRandRange(600.f,1350.f)+FVector::UpVector*FMath::FRandRange(160.f,560.f)+GetActorRightVector()*FMath::FRandRange(-420.f,420.f);
 Window->Launch(PopupCloseContent::Get(PoolCursor-1).Color,Velocity);++LaunchedWindows;
 if(FirstSprayDelay<0)FirstSprayDelay=SprayTime;
}
void APopupCloseStage::RestorePlayerControl(bool bRestoreLook)
{
 auto PC=LockedController.Get();auto Pawn=LockedPawn.Get();
 if(bMoveSuppressed && bRestoreLook)
 {
  if(auto Character=Cast<ACharacter>(Pawn))Character->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(SavedMovementMode));
  if(PC)PC->SetIgnoreMoveInput(false);bMoveSuppressed=false;
 }
 if(bRestoreLook && bLookSuppressed && PC){PC->SetIgnoreLookInput(false);bLookSuppressed=false;}
}
FString APopupCloseStage::GetSessionStatus() const
{
 const FString Status=FString::Printf(TEXT("phase=%d screen=%.2f spray=%.2f pool=%d launched=%d open=%d can_interact=%d"),int32(Phase),ScreenTime,SprayTime,WindowPool.Num(),LaunchedWindows,Desktop?Desktop->GetOpenCount():0,CanInteract(LockedController.Get()));
 UE_LOG(LogPopupClose,Display,TEXT("%s"),*Status);return Status;
}
void APopupCloseStage::WriteSessionReport()
{
 const auto PC=LockedController.Get();
 const bool bInput=PC && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && !PC->bShowMouseCursor;
 const bool bLocked=Phase==EPhase::Complete && !CanInteract(PC) && AimVolume->GetCollisionEnabled()==ECollisionEnabled::NoCollision;
 const float TotalSeconds=EnterTime+ScreenTime+SprayTime;
 const bool bPass=TotalSeconds>=40.f && TotalSeconds<41.f && FirstSprayDelay==0 && LaunchedWindows>=125 && bInput && bLocked && bPopupAccountingValid;
 const FString Report=FString::Printf(TEXT("%s\nTotal experience: %.2f s\nContinuous screen acceleration: %.2f s\nMaximum-rate hold: 0 s\nSpray: %.2f s\nScreen popups spawned: %d\nAll screen popups retained unless manually closed: %d\nPrewarmed pool: %d\nLaunched windows: %d\nFirst spray delay: %.4f s\nLargest frame during first spray second: %.2f ms\nLargest spray frame: %.2f ms\nInput restored: %d\nComputer permanently locked: %d\n"),bPass?TEXT("PASS"):TEXT("FAIL"),TotalSeconds,ScreenTime,SprayTime,TotalPopups,bPopupAccountingValid,WindowPool.Num(),LaunchedWindows,FirstSprayDelay,EjectionPeakFrame*1000,SprayPeakFrame*1000,bInput,bLocked);
 IFileManager::Get().MakeDirectory(*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Validation")),true);
 FFileHelper::SaveStringToFile(Report,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Validation/popup_office_session.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
 UE_LOG(LogPopupClose,Display,TEXT("Office session %s"),*Report);
}

FString APopupCloseStage::ValidateOffice()
{
 if(!GetWorld()->IsGameWorld() || Phase!=EPhase::InScreen || !Desktop || Desktop->GetOpenCount()==0)
  return TEXT("FAIL: run during the physical-screen session after a popup appears");
 FString Report;int32 Checks=0,Failures=0;
 auto Check=[&](bool bPass,const TCHAR* Label)
 {
  ++Checks;if(!bPass)++Failures;Report+=FString::Printf(TEXT("%s: %s\n"),bPass?TEXT("PASS"):TEXT("FAIL"),Label);
 };
 auto PC=LockedController.Get();
 Check(Floor->GetRelativeScale3D().X*Floor->GetRelativeScale3D().Y/(9.4f*7.4f)>=6.f,TEXT("office floor area is at least six times the original"));
 Check(TableTop->GetRelativeLocation().X==0 && TableTop->GetRelativeLocation().Y==0,TEXT("interactive workstation remains at room center"));
 int32 Details=0;for(TActorIterator<APopupCloseOffice> It(GetWorld());It;++It)Details+=It->GetDetailCount();
 Check(Details>=500,TEXT("office furniture, windows, fixtures and clutter are built"));
 Check(DesktopSurface->GetWidgetSpace()==EWidgetSpace::World && !Desktop->IsInViewport(),TEXT("desktop is on physical monitor, not a fullscreen overlay"));
 Check(PC && PC->GetViewTarget()==this,TEXT("camera has arrived at physical monitor"));
 Check(PC && PC->IsMoveInputIgnored() && PC->IsLookInputIgnored() && PC->bShowMouseCursor,TEXT("screen session locks movement and enables mouse"));
 FVector2D ViewSize;GEngine->GameViewport->GetViewportSize(ViewSize);bool bFits=true;
 for(float Y:{-36.f,36.f})for(float Z:{-25.f,25.f})
 {
  FVector2D P;const FVector Corner=GetActorTransform().TransformPosition(FVector(44,Y,122+Z));
  bFits &= PC->ProjectWorldLocationToScreen(Corner,P) && P.X>0 && P.Y>0 && P.X<ViewSize.X && P.Y<ViewSize.Y;
 }
 Check(bFits,TEXT("all four physical monitor bezel corners remain in view"));
 Check(ScreenEnterSeconds>=1 && ScreenExitSeconds>=.7f,TEXT("both camera transitions use smooth travel"));
 bool bAccelerates=true;
 for(int32 I=1;I<=100;++I)bAccelerates &= SpawnIntervalAt(ScreenRampSeconds*I/100.f)<SpawnIntervalAt(ScreenRampSeconds*(I-1)/100.f);
 Check(bAccelerates && FMath::IsNearlyEqual(ScreenEnterSeconds+ScreenRampSeconds+SpraySeconds,40.2f),TEXT("continuous acceleration without peak hold; total experience about 40 seconds"));
 Check(bPopupAccountingValid,TEXT("screen popups are never removed automatically during play"));
 Check(TableTop->GetRelativeScale3D().X>=1.9f,TEXT("larger detailed central workstation"));
 Check(WindowPool.Num()>0 && WindowPool.Num()<=PoolCapacity,TEXT("physical windows are preallocated within bounded pool"));
 bool bVaried=false;for(int32 I=1;I<PopupCloseContent::Count;++I)bVaried |= !PopupCloseContent::Get(I).Size.Equals(PopupCloseContent::Get(0).Size);
 Check(bVaried,TEXT("work popup templates have different sizes"));
 const FVector2D Pixel=Desktop->GetLatestClosePoint();
 const FVector Point=DesktopSurface->GetComponentTransform().TransformPosition(FVector(0,-(Pixel.X-640),-(Pixel.Y-400)));
 FHitResult Hit(this,DesktopSurface,Point,-GetActorForwardVector());
 Hit.bBlockingHit=true;Hit.Location=Point;Hit.ImpactPoint=Point;Hit.TraceStart=Point-GetActorForwardVector()*20;Hit.TraceEnd=Point+GetActorForwardVector()*20;
 const int32 Before=Desktop->GetOpenCount();
 DesktopInteraction->InteractionSource=EWidgetInteractionSource::Custom;DesktopInteraction->SetCustomHitResult(Hit);
 DesktopInteraction->TickComponent(0,LEVELTICK_All,nullptr);
 Check(DesktopInteraction->GetHoveredWidgetComponent()==DesktopSurface,TEXT("physical screen hit resolves through widget interaction"));
 DesktopInteraction->PressPointerKey(EKeys::LeftMouseButton);DesktopInteraction->ReleasePointerKey(EKeys::LeftMouseButton);
 DesktopInteraction->InteractionSource=EWidgetInteractionSource::Mouse;
 Check(Desktop->GetOpenCount()==Before-1,TEXT("actual close button responds to pointer down and up on physical screen"));
 const FString Result=FString::Printf(TEXT("%s: %d checks (%d failures)"),Failures?TEXT("FAIL"):TEXT("PASS"),Checks,Failures);
 Report+=Result+TEXT("\n");
 IFileManager::Get().MakeDirectory(*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Validation")),true);
 FFileHelper::SaveStringToFile(Report,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Validation/popup_office_runtime.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
 UE_LOG(LogPopupClose,Display,TEXT("%s"),*Report);return Result;
}

#if WITH_EDITOR
void APopupCloseStage::UpgradeOffice()
{
 if(!GEditor || GEditor->PlayWorld)return;
 auto World=GEditor->GetEditorWorldContext().World();
 if(!World || World->GetOutermost()->GetName()!=TEXT("/Game/PopupClose/Maps/Lvl_PopupClose"))return;
 APopupCloseStage* Stage=nullptr;for(TActorIterator<APopupCloseStage> It(World);It;++It){Stage=*It;break;}
 if(!Stage)return;
 Stage->Modify();Stage->ConfigureOffice();Stage->ApplyColors();Stage->SetActorLabel(TEXT("办公室 · 中央电脑"));
 Stage->SetFolderPath(TEXT("PopupOffice/Workstation"));
 APopupCloseOffice* Office=nullptr;for(TActorIterator<APopupCloseOffice> It(World);It;++It){Office=*It;break;}
 if(!Office)
 {
  FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  Office=World->SpawnActor<APopupCloseOffice>(Stage->GetActorLocation(),Stage->GetActorRotation(),Params);
 }
 if(Office){Office->OnConstruction(Office->GetActorTransform());Office->SetActorLabel(TEXT("办公室 · 工位与陈设"));Office->SetFolderPath(TEXT("PopupOffice/Environment"));}
 World->GetWorldSettings()->DefaultGameMode=APopupCloseGameMode::StaticClass();
 World->MarkPackageDirty();
 const bool Saved=UEditorLoadingAndSavingUtils::SaveMap(World,TEXT("/Game/PopupClose/Maps/Lvl_PopupClose"));
 UE_LOG(LogPopupClose,Display,TEXT("Office upgrade: 24x18m (6.21x floor area), details=%d saved=%d"),Office?Office->GetDetailCount():0,Saved);
}
void APopupCloseStage::CreateLevel()
{
 if(!GEditor || GEditor->PlayWorld)return;
 auto World=UEditorLoadingAndSavingUtils::NewBlankMap(false);if(!World)return;
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 World->SpawnActor<APopupCloseStage>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
 World->SpawnActor<APopupCloseOffice>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
 World->GetWorldSettings()->DefaultGameMode=APopupCloseGameMode::StaticClass();
 UEditorLoadingAndSavingUtils::SaveMap(World,TEXT("/Game/PopupClose/Maps/Lvl_PopupClose"));
}
static FAutoConsoleCommand CreatePopupLevel(TEXT("PopupClose.CreateLevel"),TEXT("Create the popup-close office level"),FConsoleCommandDelegate::CreateStatic(&APopupCloseStage::CreateLevel));
static FAutoConsoleCommand UpgradePopupOffice(TEXT("PopupClose.UpgradeOffice"),TEXT("Expand and furnish the existing popup-close level"),FConsoleCommandDelegate::CreateStatic(&APopupCloseStage::UpgradeOffice));
#endif
