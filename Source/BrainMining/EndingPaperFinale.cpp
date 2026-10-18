#include "Subtitles/SubtitleSubsystem.h"
#include "EndingEmergence.h"
#include "EndingComicRock.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/AudioComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "UDynamicMesh.h"
#include "Engine/Texture2D.h"
#include "Components/MeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderingThread.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "MediaSoundComponent.h"
#include "GameFramework/PlayerController.h"

namespace PaperEnding
{
    const FVector Origin(2209150,0,0);
    constexpr float FlightSpeed=(6200.f-480.f)/6.6f;
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
        if(!Mesh.HasAttributes())Mesh.EnableAttributes();
        UE::Geometry::FMeshNormals::InitializeOverlayToPerTriangleNormals(Mesh.Attributes()->PrimaryNormals());
        C->SetMesh(MoveTemp(Mesh));auto* M=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/EndingEmergence/Materials/M_EE_"))+Material)),C);
        M->SetVectorParameterValue(TEXT("Tint"),Tint);C->SetMaterial(0,M);C->RegisterComponent();AddInstanceComponent(C);return M;
    };
    // The portal is the FRONT of the future desktop. Camera approaches it from
    // positive local Z, looking DOWN, then lifts its gaze by ninety degrees.
    UE::Geometry::FDynamicMesh3 Sheet;Sheet.EnableAttributes();
    // Keep the distant portal genuinely circular.  A large quad with an
    // aperture parameter still exposes its square silhouette for a frame when
    // the camera approaches it, which reads as the unwanted desktop block.
    auto* SheetUV=Sheet.Attributes()->PrimaryUV();
    const int SheetCentre=Sheet.AppendVertex(FVector::ZeroVector);
    const int SheetCentreUV=SheetUV->AppendElement(FVector2f(.5f,.5f));
    constexpr int SheetSegments=64; constexpr float SheetRadius=60000.f;
    TArray<int> SheetVerts;TArray<int> SheetUVs;
    for(int I=0;I<SheetSegments;++I)
    {
        const float A=2*PI*I/SheetSegments;
        SheetVerts.Add(Sheet.AppendVertex(FVector(FMath::Cos(A)*SheetRadius,FMath::Sin(A)*SheetRadius,0)));
        SheetUVs.Add(SheetUV->AppendElement(FVector2f(.5f+.5f*FMath::Cos(A),.5f+.5f*FMath::Sin(A))));
    }
    for(int I=0;I<SheetSegments;++I)
    {
        const int J=(I+1)%SheetSegments;
        const int T=Sheet.AppendTriangle(SheetCentre,SheetVerts[J],SheetVerts[I]);
        SheetUV->SetTriangle(T,UE::Geometry::FIndex3i(SheetCentreUV,SheetUVs[J],SheetUVs[I]));
    }
    PaperSurfaceMaterial=MeshPart(TEXT("WhitePortalBecomesDesktop"),MoveTemp(Sheet),PaperScene,TEXT("PaperFloor"),Cream);
    PaperSurface=CastChecked<USceneComponent>(PaperSurfaceMaterial->GetOuter());
    // The pickaxe is reserved for the final giant paper cutout.
    const TCHAR* DoodleNames[]={TEXT("toilet"),TEXT("warnings"),TEXT("glasses"),TEXT("cards"),TEXT("fountain"),TEXT("thought"),TEXT("computer"),TEXT("brain"),TEXT("solar"),TEXT("ants"),TEXT("roguelike")};
    // Measured alpha bounds of the original drawings, plus room for the cut edge.
    const FIntVector4 Bounds[]={ {255,29,1026,1225},{179,166,1045,1098},{88,358,1169,897},{104,199,1150,1054},{186,61,1067,1198},{141,145,1152,1122},{113,166,1144,1126},{198,160,1056,1097},{64,405,1210,869},{64,321,1206,940},{282,140,989,1150} };
    const FLinearColor Palette[]={FLinearColor(1,.045f,.27f),FLinearColor(.02f,.7f,1),FLinearColor(1,.65f,.025f),FLinearColor(.42f,.06f,.95f),FLinearColor(.04f,.92f,.51f)};
    FRandomStream R(191067);PaperCardCount=0;
    PaperPopStrokes=Instances(TEXT("PaperPopInkBursts"),TEXT("Cube"),FLinearColor(1,.7f,.02f),PaperScene,TEXT("PaperComic"));
    for(int I=0;I<24;++I)
    {
        const int DrawingIndex=I%11,Side=I%2?1:-1;
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
    PaperRock=Group(TEXT("GiantHingedRockCutout"),FVector(8000,0,-460),PaperScene);
    PaperRockCap=Group(TEXT("BreakablePaperRockCentre"),FVector::ZeroVector,PaperRock);
    // A cut-paper silhouette uses a deliberately uneven polygon rather than a
    // soft radial hole. The sharp facets remain readable when the pickaxe hits.
    constexpr int Sides=12;
    FRandomStream RockRandom(771);
    TArray<FVector> Outer,Inner;
    for(int I=0;I<Sides;++I)
    {
        const float A=I*2*PI/Sides+RockRandom.FRandRange(-.055f,.055f);
        const float Y=FMath::Cos(A),Z=FMath::Sin(A);
        const float Edge=FMath::Min(1100/FMath::Max(FMath::Abs(Y),.001f),(Z>=0?1074.f:700.f)/FMath::Max(FMath::Abs(Z),.001f))*RockRandom.FRandRange(.88f,1.08f);
        Outer.Add(FVector(0,Y*Edge,700+Z*Edge));
        const float InnerRadius=RockRandom.FRandRange(350.f,475.f);
        Inner.Add(FVector(0,Y*InnerRadius,700+Z*InnerRadius));
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
    PaperPick=Group(TEXT("GiantHingedPickaxeCutout"),FVector(7940,1100,-460),PaperScene);
    PaperPickSwing=Group(TEXT("PickaxeStrikePivot"),FVector(0,0,1500),PaperPick);
    // Existing doodle, hinged at its handle butt (240,1090). The measured sharp
    // tip (1114,720) lands at Y=0,Z=700; texture alpha supplies the cut contour.
    const float PickScale=FMath::Sqrt((1100.f*1100+800.f*800)/(874.f*874+370.f*370));
    auto PickPoint=[&](float X,float Y){return FVector(0,(240-X)*PickScale,(1090-Y)*PickScale);};
    UE::Geometry::FDynamicMesh3 PickDrawing;PickDrawing.EnableAttributes();
    PaperEnding::Quad(PickDrawing,PickPoint(171,1170),PickPoint(1163,1170),PickPoint(1163,104),PickPoint(171,104),FVector4f(171/1254.f,104/1254.f,1163/1254.f,1170/1254.f));
    auto* PickMat=MeshPart(TEXT("OriginalDoodleGiantPickaxe"),MoveTemp(PickDrawing),PaperPickSwing,TEXT("PaperHeroDoodle"),FLinearColor::White);
    PickMat->SetTextureParameterValue(TEXT("Doodle"),LoadObject<UTexture2D>(nullptr,TEXT("/Game/EndingEmergence/PaperFinale/Doodles/T_EE_Doodle_pickaxe")));

    // The same projected video is visible through the physical aperture before
    // the camera crosses it, then fills the screen without a second scene/cut.
    PaperCaveScene=Group(TEXT("FinaleVideoAperture"),PaperEnding::Origin+FVector(8002,0,240));
    // Bound the display by the actual cut polygon. A full-screen sheet plus a
    // world-position mask can expose the film outside the rock during shader
    // preparation; physical geometry cannot leak beyond this opening.
    UE::Geometry::FDynamicMesh3 VideoOpening;VideoOpening.EnableAttributes();
    const int VideoCentre=VideoOpening.AppendVertex(FVector::ZeroVector);
    TArray<int> VideoRim;
    for(const FVector& Point:Inner)VideoRim.Add(VideoOpening.AppendVertex(Point-FVector(0,0,700)));
    for(int I=0;I<Sides;++I)
    {
        VideoOpening.AppendTriangle(VideoCentre,VideoRim[(I+1)%Sides],VideoRim[I]);
    }
    FinaleVideoMaterial=MeshPart(TEXT("FinaleVideoSurface"),MoveTemp(VideoOpening),PaperCaveScene,TEXT("VideoPortal"),FLinearColor::White);
    TArray<USceneComponent*> PaperParts;PaperScene->GetChildrenComponents(true,PaperParts);
    for(auto* Part:PaperParts)if(auto* Mesh=Cast<UMeshComponent>(Part))for(int I=0;I<Mesh->GetNumMaterials();++I)
        if(auto* M=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(I)))PaperArtMaterials.AddUnique(M);
    ResetPaperFinale();PaperScene->SetVisibility(false,true);

    // The supplied film replaces the old ceiling/desk/developer geometry.
}

