#include "EndingEmergence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"

void AEndingEmergence::InitializeScaleMotion()
{
    EarthMovingParts.Empty();OrbitingBodies.Empty();OrbitRestPositions.Empty();OrbitingRing=nullptr;
    BrainDecorationBatches.Empty();CellRevealMaterials.Empty();LastCellReveal=-1;
    TArray<USceneComponent*> BrainParts;Sets[0]->GetChildrenComponents(false,BrainParts);
    for(auto* Part:BrainParts)if(Part->GetName().StartsWith(TEXT("BackgroundNeuron"))||Part->GetName().StartsWith(TEXT("BackgroundDendrites"))||Part->GetName().StartsWith(TEXT("BackgroundSynapses")))
        if(auto* Mesh=Cast<UStaticMeshComponent>(Part))BrainDecorationBatches.Add(Mesh);
    for(auto* Root:{Cell.Get(),OtherCells.Get()})
    {
        TArray<USceneComponent*> CellParts;Root->GetChildrenComponents(true,CellParts);
        for(auto* Part:CellParts)if(auto* Mesh=Cast<UStaticMeshComponent>(Part))if(auto* Mat=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))CellRevealMaterials.Add(Mat);
    }
    TArray<USceneComponent*> Parts;Sets[2]->GetChildrenComponents(false,Parts);
    for(auto* Part:Parts)if(auto* Mesh=Cast<UStaticMeshComponent>(Part))
        if(!Mesh->GetName().StartsWith(TEXT("EarthDistantStars"))&&!Mesh->GetName().StartsWith(TEXT("EarthLights")))EarthMovingParts.Add(Mesh);
    Parts.Empty();SolarSystem->GetChildrenComponents(false,Parts);
    for(auto* Part:Parts)if(auto* Mesh=Cast<UStaticMeshComponent>(Part))
    {
        if(Mesh->GetName().StartsWith(TEXT("SolarPlanet"))){OrbitingBodies.Add(Mesh);OrbitRestPositions.Add(Mesh->GetRelativeLocation());}
        else if(Mesh->GetName().StartsWith(TEXT("SaturnRings")))OrbitingRing=Mesh;
    }
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_ScaleZoom")))
    {
        ScaleZoomMaterial=UMaterialInstanceDynamic::Create(Base,this);
        Camera->GetCameraComponent()->AddOrUpdateBlendable(ScaleZoomMaterial,1);
        ScaleBridge=NewObject<UTextureRenderTarget2D>(this);
        ScaleBridge->InitCustomFormat(960,540,PF_FloatRGBA,true);ScaleBridge->UpdateResourceImmediate(true);
        ScaleZoomMaterial->SetTextureParameterValue(TEXT("PreviousFrame"),ScaleBridge);
        ScaleCapture=NewObject<USceneCaptureComponent2D>(this,NAME_None,RF_Transient);
        ScaleCapture->bCaptureEveryFrame=false;ScaleCapture->bCaptureOnMovement=false;
        ScaleCapture->bAlwaysPersistRenderingState=true;ScaleCapture->TextureTarget=ScaleBridge;
        ScaleCapture->CaptureSource=ESceneCaptureSource::SCS_FinalToneCurveHDR;
        ScaleCapture->RegisterComponent();
    }
    ResetScaleMotion();
}

void AEndingEmergence::CaptureScaleBridge()
{
    bScaleBridgeReady=false;
    if(!ScaleBridge||!ScaleZoomMaterial||!Camera||!ScaleCapture)return;
    ScaleCapture->FOVAngle=Camera->GetCameraComponent()->FieldOfView;
    ScaleCapture->PostProcessSettings=Camera->GetCameraComponent()->PostProcessSettings;
    // Do not feed the transition texture back into itself.
    ScaleCapture->PostProcessSettings.WeightedBlendables.Array.Empty();
    ScaleCapture->SetWorldTransform(Camera->GetActorTransform());
    ScaleCapture->CaptureScene();bScaleBridgeReady=true;
}

