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
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderingThread.h"

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
    PaperEnding::Quad(Sheet,FVector(-60000,-60000,0),FVector(60000,-60000,0),FVector(60000,60000,0),FVector(-60000,60000,0));
    PaperSurfaceMaterial=MeshPart(TEXT("WhitePortalBecomesDesktop"),MoveTemp(Sheet),PaperScene,TEXT("PaperFloor"),Cream);
    PaperSurface=CastChecked<USceneComponent>(PaperSurfaceMaterial->GetOuter());
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
    // A continuous open horizon, with no ceiling, side walls or ventilation shaft.
    UE::Geometry::FDynamicMesh3 Horizon;Horizon.EnableAttributes();
    for(int I=0;I<96;++I)
    {
        const float A=I*2*PI/96,B=(I+1)*2*PI/96;
        FVector P(4000+55000*FMath::Cos(A),55000*FMath::Sin(A),-200);
        FVector Q(4000+55000*FMath::Cos(B),55000*FMath::Sin(B),-200);
        PaperEnding::Quad(Horizon,P,Q,Q+FVector(0,0,280000),P+FVector(0,0,280000),FVector4f(I/96.f,0,(I+1)/96.f,1));
    }
    MeshPart(TEXT("OpenOrangeSpeedRayHorizon"),MoveTemp(Horizon),PaperScene,TEXT("PaperHorizon"),Yellow);
    auto* RockDrawing=LoadObject<UTexture2D>(nullptr,TEXT("/Game/EndingEmergence/PaperFinale/Hero/T_EE_Doodle_rock"));
    // All faces sample the same original drawing. Removing the centre creates
    // a real opening, while the surrounding ink and flying paper stay aligned.
    auto RockUV=[](FVector P)
    {
        return FVector2f(FMath::Lerp(20.f,1234.f,float((P.Y+1100)/2200))/1254.f,
            FMath::Lerp(1117.f,138.f,float(P.Z/1774))/1254.f);
    };
    auto RockFacet=[&](const TCHAR* Name,const TArray<FVector>& Points,USceneComponent* Parent,FVector Offset=FVector::ZeroVector)
    {
        UE::Geometry::FDynamicMesh3 M;M.EnableAttributes();auto* UV=M.Attributes()->PrimaryUV();
        TArray<int> V,U;
        for(FVector P:Points){V.Add(M.AppendVertex(P-Offset));U.Add(UV->AppendElement(RockUV(P)));}
        for(int I=1;I+1<Points.Num();++I)
        {
            const int T=M.AppendTriangle(V[0],V[I+1],V[I]);
            UV->SetTriangle(T,UE::Geometry::FIndex3i(U[0],U[I+1],U[I]));
        }
        auto* Mat=MeshPart(Name,MoveTemp(M),Parent,TEXT("PaperHeroDoodle"),FLinearColor::White);
        Mat->SetTextureParameterValue(TEXT("Doodle"),RockDrawing);
    };
    PaperRock=Group(TEXT("GiantHingedRockCutout"),FVector(8000,0,0),PaperScene);
    PaperRockCap=Group(TEXT("BreakablePaperRockCentre"),FVector::ZeroVector,PaperRock);
    constexpr int Sides=20;
    TArray<FVector> Outer,Inner;
    for(int I=0;I<Sides;++I)
    {
        const float A=I*2*PI/Sides;
        const float Y=FMath::Cos(A),Z=FMath::Sin(A);
        const float Edge=FMath::Min(1100/FMath::Max(FMath::Abs(Y),.001f),(Z>=0?1074.f:700.f)/FMath::Max(FMath::Abs(Z),.001f));
        Outer.Add(FVector(0,Y*Edge,700+Z*Edge));
        Inner.Add(FVector(0,Y*440,700+Z*440));
    }
    for(int I=0;I<Sides;++I)
    {
        const int J=(I+1)%Sides;
        RockFacet(TEXT("DrawnRockPaperRim"),{Outer[I],Outer[J],Inner[J],Inner[I]},PaperRock);
        RockFacet(TEXT("DrawnRockUnbrokenCentre"),{FVector(0,0,700),Inner[I],Inner[J]},PaperRockCap);
        const FVector ShardCentre=(FVector(0,0,700)+Inner[I]+Inner[J])/3;
        auto* Chip=Group(TEXT("FlyingPaperRockShard"),ShardCentre,PaperRock);
        RockFacet(TEXT("DrawnPaperRockShard"),{FVector(0,0,700),Inner[I],Inner[J]},Chip,ShardCentre);
        PaperRockChips.Add(Chip);
    }
    PaperPick=Group(TEXT("GiantHingedPickaxeCutout"),FVector(7940,1100,0),PaperScene);
    PaperPickSwing=Group(TEXT("PickaxeStrikePivot"),FVector(0,0,1500),PaperPick);
    // Existing doodle, hinged at its handle butt (240,1090). The measured sharp
    // tip (1114,720) lands at Y=0,Z=700; texture alpha supplies the cut contour.
    const float PickScale=FMath::Sqrt((1100.f*1100+800.f*800)/(874.f*874+370.f*370));
    auto PickPoint=[&](float X,float Y){return FVector(0,(240-X)*PickScale,(1090-Y)*PickScale);};
    UE::Geometry::FDynamicMesh3 PickDrawing;PickDrawing.EnableAttributes();
    PaperEnding::Quad(PickDrawing,PickPoint(171,1170),PickPoint(1163,1170),PickPoint(1163,104),PickPoint(171,104),FVector4f(171/1254.f,104/1254.f,1163/1254.f,1170/1254.f));
    auto* PickMat=MeshPart(TEXT("OriginalDoodleGiantPickaxe"),MoveTemp(PickDrawing),PaperPickSwing,TEXT("PaperHeroDoodle"),FLinearColor::White);
    PickMat->SetTextureParameterValue(TEXT("Doodle"),LoadObject<UTexture2D>(nullptr,TEXT("/Game/EndingEmergence/PaperFinale/Doodles/T_EE_Doodle_pickaxe")));

    // The model beyond the hole lives in the same world: no portal image swap.
    PaperCaveScene=Group(TEXT("OrbitCaveStage"),PaperEnding::Origin+FVector(10500,0,0));
    // Ending-only duplicate of the actual MainGame starting mine, sliced vertically.
    auto* Mine=NewObject<UStaticMeshComponent>(this,TEXT("MainGameVerticalCutaway"),RF_Transient);
    Mine->SetupAttachment(PaperCaveScene);Mine->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/EndingEmergence/PaperFinale/SM_EE_MainGameCutaway")));
    Mine->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mine->SetCastShadow(false);Mine->SetVisibleInRayTracing(false);
    for(int Slot=0;Slot<FMath::Max(3,Mine->GetNumMaterials());++Slot)
    {
        auto* Mat=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_PaperCaveSurface")),Mine);
        Mat->SetVectorParameterValue(TEXT("Tint"),Slot==2?FLinearColor(.65f,.39f,.17f):(Slot==1?FLinearColor(.18f,.10f,.065f):FLinearColor(.29f,.16f,.08f)));
        Mat->SetScalarParameterValue(TEXT("Seed"),Slot==2?1.f:0.f);
        Mine->SetMaterial(Slot,Mat);
    }
    Mine->RegisterComponent();AddInstanceComponent(Mine);
    Shape(TEXT("CaveStageFloor"),FVector(0,0,-40),FVector(8000,8000,50),FLinearColor(.012f,.017f,.03f),TEXT("Cube"),PaperCaveScene,TEXT("PaperCaveSurface"));
    for(int Side:{-1,1})Shape(TEXT("CaveStageSide"),FVector(0,Side*3000,1500),FVector(6000,20,4000),Ink*.18f,TEXT("Cube"),PaperCaveScene,TEXT("PaperCaveSurface"));
    Shape(TEXT("CaveStageFar"),FVector(2800,0,1500),FVector(20,6000,4000),Ink*.18f,TEXT("Cube"),PaperCaveScene,TEXT("PaperCaveSurface"));
    Shape(TEXT("CaveStageTop"),FVector(0,0,3500),FVector(6000,6000,20),Ink*.18f,TEXT("Cube"),PaperCaveScene,TEXT("PaperCaveSurface"));
    PaperCaveSeal=Shape(TEXT("CaveStageEntrySeal"),FVector(-2300,0,1500),FVector(20,6000,4000),Ink*.18f,TEXT("Cube"),PaperCaveScene,TEXT("PaperCaveSurface"));
    PaperCaveTexture=NewObject<UTextureRenderTarget2D>(this);PaperCaveTexture->InitCustomFormat(1920,1080,PF_FloatRGBA,true);PaperCaveTexture->ClearColor=FLinearColor::Black;
    PaperCaveCapture=NewObject<USceneCaptureComponent2D>(this,NAME_None,RF_Transient);PaperCaveCapture->SetupAttachment(PaperCaveScene);PaperCaveCapture->TextureTarget=PaperCaveTexture;
    PaperCaveCapture->CaptureSource=ESceneCaptureSource::SCS_SceneColorHDR;PaperCaveCapture->bCaptureEveryFrame=false;PaperCaveCapture->bCaptureOnMovement=false;PaperCaveCapture->FOVAngle=55;
    PaperCaveCapture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    TArray<USceneComponent*> CaveParts;PaperCaveScene->GetChildrenComponents(true,CaveParts);
    for(auto* Part:CaveParts)if(auto* Primitive=Cast<UPrimitiveComponent>(Part))PaperCaveCapture->ShowOnlyComponent(Primitive);
    PaperCaveCapture->RegisterComponent();AddInstanceComponent(PaperCaveCapture);
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
    PaperEnding::Quad(Screen,FVector(102,-62.5,123.84375),FVector(102,62.5,123.84375),FVector(102,62.5,194.15625),FVector(102,-62.5,194.15625));
    CreditsScreenMaterial=MeshPart(TEXT("DeveloperCreditsScreen"),MoveTemp(Screen),FinaleReality,TEXT("DeveloperCredits"),FLinearColor::White);
    CreditsScreenMaterial->SetTextureParameterValue(TEXT("CaveImage"),PaperCaveTexture);
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
    for(auto M:PaperArtMaterials)M->SetScalarParameterValue(TEXT("WhiteBlend"),0);
    if(PaperSurfaceMaterial){PaperSurfaceMaterial->SetScalarParameterValue(TEXT("DotsReveal"),0);PaperSurfaceMaterial->SetScalarParameterValue(TEXT("ApertureRadius"),35);}
    bPaperRockBroken=false;
    if(PaperRock)PaperRock->SetRelativeRotation(FRotator(-90,0,0));
    if(PaperRockCap)PaperRockCap->SetVisibility(true,true);
    if(PaperPick)PaperPick->SetRelativeRotation(FRotator(-90,0,0));
    if(PaperPickSwing)PaperPickSwing->SetRelativeRotation(FQuat(FVector::ForwardVector,FMath::DegreesToRadians(-35.f)));
    for(auto Chip:PaperRockChips)Chip->SetVisibility(false,true);
    if(PaperCaveScene)PaperCaveScene->SetVisibility(false,true);
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