void AEndingEmergence::PrepareFinaleVideo()
{
    if(!FinaleVideoSource)FinaleVideoSource=LoadObject<UMediaSource>(nullptr,TEXT("/Game/EndingEmergence/PaperFinale/MS_EndingFinale"));
    if(!FinaleVideoSource||FinaleVideoPlayer)return;
    FinaleVideoPlayer=NewObject<UMediaPlayer>(this);
    FinaleVideoPlayer->PlayOnOpen=false;FinaleVideoPlayer->SetLooping(false);
    FinaleVideoPlayer->OnMediaOpened.AddDynamic(this,&AEndingEmergence::FinaleVideoOpened);
    FinaleVideoPlayer->OnEndReached.AddDynamic(this,&AEndingEmergence::FinaleVideoEnded);
    FinaleVideoTexture=NewObject<UMediaTexture>(this);
    FinaleVideoTexture->NewStyleOutput=true;FinaleVideoTexture->AutoClear=false;
    FinaleVideoTexture->SetMediaPlayer(FinaleVideoPlayer);FinaleVideoTexture->UpdateResource();
    // Bind the decoder long before the rock appears. Keep the exact still frame
    // on screen until real video samples arrive, without changing texture types
    // (and preparing new shaders) during the final rush.
    FinaleVideoMaterial->SetTextureParameterValue(TEXT("VideoFrame"),FinaleVideoTexture);
    FinaleVideoMaterial->SetScalarParameterValue(TEXT("VideoBlend"),0);
    FinaleVideoSound=NewObject<UMediaSoundComponent>(this,NAME_None,RF_Transient);
    FinaleVideoSound->bIsUISound=true;FinaleVideoSound->SetMediaPlayer(FinaleVideoPlayer);
    FinaleVideoSound->SetVolumeMultiplier(0);FinaleVideoSound->RegisterComponent();FinaleVideoSound->Start();
    // Asynchronous preparation starts well before impact, never during the hit.
    if(!FinaleVideoPlayer->OpenSource(FinaleVideoSource))
        UE_LOG(LogTemp,Warning,TEXT("Ending video could not open; keeping the preview frame."));
}

