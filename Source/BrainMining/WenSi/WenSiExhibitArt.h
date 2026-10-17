#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace WenSiExhibitArt
{
 // Keep the original visible-flagged proxy parts for all existing collision,
 // bounds and contact rules, but render only the authored model. No new
 // collider is introduced by the art and its bounds stay inside the proxies.
 inline void Apply(UStaticMeshComponent* Visual,const TArray<TObjectPtr<UStaticMeshComponent>>& Parts,UStaticMesh* Mesh,const FRotator& Rotation,bool bUniform,bool bGhost,UMaterialInterface* Ghost,UMaterialInterface* Rim)
 {
  const bool bUseArt=Mesh!=nullptr;
  FBox Proxy(ForceInit);
  for(auto Part:Parts)
  {
   Part->SetRenderInMainPass(!bUseArt);Part->SetRenderInDepthPass(!bUseArt);
   Part->SetCastShadow(!bUseArt);Part->SetVisibleInRayTracing(!bUseArt);
   if(bUseArt)Part->SetOverlayMaterial(nullptr);
   if(Part->GetStaticMesh())Proxy+=Part->GetStaticMesh()->GetBoundingBox().TransformBy(Part->GetRelativeTransform());
  }
  Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Visual->SetStaticMesh(Mesh);Visual->SetVisibility(bUseArt && Proxy.IsValid);
  if(!bUseArt || !Proxy.IsValid)return;
  const FBox Source=Mesh->GetBoundingBox();
  const FVector RotatedSize=Source.TransformBy(FTransform(Rotation)).GetSize().ComponentMax(FVector(.001));
  const FVector Ratio=Proxy.GetSize()/RotatedSize;
  const bool bAxisAligned=FMath::IsNearlyZero(Rotation.Pitch) && FMath::IsNearlyZero(Rotation.Roll) && FMath::IsNearlyZero(FMath::Fmod(Rotation.Yaw,90.f),.001f);
  const FVector Scale=bUniform || !bAxisAligned ? FVector(Ratio.GetMin()) : Rotation.UnrotateVector(Ratio).GetAbs();
  Visual->SetRelativeRotation(Rotation);Visual->SetRelativeScale3D(Scale);
  FVector Center=Proxy.GetCenter();
  // Sculptures should stand on the original plinth even when their narrower
  // aspect ratio leaves space above them inside the gameplay envelope.
  Center.Z=Proxy.Min.Z+Source.TransformBy(FTransform(Rotation,FVector::ZeroVector,Scale)).GetExtent().Z;
  Visual->SetRelativeLocation(Center-Rotation.RotateVector(Source.GetCenter()*Scale));
  for(int32 I=0;I<Mesh->GetStaticMaterials().Num();++I)
  {
   UMaterialInterface* Material=Mesh->GetMaterial(I);
   if(bGhost && Ghost)
   {
    auto Transparent=UMaterialInstanceDynamic::Create(Ghost,Visual);
    Transparent->SetVectorParameterValue(TEXT("Color"),FLinearColor(.68f,.72f,.78f,.25f));
    Transparent->SetScalarParameterValue(TEXT("Opacity"),.25f);Material=Transparent;
   }
   Visual->SetMaterial(I,Material);
  }
  if(Rim && !Cast<UMaterialInstanceDynamic>(Visual->GetOverlayMaterial()))Visual->SetOverlayMaterial(UMaterialInstanceDynamic::Create(Rim,Visual));
 }

 inline void Highlight(UStaticMeshComponent* Visual,float Strength,float Width,const FLinearColor& Tint)
 {
  if(auto Glow=Cast<UMaterialInstanceDynamic>(Visual->GetOverlayMaterial()))
  {
   Glow->SetScalarParameterValue(TEXT("Strength"),Strength);Glow->SetScalarParameterValue(TEXT("OutlineWidth"),Width);Glow->SetVectorParameterValue(TEXT("Tint"),Tint);
  }
 }
}
