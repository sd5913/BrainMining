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
    // Restore the original layered composition on serialized maps as well.
    Parts.Empty();NebulaUniverse->GetChildrenComponents(false,Parts);
    int NebulaLayer=0;
    for(auto* Part:Parts)
    {
        FVector P=Part->GetRelativeLocation();
        if(Part->GetName().StartsWith(TEXT("DeepStarField")))
        {const int L=FMath::RoundToInt((-P.X-19000)/4300);P.Y=(L%2?1:-1)*12500;Part->SetRelativeLocation(P);}
        else if(Part->GetName().StartsWith(TEXT("ParallaxNebula")))
        {
            const int L=NebulaLayer++;
            // Put the first sheet behind the arriving camera. The pullback must
            // leave the solar system before it physically reaches the nebula.
            P.X=-16700-L*4300;P.Y=(L%2?1:-1)*(6200+L*1400);Part->SetRelativeLocation(P);
        }
    }
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_ScaleZoom")))
    {
        ScaleZoomMaterial=UMaterialInstanceDynamic::Create(Base,this);
        Camera->GetCameraComponent()->AddOrUpdateBlendable(ScaleZoomMaterial,1);
        ScaleBridge=NewObject<UTextureRenderTarget2D>(this);
        // The bridge is heavily blurred and lives for less than half a second.
        ScaleBridge->InitCustomFormat(640,360,PF_FloatRGBA,true);ScaleBridge->UpdateResourceImmediate(true);
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
    CelestialAge=0;bScaleBridgeReady=false;bScaleBridgePrepared=false;LastCellReveal=-1;
    FilmCameraVelocity=TitleCameraVelocity=FVector::ZeroVector;
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

void AEndingEmergence::UpdateCosmicRail()
{
    // One C1-continuous rail through the solar system, cell and cell cloud.
    // Interpolating log distance keeps apparent retreat alive at every scale.
    const FVector Points[]={FVector(-14500,-1800,4300),FVector(-42000,-4000,10500),FVector(-5200,-250,900)*32,FVector(-760000,-135000,230000)};
    const int Segment=FMath::Clamp(Shot-4,0,2);
    const double Span=Duration(Shot),U=FMath::Clamp(double(ShotTime)/Span,0.,1.);
    double LogR[4],Slope[3],Tangent[4];
    for(int I=0;I<4;++I)LogR[I]=FMath::Loge(Points[I].Size());
    for(int I=0;I<3;++I)Slope[I]=(LogR[I+1]-LogR[I])/Duration(I+4);
    Tangent[0]=Slope[0];Tangent[3]=Slope[2];
    for(int I=1;I<3;++I)Tangent[I]=2*Slope[I-1]*Slope[I]/(Slope[I-1]+Slope[I]);
    const double Radius=FMath::Exp(FMath::CubicInterp(LogR[Segment],Tangent[Segment]*Span,LogR[Segment+1],Tangent[Segment+1]*Span,U));
    const double Ease=U*U*(3-2*U);
    const FVector Direction=FMath::Lerp(Points[Segment].GetSafeNormal(),Points[Segment+1].GetSafeNormal(),Ease).GetSafeNormal();
    const float Elapsed=ShotTime+(Shot>=5?Duration(4):0)+(Shot>=6?Duration(5):0);
    const float W=(bComfortMode?0:FMath::Clamp(Weightlessness,0.f,1.f))*(1-FMath::SmoothStep(0.f,Duration(4)+Duration(5)+Duration(6),Elapsed));
    const FRotator Float(FMath::Sin(Elapsed*.13f)*.4f*W,FMath::Sin(Elapsed*.19f)*.6f*W,0);
    const FVector Cosmos(95000,0,0);
    const FVector SolarEarth(FMath::Cos(2*.93f+.35f)*2400*.18f,FMath::Cos(2*.93f+.35f)*2400,FMath::Sin(2*.93f+.35f)*2400*.55f);
    const FVector Look=SolarEarth*(1-FMath::SmoothStep(Duration(4),Duration(4)+Duration(5)+Duration(6),Elapsed));
    Pose(Cosmos+Float.RotateVector(Direction*Radius),Cosmos+Look,62,FMath::Sin(Elapsed*.16f)*3.5f*W);
}

void AEndingEmergence::UpdateScaleMotion(float Dt)
{
    // The visible cytoplasm shares the main membrane's 3500:4200:3300 axes.
    // Ease the art only once the solar-system shot is over; keep its camera rail.
    if(Shot>=5&&Shot<=8)
    {
        const float Shape=Shot==5?FMath::SmoothStep(0.f,4.f,ShotTime):1.f;
        Universe->SetRelativeScale3D(FMath::Lerp(FVector::OneVector,FVector(1,4200.f/3500.f,3300.f/3500.f),Shape));
    }
    if(Shot>=3&&Shot<=6)CelestialAge+=Dt;
    const FVector EarthOrbit(4000*(FMath::Cos(CelestialAge*.012f)-1),4000*FMath::Sin(CelestialAge*.012f),30*FMath::Sin(CelestialAge*.25f));
    for(auto Part:EarthMovingParts)if(Part)
    {
        Part->SetRelativeLocation(EarthOrbit);
        if(auto* M=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))M->SetScalarParameterValue(TEXT("Spin"),CelestialAge*.012f);
    }
    for(int I=0;I<OrbitingBodies.Num();++I)
    {
        const FVector Rest=OrbitRestPositions[I];const float R=FMath::Sqrt(Rest.Y*Rest.Y+FMath::Square(Rest.Z/.55f));
        const float A=FMath::Atan2(Rest.Z/.55f,Rest.Y)+CelestialAge*.12f/(1+I*.24f);
        OrbitingBodies[I]->SetRelativeLocation(FVector(FMath::Cos(A)*R*.18f,FMath::Cos(A)*R,FMath::Sin(A)*R*.55f));
        if(auto* M=Cast<UMaterialInstanceDynamic>(OrbitingBodies[I]->GetMaterial(0)))M->SetScalarParameterValue(TEXT("Spin"),CelestialAge*(.012f+I*.0012f));
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
    const float Exit=Shot>=1&&Shot<=3?FMath::SmoothStep(0.f,.5f,ShotTime-Duration(Shot)+.5f):0;
    const float ArrivalDuration=Shot==0?.45f:.36f;
    const float Arrival=(Shot==0||(Shot>=2&&Shot<=4))?1-FMath::SmoothStep(0.f,ArrivalDuration,ShotTime):0;
    // A real spatial lift and dolly, with the lens held fixed. The old scene
    // recedes beneath the rising camera; the next scene starts closer/lower
    // and carries the same outward/upward movement into its authored rail.
    // Screen-space effects below only supply radial shutter blur, never a Y pan.
    if(Shot>=1&&Shot<=4&&(Exit>0||Arrival>0))
    {
        const FVector Eye=Camera->GetActorLocation(),Forward=Camera->GetActorForwardVector();
        const FVector LocalAim=Shot==1?FVector(22100,0,75):Shot==2?FVector(37000,0,-1000):Shot==3?FVector(50000,0,0):FVector(95000,0,0);
        const FVector Aim=GetActorTransform().TransformPosition(LocalAim);
        const double Depth=FVector::Dist(Eye,Aim);
        const FVector Up=GetActorUpVector();
        // A pure dolly on arrival preserves the authored pitch: dropping the
        // camera and re-aiming it flattened the continent before it recovered.
        const FVector Move=(-Forward*.28+Up*.25)*(Depth*Exit*Exit)+Forward*(.22*Depth*(Shot>=2?Arrival:0));
        const FRotator Before=Camera->GetActorRotation();
        FRotator Facing=(Eye+Forward*Depth-(Eye+Move)).Rotation();Facing.Roll=Before.Roll;
        Camera->SetActorLocationAndRotation(Eye+Move,Facing);
    }
    // Start the bridge as soon as a scale shot begins and keep it strong
    // enough to smear the previous scene before its geometry becomes legible.
    const float Strength=FMath::Max(Exit,Arrival)*(bComfortMode?.35f:1.f)*2.5f;
    ScaleZoomMaterial->SetScalarParameterValue(TEXT("Strength"),Strength);
    ScaleZoomMaterial->SetScalarParameterValue(TEXT("Bridge"),bScaleBridgeReady?Arrival*Arrival*(Shot==0?1.f:.62f):0);
    ScaleZoomMaterial->SetScalarParameterValue(TEXT("Travel"),FMath::Clamp(ShotTime/ArrivalDuration,0.f,1.f));
    ScaleZoomMaterial->SetScalarParameterValue(TEXT("Handoff"),Exit);
    ScaleZoomMaterial->SetScalarParameterValue(TEXT("Rise"),Shot>=2&&Shot<=4?Arrival:0);
    if(Shot>=0&&Shot<=4)
    {
        // Keep the authored lens. Widening then rapidly narrowing it on arrival
        // magnified the new scene and visually cancelled the outward camera rail.
        auto& PP=Camera->GetCameraComponent()->PostProcessSettings;
        PP.bOverride_MotionBlurAmount=true;PP.MotionBlurAmount=FMath::Clamp(Strength*.2f,0.f,.45f);
    }
    else if(Camera)
    {
        Camera->GetCameraComponent()->PostProcessSettings.MotionBlurAmount=0;
    }
    if(Arrival==0&&!bScaleBridgePrepared)bScaleBridgeReady=false;
    // Queue the old exposure before the cut. No readback, resource allocation,
    // or render-thread flush occurs on the frame that reveals the next scene.
    if(Shot>=1&&Shot<=3&&ShotTime>=Duration(Shot)-.20f&&!bScaleBridgePrepared)
    {CaptureScaleBridge();bScaleBridgePrepared=true;}
}
