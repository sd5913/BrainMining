#include "MiningRoomEscape.h"
#include "CardRoomStage.h"
#include "DiggableRock.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"

void AMiningRoomEscape::BuildStudyLayout()
{
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Surface=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Rooms/CardDuel/Materials/M_CardRoomSurface.M_CardRoomSurface"));
    auto* Glow=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Rooms/CardDuel/Materials/M_CardRoomGlow.M_CardRoomGlow"));
    auto Material=[&](FLinearColor Color,bool Emissive=false)
    {
        auto* M=UMaterialInstanceDynamic::Create(Emissive?Glow:Surface,this);
        M->SetVectorParameterValue(TEXT("Color"),Color);return M;
    };
    auto* Dark=Material(FLinearColor(.008f,.018f,.028f));
    auto* Amber=Material(FLinearColor(2,1.1f,.4f),true);
    auto* Wood=Material(FLinearColor(.14f,.065f,.028f));
    auto* Red=Material(FLinearColor(.24f,.018f,.025f));
    auto* Cardboard=Material(FLinearColor(.42f,.28f,.13f));
    auto* Blue=Material(FLinearColor(.015f,.12f,.23f));
    auto* Paper=Material(FLinearColor(.72f,.67f,.48f));
    auto* TealCover=Material(FLinearColor(.24f,.62f,.55f));
    auto* Plaster=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Rooms/CardDuel/Materials/M_Plaster.M_Plaster"));
    auto Shape=[&](FVector P,FVector Size,UMaterialInterface* M,bool Collision=true,FRotator Rotation=FRotator::ZeroRotator)
    {
        auto* C=NewObject<UStaticMeshComponent>(this);AddInstanceComponent(C);C->SetupAttachment(RootComponent);
        C->SetStaticMesh(Cube);C->SetMaterial(0,M);C->SetMobility(EComponentMobility::Movable);
        C->SetCollisionProfileName(Collision?TEXT("BlockAll"):TEXT("NoCollision"));C->SetCastShadow(Collision);
        C->RegisterComponent();C->SetWorldLocationAndRotation(P,Rotation);C->SetWorldScale3D(Size/100);return C;
    };
    auto Text=[&](FVector P,const FString& Value,float Size,FRotator Rotation)
    {
        auto* C=NewObject<UTextRenderComponent>(this);AddInstanceComponent(C);C->SetupAttachment(RootComponent);
        C->SetMobility(EComponentMobility::Movable);
        if(auto* TextMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Rooms/CardDuel/Materials/M_EscapeScreenText.M_EscapeScreenText")))C->SetTextMaterial(TextMaterial);
        C->SetText(FText::FromString(Value));C->SetWorldSize(Size);C->SetHorizontalAlignment(EHTA_Center);
        C->SetTextRenderColor(FColor(80,235,255));C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->RegisterComponent();C->SetWorldLocationAndRotation(P,Rotation);return C;
    };
    auto Light=[&](FVector P,float Power,float Radius)
    {
        auto* C=NewObject<UPointLightComponent>(this);AddInstanceComponent(C);C->SetupAttachment(RootComponent);
        C->SetIntensity(Power);C->SetAttenuationRadius(Radius);C->SetLightColor(FLinearColor(1,.72f,.42f));C->SetCastShadows(false);
        C->RegisterComponent();C->SetWorldLocation(P);return C;
    };
    auto Rock=[&](FVector P,FVector Size,UMaterialInterface* M,FRotator Rotation)
    {
        const FTransform T(Rotation,P);
        auto* R=GetWorld()->SpawnActorDeferred<ADiggableRock>(ADiggableRock::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        R->RockSize=Size;R->RockMaterial=M;R->FinishSpawning(T);GeneratedActors.Add(R);return R;
    };
    // Replace only the rear plaster wall at runtime, using its actual bounds (V08 shortened it).
    FVector WallCenter=FVector(23700,3600,6000)+SourceLevelOffset;
    FVector WallExtent(100,10000,6000);
    for(TActorIterator<AStaticMeshActor> It(GetWorld());It;++It)
    {
        FVector P,E;It->GetActorBounds(false,P,E);
        if(FMath::Abs(P.X-WallCenter.X)<250 && E.Y>9000 && E.Z>5500 && E.X<200)
        {ReplacedWall=*It;WallCenter=P;WallExtent=E;It->SetActorHiddenInGame(true);It->SetActorEnableCollision(false);break;}
    }
    const FVector Impact(WallCenter.X,3600+SourceLevelOffset.Y,9400+SourceLevelOffset.Z);
    const FVector Min=WallCenter-WallExtent,Max=WallCenter+WallExtent;
    // Partition the rear wall around BOTH openings; the TV tunnel stays behind it.
    const float Ys[]={float(Min.Y),float(Impact.Y-1400),float(Impact.Y+1400),float(ComputerEntry.Y-650),float(ComputerEntry.Y+650),float(Max.Y)};
    const float Zs[]={float(Min.Z),float(Impact.Z-1400),float(Impact.Z+1400),float(ComputerEntry.Z-680),float(ComputerEntry.Z+650),float(Max.Z)};
    TArray<float> ZCuts;ZCuts.Append(Zs,UE_ARRAY_COUNT(Zs));ZCuts.Sort();
    for(int32 y=0;y<5;++y)for(int32 z=0;z<ZCuts.Num()-1;++z)
    {
        const float Y=(Ys[y]+Ys[y+1])*.5f,Z=(ZCuts[z]+ZCuts[z+1])*.5f;
        if((FMath::Abs(Y-Impact.Y)<1400 && FMath::Abs(Z-Impact.Z)<1400) ||
           (FMath::Abs(Y-ComputerEntry.Y)<650 && Z>ComputerEntry.Z-680 && Z<ComputerEntry.Z+650))continue;
        Shape(FVector(WallCenter.X,Y,Z),FVector(WallExtent.X*2,Ys[y+1]-Ys[y],ZCuts[z+1]-ZCuts[z]),Plaster);
    }
    BreachWall=Rock(Impact,FVector(WallExtent.X*2,2800,2800),Plaster,FRotator::ZeroRotator);
    CardStage->OpponentExitLocal=CardStage->GetActorTransform().InverseTransformPosition(Impact);
    CardStage->OpponentExitCenterOffset=FVector(0,0,70);
    // A dark cavity behind the real opening makes its depth legible.
    Shape(Impact+FVector(500,0,0),FVector(80,2800,2800),Dark);

    auto WaterActor=[&](FVector P,FVector Size,UMaterialInterface* M)
    {
        auto* A=GetWorld()->SpawnActor<AStaticMeshActor>(P,FRotator::ZeroRotator);
        A->SetMobility(EComponentMobility::Movable);auto* C=A->GetStaticMeshComponent();C->SetStaticMesh(Cube);C->SetMaterial(0,M);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(false);A->SetActorScale3D(Size/100);
        GeneratedActors.Add(A);return A;
    };
    auto* WaterMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Rooms/CardDuel/Materials/M_EscapeWater.M_EscapeWater"));
    if(!WaterMat)WaterMat=Blue;
    Water=WaterActor(FVector(15000,3600,0)+SourceLevelOffset,FVector(17000,19800,60),WaterMat);
    Cast<AStaticMeshActor>(Water)->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
    // One continuous curved surface avoids the seams of overlapping water primitives.
    auto* JetMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Rooms/CardDuel/Materials/M_EscapeJet.M_EscapeJet"));
    JetStream=NewObject<UDynamicMeshComponent>(this);AddInstanceComponent(JetStream);JetStream->SetupAttachment(RootComponent);
    JetStream->SetMaterial(0,JetMat?JetMat:WaterMat);JetStream->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    JetStream->SetCastShadow(false);JetStream->SetCanEverAffectNavigation(false);JetStream->RegisterComponent();JetStream->SetVisibility(false);
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    for(int32 i=0;i<64;++i)
    {
        auto* A=WaterActor(Impact,FVector(100),JetMat?JetMat:WaterMat);
        A->GetStaticMeshComponent()->SetStaticMesh(Sphere);A->SetActorHiddenInGame(true);Foam.Add(A);
    }

    // Hidden before the breach; appear with its water and float throughout the surge.
    // All their parts move together at the saved water height, including on checkpoint retry.
    auto Floating=[&](FVector P,FVector Size,UMaterialInterface* M,FRotator R=FRotator::ZeroRotator)
    {
        auto* C=Shape(P,Size,M,true,R);FloatingProps.Add(C);FloatingOffsets.Add(P.Z-SourceLevelOffset.Z);return C;
    };
    const FVector Pads[]={FVector(13900,6500,80),FVector(15100,7800,80),FVector(16500,9000,80),
        FVector(17900,10200,80),FVector(19400,11400,80),FVector(21100,11750,80)};
    // Leave a forward ascent from the final box; no book directly below the TV shelf.
    for(int32 i=0;i<UE_ARRAY_COUNT(Pads);++i)
    {
        const FVector P=Pads[i]+SourceLevelOffset;const FRotator R(0,i%2?12:-8,0);
        if(i==2 || i==5)
        {
            Floating(P+FVector(0,0,160),FVector(1050,1000,430),Cardboard,R);
            Floating(P+FVector(0,0,378),FVector(1040,100,8),Paper,R);
        }
        else
        {
            auto* Cover=i==4?TealCover:(i%2?Red:Dark);
            Floating(P,FVector(1100,1000,55),Cover,R);
            Floating(P+FVector(0,0,65),FVector(1060,965,80),Paper,R);
            Floating(P+FVector(0,0,118),FVector(1100,1000,25),Cover,R);
            // A pale cover label makes the last book legible from the previous jump.
            if(i==4)
                Floating(P+R.RotateVector(FVector(70,40,133)),FVector(460,360,5),Paper,R);
            // Binding and a pencil lying across a notebook.
            Floating(P+R.RotateVector(FVector(0,-470,70)),FVector(1100,65,160),Wood,R);
            if(i==3)Floating(P+FVector(100,0,150),FVector(820,45,45),Cardboard,FRotator(0,25,0));
        }
    }
    // Wall-hung CRT: dark glass, deep wooden case, speaker grille, tuning knobs and rods.
    const FVector Direction=(TunnelEnd-ComputerEntry).GetSafeNormal();const FRotator Rotation=Direction.Rotation();
    const float Length=FVector::Distance(ComputerEntry,TunnelEnd);
    auto TunnelPoint=[&](FVector Local){return ComputerEntry+Rotation.RotateVector(Local);};
    const float W=TunnelHalfWidth,H=TunnelHalfHeight;
    auto* Walnut=Material(FLinearColor(.28f,.13f,.045f));
    auto* Face=Material(FLinearColor(.48f,.40f,.26f));
    auto* Metal=Material(FLinearColor(.26f,.30f,.31f));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto Round=[&](FVector P,float Radius,float Depth,UMaterialInterface* M,FVector Axis)
    {
        auto* C=Shape(P,FVector(Radius*2,Radius*2,Depth),M,false,FQuat::FindBetweenNormals(FVector::UpVector,Axis.GetSafeNormal()).Rotator());
        C->SetStaticMesh(Cylinder);return C;
    };
    // Wider wood cabinet and a separate cream control panel give the silhouette a CRT identity.
    Shape(ComputerEntry+FVector(180,-685,-15),FVector(820,270,1330),Walnut);
    Shape(ComputerEntry+FVector(180,800,-15),FVector(820,500,1330),Walnut);
    Shape(ComputerEntry+FVector(180,115,585),FVector(820,1870,130),Walnut);
    Shape(ComputerEntry+FVector(180,115,-590),FVector(820,1870,180),Walnut);
    Shape(ComputerEntry+FVector(-280,-685,-15),FVector(110,270,1330),Face);
    Shape(ComputerEntry+FVector(-280,800,-15),FVector(110,500,1330),Face);
    Shape(ComputerEntry+FVector(-280,0,535),FVector(110,1100,230),Face);
    Shape(ComputerEntry+FVector(-280,0,-550),FVector(110,1100,260),Face);
    // Four rounded contours form a deep, bevelled CRT bezel without blocking the dig aperture.
    {
        using namespace UE::Geometry;
        FDynamicMesh3 Mesh(true,false,false,false);
        constexpr int32 CornerSteps=8,PerRing=CornerSteps*4;
        const FVector Contours[]={FVector(-330,655,525),FVector(-395,625,500),FVector(-420,550,420),FVector(-345,550,420)};
        const float Radii[]={160,145,115,115};
        for(int32 Ring=0;Ring<4;++Ring)for(int32 Corner=0;Corner<4;++Corner)for(int32 Step=0;Step<CornerSteps;++Step)
        {
            const float Angle=(Corner*90.f+Step*90.f/(CornerSteps-1))*PI/180;
            const float YSign=(Corner==0 || Corner==3)?1.f:-1.f,ZSign=Corner<2?1.f:-1.f;
            const FVector C=Contours[Ring];const float R=Radii[Ring];
            Mesh.AppendVertex(FVector3d(GetActorTransform().InverseTransformPosition(ComputerEntry+FVector(C.X,YSign*(C.Y-R)+R*FMath::Cos(Angle),ZSign*(C.Z-R)+R*FMath::Sin(Angle)))));
        }
        for(int32 Ring=0;Ring<3;++Ring)for(int32 i=0;i<PerRing;++i)
        {
            const int32 A=Ring*PerRing+i,B=Ring*PerRing+(i+1)%PerRing,C=A+PerRing,D=B+PerRing;
            Mesh.AppendTriangle(A,C,B);Mesh.AppendTriangle(B,C,D);
        }
        FMeshNormals::QuickComputeVertexNormals(Mesh);Mesh.EnableAttributes();
        FMeshNormals::InitializeOverlayToPerVertexNormals(Mesh.Attributes()->PrimaryNormals());
        auto* Bezel=NewObject<UDynamicMeshComponent>(this);AddInstanceComponent(Bezel);Bezel->SetupAttachment(RootComponent);
        Bezel->SetMaterial(0,Dark);Bezel->SetCollisionEnabled(ECollisionEnabled::NoCollision);Bezel->SetCastShadow(false);
        Bezel->SetMesh(MoveTemp(Mesh));Bezel->RegisterComponent();
    }
    // Raised circular tuning knobs, index marks, and a recessed horizontal speaker grille.
    for(float Z:{350.f,50.f})
    {
        Round(ComputerEntry+FVector(-355,800,Z),118,45,Metal,FVector(-1,0,0));
        Round(ComputerEntry+FVector(-395,800,Z),93,65,Dark,FVector(-1,0,0));
        Shape(ComputerEntry+FVector(-432,800,Z+55),FVector(8,12,40),Face,false);
        for(int32 Mark=0;Mark<12;++Mark)
        {
            const float A=Mark*2*PI/12;
            Shape(ComputerEntry+FVector(-340,800+137*FMath::Cos(A),Z+137*FMath::Sin(A)),FVector(8,10,10),Dark,false);
        }
    }
    Shape(ComputerEntry+FVector(-340,800,-295),FVector(18,310,335),Dark,false);
    for(int32 i=0;i<12;++i)Shape(ComputerEntry+FVector(-353,800,-445+i*27),FVector(12,300,8),Metal,false);
    auto* Brand=Text(ComputerEntry+FVector(-340,0,-575),TEXT("AURORA  /  COLOR TV"),32,FRotator(0,180,0));
    Brand->SetTextMaterial(LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/EngineMaterials/DefaultTextMaterialOpaque.DefaultTextMaterialOpaque")));
    Brand->SetTextRenderColor(FColor(65,48,28));
    // Rabbit-ear aerials are distinct from the straight ceiling suspension rods.
    const FVector AerialBase=ComputerEntry+FVector(-90,80,700);
    Round(AerialBase,100,80,Dark,FVector::UpVector);
    for(float Side:{-1.f,1.f})
    {
        const FVector Tip=AerialBase+FVector(0,Side*580,540),D=Tip-AerialBase;
        Round((Tip+AerialBase)*.5f,13,D.Size(),Metal,D);
        auto* Cap=Shape(Tip,FVector(40),Metal,false);Cap->SetStaticMesh(Sphere);
        Shape(ComputerEntry+FVector(300,Side*650,3000),FVector(45,45,4700),Dark);
    }
    Light(ComputerEntry+FVector(-900,-900,700),300000,2300);
    // Top Z=6000: a 20 cm gap below the cabinet, with standing room in front of it.
    // Keep the checkpoint above this deck rather than inside the lower CRT bezel.
    Shape(ComputerEntry+FVector(-400,100,-750),FVector(1000,2100,100),Wood);
    ScreenMaterial=Material(FLinearColor::Black,true);
    TelevisionScreen=Rock(ComputerEntry+FVector(-308,0,0),FVector(100,1100,840),ScreenMaterial,FRotator::ZeroRotator);
    ComputerRocks.Add(TelevisionScreen);
    Beacon=Light(ComputerEntry+FVector(-500,0,400),180000,2200);

    auto* Interior=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Rooms/CardDuel/Materials/M_EscapeTVInterior.M_EscapeTVInterior"));
    if(!Interior)Interior=Dark;
    auto* TVInterior=UMaterialInstanceDynamic::Create(Interior,this);
    const FVector TunnelUp=Rotation.RotateVector(FVector::UpVector);
    TVInterior->SetVectorParameterValue(TEXT("TunnelOrigin"),FLinearColor(ComputerEntry));
    TVInterior->SetVectorParameterValue(TEXT("TunnelAxis"),FLinearColor(Direction));
    TVInterior->SetVectorParameterValue(TEXT("TunnelUp"),FLinearColor(TunnelUp));
    Interior=TVInterior;
    const float ShellLength=7000;
    auto* ExposureBounds=NewObject<UBoxComponent>(this);AddInstanceComponent(ExposureBounds);ExposureBounds->SetupAttachment(RootComponent);
    ExposureBounds->SetBoxExtent(FVector(Length*.5f+500,W+100,H+150));ExposureBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ExposureBounds->SetCollisionResponseToAllChannels(ECR_Ignore);ExposureBounds->SetGenerateOverlapEvents(false);
    ExposureBounds->RegisterComponent();ExposureBounds->SetWorldLocationAndRotation(TunnelPoint(FVector(Length*.5f,0,0)),Rotation);
    auto* Exposure=NewObject<UPostProcessComponent>(this);AddInstanceComponent(Exposure);Exposure->SetupAttachment(ExposureBounds);
    Exposure->bUnbound=false;Exposure->BlendRadius=100;Exposure->Priority=20;
    // The card room uses manual -6 EV exposure. Min/max adaptation settings alone do not
    // override that, so the first section used to be much darker than the far tunnel.
    Exposure->Settings.bOverride_AutoExposureMethod=true;Exposure->Settings.AutoExposureMethod=AEM_Manual;
    Exposure->Settings.bOverride_AutoExposureBias=true;Exposure->Settings.AutoExposureBias=-1.5f;
    Exposure->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;
    Exposure->Settings.AutoExposureApplyPhysicalCameraExposure=false;
    Exposure->Settings.bOverride_MotionBlurAmount=true;Exposure->Settings.MotionBlurAmount=0;
    Exposure->Settings.bOverride_BloomIntensity=true;Exposure->Settings.BloomIntensity=.15f;
    Exposure->Settings.bOverride_SceneFringeIntensity=true;Exposure->Settings.SceneFringeIntensity=0;
    Exposure->RegisterComponent();
    Shape(TunnelPoint(FVector(ShellLength*.5f,0,-H-30)),FVector(ShellLength+200,W*2+120,60),Interior,true,Rotation);
    Shape(TunnelPoint(FVector(ShellLength*.5f,0,H+30)),FVector(ShellLength+200,W*2+120,60),Interior,true,Rotation);
    for(float Side:{-1.f,1.f})
        Shape(TunnelPoint(FVector(ShellLength*.5f,Side*(W+30),0)),FVector(ShellLength+200,60,H*2),Interior,true,Rotation);
    // Seventy metres of touching solid fill: no curtains, recesses or empty shortcuts.
    for(int32 i=0;i<14;++i)
        ComputerRocks.Add(Rock(TunnelPoint(FVector(250+i*500,0,0)),FVector(500,W*2,H*2),Interior,Rotation));
    for(float Along:{120.f,800.f,1600.f,2400.f,3200.f,4000.f,4800.f,5600.f,6400.f})
    {
        auto* CircuitLight=Light(TunnelPoint(FVector(Along,0,H-70)),55000,1250);
        InteriorLights.Add(CircuitLight);CircuitLight->SetVisibility(false);
        CircuitLight->SetLightColor(FLinearColor(.35f,.65f,.8f));
    }
    // Fixed lower-edge guides and repeated forward chevrons give a stable frame of reference.
    auto* GuideMaterial=Material(FLinearColor(.12f,.45f,.5f),true);
    for(float Side:{-1.f,1.f})
    {
        Shape(TunnelPoint(FVector(3500,Side*(W-10),-H+75)),FVector(7000,12,18),GuideMaterial,false,Rotation);
        for(float Along:{900.f,1900.f,2900.f,3900.f,4900.f,5900.f,6900.f})
            for(float Tilt:{-1.f,1.f})
            {
                const FVector A(Along-75,Side*(W-12),-H+180+Tilt*50),B(Along,Side*(W-12),-H+180);
                Shape(TunnelPoint((A+B)*.5f),FVector((B-A).Size(),10,10),GuideMaterial,false,Rotation.Quaternion().RotateVector(B-A).Rotation());
            }
    }
    ComputerWater=Shape(TunnelPoint(FVector(-400,0,0)),FVector(35,W*2,H*2),WaterMat,false,Rotation);
    // The final face opens directly towards a bright doorway, never a closed luminous panel.
    for(float Side:{-1.f,1.f})
        Shape(TunnelPoint(FVector(7050,Side*(W-20),0)),FVector(30,35,H*2),Amber,false,Rotation);
    Shape(TunnelPoint(FVector(7050,0,H-20)),FVector(30,W*2,35),Amber,false,Rotation);
    Light(TunnelPoint(FVector(7050,0,180)),240000,1800);
    // Separate the tunnel axis endpoint from the resting target. This ramp rises ~11 degrees.
    const FVector RampStart=TunnelPoint(FVector(7100,0,-H)),RampEnd(31100+SourceLevelOffset.X,ComputerEntry.Y,9900+SourceLevelOffset.Z);
    ExitRamp=Shape((RampStart+RampEnd)*.5f-FVector(0,0,20),FVector(FVector::Distance(RampStart,RampEnd)+80,W*2,40),Wood,true,(RampEnd-RampStart).Rotation());
    const FVector Deck(31900+SourceLevelOffset.X,ComputerEntry.Y,9850+SourceLevelOffset.Z);
    Shape(Deck,FVector(1800,2000,100),Wood);
    if(!bWalkThroughConnections) Shape(Deck+FVector(950,0,500),FVector(100,2200,1100),Plaster);
    else
    {
        for(float Side:{-1.f,1.f}) Shape(Deck+FVector(950,Side*690,500),FVector(100,820,1100),Plaster);
        Shape(Deck+FVector(950,0,900),FVector(100,560,300),Plaster);
    }
    for(float Side:{-1.f,1.f})
    {
        Shape(Deck+FVector(0,Side*950,260),FVector(1800,60,400),Wood);
        Shape(Deck+FVector(0,Side*950,470),FVector(1800,65,25),Amber,false);
    }
    Text(Deck+FVector(885,0,590),bWalkThroughConnections?TEXT("MINE 03"):TEXT("EXIT\nREST AREA"),90,FRotator(0,180,0));
    Light(Deck+FVector(0,0,650),350000,2200);
}
void AMiningRoomEscape::TickPresentation(float DeltaSeconds)
{
    BreachBurstTime+=DeltaSeconds;
    const bool Active=bWallBroken && Phase!=EMiningEscapePhase::Escaped;
    if(!BreachWall)return;
    const FVector Origin=BreachWall->GetActorLocation()+FVector(-130,0,0);
    const float Time=GetWorld()->GetTimeSeconds();
    const float FallTime=FMath::Sqrt(FMath::Max(1.f,float(Origin.Z-WaterZ))/900.f);
    const float Front=FMath::Min(FallTime,BreachBurstTime*3.f);
    auto Trajectory=[&](float T){return Origin+FVector(-2400*T,0,-900*T*T);};
    if(JetStream)JetStream->SetVisibility(Active && Front>.01f);
    JetUpdateTime+=DeltaSeconds;
    if(JetStream && Active && Front>.01f && JetUpdateTime>=1.f/30)
    {
        JetUpdateTime=0;
        using namespace UE::Geometry;
        FDynamicMesh3 Mesh(true,false,false,false);
        constexpr int32 Rings=48,Sides=24;
        for(int32 Ring=0;Ring<=Rings;++Ring)
        {
            const float T=Front*Ring/Rings;
            const FVector Center=Trajectory(T),Tangent=FVector(-2400,0,-1800*T).GetSafeNormal();
            const FVector Across(0,1,0),Up=FVector::CrossProduct(Tangent,Across);
            for(int32 Side=0;Side<Sides;++Side)
            {
                const float Angle=2*PI*Side/Sides;
                const float Ripple=1+.035f*FMath::Sin(T*14-Time*5+Angle*3)+.028f*FMath::Sin(T*31-Time*9-Angle*4);
                const float Radius=625*(1-.12f*T/FallTime)*Ripple;
                Mesh.AppendVertex(FVector3d(JetStream->GetComponentTransform().InverseTransformPosition(Center+Radius*(Across*FMath::Cos(Angle)+Up*FMath::Sin(Angle)))));
            }
        }
        for(int32 Ring=0;Ring<Rings;++Ring)for(int32 Side=0;Side<Sides;++Side)
        {
            const int32 A=Ring*Sides+Side,B=Ring*Sides+(Side+1)%Sides,C=A+Sides,D=B+Sides;
            Mesh.AppendTriangle(A,B,C);Mesh.AppendTriangle(B,D,C);
        }
        FMeshNormals::QuickComputeVertexNormals(Mesh);Mesh.EnableAttributes();
        FMeshNormals::InitializeOverlayToPerVertexNormals(Mesh.Attributes()->PrimaryNormals());
        JetStream->SetMesh(MoveTemp(Mesh));
    }
    for(int32 i=0;i<Foam.Num();++i)
    {
        auto* A=Foam[i].Get();if(!A)continue;
        const float Cycle=FMath::Frac(Time*(.6f+(i%5)*.07f)+i*.618f);
        const float T=Cycle*FallTime;
        const float Spread=FMath::Sin(i*12.37f);
        FVector P=Trajectory(T)+FVector(FMath::Cos(i*7.1f)*T*150,Spread*(220+T*280),FMath::Sin(i*4.7f)*T*150);
        // Spray near the impact spreads into short upward splashes at the water surface.
        if(i>=48)
        {
            P=Trajectory(FallTime)+FVector(FMath::Cos(i*2.4f)*Cycle*1100,Spread*Cycle*1200,0);
            P.Z=WaterZ+40+650*4*Cycle*(1-Cycle);
        }
        A->SetActorHiddenInGame(!Active || T>Front || (i>=48 && Front<FallTime));
        A->SetActorLocation(P);A->SetActorRotation(FRotator(-25,Spread*70,0));
        const float Size=30+(i%7)*12;
        A->SetActorScale3D(FVector(Size*2,Size,Size)/100);
    }
}
