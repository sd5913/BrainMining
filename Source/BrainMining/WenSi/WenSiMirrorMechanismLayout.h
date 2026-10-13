#pragma once
#include "WenSi/WenSiMirrorPuzzle.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"

namespace WenSiMuseumMechanisms
{
 constexpr float BlockRestZ=89.f;
 constexpr float BalanceTravel=24.f;
 inline void CylinderBetween(AActor* Actor,const FVector& Start,const FVector& End,float Diameter)
 {
  const FVector Delta=End-Start;
  Actor->SetActorLocation((Start+End)*.5f);
  Actor->SetActorRotation(FRotationMatrix::MakeFromZ(Delta).Rotator());
  Actor->SetActorScale3D(FVector(Diameter,Diameter,Delta.Size())/100.f);
 }
 inline void UpdateBalance(UWorld* World,float Alpha,FVector Origin=FVector::ZeroVector)
 {
  Alpha=FMath::Clamp(Alpha,0.f,1.f);
  const float Drop=BalanceTravel*Alpha;
  const float Angle=FMath::RadiansToDegrees(FMath::Asin(Drop/90.f));
  const float Offset=90.f*FMath::Cos(FMath::DegreesToRadians(Angle));
  for(TActorIterator<AActor> It(World);It;++It)
  {
   if(It->ActorHasTag(TEXT("WenSiBalanceBeam")))It->SetActorRotation(FRotator(Angle,0,0));
   else if(It->ActorHasTag(TEXT("WenSiBalanceLeftTray")))It->SetActorLocation(Origin+FVector(1500,-430,44-Drop));
   else if(It->ActorHasTag(TEXT("WenSiBalanceRightTray")))It->SetActorLocation(Origin+FVector(1680,-430,44+Drop));
   else if(It->ActorHasTag(TEXT("WenSiBalanceCounterweight")))It->SetActorLocation(Origin+FVector(1680,-430,66+Drop));
   else if(It->ActorHasTag(TEXT("WenSiBalanceButton")))It->SetActorLocation(Origin+FVector(1500,-430,18.f-FMath::Max(0.f,Drop-14.f)));
   else if(It->ActorHasTag(TEXT("WenSiBalanceLeftSpreader")))It->SetActorLocation(Origin+FVector(1500,-430,170-Drop));
   else if(It->ActorHasTag(TEXT("WenSiBalanceRightSpreader")))It->SetActorLocation(Origin+FVector(1680,-430,170+Drop));
   else
    for(int32 Side=0;Side<2;++Side)for(int32 Edge=0;Edge<2;++Edge)
     if(It->ActorHasTag(*FString::Printf(TEXT("WenSiBalanceHanger%d%d"),Side,Edge)))
     {
      const float Sign=Side?1.f:-1.f;
      CylinderBetween(*It,Origin+FVector(1590+Sign*Offset,Edge?-379:-481,170+Sign*Drop),Origin+FVector(Side?1680:1500,Edge?-379:-481,49+Sign*Drop),2.2f);
     }
  }
 }

#if WITH_EDITOR
 inline void Build(UWorld* World)
 {
  auto Mat=[](const TCHAR* Name){return LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/WenSi/Museum/Materials/"))+Name+TEXT(".")+Name));};
  auto Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
  auto Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
  auto Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
  auto Brass=Mat(TEXT("MI_Brass"));auto Dark=Mat(TEXT("MI_Charcoal"));auto Blue=Mat(TEXT("MI_DeepBlue"));auto Red=Mat(TEXT("MI_Burgundy"));
  auto ButtonRed=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WenSi/Mirror/Materials/MI_BalanceButtonRed.MI_BalanceButtonRed"));if(!ButtonRed)ButtonRed=Red;
  int32 Number=0;
  auto Part=[&](const TCHAR* Name,FVector P,FVector Size,UStaticMesh* Mesh,UMaterialInterface* Material,const TCHAR* Role=nullptr,bool Solid=false)
  {
   auto Actor=World->SpawnActor<AStaticMeshActor>();Actor->SetActorLabel(FString::Printf(TEXT("%s_%03d"),Name,++Number));Actor->SetFolderPath(TEXT("MuseumMirror/BalanceAndCircuit"));
   Actor->Tags.Add(TEXT("WenSiMechanismRevision2"));if(Role)Actor->Tags.Add(Role);
   auto Component=Actor->GetStaticMeshComponent();Component->SetMobility(EComponentMobility::Movable);Component->SetStaticMesh(Mesh);Component->SetMaterial(0,Material);Component->SetCollisionProfileName(Solid?TEXT("BlockAll"):TEXT("NoCollision"));Component->SetCastShadow(true);
   Actor->SetActorLocation(P);Actor->SetActorScale3D(Size/100.f);return Actor;
  };
  Part(TEXT("BalanceBase"),FVector(1590,-430,10),FVector(110,100,20),Cube,Blue,nullptr,true);
  Part(TEXT("BalanceColumn"),FVector(1590,-430,90),FVector(18,18,160),Cylinder,Brass,nullptr,true);
  Part(TEXT("BalancePivot"),FVector(1590,-430,170),FVector(26),Sphere,Brass);
  Part(TEXT("BalanceBeam"),FVector(1590,-430,170),FVector(220,16,12),Cube,Brass,TEXT("WenSiBalanceBeam"));
  Part(TEXT("BalanceSpreader"),FVector(1500,-430,170),FVector(12,120,8),Cube,Brass,TEXT("WenSiBalanceLeftSpreader"));
  Part(TEXT("BalanceSpreader"),FVector(1680,-430,170),FVector(12,120,8),Cube,Brass,TEXT("WenSiBalanceRightSpreader"));
  Part(TEXT("BalanceLeftTray"),FVector(1500,-430,44),FVector(112,112,10),Cylinder,Brass,TEXT("WenSiBalanceLeftTray"));
  Part(TEXT("BalanceRightTray"),FVector(1680,-430,44),FVector(112,112,10),Cylinder,Brass,TEXT("WenSiBalanceRightTray"));
  Part(TEXT("BalanceCounterweight"),FVector(1680,-430,66),FVector(54,54,34),Cylinder,Dark,TEXT("WenSiBalanceCounterweight"));
  for(int32 Side=0;Side<2;++Side)for(int32 Edge=0;Edge<2;++Edge)
  {
   const FString Role=FString::Printf(TEXT("WenSiBalanceHanger%d%d"),Side,Edge);
   auto Actor=Part(TEXT("BalanceSuspension"),FVector::ZeroVector,FVector::OneVector,Cylinder,Brass,*Role);
   CylinderBetween(Actor,FVector(Side?1680:1500,Edge?-379:-481,170),FVector(Side?1680:1500,Edge?-379:-481,49),2.2f);
  }
  // The lower ring has a real central opening around the taller red cylinder.
  Part(TEXT("BalanceRedButton"),FVector(1500,-430,18),FVector(36,36,14),Cylinder,ButtonRed,TEXT("WenSiBalanceButton"));
  for(int32 I=0;I<32;++I)
  {
   const float Angle=I*360.f/32.f;const float Radians=FMath::DegreesToRadians(Angle);
   auto Segment=Part(TEXT("BalanceButtonRing"),FVector(1500+24.f*FMath::Cos(Radians),-430+24.f*FMath::Sin(Radians),6),FVector(6,5,6),Cube,Brass,TEXT("WenSiBalanceButtonRing"));
   Segment->SetActorRotation(FRotator(0,Angle,0));
  }
  Part(TEXT("ButtonWireSocket"),FVector(1500,-453,6),FVector(12,8,8),Cube,Dark);
  // Wall outlet and an inserted plug replace the battery and all polarity markings.
  Part(TEXT("WallOutletFrame"),FVector(1880,641,130),FVector(66,6,84),Cube,Dark);
  Part(TEXT("WallOutlet"),FVector(1880,637,130),FVector(58,8,76),Cube,Mat(TEXT("MI_Limestone")),TEXT("WenSiWallOutlet"));
  for(float X:{1871.f,1889.f})
  {
   Part(TEXT("OutletOpening"),FVector(X,632,137),FVector(4,2,15),Cube,Dark);
   Part(TEXT("PlugProng"),FVector(X,630,137),FVector(4,12,13),Cube,Brass);
  }
  Part(TEXT("WallPlug"),FVector(1880,613,130),FVector(38,34,46),Cube,Dark,TEXT("WenSiWallPlug"));
  Part(TEXT("PlugFace"),FVector(1880,595,130),FVector(34,3,40),Cube,Blue);
  auto Relief=Part(TEXT("PlugCordRelief"),FVector::ZeroVector,FVector::OneVector,Cylinder,Dark);CylinderBetween(Relief,FVector(1880,594,130),FVector(1880,576,130),10.f);
  auto Cable=[&](const TCHAR* Name,const TArray<FVector>& Points,UMaterialInterface* Material,const TCHAR* Role)
  {
   for(int32 I=1;I<Points.Num();++I)
   {
    auto Actor=Part(Name,FVector::ZeroVector,FVector::OneVector,Cylinder,Material,Role);CylinderBetween(Actor,Points[I-1],Points[I],4.5f);
    if(I<Points.Num()-1)Part(TEXT("CableBend"),Points[I],FVector(4.7f),Sphere,Material,Role);
   }
  };
  Cable(TEXT("BalanceSignalWire"),{FVector(1500,-453,6),FVector(1500,-535,6),FVector(2120,-535,6),FVector(2155,-395,6),FVector(2155,-395,145),FVector(2180,-395,145)},Dark,TEXT("WenSiBalanceCable"));
  Cable(TEXT("CircuitOutputWire"),{FVector(1900,148,80),FVector(2020,148,80),FVector(2020,-305,6),FVector(2140,-305,6),FVector(2140,-305,145),FVector(2180,-305,145)},Red,TEXT("WenSiPowerCable"));
  Cable(TEXT("PlugSupplyWire"),{FVector(1880,576,130),FVector(1880,545,110),FVector(1880,520,6),FVector(1810,520,6),FVector(1810,92,6),FVector(1900,92,80)},Red,TEXT("WenSiPlugSupplyCable"));
  for(TActorIterator<AWenSiMirrorExhibit> It(World);It;++It)if(It->Kind==6)It->SetActorLocation(FVector(1500,-430,BlockRestZ));
 }
#endif
}
