#include "EndingEmergence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "UDynamicMesh.h"
#include "Engine/Texture2D.h"
#include "Components/MeshComponent.h"

namespace PaperEnding
{
    const FVector Origin(2209150,0,0),Desk(155000,0,0);
    float Ease(float T){T=FMath::Clamp(T,0.f,1.f);return T*T*(3-2*T);}
    void Quad(UE::Geometry::FDynamicMesh3& Mesh,FVector A,FVector B,FVector C,FVector D,FVector4f Bounds=FVector4f(0,0,1,1))
    {
        const int V[]={Mesh.AppendVertex(A),Mesh.AppendVertex(B),Mesh.AppendVertex(C),Mesh.AppendVertex(D)};
        auto* UV=Mesh.Attributes()->PrimaryUV();
        const int U[]={UV->AppendElement(FVector2f(Bounds.X,Bounds.W)),UV->AppendElement(FVector2f(Bounds.Z,Bounds.W)),UV->AppendElement(FVector2f(Bounds.Z,Bounds.Y)),UV->AppendElement(FVector2f(Bounds.X,Bounds.Y))};
        int T=Mesh.AppendTriangle(V[0],V[2],V[1]);UV->SetTriangle(T,UE::Geometry::FIndex3i(U[0],U[2],U[1]));
        T=Mesh.AppendTriangle(V[0],V[3],V[2]);UV->SetTriangle(T,UE::Geometry::FIndex3i(U[0],U[3],U[2]));
    }
}

