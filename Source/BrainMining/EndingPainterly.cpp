#include "EndingEmergence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

float AEndingEmergence::PaintWeightForStage(int32 InGate,int32 InShot,bool EnteringBrain,float CellWeight,float BloodWeight)
{
 if(InShot>=0 || EnteringBrain || InGate>1 || InGate<0)return 0;
 return FMath::Clamp(InGate==0?CellWeight:BloodWeight,0.f,1.f);
}

void AEndingEmergence::UpdatePainterly()
{
 ActivePaintWeight=PainterlyMaterial?PaintWeightForStage(GateIndex,Shot,bEnteringBrain,CellPaintWeight,BloodPaintWeight):0;
 if(!PainterlyMaterial)return;
 // The cell uses the pawn camera, the vessel uses the cinematic camera.
 // Remove the effect entirely after the vessel, including film/debug jumps.
 auto Apply=[&](UCameraComponent* C)
 {
  if(!C)return;
  if(ActivePaintWeight>0)C->AddOrUpdateBlendable(PainterlyMaterial,ActivePaintWeight);
  else C->RemoveBlendable(PainterlyMaterial);
 };
 Apply(Camera?Camera->GetCameraComponent():nullptr);
 Apply(Player && Player->GetPawn()?Player->GetPawn()->FindComponentByClass<UCameraComponent>():nullptr);
}
