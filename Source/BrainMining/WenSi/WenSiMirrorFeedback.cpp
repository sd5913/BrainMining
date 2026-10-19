#include "WenSi/WenSiMirrorPuzzle.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"

void AWenSiMirrorExhibit::BuildMirrorPresence()
{
 MirrorPresence=GetWorld()->SpawnActor<AActor>();
 if(!MirrorPresence)return;
 MirrorPresence->SetOwner(this);
 auto Root=NewObject<USceneComponent>(MirrorPresence);MirrorPresence->SetRootComponent(Root);Root->RegisterComponent();
 auto Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 auto Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WenSi/Mirror/Materials/M_MirrorMiner.M_MirrorMiner"));
 if(!Material)Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WenSi/Museum/Materials/MI_Limestone.MI_Limestone"));
 auto Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
 auto Cone=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cone.Cone"));
 const FLinearColor Steel(.39f,.48f,.52f),Wood(.30f,.15f,.055f);
 auto Shape=[&](const TCHAR* Name,UStaticMesh* Mesh,FVector Position,FVector Size,FLinearColor Tint,FRotator Rotation=FRotator::ZeroRotator)
 {
  auto Part=NewObject<UStaticMeshComponent>(MirrorPresence,FName(Name));Part->SetupAttachment(Root);
  Part->SetStaticMesh(Mesh);Part->SetRelativeLocationAndRotation(Position,Rotation);Part->SetRelativeScale3D(Size/100.f);
  Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetCastShadow(false);Part->SetVisibleInRayTracing(false);
  Part->SetVisibleInSceneCaptureOnly(true);
  if(Material){auto Paint=UMaterialInstanceDynamic::Create(Material,this);Paint->SetVectorParameterValue(TEXT("Tint"),Tint);Part->SetMaterial(0,Paint);}
  Part->RegisterComponent();PresenceParts.Add(Part);return Part;
 };
 auto Glow=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WenSi/Mirror/Materials/M_MirrorEgg.M_MirrorEgg"));
 auto Egg=[&](const TCHAR* Name,FVector Position,FVector Size,float Strength,float Opacity)
 {
  auto Part=Shape(Name,Sphere,Position,Size,FLinearColor(1.f,.88f,.64f));
  if(Glow){auto Paint=UMaterialInstanceDynamic::Create(Glow,this);Paint->SetScalarParameterValue(TEXT("Strength"),Strength);Paint->SetScalarParameterValue(TEXT("SoftOpacity"),Opacity);Part->SetMaterial(0,Paint);}
 };
 // A soft, anonymous egg of light carries the tool. The outer shell feathers
 // into the reflection; there are no facial features, clothes or limbs.
 Egg(TEXT("MirrorEggCore"),FVector(0,0,-2),FVector(104,108,120),6.f,.8f);
 Egg(TEXT("MirrorEggHalo"),FVector(0,0,-2),FVector(124,128,140),2.f,.22f);
 Egg(TEXT("MirrorEggGrip"),FVector(20,66,-12),FVector(18),4.f,.7f);
 Shape(TEXT("MinerPickaxeHandle"),Cylinder,FVector(23,69,4),FVector(5,5,94),Wood);
 Shape(TEXT("MinerPickaxeHead"),Cube,FVector(23,69,49),FVector(10,43,10),Steel);
 Shape(TEXT("MinerPickaxeTipLeft"),Cone,FVector(23,40,45),FVector(11,11,26),Steel,FRotator(0,0,110));
 Shape(TEXT("MinerPickaxeTipRight"),Cone,FVector(23,98,45),FVector(11,11,26),Steel,FRotator(0,0,-110));

}

void AWenSiMirrorExhibit::EndPlay(const EEndPlayReason::Type Reason)
{
 if(MirrorPresence)MirrorPresence->Destroy();
 Super::EndPlay(Reason);
}

void AWenSiMirrorExhibit::UpdateConductive(float Dt)
{
 const bool Active=Word==EWenSiMirrorWord::Conductive && !IsHidden();
 if(!Active){for(auto Arc:ConductiveArcs)Arc->SetVisibility(false);return;}
 ConductiveClock+=Dt;
 if(ConductiveFX && !ConductiveFX->IsActive())ConductiveFX->Activate(true);
 if(ConductiveArcs.IsEmpty())
 {
  auto Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
  auto Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WenSi/Mirror/Materials/M_ConductiveArc"));
  for(int32 I=0;I<18;++I)
  {
   auto Arc=NewObject<UStaticMeshComponent>(this);Arc->SetupAttachment(RootComponent);Arc->SetStaticMesh(Cylinder);
   Arc->SetCollisionEnabled(ECollisionEnabled::NoCollision);Arc->SetCastShadow(false);Arc->SetMaterial(0,Material);Arc->RegisterComponent();ConductiveArcs.Add(Arc);
  }
 }
 FBox Box(ForceInit);
 for(auto Part:Parts)if(Part->IsVisible() && Part->GetStaticMesh())Box+=Part->Bounds.GetBox();
 if(!Box.IsValid)return;
 const FVector C=Box.GetCenter(),E=Box.GetExtent()+FVector(3.f);
 const int32 Pulse=FMath::FloorToInt(ConductiveClock*9.f);
 for(int32 Bolt=0;Bolt<3;++Bolt)
 {
  FRandomStream Random(Pulse*193+Bolt*71+Kind*13);
  FVector Last;
  for(int32 J=0;J<=6;++J)
  {
   const float T=J/6.f;
   FVector P=C;
   if(Bolt==0)P+=FVector(E.X, FMath::Lerp(-E.Y*.8f,E.Y*.8f,T), E.Z*(.2f+Random.FRandRange(-.22f,.22f)));
   if(Bolt==1)P+=FVector(FMath::Lerp(-E.X*.75f,E.X*.75f,T),-E.Y,E.Z*(.45f+Random.FRandRange(-.2f,.2f)));
   if(Bolt==2)P+=FVector(E.X*Random.FRandRange(-.3f,.3f),FMath::Lerp(-E.Y*.7f,E.Y*.7f,T),E.Z);
   if(J)
   {
    auto Arc=ConductiveArcs[Bolt*6+J-1];const FVector D=P-Last;
    Arc->SetWorldLocationAndRotation((Last+P)*.5f,FRotationMatrix::MakeFromZ(D).Rotator());
    Arc->SetWorldScale3D(FVector(.012f,.012f,D.Size()/100.f));
    Arc->SetVisibility((Pulse+Bolt)%4!=0);
   }
   Last=P;
  }
 }
}