void AEndingEmergence::BuildPaperFinale()
{
    const FLinearColor Cream(.84f,.75f,.57f),Ink(.018f,.012f,.045f),Blue(.05f,.48f,.68f),Yellow(.95f,.57f,.07f);
    PaperScene=Group(TEXT("PaperTableJourney"),PaperEnding::Origin);
    auto MeshPart=[&](const TCHAR* Name,UE::Geometry::FDynamicMesh3 Mesh,USceneComponent* Parent,const TCHAR* Material,FLinearColor Tint)
    {
        auto* C=NewObject<UDynamicMeshComponent>(this,MakeUniqueObjectName(this,UDynamicMeshComponent::StaticClass(),Name),RF_Transient);
        C->SetupAttachment(Parent);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(false);C->SetEnableRaytracing(false);C->SetVisibleInRayTracing(false);C->SetCanEverAffectNavigation(false);
        UE::Geometry::FMeshNormals::InitializeOverlayToPerTriangleNormals(Mesh.Attributes()->PrimaryNormals());
        C->SetMesh(MoveTemp(Mesh));auto* M=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/EndingEmergence/Materials/M_EE_"))+Material)),C);
        M->SetVectorParameterValue(TEXT("Tint"),Tint);C->SetMaterial(0,M);C->RegisterComponent();AddInstanceComponent(C);return M;
    };
    // The portal is the FRONT of the future desktop. Camera approaches it from
    // positive local Z, looking DOWN, then lifts its gaze by ninety degrees.
    UE::Geometry::FDynamicMesh3 Sheet;Sheet.EnableAttributes();
    PaperEnding::Quad(Sheet,FVector(-1400,-1800,0),FVector(9400,-1800,0),FVector(9400,1800,0),FVector(-1400,1800,0));
    MeshPart(TEXT("WhitePortalBecomesDesktop"),MoveTemp(Sheet),PaperScene,TEXT("PaperFloor"),Cream);
    const TCHAR* DoodleNames[]={TEXT("toilet"),TEXT("warnings"),TEXT("glasses"),TEXT("cards"),TEXT("fountain"),TEXT("thought"),TEXT("computer"),TEXT("brain"),TEXT("solar"),TEXT("ants"),TEXT("roguelike"),TEXT("pickaxe")};
    // Measured alpha bounds of the original drawings, plus room for the cut edge.
    const FIntVector4 Bounds[]={ {255,29,1026,1225},{179,166,1045,1098},{88,358,1169,897},{104,199,1150,1054},{186,61,1067,1198},{141,145,1152,1122},{113,166,1144,1126},{198,160,1056,1097},{64,405,1210,869},{64,321,1206,940},{282,140,989,1150},{189,122,1145,1152} };
    const FLinearColor Palette[]={FLinearColor(1,.045f,.27f),FLinearColor(.02f,.7f,1),FLinearColor(1,.65f,.025f),FLinearColor(.42f,.06f,.95f),FLinearColor(.04f,.92f,.51f)};
    FRandomStream R(191067);PaperCardCount=0;
    PaperPopStrokes=Instances(TEXT("PaperPopInkBursts"),TEXT("Cube"),FLinearColor(1,.7f,.02f),PaperScene,TEXT("PaperComic"));
    for(int I=0;I<24;++I)
    {
        const int DrawingIndex=I%12,Side=I%2?1:-1;
        const auto B=Bounds[DrawingIndex];const FVector4f UV((B.X-18)/1254.f,(B.Y-18)/1254.f,(B.Z+18)/1254.f,(B.W+18)/1254.f);
        const float H=R.FRandRange(210,450)*(I%7==0?1.2f:1.f),W=H*(UV.Z-UV.X)/(UV.W-UV.Y);
        const FVector Position(900+(I/2)*470+R.FRandRange(-85,85),Side*(W*.5f+R.FRandRange(140,270)),0);
        const FRotator Rest(R.FRandRange(-7,7),R.FRandRange(-22,22),R.FRandRange(-8,8));
        auto* Card=Group(TEXT("HingedContourDoodle"),Position,PaperScene);
        PaperCards.Add(Card);PaperRestRotations.Add(Rest);PaperPopAges.Add(-1);
        // Alpha-cut front and back share a thin offset: no rectangular paper body.
        UE::Geometry::FDynamicMesh3 Drawing;Drawing.EnableAttributes();
        for(float Depth:{-1.1f,1.1f})PaperEnding::Quad(Drawing,FVector(Depth,-W*.5f,-3),FVector(Depth,W*.5f,-3),FVector(Depth,W*.5f,H-3),FVector(Depth,-W*.5f,H-3),UV);
        auto* M=MeshPart(TEXT("ContourPaperFrontAndBack"),MoveTemp(Drawing),Card,TEXT("PaperCutout"),Palette[I%5]);
        auto* Texture=LoadObject<UTexture2D>(nullptr,*(FString(TEXT("/Game/EndingEmergence/PaperFinale/Doodles/T_EE_Doodle_"))+DoodleNames[DrawingIndex]));
        M->SetTextureParameterValue(TEXT("Doodle"),Texture);M->SetVectorParameterValue(TEXT("Accent"),Palette[(I+2)%5]);M->SetScalarParameterValue(TEXT("Seed"),I*.37f);
        auto* Shadow=Group(TEXT("ContourPaperCastShadow"),Position+FVector(0,0,.6f),PaperScene);Shadow->SetRelativeRotation(FRotator(0,Rest.Yaw,0));PaperShadows.Add(Shadow);
        UE::Geometry::FDynamicMesh3 ShadowMesh;ShadowMesh.EnableAttributes();
        PaperEnding::Quad(ShadowMesh,FVector(0,-W*.5f,0),FVector(0,W*.5f,0),FVector(H*.65f,W*.5f,0),FVector(H*.65f,-W*.5f,0),UV);
        auto* SM=MeshPart(TEXT("DoodleContourShadow"),MoveTemp(ShadowMesh),Shadow,TEXT("PaperShadow"),Ink);SM->SetTextureParameterValue(TEXT("Doodle"),Texture);
        for(int J=0;J<6;++J)PaperPopStrokes->AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector));
        ++PaperCardCount;
    }
    // Oversized stylized desk objects add close parallax and a physical tabletop.
    for(int I=0;I<10;++I)
    {
        auto* DeskPencil=Shape(TEXT("GraphicDeskPencil"),FVector(800+I*640,(I%2?1:-1)*1370,32),FVector(680,34,34),Palette[I%5],TEXT("Cube"),PaperScene,TEXT("PaperComic"));
        DeskPencil->SetRelativeRotation(FRotator(0,15+I*17,0));
    }
    Shape(TEXT("DeskPaperStack"),FVector(4800,-1400,100),FVector(880,600,190),FLinearColor(1,.55f,.04f),TEXT("Cube"),PaperScene,TEXT("PaperComic"));
    for(int I=0;I<15;++I)Shape(TEXT("PaperStackInkEdges"),FVector(4800,-1098,18+I*12),FVector(880,2,3),Ink,TEXT("Cube"),PaperScene,TEXT("PaperComic"));
    for(int Side:{-1,1})
    {
        Shape(TEXT("GraffitiRoomSide"),FVector(4000,Side*1810,1100),FVector(10800,20,2200),Side>0?FLinearColor(.09f,.025f,.35f):FLinearColor(.015f,.32f,.56f),TEXT("Cube"),PaperScene,TEXT("PaperComic"));
        Shape(TEXT("InkSkirting"),FVector(4000,Side*1795,35),FVector(10800,12,70),Ink,TEXT("Cube"),PaperScene,TEXT("PaperComic"));
    }
    auto* FarWall=Shape(TEXT("GraffitiSunburstWall"),FVector(9410,0,1100),FVector(20,3640,2200),FLinearColor(1,.22f,.025f),TEXT("Cube"),PaperScene,TEXT("PaperComic"));
    CastChecked<UMaterialInstanceDynamic>(FarWall->GetMaterial(0))->SetScalarParameterValue(TEXT("Sunburst"),1);
    Shape(TEXT("PaperRoomBackWall"),FVector(-1410,0,1100),FVector(20,3640,2200),FLinearColor(.12f,.025f,.3f),TEXT("Cube"),PaperScene,TEXT("PaperComic"));
    const FVector Vent(7500,0,2200);
    for(int S:{-1,1})
    {
        Shape(TEXT("ComicCeilingSide"),Vent+FVector(0,S*1510,0),FVector(19000,2500,25),FLinearColor(.13f,.025f,.38f),TEXT("Cube"),PaperScene,TEXT("PaperComic"));
        Shape(TEXT("ComicCeilingEnd"),Vent+FVector(S*5010,0,0),FVector(9500,520,25),FLinearColor(.13f,.025f,.38f),TEXT("Cube"),PaperScene,TEXT("PaperComic"));
        Shape(TEXT("VentFrameLong"),Vent+FVector(S*265,0,-15),FVector(30,560,35),FLinearColor(.05f,.75f,.85f),TEXT("Cube"),PaperScene,TEXT("PaperComic"));
        Shape(TEXT("VentFrameShort"),Vent+FVector(0,S*265,-15),FVector(520,30,35),FLinearColor(1,.18f,.35f),TEXT("Cube"),PaperScene,TEXT("PaperComic"));
        Shape(TEXT("VentShaftWall"),Vent+FVector(S*270,0,400),FVector(20,560,800),FLinearColor::Black,TEXT("Cube"),PaperScene,TEXT("Glow"));
        Shape(TEXT("VentShaftSide"),Vent+FVector(0,S*270,400),FVector(520,20,800),FLinearColor::Black,TEXT("Cube"),PaperScene,TEXT("Glow"));
    }
    for(int I=-5;I<=5;++I)if(FMath::Abs(I)>1)
    {
        auto* Slat=Shape(TEXT("AirConditionerLouvre"),Vent+FVector(I*43,0,0),FVector(14,500,30),I%2?FLinearColor(.2f,.65f,.85f):FLinearColor(.8f,.1f,.4f),TEXT("Cube"),PaperScene,TEXT("PaperComic"));Slat->SetRelativeRotation(FRotator(23,0,0));
    }
    Shape(TEXT("VentBlackDepth"),Vent+FVector(0,0,800),FVector(560,560,20),FLinearColor::Black,TEXT("Cube"),PaperScene,TEXT("Glow"));
    TArray<USceneComponent*> PaperParts;PaperScene->GetChildrenComponents(true,PaperParts);
    for(auto* Part:PaperParts)if(auto* Mesh=Cast<UMeshComponent>(Part))for(int I=0;I<Mesh->GetNumMaterials();++I)
        if(auto* M=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(I)))PaperArtMaterials.AddUnique(M);
    ResetPaperFinale();PaperScene->SetVisibility(false,true);

    FinaleReality=Group(TEXT("FinaleRealityDesk"),PaperEnding::Desk);
    const FLinearColor Wood(.14f,.085f,.055f),Dark(.012f,.017f,.026f);
    Shape(TEXT("FinaleDesktop"),FVector(45,0,78),FVector(460,420,12),Wood,TEXT("Cube"),FinaleReality,TEXT("PaperStock"));
    Shape(TEXT("FinaleWall"),FVector(260,0,210),FVector(15,1000,460),FLinearColor(.13f,.16f,.21f),TEXT("Cube"),FinaleReality,TEXT("PaperStock"));
    Shape(TEXT("FinaleMonitorBase"),FVector(108,0,88),FVector(42,64,7),Dark,TEXT("Cube"),FinaleReality,TEXT("PaperStock"));
    Shape(TEXT("FinaleMonitorStem"),FVector(111,0,119),FVector(12,14,56),Dark,TEXT("Cube"),FinaleReality,TEXT("PaperStock"));
    Shape(TEXT("FinaleMonitor"),FVector(108,0,159),FVector(9,136,84),FLinearColor(.025f,.033f,.045f),TEXT("Cube"),FinaleReality,TEXT("PaperStock"));
    UE::Geometry::FDynamicMesh3 Screen;Screen.EnableAttributes();
    PaperEnding::Quad(Screen,FVector(102,-62.5,122.5),FVector(102,62.5,122.5),FVector(102,62.5,195.5),FVector(102,-62.5,195.5));
    CreditsScreenMaterial=MeshPart(TEXT("DeveloperCreditsScreen"),MoveTemp(Screen),FinaleReality,TEXT("DeveloperCredits"),FLinearColor::White);
    Shape(TEXT("FinaleKeyboard"),FVector(32,-5,87),FVector(34,95,4),Dark,TEXT("Cube"),FinaleReality,TEXT("PaperStock"));
    auto* Keys=Instances(TEXT("FinaleKeyboardKeys"),TEXT("Cube"),FLinearColor(.16f,.18f,.20f),FinaleReality,TEXT("PaperStock"));
    for(int X=0;X<4;++X)for(int Y=0;Y<13;++Y)Keys->AddInstance(FTransform(FRotator::ZeroRotator,FVector(21+X*7,-46+Y*7,90),FVector(.05f,.05f,.02f)));
    Shape(TEXT("FinaleMouse"),FVector(30,80,88),FVector(14,9,6),Dark,TEXT("Sphere"),FinaleReality,TEXT("PaperStock"));
    Shape(TEXT("FinaleCup"),FVector(92,-105,102),FVector(22,22,35),Cream*.5f,TEXT("Cylinder"),FinaleReality,TEXT("PaperStock"));
    Shape(TEXT("FinaleCoffee"),FVector(92,-105,120),FVector(20,20,1),Ink,TEXT("Cylinder"),FinaleReality,TEXT("PaperStock"));
    for(int I=0;I<5;++I){auto* Note=Shape(TEXT("FinaleDeskNote"),FVector(5+I*8,-80,86+I*.3),FVector(35,40,.3),Cream,TEXT("Cube"),FinaleReality,TEXT("PaperStock"));Note->SetRelativeRotation(FRotator(0,I*13,0));}
    CreditsScreenLight=NewObject<UPointLightComponent>(this,NAME_None,RF_Transient);CreditsScreenLight->SetupAttachment(FinaleReality);CreditsScreenLight->SetRelativeLocation(FVector(90,0,160));CreditsScreenLight->SetLightColor(FLinearColor(.55f,.73f,1));CreditsScreenLight->SetAttenuationRadius(300);CreditsScreenLight->SetCastShadows(false);CreditsScreenLight->SetIntensity(0);CreditsScreenLight->RegisterComponent();
    FinaleReality->SetVisibility(false,true);
}