void AEndingEmergence::CapturePaperCave()
{
    PaperCaveCapture->SetRelativeLocation(FVector(1343.5029,1343.5029,650));
    PaperCaveCapture->SetRelativeRotation((FVector(0,0,350)-FVector(1343.5029,1343.5029,650)).Rotation());
    PaperCaveCapture->PostProcessSettings=Camera->GetCameraComponent()->PostProcessSettings;
    PaperCaveCapture->PostProcessSettings.WeightedBlendables.Array.Empty();
    PaperCaveCapture->CaptureScene();
    FlushRenderingCommands();
}

void AEndingEmergence::UpdatePaperFinale(float Dt)
{
    auto PaperPose=[&](FVector P,FVector Direction,float Fov,float Roll=0.f)
    {
        const FTransform T=PaperScene->GetRelativeTransform();Pose(T.TransformPosition(P),T.TransformPosition(P+Direction*1000),Fov,Roll);
    };
    if(Shot==10)
    {
        constexpr float BlackFlight=2.6f;
        UpdateFinaleExit(Dt);
        // One physical sheet stays visible throughout approach and the turn.
        // A growing aperture reveals it against black; printed dots stay in its UVs.
        const float Approach=PaperEnding::Ease((ShotTime-.7f)/1.9f);
        PaperSurfaceMaterial->SetScalarParameterValue(TEXT("ApertureRadius"),FMath::Lerp(35.f,90000.f,FMath::Pow(Approach,5)));
        PaperSurfaceMaterial->SetScalarParameterValue(TEXT("DotsReveal"),PaperEnding::Ease((ShotTime-1.25f)/1.1f));
        if(ShotTime<BlackFlight)
        {
            const float U=ShotTime/BlackFlight;
            const FVector Side=FinaleSideVector*(1-PaperEnding::Ease(U));
            Pose(FVector(2207800+650*U,Side.Y,Side.Z),FVector(2208800+650*U,Side.Y,Side.Z),94);
            TransitEffects(0);return;
        }
        if(!PaperScene->IsVisible())
        {
            PaperScene->SetVisibility(true,true);
            for(auto Chip:PaperRockChips)Chip->SetVisibility(false,true);
        }
        const float PaperTime=ShotTime-BlackFlight,T=FMath::Clamp(PaperTime/3.2f,0.f,1.f),Turn=PaperEnding::Ease(PaperTime/2.6f);
        PaperScene->SetRelativeRotation(FRotator(90*(1-Turn),0,0));
        const float X=480*FMath::Pow(T,6),Z=FMath::Lerp(700.f,24.f,PaperEnding::Ease(PaperTime/2.4f)),Pitch=-HALF_PI*(1-Turn);
        PaperPose(FVector(X,0,Z),FVector(FMath::Cos(Pitch),0,FMath::Sin(Pitch)),FMath::Lerp(94.f,108.f,Turn));
        UpdatePaperProps(X,Dt);TransitEffects(0);
    }
    else if(Shot==11)
    {
        const float T=FMath::Clamp(ShotTime/6.6f,0.f,1.f),X=FMath::Lerp(480.f,6200.f,T),Y=FMath::Sin(T*2*PI)*65;
        const float FrameRock=PaperEnding::Ease((ShotTime-4.8f)/1.8f);
        FVector Eye(X,Y,24+FMath::Sin(T*PI)*6+196*FrameRock);
        FVector Aim=FMath::Lerp(FVector(1,FMath::Cos(T*2*PI)*.025f,.012f*(1-T)).GetSafeNormal(),(FVector(8000,250,1100)-Eye).GetSafeNormal(),FrameRock);
        PaperPose(Eye,Aim,108,bComfortMode?0:FMath::Sin(T*2*PI)*4.5f);
        PaperRock->SetRelativeRotation(FRotator(-90*(1-PaperEnding::Ease((ShotTime-4.4f)/1.1f)),0,0));
        PaperPick->SetRelativeRotation(FRotator(-90*(1-PaperEnding::Ease((ShotTime-5.35f)/1.15f)),0,0));
        UpdatePaperProps(X,Dt);
    }
    else if(Shot==12)
    {
        const float T=FMath::Clamp(ShotTime/4.4f,0.f,1.f),Swing=PaperEnding::Ease(ShotTime/.75f);
        const float StrikeAngle=FMath::RadiansToDegrees(FMath::Atan2(800.f,1100.f)+FMath::Atan2(370.f,874.f));
        const float Recoil=PaperEnding::Ease((ShotTime-.86f)/1.1f);
        const float PickAngle=FMath::Lerp(FMath::Lerp(-35.f,StrikeAngle,Swing),-35.f,Recoil);
        PaperPickSwing->SetRelativeRotation(FQuat(FVector::ForwardVector,FMath::DegreesToRadians(PickAngle)));
        if(ShotTime>=.75f&&!bPaperRockBroken)
        {
            bPaperRockBroken=true;PaperRockCap->SetVisibility(false,true);
            for(auto Chip:PaperRockChips)Chip->SetVisibility(true,true);
            PaperCaveScene->SetVisibility(true,true);PaperCaveSeal->SetVisibility(false);
            Sound(TEXT("Connect"),.8f,.55f);
        }
        const float ShardAge=FMath::Max(0.f,ShotTime-.75f);
        for(int I=0;I<PaperRockChips.Num();++I)
        {
            const float A=I*2*PI/PaperRockChips.Num();
            PaperRockChips[I]->SetRelativeLocation(FVector(-ShardAge*100,FMath::Cos(A)*(290+ShardAge*980),700+FMath::Sin(A)*(290+ShardAge*980)-ShardAge*ShardAge*350));
            PaperRockChips[I]->SetRelativeRotation(FRotator(ShardAge*(55+I*11),ShardAge*(I%2?90:-75),ShardAge*60));
            PaperRockChips[I]->SetRelativeScale3D(FVector(1-PaperEnding::Ease((ShardAge-1.2f)/.9f)));
        }
        const float X=FMath::CubicInterp(6200.f,(5720.f/6.6f)*4.4f,8600.f,0.f,T);
        const float Z=220+330*PaperEnding::Ease(T/.72f);
        FVector P(X,0,Z),Target=FMath::Lerp(FMath::Lerp(FVector(8000,250,1100),FVector(8000,0,700),PaperEnding::Ease(T/.4f)),FVector(10500,0,350),PaperEnding::Ease((T-.45f)/.55f));
        const float Shake=bComfortMode?0:FMath::Exp(-ShardAge*8)*FMath::Sin(ShardAge*70)*(bPaperRockBroken?6:0);
        P.Y+=Shake;P.Z+=Shake*.6f;
        PaperPose(P,(Target-P).GetSafeNormal(),FMath::Lerp(108.f,65.f,PaperEnding::Ease(T)));
        if(X>8300){PaperCaveSeal->SetVisibility(true);PaperScene->SetVisibility(false,true);}
        UpdatePaperProps(10000,Dt);
    }
    else if(Shot==13)
    {
        const float T=PaperEnding::Ease(ShotTime/6.f),Angle=FMath::Lerp(PI,PI*.25f,T);
        const FVector P=FVector(1900*FMath::Cos(Angle),1900*FMath::Sin(Angle),550+100*T+120*FMath::Sin(T*PI));
        const FVector Centre=PaperEnding::Origin+FVector(10500,0,0);
        Pose(Centre+P,Centre+FVector(0,0,350),FMath::Lerp(65.f,55.f,T));
    }
    else if(Shot==14)
    {
        const float Back=PaperEnding::Ease(ShotTime/5.f);
        // A 16:9 screen exactly fills the preceding 55 degree camera frustum.
        const float MatchX=102-62.5f/FMath::Tan(FMath::DegreesToRadians(27.5f));
        Pose(PaperEnding::Desk+FVector(FMath::Lerp(MatchX,-200.f,Back),0,159),PaperEnding::Desk+FVector(108,0,159),55);
        CreditsScreenMaterial->SetScalarParameterValue(TEXT("Power"),1);
        CreditsScreenMaterial->SetScalarParameterValue(TEXT("CreditMix"),PaperEnding::Ease((ShotTime-5.8f)/1.6f));
        CreditsScreenLight->SetIntensity(110);bComplete=ShotTime>=8;
    }
}
