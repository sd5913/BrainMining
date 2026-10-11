#include "PopupClose/PopupOfficeDevice.h"
#include "PopupClose/PopupCloseContent.h"
#include "PopupClose/PopupCloseFlyingWindow.h"
#include "PopupClose/PopupCloseWidgets.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

APopupOfficeDevice::APopupOfficeDevice()
{
 PrimaryActorTick.bCanEverTick=false;
 SceneRoot=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));SetRootComponent(SceneRoot);
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
 Warning->SetWidgetSpace(EWidgetSpace::Screen);Warning->SetDrawSize(FVector2D(140,125));
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
  Mat->SetVectorParameterValue(TEXT("Color"),Colors[I]);Model[I]->SetMaterial(0,Mat);
 }
 PaperStack->SetMaterial(0,Model[4]->GetMaterial(0));
 auto Box=[&](int32 G,FVector P,FVector Size){Model[G]->AddInstance(FTransform(FRotator::ZeroRotator,P,Size/100.f));};
 Surface->SetVisibility(!IsPrinter());Surface->SetCollisionEnabled(IsPrinter()?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryOnly);
 Aim->SetRelativeLocation(IsPrinter()?FVector(-20,0,116):FVector(42,0,124));
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
  WarningUI=CreateWidget<UPopupCloseWarningWidget>(PC);Warning->SetWidget(WarningUI);
  if(!IsPrinter())
  {
   Desktop=CreateWidget<UPopupCloseDesktopWidget>(PC);Desktop->TakeWidget();Desktop->SetRepliesEnabled(false);Surface->SetWidget(Desktop);Surface->RequestRedraw();
   TWeakObjectPtr<APopupOfficeDevice> WeakThis(this);
   Desktop->OnReply=[WeakThis] { if(auto Self=WeakThis.Get())Self->RefreshWarning(); };
  }
 }
 RefreshWarning();
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
 return GetWorld()->LineTraceSingleByChannel(Hit,P,P+R.Vector()*450,ECC_Visibility,Query) && Hit.GetActor()==this;
}
void APopupOfficeDevice::AddProblem()
{
 if(bLocked)return;
 if(IsPrinter())
 {
  const int32 I=Pending();
  PaperStack->AddInstance(FTransform(FRotator(0,(I%5-2)*1.1f,0),FVector(-49+(I%3-1)*.5f,(I%5-2)*.4f,112+I*.18f),FVector(.297f,.21f,.0015f)));
 }
 else if(Desktop)Desktop->SpawnPopup();else return;
 ++ProblemsCreated;RefreshWarning();
}
bool APopupOfficeDevice::ProcessPaper()
{
 if(bLocked || !IsPrinter() || Pending()==0)return false;
 PaperStack->RemoveInstance(Pending()-1);++PapersHandled;RefreshWarning();return true;
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
 FitCamera();Pointer->TickComponent(Dt,LEVELTICK_All,nullptr);
 if(PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))Pointer->PressPointerKey(EKeys::LeftMouseButton);
 if(PC->WasInputKeyJustReleased(EKeys::LeftMouseButton))Pointer->ReleasePointerKey(EKeys::LeftMouseButton);
}
void APopupOfficeDevice::FitCamera()
{
 FVector2D Size(1280,800);if(GEngine && GEngine->GameViewport)GEngine->GameViewport->GetViewportSize(Size);
 const float Aspect=Size.Y>0?Size.X/Size.Y:1.6f;
 Camera->SetFieldOfView(FMath::RadiansToDegrees(2.f*FMath::Atan(FMath::Max(94.f,50.f*Aspect/.78f)/(2.f*114.5f))));
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
 auto Item=Pool[PoolCursor++];const FVector Out=-GetActorForwardVector();
 const FVector Origin=GetActorTransform().TransformPosition(IsPrinter()?FVector(-65,0,118):FVector(24,0,124))+GetActorRightVector()*FMath::FRandRange(-20.f,20.f);
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
 const int32 Before=Pending();Pointer->InteractionSource=EWidgetInteractionSource::Custom;
 Pointer->SetCustomHitResult(Hit);Pointer->TickComponent(0,LEVELTICK_All,nullptr);
 Pointer->PressPointerKey(EKeys::LeftMouseButton);Pointer->ReleasePointerKey(EKeys::LeftMouseButton);
 Pointer->InteractionSource=EWidgetInteractionSource::Mouse;
 return Pending()==Before-1;
}