void AEndingEmergence::ResetPaperFinale()
{
    if(PaperScene)PaperScene->SetRelativeRotation(FRotator(90,0,0));
    for(auto M:PaperArtMaterials)M->SetScalarParameterValue(TEXT("WhiteBlend"),1);
    for(int I=0;I<PaperCards.Num();++I)
    {
        PaperPopAges[I]=-1;FRotator Rot=PaperRestRotations[I];Rot.Pitch=-90;
        PaperCards[I]->SetRelativeRotation(Rot);PaperCards[I]->SetRelativeScale3D(FVector(1));
        PaperShadows[I]->SetRelativeScale3D(FVector(.01f,1,1));
    }
    if(PaperPopStrokes)for(int I=0;I<PaperPopStrokes->GetInstanceCount();++I)PaperPopStrokes->UpdateInstanceTransform(I,FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector),false,I==PaperPopStrokes->GetInstanceCount()-1,true);
}

void AEndingEmergence::UpdatePaperProps(float CameraX,float Dt)
{
    for(int I=0;I<PaperCards.Num();++I)
    {
        auto* Card=PaperCards[I].Get();float& PopAge=PaperPopAges[I];
        if(PopAge<0&&CameraX+760>Card->GetRelativeLocation().X)PopAge=0;
        if(PopAge>=0)PopAge+=Dt;
        const float T=FMath::Clamp(PopAge/.60f,0.f,1.f),U=T-1;
        const float Pop=1+2.5f*U*U*U+1.5f*U*U;
        FRotator Rot=PaperRestRotations[I];Rot.Pitch=Rot.Pitch*Pop-90*(1-Pop);
        Card->SetRelativeRotation(Rot);Card->SetRelativeScale3D(FVector(1,1+.035f*FMath::Sin(T*PI),1));
        PaperShadows[I]->SetRelativeScale3D(FVector(FMath::Lerp(1.5f,1.f,PaperEnding::Ease(T)),1,1));
        const float Burst=PopAge>.12f&&PopAge<.48f?FMath::Sin((PopAge-.12f)/.36f*PI):0;
        for(int J=0;J<6;++J)
        {
            const float A=(J/5.f)*PI;
            const FVector Ray(0,FMath::Cos(A),FMath::Sin(A));
            const FVector P=Card->GetRelativeLocation()+FVector(-30,0,30)+Ray*(75+T*90);
            PaperPopStrokes->UpdateInstanceTransform(I*6+J,FTransform(FQuat::FindBetweenNormals(FVector::ForwardVector,Ray),P,FVector(.6f,.025f,.035f)*Burst),false,false,true);
        }
    }
    PaperPopStrokes->MarkRenderStateDirty();
}

