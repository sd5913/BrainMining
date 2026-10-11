#include "PopupClose/PopupCloseFlyingWindow.h"
#include "PopupClose/PopupCloseContent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

APopupCloseFlyingWindow::APopupCloseFlyingWindow()
{
 PrimaryActorTick.bCanEverTick=false;
 Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));SetRootComponent(Body);
 Body->SetMobility(EComponentMobility::Movable);
 Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
 Body->SetCollisionObjectType(ECC_WorldDynamic);Body->SetCollisionResponseToAllChannels(ECR_Ignore);
 Body->SetEnableGravity(false);Body->SetLinearDamping(.35f);Body->SetAngularDamping(.6f);Body->SetCastShadow(false);
 TitleBar=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TitleBar"));TitleBar->SetupAttachment(Body);
 CloseButton=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CloseButton"));CloseButton->SetupAttachment(Body);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 Body->SetStaticMesh(Cube.Object);
 for(auto Part:{TitleBar.Get(),CloseButton.Get()})
 {
  Part->SetStaticMesh(Cube.Object);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetCastShadow(false);
 }
 TitleBar->SetRelativeLocation(FVector(-60,0,43));TitleBar->SetRelativeScale3D(FVector(1.3,1,.14f));
 CloseButton->SetRelativeLocation(FVector(-110,43,43));CloseButton->SetRelativeScale3D(FVector(1.5f,.1f,.11f));
 TitleText=CreateDefaultSubobject<UTextRenderComponent>(TEXT("WorkTitle"));TitleText->SetupAttachment(Body);
 BodyText=CreateDefaultSubobject<UTextRenderComponent>(TEXT("WorkBody"));BodyText->SetupAttachment(Body);
 for(auto Text:{TitleText.Get(),BodyText.Get()})
 {
  Text->SetRelativeRotation(FRotator(0,180,0));Text->SetHorizontalAlignment(EHTA_Center);
  Text->SetVerticalAlignment(EVRTA_TextCenter);Text->SetTextRenderColor(FColor(24,31,40));Text->SetCastShadow(false);
 }
 SetActorHiddenInGame(true);
}

void APopupCloseFlyingWindow::PrepareForPool(int32 Variant)
{
 const auto& Copy=PopupCloseContent::Get(Variant);
 const float Width=Copy.Size.X*.13f,Height=Copy.Size.Y*.13f;
 Body->SetRelativeScale3D(FVector(.025f,Width/100.f,Height/100.f));
 TitleText->SetRelativeLocation(FVector(-150,-3,43));
 BodyText->SetRelativeLocation(FVector(-60,0,-4));
 const FVector TextScale(40,100.f/Width,100.f/Height);
 TitleText->SetRelativeScale3D(TextScale);BodyText->SetRelativeScale3D(TextScale);
 TitleText->SetWorldSize(4.f);BodyText->SetWorldSize(5.f);
 TitleText->SetText(FText::FromString(Copy.PhysicalTitle));
 BodyText->SetText(FText::FromString(TEXT("ACTION REQUIRED\nTODAY")));
 auto Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
 const FLinearColor Colors[]={Copy.Color,Copy.Color*.62f,FLinearColor(.8f,.14f,.1f)};
 int32 I=0;
 for(auto Mesh:{Body.Get(),TitleBar.Get(),CloseButton.Get()})
 {
  auto Material=UMaterialInstanceDynamic::Create(Parent,this);Material->SetVectorParameterValue(TEXT("Color"),Colors[I++]);Mesh->SetMaterial(0,Material);
 }
 // Allocate Chaos bodies ahead of the eruption, while the window is hidden and sleeping.
 Body->SetSimulatePhysics(true);Body->SetMassOverrideInKg(NAME_None,.35f,true);
 Body->PutAllRigidBodiesToSleep();
}

void APopupCloseFlyingWindow::Launch(const FLinearColor& Color,const FVector& Velocity)
{
 SetActorHiddenInGame(false);
 // Windows collide with the room, but not each other or the player: avoid quadratic pile collisions.
 Body->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
 Body->SetEnableGravity(true);Body->WakeAllRigidBodies();
 Body->SetPhysicsLinearVelocity(Velocity);
 Body->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-80.f,80.f),FMath::FRandRange(-40.f,40.f),FMath::FRandRange(-120.f,120.f)));
}