void AEndingEmergence::FinaleVideoOpened(FString OpenedUrl)
{
    if(!FinaleVideoPlayer||!FinaleVideoMaterial)return;
    const float Aspect=FinaleVideoPlayer->GetVideoTrackAspectRatio(INDEX_NONE,INDEX_NONE);
    if(Aspect>0)FinaleVideoMaterial->SetScalarParameterValue(TEXT("VideoAspect"),Aspect);
    // Decode a few samples silently, then park at frame zero while the player
    // approaches. The imported exact first frame covers the aperture meanwhile.
    FinaleVideoPlayer->Play();
    if(bFinaleVideoRequested)StartFinaleVideo();
}

void AEndingEmergence::StartFinaleVideo()
{
    bFinaleVideoRequested=true;
    if(FinaleVideoPlayer&&FinaleVideoPlayer->IsReady())
    {
        if(FinaleVideoSound)FinaleVideoSound->SetVolumeMultiplier(1);
        if(Ambience)Ambience->AdjustVolume(.35f,0);
        FinaleVideoPlayer->Play();
    }
}

void AEndingEmergence::FinaleVideoEnded(){if(bFinaleVideoRequested)bComplete=true;}

void AEndingEmergence::ResetFinaleVideo()
{
    if(FinaleVideoPlayer)FinaleVideoPlayer->Close();
    if(FinaleVideoSound){FinaleVideoSound->Stop();FinaleVideoSound->DestroyComponent();}
    FinaleVideoSound=nullptr;FinaleVideoPlayer=nullptr;FinaleVideoTexture=nullptr;bFinaleVideoRequested=false;
    if(FinaleVideoMaterial)FinaleVideoMaterial->SetScalarParameterValue(TEXT("VideoBlend"),0);
    if(FinaleVideoMaterial)if(auto* FirstFrame=LoadObject<UTexture2D>(nullptr,TEXT("/Game/EndingEmergence/PaperFinale/T_EndingFinaleFirstFrame")))FinaleVideoMaterial->SetTextureParameterValue(TEXT("VideoFrame"),FirstFrame);
}