void AEndingEmergence::UpdatePaperFinale(float Dt)
{
    auto PaperPose=[&](FVector P,FVector Direction,float Fov,float Roll=0.f)
    {
        const FTransform T=PaperScene->GetRelativeTransform();Pose(T.TransformPosition(P),T.TransformPosition(P+Direction*1000),Fov,Roll);
    };
    if(Shot==10)
    {
        // First leave the rock into black space. Only the distant white aperture
        // and the fading monochrome strokes are visible, not the whole desktop.
        constexpr float BlackFlight=2.6f;
        UpdateFinaleExit(Dt);
        if(ShotTime<BlackFlight)
        {
            const float U=ShotTime/BlackFlight;
            const float X=FMath::Lerp(7800.f,8450.f,U);
            const FVector Side=FinaleSideVector*(1-PaperEnding::Ease(U));
            Pose(FVector(2200000+X,Side.Y,Side.Z),FVector(2201000+X,Side.Y,Side.Z),94);
            const float RadiusScale=FMath::Lerp(.75f,70.f,FMath::Pow(PaperEnding::Ease((ShotTime-1.3f)/1.3f),3));
            FinaleLight->SetRelativeScale3D(FVector(.04f,RadiusScale,RadiusScale));
            TransitEffects(0);
            return;
        }
        // Switch surfaces while the approaching aperture fills the frame white.
        if(!PaperScene->IsVisible()){PaperScene->SetVisibility(true,true);FinaleLight->SetVisibility(false);}
        const float PaperTime=ShotTime-BlackFlight;
        const float T=FMath::Clamp(PaperTime/3.2f,0.f,1.f),Turn=PaperEnding::Ease((PaperTime-.15f)/2.6f);
        PaperScene->SetRelativeRotation(FRotator(90*(1-Turn),0,0));
        const float X=480*FMath::Pow(T,6),Z=FMath::Lerp(700.f,24.f,PaperEnding::Ease(PaperTime/1.25f)),Pitch=-HALF_PI*(1-Turn);
        PaperPose(FVector(X,0,Z),FVector(FMath::Cos(Pitch),0,FMath::Sin(Pitch)),FMath::Lerp(94.f,108.f,Turn));
        const float White=1-PaperEnding::Ease((PaperTime-.55f)/1.2f);
        for(auto M:PaperArtMaterials)M->SetScalarParameterValue(TEXT("WhiteBlend"),White);
        UpdatePaperProps(X,Dt);TransitEffects((1-Turn)*.08f);
    }
    else if(Shot==11)
    {
        const float T=FMath::Clamp(ShotTime/6.6f,0.f,1.f),X=FMath::Lerp(480.f,6800.f,T),Y=FMath::Sin(T*2*PI)*65;
        PaperPose(FVector(X,Y,24+FMath::Sin(T*PI)*6),FVector(1,FMath::Cos(T*2*PI)*.025f,.012f*(1-T)),108,bComfortMode?0:FMath::Sin(T*2*PI)*4.5f);
        UpdatePaperProps(X,Dt);
    }
    else if(Shot==12)
    {
        const float T=FMath::Clamp(ShotTime/6.5f,0.f,1.f);
        const float U=FMath::Clamp(ShotTime/1.6f,0.f,1.f);
        const float X=6800+700*(3*U*U-2*U*U*U)+(6320.f/6.6f)*1.6f*(U*U*U-2*U*U+U);
        const FVector P(X,0,24+2756*FMath::Pow(T,1.55f));
        const float Tilt=PaperEnding::Ease(U)*HALF_PI;
        PaperPose(P,FVector(FMath::Cos(Tilt),0,FMath::Sin(Tilt)),FMath::Lerp(108.f,68.f,PaperEnding::Ease(T)));
        UpdatePaperProps(10000,Dt);
    }
    else if(Shot==13)
    {
        const float Back=PaperEnding::Ease((ShotTime-.8f)/4.8f);
        // Start beyond the 5 cm near plane. The off screen fills the entire view,
        // including the first uncovered frames; no glimpse through the monitor.
        Pose(PaperEnding::Desk+FVector(FMath::Lerp(92.f,-135.f,Back),0,159),PaperEnding::Desk+FVector(108,0,159),55);
        const float Power=PaperEnding::Ease((ShotTime-6.2f)/1.6f);
        CreditsScreenMaterial->SetScalarParameterValue(TEXT("Power"),Power);CreditsScreenLight->SetIntensity(Power*140);
        bComplete=ShotTime>=7.8f;
    }
}
