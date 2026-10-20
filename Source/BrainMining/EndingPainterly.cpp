#include "EndingEmergence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

float AEndingEmergence::PaintWeightForStage(int32 InGate,int32 InShot,bool EnteringBrain,float CellWeight,float BloodWeight)
{
 if(InShot>=0 || EnteringBrain || InGate<0 || InGate>1)return 0;
 return FMath::Clamp(InGate==0?CellWeight:BloodWeight,0.f,1.f);
}

void AEndingEmergence::UpdatePainterly()
{
 ActivePaintWeight=PainterlyMaterial?PaintWeightForStage(GateIndex,Shot,bEnteringBrain,CellPaintWeight,BloodPaintWeight):0;
 if(PainterlyMaterial && bEnteringBrain && GateIndex==1 && Shot<0)
  ActivePaintWeight=BloodPaintWeight*(1.f-FMath::SmoothStep(0.f,1.25f,Age-BrainEntryAt));
 if(!PainterlyMaterial)return;
 // The cell uses the pawn camera; blood flow uses the cinematic camera.
 // Remove each camera's blendable when its painted stage ends.
 auto Apply=[&](UCameraComponent* C,float Weight)
 {
  if(!C)return;
  if(Weight>0)C->AddOrUpdateBlendable(PainterlyMaterial,Weight);
  else C->RemoveBlendable(PainterlyMaterial);
 };
 Apply(Camera?Camera->GetCameraComponent():nullptr,GateIndex==1?ActivePaintWeight:0.f);
 Apply(Player && Player->GetPawn()?Player->GetPawn()->FindComponentByClass<UCameraComponent>():nullptr,GateIndex==0?ActivePaintWeight:0.f);
}
