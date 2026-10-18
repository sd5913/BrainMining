#include "EndingEmergence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

float AEndingEmergence::PaintWeightForStage(int32 InGate,int32 InShot,bool EnteringBrain,float CellWeight,float BloodWeight)
{
 if(InShot>=0 || EnteringBrain || InGate!=0)return 0;
 return FMath::Clamp(CellWeight,0.f,1.f);
}

void AEndingEmergence::UpdatePainterly()
{
 ActivePaintWeight=PainterlyMaterial?PaintWeightForStage(GateIndex,Shot,bEnteringBrain,CellPaintWeight,BloodPaintWeight):0;
 if(!PainterlyMaterial)return;
 // Only the opening cell is painted; vessel, brain and film cameras stay clear.
 auto Apply=[&](UCameraComponent* C)
 {
  if(!C)return;
  if(ActivePaintWeight>0)C->AddOrUpdateBlendable(PainterlyMaterial,ActivePaintWeight);
  else C->RemoveBlendable(PainterlyMaterial);
 };
 // This camera never renders the playable cell. Do not seed it with the cell shader.
 if(Camera)Camera->GetCameraComponent()->RemoveBlendable(PainterlyMaterial);
 Apply(Player && Player->GetPawn()?Player->GetPawn()->FindComponentByClass<UCameraComponent>():nullptr);
}