void AEndingEmergence::ResetPaperFinale()
{
    ResetFinaleVideo();
    if(PaperCaveScene)PaperCaveScene->SetRelativeLocation(PaperEnding::Origin+FVector(8002,0,240));
    if(PaperScene)PaperScene->SetRelativeRotation(FRotator(90,0,0));
    for(auto M:PaperArtMaterials)M->SetScalarParameterValue(TEXT("WhiteBlend"),0);
    if(PaperSurfaceMaterial){PaperSurfaceMaterial->SetScalarParameterValue(TEXT("DotsReveal"),0);PaperSurfaceMaterial->SetScalarParameterValue(TEXT("ApertureRadius"),35);}
    bPaperRockBroken=false;LastPaperSubtitle=-1;bMealSubtitle=false;
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
    if(!bMealSubtitle)
    {
        const TCHAR* Captions[]={TEXT("马桶"),TEXT("警告"),TEXT("眼镜"),TEXT("卡牌"),TEXT("喷泉"),TEXT("思考"),TEXT("电脑"),TEXT("大脑"),TEXT("太阳系"),TEXT("蚂蚁"),TEXT("肉鸽")};
        int32 Pair=LastPaperSubtitle;
        for(int32 I=0;I+1<PaperCards.Num();I+=2)if(CameraX+600>PaperCards[I]->GetRelativeLocation().X)Pair=FMath::Max(Pair,I/2);
        if(Pair!=LastPaperSubtitle)
        {
            LastPaperSubtitle=Pair;
            if(auto* Sub=USubtitleSubsystem::Get(this)){Sub->StopAll();Sub->PlayStatusLine(FText::FromString(FString(Captions[(Pair*2)%11])+TEXT(" · ")+Captions[(Pair*2+1)%11]),.8f);}
        }
    }
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
    if(bFinaleVideoRequested&&FinaleVideoPlayer&&FinaleVideoPlayer->IsPlaying()&&FinaleVideoTexture&&FinaleVideoTexture->GetWidth()>0)
        FinaleVideoMaterial->SetScalarParameterValue(TEXT("VideoBlend"),PaperEnding::Ease(float(FinaleVideoPlayer->GetTime().GetTotalSeconds()-.04)/.12f));
    if(FinaleVideoPlayer&&!bFinaleVideoRequested&&FinaleVideoPlayer->IsPlaying()&&FinaleVideoPlayer->GetTime().GetTotalSeconds()>.08)
    {
        FinaleVideoPlayer->Pause();FinaleVideoPlayer->Seek(FTimespan::Zero());
    }
    if(FinaleVideoMaterial&&Player)
    {
        int32 Width=0,Height=0;Player->GetViewportSize(Width,Height);
        if(Height>0)FinaleVideoMaterial->SetScalarParameterValue(TEXT("ViewportAspect"),float(Width)/Height);
    }
    auto PaperPose=[&](FVector P,FVector Direction,float Fov,float Roll=0.f)
    {
        const FTransform T=PaperScene->GetRelativeTransform();Pose(T.TransformPosition(P),T.TransformPosition(P+Direction*1000),Fov,Roll);
    };
    if(Shot==10)
    {
        UpdateFinaleExit(Dt);
        // First fly through black space to the same physical white disk that
        // becomes the desktop. Only turn after the aperture fills the view.
        const float Flight=FMath::Clamp(ShotTime/2.6f,0.f,1.f);
        const float PaperTime=FMath::Max(0.f,ShotTime-2.6f);
        const float T=FMath::Clamp(PaperTime/1.1f,0.f,1.f),Turn=PaperEnding::Ease(T);
        // Approach the same 75 cm diameter disk until it covers the viewport.
        // Extend its unseen edge gradually during the turn, never in a 40 ms jump.
        PaperSurfaceMaterial->SetScalarParameterValue(TEXT("ApertureRadius"),37.5f+90000.f*FMath::Pow(T,4.f));
        PaperSurfaceMaterial->SetScalarParameterValue(TEXT("DotsReveal"),0);
        if(!PaperScene->IsVisible())PaperScene->SetVisibility(true,true);
        if(PaperTime>0&&PaperCards.Num()>0&&!PaperCards[0]->IsVisible())
        {
            TArray<USceneComponent*> PaperParts;PaperScene->GetChildrenComponents(true,PaperParts);
            for(auto* Part:PaperParts)if(Part!=PaperSurface)Part->SetVisibility(true,true);
        }
        PaperScene->SetRelativeRotation(FRotator(90*(1-Turn),0,0));
        // Match the drilling endpoint at Z=1350 and the next flight at Z=90.
        // The x rail ends at the next shot's exact position and forward speed.
        const float FlightZ=PaperTime>0?FMath::Lerp(24.f,90.f,Turn):FMath::Lerp(1350.f,24.f,Flight);
        const float ExitSpeed=(6200.f-480.f)/6.6f;
        const float X=480*FMath::Pow(T,ExitSpeed*1.1f/480),Pitch=-HALF_PI*(1-Turn);
        const float Side=FinaleSide*(1-PaperEnding::Ease(Flight));
        PaperPose(FVector(X,Side,FlightZ),FVector(FMath::Cos(Pitch),0,FMath::Sin(Pitch)),FMath::Lerp(94.f,108.f,Turn));
        UpdatePaperProps(X,Dt);TransitEffects(0);
    }
    else if(Shot==11)
    {
        const float T=FMath::Clamp(ShotTime/6.6f,0.f,1.f),X=FMath::Lerp(480.f,6200.f,T),Y=FMath::Sin(T*2*PI)*65;
        const float FrameRock=PaperEnding::Ease((ShotTime-4.8f)/1.8f);
        // Keep the flight at one height.  The paper rock is lowered to this
        // sightline, so the approach never has to pitch down or up to find it.
        FVector Eye(X,Y,90);
        FVector Aim=FMath::Lerp(FVector(1,FMath::Cos(T*2*PI)*.025f,0).GetSafeNormal(),(FVector(8000,0,240)-Eye).GetSafeNormal(),FrameRock);
        PaperPose(Eye,Aim,108,bComfortMode?0:FMath::Sin(T*2*PI)*4.5f);
        if(ShotTime>=4.05f && !bMealSubtitle)
        {bMealSubtitle=true;if(auto* Sub=USubtitleSubsystem::Get(this)){Sub->StopAll();Sub->PlayStatusLine(FText::FromString(TEXT("走吧，吃饭吧")),4.f);}}
        PaperRock->SetRelativeRotation(FRotator(-90*(1-PaperEnding::Ease((ShotTime-4.05f)/.70f)),0,0));
        PaperPick->SetRelativeRotation(FRotator(-90*(1-PaperEnding::Ease((ShotTime-4.95f)/.65f)),0,0));
        // Submit the film surface while the upright, unbroken rock still covers
        // it. First-use shader preparation must finish before the cap flies away.
        if(ShotTime>=4.75f&&!PaperCaveScene->IsVisible())PaperCaveScene->SetVisibility(true,true);
        UpdatePaperProps(X,Dt);
    }
    else if(Shot==12)
    {
        const float T=FMath::Clamp(ShotTime/2.8f,0.f,1.f),Swing=PaperEnding::Ease(ShotTime/.75f);
        const float StrikeAngle=FMath::RadiansToDegrees(FMath::Atan2(800.f,1100.f)+FMath::Atan2(370.f,874.f));
        const float Recoil=PaperEnding::Ease((ShotTime-.86f)/1.1f);
        const float PickAngle=FMath::Lerp(FMath::Lerp(-35.f,StrikeAngle,Swing),-35.f,Recoil);
        PaperPickSwing->SetRelativeRotation(FQuat(FVector::ForwardVector,FMath::DegreesToRadians(PickAngle)));
        if(ShotTime>=.75f&&!bPaperRockBroken)
        {
            if(auto* Sub=USubtitleSubsystem::Get(this)){Sub->StopAll();Sub->PlayStatusLine(FText::FromString(TEXT("等一下！")),5.f);}
            bPaperRockBroken=true;PaperRockCap->SetVisibility(false,true);
            for(auto Chip:PaperRockChips)Chip->SetVisibility(true,true);
            PaperCaveScene->SetVisibility(true,true);
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
        // Continue the exact incoming forward speed, including through impact.
        // The old ease-in restarted at zero here and looked like a hit-stop.
        const float X=6200.f+PaperEnding::FlightSpeed*ShotTime;
        const float Side=65*FMath::Sin((6.6f+ShotTime)*2*PI/6.6f)*(1-PaperEnding::Ease(ShotTime/1.2f));
        const FVector Eye(X,Side,FMath::Lerp(90.f,240.f,PaperEnding::Ease(ShotTime/2.1f)));
        const FVector ToHole=(FVector(8000,0,240)-Eye).GetSafeNormal();
        const FVector Facing=FMath::Lerp(ToHole,FVector::ForwardVector,PaperEnding::Ease((ShotTime-1.1f)/.9f)).GetSafeNormal();
        const float Roll=bComfortMode?0:FMath::Sin((6.6f+ShotTime)*2*PI/6.6f)*4.5f*(1-PaperEnding::Ease(ShotTime/1.2f));
        PaperPose(Eye,Facing,108,Roll);
        if(X>8005)PaperScene->SetVisibility(false,true);
        // Keep the already screen-filling opening ahead of the near clip plane.
        // Screen-space sampling makes this invisible; playback starts at the rim.
        if(X>=7950)
            PaperCaveScene->SetWorldLocation(Camera->GetActorLocation()+Camera->GetActorForwardVector()*50);
        if(X>=8000&&!bFinaleVideoRequested)StartFinaleVideo();
        UpdatePaperProps(X,Dt);
    }
    else if(Shot>=13)
    {
        // Once the aperture fills the view, keep the exact same video projection.
        // No ceiling scene, new camera, seek or full-screen player is introduced.
        PaperPose(FVector(6200.f+PaperEnding::FlightSpeed*Duration(12),0,240),FVector::ForwardVector,108);
        PaperCaveScene->SetWorldLocation(Camera->GetActorLocation()+Camera->GetActorForwardVector()*50);
        if(!FinaleVideoSource)bComplete=true;

    }
}
