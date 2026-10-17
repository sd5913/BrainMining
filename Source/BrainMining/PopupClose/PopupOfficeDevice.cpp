#include "PopupClose/PopupOfficeDevice.h"
#include "MiningInteractionTrace.h"
#include "MiningRoomAudioSubsystem.h"
#include "PopupClose/PopupCloseContent.h"
#include "PopupClose/PopupCloseFlyingWindow.h"
#include "PopupClose/PopupCloseWidgets.h"
#include "MiningWalletSubsystem.h"
#include "Engine/GameInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
 void AwardOfficeCoins(APopupOfficeDevice& Device,int64 Amount)
 {
  if(auto GameInstance=Device.GetGameInstance())
   if(auto Wallet=GameInstance->GetSubsystem<UMiningWalletSubsystem>(); Wallet && !Wallet->UsesMineralInventory())Wallet->AddCoins(Amount);
 }
}

APopupOfficeDevice::APopupOfficeDevice()
{
 PrimaryActorTick.bCanEverTick=true;
 SceneRoot=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));SetRootComponent(SceneRoot);
 ArtShell=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OfficeArtShell"));ArtShell->SetupAttachment(SceneRoot);
 // Interactions remain on Aim / DesktopSurface; the imported shell is decorative.
 ArtShell->SetCollisionEnabled(ECollisionEnabled::NoCollision);ArtShell->SetCanEverAffectNavigation(false);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 for(int32 I=0;I<5;++I)
 {
  auto Group=CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("DeviceModel%d"),I));
  Group->SetupAttachment(SceneRoot);Group->SetStaticMesh(Cube.Object);
  Group->SetCollisionProfileName(TEXT("BlockAll"));Model.Add(Group);
 }
 PaperStack=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PaperStack"));
 PaperStack->SetupAttachment(SceneRoot);PaperStack->SetStaticMesh(Cube.Object);
 PaperStack->SetCollisionProfileName(TEXT("NoCollision"));PaperStack->SetCastShadow(false);
 Aim=CreateDefaultSubobject<UBoxComponent>(TEXT("Aim"));Aim->SetupAttachment(SceneRoot);
 Aim->SetCollisionResponseToAllChannels(ECR_Ignore);Aim->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 Aim->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("ScreenCamera"));Camera->SetupAttachment(SceneRoot);
 Camera->SetRelativeLocation(FVector(-75,0,124));Camera->bUsePawnControlRotation=false;
 Camera->bOverrideAspectRatioAxisConstraint=true;Camera->AspectRatioAxisConstraint=AspectRatio_MaintainXFOV;
 Camera->PostProcessBlendWeight=0;
 Surface=CreateDefaultSubobject<UWidgetComponent>(TEXT("DesktopSurface"));Surface->SetupAttachment(SceneRoot);
 Surface->SetWidgetSpace(EWidgetSpace::World);Surface->SetDrawSize(FVector2D(1280,800));
 Surface->SetRelativeLocation(FVector(39.5f,0,124));Surface->SetRelativeRotation(FRotator(0,180,0));
 Surface->SetRelativeScale3D(FVector(.0484375f));Surface->SetBlendMode(EWidgetBlendMode::Opaque);
 Surface->SetTwoSided(false);Surface->SetWindowFocusable(false);Surface->SetCollisionProfileName(TEXT("UI"));Surface->SetCastShadow(false);
 Pointer=CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("DesktopPointer"));Pointer->SetupAttachment(SceneRoot);
 Pointer->InteractionSource=EWidgetInteractionSource::Mouse;Pointer->InteractionDistance=1000;
 Pointer->VirtualUserIndex=1;Pointer->bEnableHitTesting=false;Pointer->SetComponentTickEnabled(false);
 Warning=CreateDefaultSubobject<UWidgetComponent>(TEXT("PendingWarning"));Warning->SetupAttachment(SceneRoot);
 Warning->SetWidgetSpace(EWidgetSpace::Screen);Warning->SetDrawSize(FVector2D(80,110));
 Warning->SetRelativeLocation(FVector(15,0,225));Warning->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Warning->SetWindowFocusable(false);Warning->SetVisibility(false);
}
void APopupOfficeDevice::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);BuildModel();
}
void APopupOfficeDevice::EndPlay(const EEndPlayReason::Type Reason)
{
 for (auto Item : Pool) if (IsValid(Item)) Item->Destroy();
 Pool.Reset();
 Super::EndPlay(Reason);
}
void APopupOfficeDevice::ResetSession()
{
 SetScreenInteractive(false);
 if (Desktop)
 {
  Desktop->ClearPopups();
  Desktop->SetRepliesEnabled(false);
 }
 for (auto Item : Pool) if (IsValid(Item)) Item->Destroy();
 Pool.Reset();
 WarmDebt = 0;
 PaperStack->ClearInstances();
 PoolCursor = ProblemsCreated = PapersHandled = 0;
 bLocked = false;
 RefreshWarning();
}
void APopupOfficeDevice::BuildModel()
{
 auto Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
 const FLinearColor Colors[]={FLinearColor(.065f,.08f,.1f),FLinearColor(.8f,.82f,.79f),FLinearColor(.38f,.25f,.14f),FLinearColor(.2f,.42f,.58f),FLinearColor(.94f,.92f,.86f)};
 for(int32 I=0;I<Model.Num();++I)
 {
  Model[I]->ClearInstances();auto Mat=UMaterialInstanceDynamic::Create(Parent,this);
  Mat->SetVectorParameterValue(TEXT("Color"),Colors[I]);
  Model[I]->SetMaterial(0,ModelMaterialOverrides.IsValidIndex(I) && ModelMaterialOverrides[I]?ModelMaterialOverrides[I].Get():Mat);
  Model[I]->SetVisibility(!ReplacementModel);
  Model[I]->SetCollisionEnabled(ReplacementModel?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryAndPhysics);
 }
 ArtShell->SetStaticMesh(ReplacementModel);ArtShell->SetRelativeTransform(ReplacementTransform);ArtShell->SetVisibility(ReplacementModel!=nullptr);
 if(ScreenProjectionMaterial && !IsPrinter())ArtShell->SetMaterial(1,ScreenProjectionMaterial);
 ArtShell->SetCollisionProfileName(ReplacementModel?TEXT("BlockAll"):TEXT("NoCollision"));
 // The imported monitor's convex hull must not intercept mouse rays before the live desktop widget.
 if(ReplacementModel && !IsPrinter())ArtShell->SetCollisionResponseToChannel(ECC_Visibility,ECR_Ignore);
 PaperStack->SetMaterial(0,Model[4]->GetMaterial(0));
 auto Box=[&](int32 G,FVector P,FVector Size){Model[G]->AddInstance(FTransform(FRotator::ZeroRotator,P,Size/100.f));};
 Surface->SetVisibility(!IsPrinter());Surface->SetCollisionEnabled(IsPrinter()?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryOnly);
 Surface->SetRenderInMainPass(!ScreenProjectionMaterial);Surface->SetRenderInDepthPass(!ScreenProjectionMaterial);
 Surface->SetTickWhenOffscreen(ScreenProjectionMaterial!=nullptr);
 Aim->SetRelativeLocation(IsPrinter()?FVector(-20,0,116):Surface->GetRelativeLocation());
 if(!IsPrinter())
 {
  const FVector ScreenLocation=Surface->GetRelativeLocation();
  // Follow translated replacement screens without retaining the old monitor's center.
  Camera->SetRelativeLocation(FVector(-75,ScreenLocation.Y,ScreenLocation.Z));
 }
 Aim->SetBoxExtent(IsPrinter()?FVector(55,46,42):FVector(36,48,36));
 if(IsPrinter())
 {
  Box(2,FVector(0,0,45),FVector(85,90,90));Box(1,FVector(0,0,93),FVector(95,100,6));
  Box(1,FVector(0,0,119),FVector(68,76,46));Box(0,FVector(0,0,145),FVector(74,80,6));
  Box(0,FVector(-35,0,120),FVector(4,66,12));Box(3,FVector(-22,-27,145),FVector(20,14,3));
  Box(0,FVector(-49,0,110),FVector(35,62,3));
  for(float Y:{-32.f,32.f})Box(1,FVector(-49,Y,113),FVector(35,2,6));
  for(float Z:{18.f,49.f,79.f}){Box(1,FVector(-44,0,Z),FVector(2,81,27));Box(0,FVector(-46,0,Z+5),FVector(2,24,3));}
 }
 else
 {
  Box(0,FVector(48,0,122),FVector(8,72,50));Box(0,FVector(48,0,92),FVector(8,8,22));
  Box(0,FVector(40,0,80),FVector(28,16,3));
  if(DeviceIndex==1)
  {
   Box(2,FVector(0,0,74),FVector(190,100,6));Box(0,FVector(0,0,69),FVector(184,94,3));
   for(float X:{-75.f,75.f})for(float Y:{-42.f,42.f})Box(0,FVector(X,Y,35),FVector(5,5,70));
   Box(0,FVector(-10,0,80),FVector(16,36,3));Box(0,FVector(-12,30,80),FVector(10,6,3));
   Box(0,FVector(35,-32,31),FVector(34,18,58));Box(4,FVector(-45,-25,79),FVector(25,22,3));
   for(int32 R=0;R<4;++R)for(int32 K=0;K<11;++K)Box(1,FVector(-15+R*3.f,-14+K*2.8f,81.7f),FVector(2.3f,2.1f,.4f));
  }
 }
}
void APopupOfficeDevice::BeginPlay()
{
 Super::BeginPlay();BuildModel();Pool.Reserve(102);PaperStack->ClearInstances();
 if(auto PC=GetWorld()->GetFirstPlayerController())
 {
  WarningUI=CreateWidget<UPopupCloseWarningWidget>(PC);WarningUI->SetVisibility(ESlateVisibility::HitTestInvisible);Warning->SetDrawSize(FVector2D(80,110));Warning->SetWidget(WarningUI);
  if(!IsPrinter())
  {
   Desktop=CreateWidget<UPopupCloseDesktopWidget>(PC);Desktop->TakeWidget();Desktop->SetRepliesEnabled(false);Surface->SetWidget(Desktop);Surface->RequestRedraw();
   TWeakObjectPtr<APopupOfficeDevice> WeakThis(this);
   Desktop->OnReply=[WeakThis]
   {
    if(auto Self=WeakThis.Get();Self && !Self->IsLocked())
    {
     AwardOfficeCoins(*Self,20);Self->RefreshWarning();
     UMiningRoomAudioSubsystem::Emit(Self,TEXT("S_Reply"),.4f);
    }
   };
  }
 }
 RefreshWarning();
}
void APopupOfficeDevice::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 if(Desktop && ScreenProjectionMaterial && Surface->GetRenderTarget())
 {
  if(!ProjectedDesktop)
  {
   ProjectedDesktop=UMaterialInstanceDynamic::Create(ScreenProjectionMaterial,this);
   ArtShell->SetMaterial(1,ProjectedDesktop);
  }
  ProjectedDesktop->SetTextureParameterValue(TEXT("DesktopTexture"),Surface->GetRenderTarget());
 }
 if(!WarningUI || !Warning->IsVisible())return;
 auto PC=GetWorld()->GetFirstPlayerController();if(!PC)return;
 int32 Width=0,Height=0;PC->GetViewportSize(Width,Height);if(Width<=0 || Height<=0)return;
 const FVector Anchor=GetActorTransform().TransformPosition(FVector(15,0,225));
 FVector2D Screen;
 const bool bTaskView=Cast<APopupOfficeDevice>(PC->GetViewTarget())!=nullptr;
 const bool bInFront=PC->ProjectWorldLocationToScreen(Anchor,Screen);
 WarningUI->SetRenderOpacity(bInFront || bTaskView?1.f:0.f);
 if(!bInFront && !bTaskView)return;
 // Screen-space billboard stays crisp and camera-facing. Clamp close markers to
 // the viewport so their original high world anchor cannot disappear above it.
 if(bTaskView)Screen=FVector2D(Width-78,180+DeviceIndex*112);
 else Screen=FVector2D(FMath::Clamp(Screen.X,48.,double(Width-48)),FMath::Clamp(Screen.Y,70.,double(Height-74)));
 FVector Origin,Direction;
 if(PC->DeprojectScreenPositionToWorld(Screen.X,Screen.Y,Origin,Direction))
  Warning->SetWorldLocation(Origin+Direction*FMath::Max(100.,FVector::Dist(Origin,Anchor)));
}
int32 APopupOfficeDevice::Pending() const { return IsPrinter()?PaperStack->GetInstanceCount():(Desktop?Desktop->GetOpenCount():0); }
int32 APopupOfficeDevice::Handled() const { return IsPrinter()?PapersHandled:(Desktop?Desktop->GetManuallyClosedCount():0); }
bool APopupOfficeDevice::AccountingValid() const { return Pending()+Handled()==ProblemsCreated; }
bool APopupOfficeDevice::WarningVisible() const { return Warning->IsVisible(); }
FVector APopupOfficeDevice::InteractionPoint() const { return Aim->GetComponentLocation(); }
bool APopupOfficeDevice::CanInteract(APlayerController* PC) const
{
 if(bLocked || !PC || !PC->GetPawn() || FVector::Dist(PC->GetPawn()->GetActorLocation(),InteractionPoint())>320)return false;
 FVector P;FRotator R;PC->GetPlayerViewPoint(P,R);
 FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(OfficeDeviceAim),false,PC->GetPawn());
 return MiningInteraction::Trace(GetWorld(),Hit,P,R,450,Query,[this](const FHitResult& Candidate){return Candidate.GetActor()==this;});
}
void APopupOfficeDevice::AddProblem()
{
 if(bLocked)return;
 if(IsPrinter())
 {
  const int32 I=Pending();
  PaperStack->AddInstance(FTransform(FRotator(0,PaperOutputYaw+(I%5-2)*1.1f,0),PaperOutputPosition+FVector((I%3-1)*.5f,(I%5-2)*.4f,I*.18f),FVector(.297f,.21f,.0015f)));
 }
 else if(Desktop)Desktop->SpawnPopup();else return;
 ++ProblemsCreated;RefreshWarning();
 UMiningRoomAudioSubsystem::Emit(this,IsPrinter()?TEXT("S_Printer"):TEXT("S_OfficeNotify"),IsPrinter()?.35f:.3f,true,IsPrinter()?.7f:.35f);
}
bool APopupOfficeDevice::ProcessPaper()
{
 if(bLocked || !IsPrinter() || Pending()==0)return false;
 if(!PaperStack->RemoveInstance(Pending()-1))return false;
 ++PapersHandled;AwardOfficeCoins(*this,10);RefreshWarning();UMiningRoomAudioSubsystem::Emit(this,TEXT("S_Paper"),.55f,true);return true;
}
void APopupOfficeDevice::RefreshWarning()
{
 if(WarningUI)WarningUI->PendingCount=Pending();
 Warning->SetVisibility(Pending()>0);
}
void APopupOfficeDevice::Lock()
{
 bLocked=true;Aim->SetCollisionEnabled(ECollisionEnabled::NoCollision);SetScreenInteractive(false);
 if(Desktop)Desktop->SetRepliesEnabled(false);
}
void APopupOfficeDevice::HideWarning() { Warning->SetVisibility(false); }
void APopupOfficeDevice::SetScreenInteractive(bool bEnabled)
{
 if(Desktop)Desktop->SetRepliesEnabled(bEnabled && !bLocked);
 Pointer->ReleasePointerKey(EKeys::LeftMouseButton);Pointer->bEnableHitTesting=bEnabled && !bLocked;
 Aim->SetCollisionEnabled(!bLocked && !bEnabled?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
}
void APopupOfficeDevice::TickScreen(float Dt,APlayerController* PC)
{
 if(bLocked || !PC)return;
 if(ScreenProjectionMaterial)
 {
  FVector Origin,Direction;FHitResult WidgetHit;
  if(PC->DeprojectMousePositionToWorld(Origin,Direction))TraceDesktopGlass(Origin,Direction,WidgetHit);
  Pointer->InteractionSource=EWidgetInteractionSource::Custom;Pointer->SetCustomHitResult(WidgetHit);
 }
 else Pointer->InteractionSource=EWidgetInteractionSource::Mouse;
 FitCamera();Pointer->TickComponent(Dt,LEVELTICK_All,nullptr);
 if(PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))Pointer->PressPointerKey(EKeys::LeftMouseButton);
 if(PC->WasInputKeyJustReleased(EKeys::LeftMouseButton))Pointer->ReleasePointerKey(EKeys::LeftMouseButton);
}
bool APopupOfficeDevice::TraceDesktopGlass(const FVector& Origin,const FVector& Direction,FHitResult& WidgetHit) const
{
 FHitResult GlassHit;FCollisionQueryParams Query(SCENE_QUERY_STAT(CRTDesktopPointer),true);
 Query.bReturnFaceIndex=true;
 if(!ArtShell->LineTraceComponent(GlassHit,Origin,Origin+Direction*1000.f,Query))return false;
 int32 Section=INDEX_NONE;
 if(ArtShell->GetMaterialFromCollisionFaceIndex(GlassHit.FaceIndex,Section)!=ArtShell->GetMaterial(1))return false;
 // CustomScreen UVs map linearly across the curved glass's original local X/Z.
 const FVector Local=ArtShell->GetComponentTransform().InverseTransformPosition(GlassHit.ImpactPoint);
 const FVector2D UV(.5f-Local.X/35.87354f,(41.48716f-Local.Z)/27.40217f);
 if(UV.X<0.f || UV.X>1.f || UV.Y<0.f || UV.Y>1.f)return false;
 const FVector2D Pixel=UV*Surface->GetDrawSize();
 const FVector Point=Surface->GetComponentTransform().TransformPosition(FVector(0,-(Pixel.X-640.f),-(Pixel.Y-400.f)));
 WidgetHit=FHitResult(const_cast<APopupOfficeDevice*>(this),Surface,Point,-GetActorForwardVector());
 WidgetHit.bBlockingHit=true;WidgetHit.Location=Point;WidgetHit.ImpactPoint=Point;
 WidgetHit.TraceStart=Point-GetActorForwardVector()*20.f;WidgetHit.TraceEnd=Point+GetActorForwardVector()*20.f;
 return true;
}
void APopupOfficeDevice::FitCamera()
{
 FVector2D Size(1280,800);if(GEngine && GEngine->GameViewport)GEngine->GameViewport->GetViewportSize(Size);
 const float Aspect=Size.Y>0?Size.X/Size.Y:1.6f;
 const FVector2D DrawSize=Surface->GetDrawSize();
 const FVector Scale=Surface->GetRelativeScale3D().GetAbs();
 const float ScreenWidth=DrawSize.X*Scale.Y;
 const float ScreenHeight=DrawSize.Y*Scale.Z;
 const float Distance=FMath::Max(1.f,FMath::Abs(Surface->GetRelativeLocation().X-Camera->GetRelativeLocation().X));
 // Fit the actual CRT aperture, retaining a little bezel and the warning column.
 const float ViewWidth=FMath::Max(ScreenWidth/.66f,ScreenHeight*Aspect/.78f);
 Camera->SetFieldOfView(FMath::RadiansToDegrees(2.f*FMath::Atan(ViewWidth/(2.f*Distance))));
}
void APopupOfficeDevice::WarmPool(float Dt)
{
 if(bLocked || Pool.Num()>=102)return;
 WarmDebt+=Dt;if(WarmDebt<.06f)return;WarmDebt=0;
 FActorSpawnParameters Params;Params.Owner=this;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto Item=GetWorld()->SpawnActor<APopupCloseFlyingWindow>(FVector(DeviceIndex*100,0,-10000-Pool.Num()*80),FRotator::ZeroRotator,Params);
 if(Item){Item->PrepareForPool(Pool.Num(),IsPrinter());Pool.Add(Item);}
}
void APopupOfficeDevice::Emit()
{
 if(PoolCursor>=Pool.Num())return;
 auto Item=Pool[PoolCursor++];const FVector Out=IsPrinter()?GetActorTransform().TransformVectorNoScale(PaperOutputDirection).GetSafeNormal():-GetActorForwardVector();
 const FVector Origin=GetActorTransform().TransformPosition(IsPrinter()?PaperOutputPosition+PaperOutputDirection.GetSafeNormal()*16+FVector(0,0,6):Surface->GetRelativeLocation()+FVector(-15.5f,0,0))+GetActorRightVector()*FMath::FRandRange(-20.f,20.f);
 Item->SetActorLocationAndRotation(Origin,FRotator(FMath::FRandRange(-25.f,25.f),GetActorRotation().Yaw+FMath::FRandRange(-30.f,30.f),FMath::FRandRange(-25.f,25.f)),false,nullptr,ETeleportType::TeleportPhysics);
 Item->Launch(PopupCloseContent::Get(PoolCursor).Color,Out*FMath::FRandRange(550.f,1100.f)+FVector::UpVector*FMath::FRandRange(180.f,430.f)+GetActorRightVector()*FMath::FRandRange(-280.f,280.f));
}
void APopupOfficeDevice::CleanupOnePopup() { if(Desktop)Desktop->ClearOldestPopup(); }
bool APopupOfficeDevice::ClickReplyForValidation()
{
 if(!Desktop || Pending()==0 || bLocked)return false;
 const FVector2D Pixel=Desktop->GetLatestClosePoint();
 const FVector Point=Surface->GetComponentTransform().TransformPosition(FVector(0,-(Pixel.X-640),-(Pixel.Y-400)));
 FHitResult Hit(this,Surface,Point,-GetActorForwardVector());Hit.bBlockingHit=true;Hit.Location=Point;Hit.ImpactPoint=Point;
 Hit.TraceStart=Point-GetActorForwardVector()*20;Hit.TraceEnd=Point+GetActorForwardVector()*20;
 if(ScreenProjectionMaterial)
 {
  const FVector2D UV=Pixel/Surface->GetDrawSize();
  FVector TargetLocal((.5f-UV.X)*35.87354f,-18.f,41.48716f-UV.Y*27.40217f);
  const FVector Origin=Camera->GetComponentLocation();
  // Converge onto the curved surface, then exercise the same trace as the real mouse.
  for(int32 I=0;I<4;++I)
  {
   const FVector Direction=(ArtShell->GetComponentTransform().TransformPosition(TargetLocal)-Origin).GetSafeNormal();
   FHitResult GlassHit;FCollisionQueryParams Query(SCENE_QUERY_STAT(CRTReplyValidation),true);Query.bReturnFaceIndex=true;
   if(!ArtShell->LineTraceComponent(GlassHit,Origin,Origin+Direction*1000.f,Query))return false;
   TargetLocal.Y=ArtShell->GetComponentTransform().InverseTransformPosition(GlassHit.ImpactPoint).Y;
  }
  if(!TraceDesktopGlass(Origin,(ArtShell->GetComponentTransform().TransformPosition(TargetLocal)-Origin).GetSafeNormal(),Hit))return false;
 }
 const int32 Before=Pending();Pointer->InteractionSource=EWidgetInteractionSource::Custom;
 Pointer->SetCustomHitResult(Hit);Pointer->TickComponent(0,LEVELTICK_All,nullptr);
 Pointer->PressPointerKey(EKeys::LeftMouseButton);Pointer->ReleasePointerKey(EKeys::LeftMouseButton);
 Pointer->InteractionSource=EWidgetInteractionSource::Mouse;
 return Pending()==Before-1;
}