void AEndingEmergence::ResetScaleMotion()
{
    CelestialAge=0;bScaleBridgeReady=false;LastCellReveal=-1;
    if(ScaleZoomMaterial){ScaleZoomMaterial->SetScalarParameterValue(TEXT("Strength"),0);ScaleZoomMaterial->SetScalarParameterValue(TEXT("Bridge"),0);}
    for(auto Part:EarthMovingParts)if(Part)
    {
        Part->SetRelativeLocation(FVector::ZeroVector);
        if(auto* M=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))M->SetScalarParameterValue(TEXT("Spin"),0);
    }
    for(int I=0;I<OrbitingBodies.Num();++I)if(OrbitingBodies[I])
    {
        OrbitingBodies[I]->SetRelativeLocation(OrbitRestPositions[I]);
        if(auto* M=Cast<UMaterialInstanceDynamic>(OrbitingBodies[I]->GetMaterial(0)))M->SetScalarParameterValue(TEXT("Spin"),0);
    }
    if(OrbitingRing&&OrbitingBodies.IsValidIndex(5))OrbitingRing->SetRelativeLocation(OrbitRestPositions[5]);
}

void AEndingEmergence::UpdateScaleMotion(float Dt)
{
    if(Shot>=3&&Shot<=6)CelestialAge+=Dt;
    const FVector EarthOrbit(4000*(FMath::Cos(CelestialAge*.003f)-1),4000*FMath::Sin(CelestialAge*.003f),30*FMath::Sin(CelestialAge*.09f));
    for(auto Part:EarthMovingParts)if(Part)
    {
        Part->SetRelativeLocation(EarthOrbit);
        if(auto* M=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))M->SetScalarParameterValue(TEXT("Spin"),CelestialAge*.003f);
    }
    for(int I=0;I<OrbitingBodies.Num();++I)
    {
        const FVector Rest=OrbitRestPositions[I];const float R=FMath::Sqrt(Rest.Y*Rest.Y+FMath::Square(Rest.Z/.55f));
        const float A=FMath::Atan2(Rest.Z/.55f,Rest.Y)+CelestialAge*.065f/(1+I*.24f);
        OrbitingBodies[I]->SetRelativeLocation(FVector(FMath::Cos(A)*R*.18f,FMath::Cos(A)*R,FMath::Sin(A)*R*.55f));
        if(auto* M=Cast<UMaterialInstanceDynamic>(OrbitingBodies[I]->GetMaterial(0)))M->SetScalarParameterValue(TEXT("Spin"),CelestialAge*(.003f+I*.0004f));
    }
    if(OrbitingRing&&OrbitingBodies.IsValidIndex(5))OrbitingRing->SetRelativeLocation(OrbitingBodies[5]->GetRelativeLocation());
    // Keep all anatomy out of the longer nebula passage. Reveal the surrounding
    // structures gently only when the next pullback is ready to leave their interior.
    const float CellReveal=Shot==4?0:Shot==5?FMath::SmoothStep(0.f,4.f,ShotTime):1;
    if(Shot>=4&&Shot<=6)for(auto* Root:{Cell.Get(),OtherCells.Get()})
    {
        if(Root->IsVisible()!=(Shot>=5))Root->SetVisibility(Shot>=5,true);
    }
    if(Shot>=4&&Shot<=6&&!FMath::IsNearlyEqual(LastCellReveal,CellReveal))
    {for(auto Mat:CellRevealMaterials)Mat->SetScalarParameterValue(TEXT("Reveal"),CellReveal);LastCellReveal=CellReveal;}
    if(!ScaleZoomMaterial)return;
    const float Exit=Shot>=1&&Shot<=3?FMath::SmoothStep(0.f,1.35f,ShotTime-Duration(Shot)+1.35f):0;
    const float Arrival=(Shot==0||Shot>=2&&Shot<=4)?1-FMath::SmoothStep(0.f,1.25f,ShotTime):0;
    const float Strength=FMath::Max(Exit,Arrival)*(bComfortMode?.35f:1.f);
    ScaleZoomMaterial->SetScalarParameterValue(TEXT("Strength"),Strength);
    ScaleZoomMaterial->SetScalarParameterValue(TEXT("Bridge"),bScaleBridgeReady?Arrival:0);
    ScaleZoomMaterial->SetScalarParameterValue(TEXT("Travel"),FMath::Clamp(ShotTime/1.25f,0.f,1.f));
    if(Shot>=0&&Shot<=4)Camera->GetCameraComponent()->SetFieldOfView(Camera->GetCameraComponent()->FieldOfView+Strength*13);
    if(Arrival==0)bScaleBridgeReady=false;
}
