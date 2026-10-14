#include "EndingEmergence.h"
#include "DiggableRock.h"
#include "MiningComponent.h"
#include "BrainMiningPlayerController.h"
#include "MiningInteractionHUDWidget.h"
#include "MiningCharacterMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "PhysicsEngine/BodySetup.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/HUD.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "Widgets/SCompoundWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace Ending
{
    const FName Tag(TEXT("EmergenceEndingGenerated"));
    const FLinearColor Gold(.95f,.47f,.09f), Teal(.035f,.62f,.58f), Blue(.065f,.18f,.42f), Rose(.48f,.035f,.085f);
    const FVector City(22000,0,0), Earth(50000,0,0), Cosmos(95000,0,0), Desk(155000,0,0);
    const float GateX[]={1600,5800,7300,8650,10000};
    const float GateY[]={0,0,-170,170,0};
    float Ease(float V) { V=FMath::Clamp(V,0.f,1.f); return V*V*(3-2*V); }
    const FVector BrainCenter(16500,0,-5500);
    constexpr float BrainRadius=6500;
    constexpr int NeuronCount=22;
    int Layer(int I) { return I==0?0:I==NeuronCount-1?5:1+(I-1)/5; }
    float Ground(float X,float Y=0)
    {
        if(X<1600)return 850-1140*FMath::Sqrt(FMath::Max(0.f,1-X*X/(2400*2400.f)-Y*Y/(1300*1300.f)));
        if(X<=12000)return 850-FMath::Sqrt(FMath::Max(0.f,850*850.f-Y*Y));
        return BrainCenter.Z+FMath::Sqrt(FMath::Max(0.f,BrainRadius*BrainRadius-FMath::Square(X-BrainCenter.X)-Y*Y));
    }
}

class SEmergenceEndingView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SEmergenceEndingView) {} SLATE_ARGUMENT(AEndingEmergence*, Owner) SLATE_END_ARGS()
    void Construct(const FArguments& A) { Owner=A._Owner; }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 L,const FWidgetStyle&,bool) const override
    {
        if(!Owner.IsValid()) return L;
        const auto* D=Owner.Get(); const FVector2D Size=G.GetLocalSize();
        const float S=FMath::Min(Size.X/1600,Size.Y/900); const FVector2D O=(Size-FVector2D(1600,900)*S)*.5;
        auto Geo=[&](float X,float Y,float W,float H){return G.ToPaintGeometry(FVector2f(W*S,H*S),FSlateLayoutTransform(FVector2f(O+FVector2D(X,Y)*S)));};
        auto Box=[&](float X,float Y,float W,float H,FLinearColor C){FSlateDrawElement::MakeBox(Out,++L,Geo(X,Y,W,H),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,C);};
        auto Text=[&](const FString& T,float X,float Y,int F,FLinearColor C){FSlateDrawElement::MakeText(Out,++L,Geo(X,Y,1500,70),T,FCoreStyle::GetDefaultFontStyle("Regular",FMath::Max(9,FMath::RoundToInt(F*S))),ESlateDrawEffect::None,C);};
        // A hard cut to complete black; no labels or controls may leak through.
        if(D->Shot==7){Box(-O.X/S,-O.Y/S,Size.X/S,Size.Y/S,FLinearColor::Black);return L;}
        if(D->Shot==8 || (D->Shot==9&&D->ShotTime<2))return L;
        const FLinearColor Ink(.007f,.012f,.022f,.78f), Paper(.94f,.9f,.8f), Muted(.55f,.64f,.65f);
        const float Flash=D->OverlayAlpha();
        if(Flash>0) Box(-O.X/S,-O.Y/S,Size.X/S,Size.Y/S,FLinearColor(.86f,.96f,.95f,Flash));
        if(D->Shot>=0) {Box(0,0,1600,48,FLinearColor::Black);Box(0,852,1600,48,FLinearColor::Black);}
        if(D->Shot<0 || D->ShotTime<4 || D->Shot>=7)
        {
            Box(40,74,3,53,Ending::Gold);
            Text(TEXT("涌  现"),60,72,23,Paper);
            Text(D->Title(),61,108,12,Muted);
        }
        if(D->Shot<0&&D->GateIndex<5)
        {
            const auto* P=D->GetWorld()->GetFirstPlayerController();
            const auto* M=P&&P->GetPawn()?P->GetPawn()->FindComponentByClass<UMiningComponent>():nullptr;
            const FLinearColor C=D->GateIndex==2?(D->TargetNeuron>=0?Ending::Gold:Paper):M&&M->TargetRock?Ending::Teal:Paper;
            Box(795,449,10,2,C); Box(799,445,2,10,C);
            if(D->GateIndex==2)
            {
                Text(FString::Printf(TEXT("脉冲跃迁 %d 次  ·  汇向最大的金色神经元"),D->PossessionCount),61,143,13,Muted);
                for(int I=0;I<D->NeuronPositions.Num();++I)if(D->CanPossess(I))
                {
                    FVector2D Screen;
                    if(P&&P->ProjectWorldLocationToScreen(D->NeuronWorldPosition(I),Screen,false))
                    {
                        // Projection returns viewport pixels; Slate's geometry
                        // can have a different DPI/render scale (including PIE).
                        int32 Width=0,Height=0;P->GetViewportSize(Width,Height);
                        if(Width<=0||Height<=0)continue;
                        Screen*=FVector2D(Size.X/Width,Size.Y/Height);
                        const float X=(Screen.X-O.X)/S,Y=(Screen.Y-O.Y)/S;
                        if(X>45&&X<1555&&Y>160&&Y<710)
                        {
                            const FLinearColor Mark=I==D->TargetNeuron?Ending::Gold:Ending::Teal;
                            const float R=I==D->TargetNeuron?26:18;
                            Box(X-R,Y-R,10,2,Mark);Box(X-R,Y-R,2,10,Mark);
                            Box(X+R-10,Y+R,10,2,Mark);Box(X+R,Y+R-8,2,10,Mark);
                        }
                    }
                }
            }
        }
        const FString Sub=D->Subtitle();
        if(!Sub.IsEmpty()) {Box(220,722,1160,66,Ink);Text(Sub,248,739,23,Paper);}
        Text(D->Hint(),61,811,14,Paper);
        if(D->Shot<0) Text(D->GateIndex==0?TEXT("WASD 移动   鼠标 观察   左键 挖掘   R 重玩   P 镜头预览"):
            D->GateIndex==1?TEXT("自动漂浮前进   W 加速   S 无效   A / D 微调   鼠标 观察   C 平稳   R 重玩"):
            D->GateIndex==2?TEXT("鼠标 瞄准神经元   空格 穿梭附身   C 平稳   R 重玩   P 镜头预览"):TEXT("R 重玩"),61,865,12,Muted);
        else Text(D->bComfortMode?TEXT("C 恢复漂移   ·   R 重玩"):TEXT("C 平稳镜头   ·   R 重玩"),1210,866,11,Muted);
        if(D->bComplete)
        {
            Text(TEXT("一个点子诞生的下午"),61,162,20,Paper);
        }
        return L;
    }
private:
    TWeakObjectPtr<AEndingEmergence> Owner;
};

AEndingEmergenceGameMode::AEndingEmergenceGameMode() { HUDClass=AHUD::StaticClass(); }
AEndingEmergence::AEndingEmergence()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

USceneComponent* AEndingEmergence::Group(const TCHAR* Name,FVector Position,USceneComponent* Parent)
{
    auto* C=NewObject<USceneComponent>(this,MakeUniqueObjectName(this,USceneComponent::StaticClass(),Name),RF_Transactional);
    AddInstanceComponent(C); C->ComponentTags.Add(Ending::Tag); C->SetupAttachment(Parent?Parent:RootComponent.Get());
    C->SetRelativeLocation(Position); C->RegisterComponent(); return C;
}

UStaticMeshComponent* AEndingEmergence::Shape(const TCHAR* Name,FVector Position,FVector Size,FLinearColor Tint,const TCHAR* Mesh,USceneComponent* Parent,const TCHAR* Material,bool Collision)
{
    auto* C=NewObject<UStaticMeshComponent>(this,MakeUniqueObjectName(this,UStaticMeshComponent::StaticClass(),Name),RF_Transactional);
    AddInstanceComponent(C); C->ComponentTags.Add(Ending::Tag); C->SetupAttachment(Parent?Parent:RootComponent.Get());
    C->SetMobility(EComponentMobility::Movable); C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*(FString(TEXT("/Engine/BasicShapes/"))+Mesh)));
    C->SetRelativeLocation(Position); C->SetRelativeScale3D(Size/100); C->SetCollisionProfileName(Collision?TEXT("BlockAll"):TEXT("NoCollision"));
    C->SetGenerateOverlapEvents(false); C->SetCanEverAffectNavigation(false); C->SetCastShadow(false);
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/EndingEmergence/Materials/M_EE_"))+Material)))
    {auto* M=UMaterialInstanceDynamic::Create(Base,C);M->SetVectorParameterValue(TEXT("Tint"),Tint);C->SetMaterial(0,M);}
    C->RegisterComponent();return C;
}

UStaticMeshComponent* AEndingEmergence::Rod(FVector A,FVector B,float Width,FLinearColor Tint,USceneComponent* Parent,const TCHAR* Material)
{
    auto* C=Shape(TEXT("Connection"),(A+B)*.5,FVector(Width,Width,(B-A).Size()),Tint,TEXT("Cylinder"),Parent,Material);
    C->SetRelativeRotation(FRotationMatrix::MakeFromZ(B-A).Rotator());return C;
}

void AEndingEmergence::Ring(FVector Center,float Radius,FLinearColor Tint,USceneComponent* Parent,int32 Plane,float Width)
{
    for(int I=0;I<64;++I)
    {
        auto P=[&](float A){ const float X=FMath::Cos(A)*Radius,Y=FMath::Sin(A)*Radius;return Center+(Plane==0?FVector(0,X,Y):Plane==1?FVector(X,Y,0):FVector(X,0,Y));};
        Rod(P(I*2*PI/64),P((I+1)*2*PI/64),Width,Tint,Parent);
    }
}

UInstancedStaticMeshComponent* AEndingEmergence::Instances(const TCHAR* Name,const TCHAR* Mesh,FLinearColor Tint,USceneComponent* Parent,const TCHAR* Material)
{
    auto* C=NewObject<UInstancedStaticMeshComponent>(this,MakeUniqueObjectName(this,UInstancedStaticMeshComponent::StaticClass(),Name),RF_Transactional);
    AddInstanceComponent(C);C->ComponentTags.Add(Ending::Tag);C->SetupAttachment(Parent);C->SetMobility(EComponentMobility::Movable);
    C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*(FString(TEXT("/Engine/BasicShapes/"))+Mesh)));
    C->SetCollisionProfileName(TEXT("NoCollision"));C->SetCastShadow(false);C->SetCanEverAffectNavigation(false);
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/EndingEmergence/Materials/M_EE_"))+Material)))
    {auto* M=UMaterialInstanceDynamic::Create(Base,C);M->SetVectorParameterValue(TEXT("Tint"),Tint);C->SetMaterial(0,M);}
    C->RegisterComponent();return C;
}

void AEndingEmergence::Person(FVector P,float S,FLinearColor Tint,USceneComponent* Parent)
{
    Shape(TEXT("PersonHead"),P+FVector(0,0,162)*S,FVector(35,34,42)*S,FLinearColor(.6f,.38f,.24f),TEXT("Sphere"),Parent);
    Shape(TEXT("Hair"),P+FVector(-4,0,174)*S,FVector(34,36,25)*S,FLinearColor(.035f,.025f,.023f),TEXT("Sphere"),Parent);
    Shape(TEXT("Shirt"),P+FVector(0,0,112)*S,FVector(42,59,65)*S,Tint,TEXT("Sphere"),Parent);
    for(int Side:{-1,1})
    {
        Rod(P+FVector(0,Side*15,82)*S,P+FVector(8,Side*18,12)*S,17*S,FLinearColor(.035f,.052f,.072f),Parent,TEXT("Surface"));
        Rod(P+FVector(0,Side*30,130)*S,P+FVector(12,Side*34,85)*S,12*S,Tint,Parent,TEXT("Surface"));
    }
}

void AEndingEmergence::Shell(const TCHAR* Name,const TArray<FVector>& Centers,const TArray<FVector2D>& Radii,FLinearColor Tint,const TCHAR* Material,bool FollowCurve)
{
    using namespace UE::Geometry;
    FDynamicMesh3 Mesh(true,false,false,false);
    constexpr int Sides=96;
    for(int Row=0;Row<Centers.Num();++Row)for(int I=0;I<Sides;++I)
    {
        const float A=I*2*PI/Sides;
        const FVector Offset(0,FMath::Cos(A)*Radii[Row].X,FMath::Sin(A)*Radii[Row].Y);
        const FRotator Frame=(Centers[FMath::Min(Row+1,Centers.Num()-1)]-Centers[FMath::Max(Row-1,0)]).Rotation();
        Mesh.AppendVertex(FVector3d(Centers[Row]+(FollowCurve?Frame.RotateVector(Offset):Offset)));
    }
    for(int Row=0;Row<Centers.Num()-1;++Row)for(int I=0;I<Sides;++I)
    {
        const int A=Row*Sides+I,B=Row*Sides+(I+1)%Sides,C=A+Sides,D=B+Sides;
        // Chaos collision uses the opposite triangle winding to mesh normals.
        Mesh.AppendTriangle(A,B,C);Mesh.AppendTriangle(B,D,C);
    }
    FMeshNormals::QuickComputeVertexNormals(Mesh);Mesh.EnableAttributes();
    FMeshNormals::InitializeOverlayToPerVertexNormals(Mesh.Attributes()->PrimaryNormals());
    auto* C=NewObject<UDynamicMeshComponent>(this,MakeUniqueObjectName(this,UDynamicMeshComponent::StaticClass(),Name),RF_Transactional);
    AddInstanceComponent(C);C->ComponentTags.Add(Ending::Tag);C->SetupAttachment(Sets[0]);
    C->SetEnableRaytracing(false);C->SetVisibleInRayTracing(false);C->SetCastShadow(false);
    C->SetCollisionProfileName(TEXT("BlockAll"));C->SetGenerateOverlapEvents(false);C->SetCanEverAffectNavigation(false);
    C->SetComplexAsSimpleCollisionEnabled(true,false);C->bUseAsyncCooking=false;
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/EndingEmergence/Materials/M_EE_"))+Material)))
    {auto* M=UMaterialInstanceDynamic::Create(Base,C);M->SetVectorParameterValue(TEXT("Tint"),Tint);C->SetMaterial(0,M);}
    C->SetMesh(MoveTemp(Mesh));C->RegisterComponent();
    C->GetBodySetup()->bDoubleSidedGeometry=false;C->GetBodySetup()->SetFlags(RF_Transient);C->UpdateCollision(false);
}

void AEndingEmergence::BuildJourney()
{
    auto* Set=Sets[0].Get();FRandomStream R(803);
    TArray<FVector> Centers;TArray<FVector2D> Radii;
    // A hollow ellipsoid with a real curved floor and an open neck at the exit.
    for(int Row=0;Row<=96;++Row)
    {
        const float X=FMath::Lerp(-2399.f,1600.f,Row/96.f);
        const float Cross=FMath::Sqrt(FMath::Max(.0001f,1-X*X/(2400*2400.f)));
        Centers.Add(FVector(X,0,850));Radii.Add(FVector2D(1300*Cross,1140*Cross));
    }
    Shell(TEXT("EllipsoidalCellInterior"),Centers,Radii,Ending::Teal*.28f,TEXT("CellInterior"));
    for(int I=0;I<27;++I)
    {
        const float X=R.FRandRange(-1300,1300),A=R.FRandRange(-.25f,PI+ .25f);
        const float Cross=FMath::Sqrt(1-X*X/(2400*2400.f));
        const FVector P(X,FMath::Cos(A)*950*Cross,850+FMath::Sin(A)*800*Cross);
        auto* C=Shape(TEXT("Organelle"),P,FVector(130,240,100),I%3?Ending::Teal*.6f:Ending::Rose,TEXT("Sphere"),Set,TEXT("Organic"));
        C->SetRelativeRotation(FRotator(I*17,I*29,25));
    }
    Shape(TEXT("CellNucleus"),FVector(560,-650,650),FVector(690,520,650),FLinearColor(.11f,.075f,.28f),TEXT("Sphere"),Set,TEXT("Organic"));
    Shape(TEXT("Nucleolus"),FVector(340,-500,780),FVector(130),Ending::Gold,TEXT("Sphere"),Set,TEXT("Glow"));
    // Arc-length sampling is shared by the tube, camera and blood cells. The
    // gentle corkscrew never inverts the player or changes speed on a bend.
    VesselPath.Empty();VesselDistances.Empty();Centers.Empty();Radii.Empty();
    for(int Row=0;Row<=240;++Row)
    {
        const float T=Row/240.f,A=T*2*PI;
        const FVector P(1600+10400*T,2100*FMath::Sin(A)*FMath::Square(FMath::Sin(PI*T)),850+1100*(1-FMath::Cos(A)));
        VesselDistances.Add(Row?VesselDistances.Last()+FVector::Dist(P,VesselPath.Last()):0);VesselPath.Add(P);
        if(T<=.9f)
        {
            const float Flare=Ending::Ease((T-.77f)/.23f);
            Centers.Add(P);Radii.Add(FVector2D(FMath::Lerp(969.f,850.f,Ending::Ease(T/.025f))+Flare*400,850+Flare*400));
        }
    }
    Shell(TEXT("RedSpirallingVessel"),Centers,Radii,FLinearColor(.38f,.018f,.026f),TEXT("Vessel"),true);
    const float Length=VesselDistances.Last();
    for(int I=0;I<38;++I)
    {
        const float D=150+I*(Length-150)/38,Open=Ending::Ease((D/Length-.77f)/.23f);
        auto* Rib=Group(TEXT("CurvedVesselRib"),VesselPoint(D),Set);Rib->SetRelativeRotation(VesselRotation(D));
        Ring(FVector::ZeroVector,843+Open*400,FMath::Lerp(Ending::Rose*.44f,Ending::Teal*.18f,Open),Rib,0,10-Open*6);
    }
    // The final skin opens into a sparse lattice: the brain becomes visible
    // through it before the camera leaves the blood stream.
    for(int Strand=0;Strand<12;++Strand)for(int J=0;J<16;++J)
    {
        auto P=[&](int K){const float T=.86f+.14f*K/16,A=Strand*2*PI/12+K*.012f;return VesselPoint(Length*T)+VesselRotation(Length*T).RotateVector(FVector(0,FMath::Cos(A),FMath::Sin(A))*(850+Ending::Ease((T-.77f)/.23f)*400));};
        Rod(P(J),P(J+1),7,FMath::Lerp(Ending::Rose*.3f,Ending::Teal*.28f,J/16.f),Set);
    }
    for(int I=0;I<130;++I)
    {
        const float A=R.FRandRange(-PI,PI),Rad=R.FRandRange(440,650);
        const FVector Origin(R.FRandRange(100,Length-100),FMath::Cos(A)*Rad,FMath::Sin(A)*Rad);
        const FVector P=VesselPoint(Origin.X)+VesselRotation(Origin.X).RotateVector(FVector(0,Origin.Y,Origin.Z));
        auto* B=Shape(TEXT("BloodCell"),P,FVector(220,210,62),FLinearColor(.66f,.045f,.035f),TEXT("Sphere"),Set,TEXT("Blood"));
        B->SetRelativeRotation(FRotator(I*31,I*43,22));Blood.Add(B);BloodOrigins.Add(Origin);
    }
    Shape(TEXT("WalkableBrainPlanet"),Ending::BrainCenter,FVector(Ending::BrainRadius*2),FLinearColor(.10f,.04f,.15f),TEXT("Sphere"),Set,TEXT("BrainGround"),true);
    // Ambient neural filaments follow the small planet's surface.
    for(int I=0;I<180;++I)
    {
        const float X=R.FRandRange(13000,20200),Y=R.FRandRange(-2400,2400);
        const FVector P(X,Y,Ending::Ground(X,Y)+18);
        const FVector Q(X+110,Y+40,Ending::Ground(X+110,Y+40)+18);
        Nerves.Add(Rod(P,Q,8,Ending::Teal*.07f,Set));
        Shape(TEXT("Neuron"),P,FVector(24),Ending::Teal*.7f,TEXT("Sphere"),Set,TEXT("Glow"));
        if(I%6==0&&FMath::Abs(Y)>380)
        {
            Rod(P,P+FVector(20,0,260),9,Ending::Teal*.3f,Set);
            Nerves.Add(Rod(P+FVector(20,0,180),P+FVector(70,90,330),6,Ending::Teal*.07f,Set));
            Shape(TEXT("NeuralSpark"),P+FVector(70,90,330),FVector(37),Ending::Gold*.55f,TEXT("Sphere"),Set,TEXT("Glow"));
        }
    }
    // Twenty-two hosts: four layers with five lanes and longer visible flights.
    NeuronPositions.Add(FVector(13200,0,Ending::Ground(13200)+480));
    for(int Row=0;Row<4;++Row)for(int Lane=-2;Lane<=2;++Lane)
    {
        const float X=14500+Row*1300,Y=Lane*800;
        NeuronPositions.Add(FVector(X,Y,Ending::Ground(X,Y)+480));
    }
    NeuronPositions.Add(FVector(19700,0,Ending::Ground(19700)+600));
    for(int I=0;I<NeuronPositions.Num();++I)
    {
        const FVector P=NeuronPositions[I];const bool Goal=I==NeuronPositions.Num()-1;
        const float Size=Goal?360:140;
        NeuronCores.Add(Shape(Goal?TEXT("LargestNeuron"):TEXT("PossessableNeuron"),P,FVector(Size),Goal?Ending::Gold*.85f:Ending::Teal*.65f,TEXT("Sphere"),Set,TEXT("Glow")));
        Ring(P,Goal?190:75,Goal?Ending::Gold*.55f:Ending::Teal*.28f,Set,1,Goal?7:3);
        for(int J=0;J<6;++J)
        {
            const float A=J*2*PI/6;
            const FVector Branch=P+FVector(FMath::Cos(A)*Size,FMath::Sin(A)*Size,-Size*.4f);
            Rod(P,Branch,Goal?13:6,Ending::Teal*.35f,Set);
            Rod(Branch,Branch+FVector(FMath::Cos(A+.6f)*Size*.65f,FMath::Sin(A+.6f)*Size*.65f,Size*.35f),Goal?8:4,Ending::Teal*.25f,Set);
        }
    }
    for(int A=0;A<NeuronPositions.Num();++A)for(int B=A+1;B<NeuronPositions.Num();++B)
        if(Ending::Layer(B)-Ending::Layer(A)==1&&FVector::Dist(NeuronPositions[A],NeuronPositions[B])<2050)
            Nerves.Add(Rod(NeuronPositions[A],NeuronPositions[B],4,Ending::Teal*.16f,Set));
    PulseTrail=Rod(NeuronPositions[0],NeuronPositions[1],12,Ending::Gold,Set);
    PulseTrail->SetVisibility(false);
    // Only the final cell membrane is excavatable. Blood and brain need no tool.
    for(int I=0;I<1;++I)
    {
        const float X=Ending::GateX[I],Y=Ending::GateY[I],Z=Ending::Ground(X,Y);
        if(I<2)
            Shell(TEXT("RoundMembraneFrame"),{FVector(X,0,850),FVector(X,Y,Z+250)},{FVector2D(I==0?969:850,850),FVector2D(240,250)},I==0?Ending::Teal*.35f:Ending::Rose*.6f,I==0?TEXT("CellInterior"):TEXT("Vessel"));
        else
            Shell(TEXT("NeuralConnectionFrame"),{FVector(X,0,Z+700),FVector(X,Y,Z+250)},{FVector2D(1750,2200),FVector2D(240,250)},Ending::Blue*.35f,TEXT("Haze"));
        auto* Gate=GetWorld()->SpawnActor<ADiggableRock>(ADiggableRock::StaticClass(),GetActorTransform().TransformPosition(FVector(X,Y,Z+250)),GetActorRotation());
        Gate->AttachToActor(this,FAttachmentTransformRules::KeepWorldTransform);Gate->Tags.Add(Ending::Tag);
        Gate->RockSize=FVector(100,480,500);Gate->RockMaterial=LoadObject<UMaterialInterface>(nullptr,I==1?TEXT("/Game/EndingEmergence/Materials/M_EE_RedMembrane"):TEXT("/Game/EndingEmergence/Materials/M_EE_Membrane"));Gate->ResetRock();Gates.Add(Gate);
        Ring(FVector(X-90,Y,Z+250),252,Ending::Gold*.5f,Set,0,6);
        const float Start=I?Ending::GateX[I-1]+180:100,StartY=I?Ending::GateY[I-1]:0;
        for(int J=0;J<30;++J)
        {
            const float A=J/30.f,PX=FMath::Lerp(Start,X-170,A),PY=FMath::Lerp(StartY,Y,A);
            Shape(TEXT("FollowLight"),FVector(PX,PY,Ending::Ground(PX,PY)+8),FVector(25,7,4),Ending::Gold,TEXT("Cube"),Set,TEXT("Glow"));
        }
    }
}

void AEndingEmergence::BuildCity()
{
    auto* Set=Sets[1].Get();FRandomStream R(75);
    // A separate continental-scale view, seen from above two broken cloud decks.
    NightContinent=Group(TEXT("NightContinentFromClouds"),FVector::ZeroVector,Set);
    Shape(TEXT("NightCoastAndCities"),FVector(0,0,-1000),FVector(1600000,1600000,1),FLinearColor::White,TEXT("Plane"),NightContinent,TEXT("NightContinent"));
    Shape(TEXT("LowCloudDeck"),FVector(0,0,32000),FVector(1400000,1400000,1),FLinearColor(.12f,.18f,.26f),TEXT("Plane"),NightContinent,TEXT("CloudDeck"));
    auto* HighCloud=Shape(TEXT("HighCloudDeck"),FVector(0,0,58000),FVector(1400000,1400000,1),FLinearColor(.09f,.13f,.21f),TEXT("Plane"),NightContinent,TEXT("CloudDeck"));
    HighCloud->SetRelativeRotation(FRotator(0,67,0));
    Shape(TEXT("CityGround"),FVector(0,0,-30),FVector(17000,17000,50),FLinearColor(.015f,.026f,.035f),TEXT("Cube"),Set);
    auto* Windows=Instances(TEXT("CityWindows"),TEXT("Cube"),Ending::Gold*.6f,Set,TEXT("Glow"));
    auto* Buildings=Instances(TEXT("Buildings"),TEXT("Cube"),FLinearColor(.035f,.055f,.09f),Set);
    for(int X=-7;X<=7;++X) for(int Y=-7;Y<=7;++Y)
    {
        if(X==0||Y==0) continue;
        const FVector P(X*1050,Y*1050,0);const float H=R.FRandRange(400,2300);
        Buildings->AddInstance(FTransform(FRotator::ZeroRotator,P+FVector(0,0,H*.5),FVector(7.2,7.4,H/100)));
        for(int Z=120;Z<H-60;Z+=180) for(int J=-2;J<=2;++J)
        {
            if(R.FRand()<.22) continue;
            Windows->AddInstance(FTransform(FRotator::ZeroRotator,P+FVector(-365,J*130,Z),FVector(.04,.45,.6)));
            Windows->AddInstance(FTransform(FRotator::ZeroRotator,P+FVector(J*130,-375,Z),FVector(.45,.04,.6)));
        }
    }
    for(int I=-7;I<=7;++I)
    {
        Shape(TEXT("StreetLightGrid"),FVector(I*1050,0,1),FVector(14,17000,3),Ending::Gold*.17f,TEXT("Cube"),Set,TEXT("Glow"));
        Shape(TEXT("StreetLightGrid"),FVector(0,I*1050,1),FVector(17000,14,3),Ending::Gold*.17f,TEXT("Cube"),Set,TEXT("Glow"));
    }
    for(int I=-8;I<=8;++I) for(int Side:{-1,1})
    {
        Shape(TEXT("Crosswalk"),FVector(I*40,Side*380,4),FVector(21,170,4),FLinearColor(.5f,.6f,.62f),TEXT("Cube"),Set);
        Shape(TEXT("Crosswalk"),FVector(Side*380,I*40,4),FVector(170,21,4),FLinearColor(.5f,.6f,.62f),TEXT("Cube"),Set);
    }
    Crowd=Instances(TEXT("Crowd"),TEXT("Sphere"),FLinearColor(.13f,.22f,.26f),Set);
    CrowdHeads=Instances(TEXT("CrowdHeads"),TEXT("Sphere"),FLinearColor(.47f,.29f,.2f),Set);
    for(int I=0;I<360;++I)
    {
        FVector P(R.FRandRange(-2800,2800),R.FRandRange(-280,280),0);
        if(I%2) Swap(P.X,P.Y);
        if(FVector2D(P).Size()<130) P.X+=240;
        const FTransform T(FRotator(0,I%2?90:0,0),P+FVector(0,0,84),FVector(.32,.4,1.08));Crowd->AddInstance(T);CrowdOrigins.Add(T);
        CrowdHeads->AddInstance(FTransform(FRotator::ZeroRotator,P+FVector(0,0,153),FVector(.29,.29,.35)));
    }
    EyePerson=Group(TEXT("OurStranger"),FVector::ZeroVector,Set);Person(FVector::ZeroVector,1,Ending::Teal*.5f,EyePerson);
    for(int Side:{-1,1})
    {
        Shape(TEXT("NeonBillboard"),FVector(650,Side*665,440),FVector(20,300,150),Side>0?Ending::Rose:Ending::Teal,TEXT("Cube"),Set,TEXT("Glow"));
        Rod(FVector(460,Side*460,0),FVector(460,Side*460,360),12,Ending::Blue,Set,TEXT("Surface"));
        Shape(TEXT("TrafficSignal"),FVector(460,Side*460,340),FVector(26,46,95),Ending::Teal,TEXT("Cube"),Set,TEXT("Glow"));
    }
}

void AEndingEmergence::BuildSpace()
{
    auto* Planet=Sets[2].Get();
    Shape(TEXT("Earth"),FVector::ZeroVector,FVector(8000),FLinearColor(.015f,.10f,.28f),TEXT("Sphere"),Planet,TEXT("Planet"));
    Shape(TEXT("Atmosphere"),FVector::ZeroVector,FVector(8250),FLinearColor(.035f,.24f,.5f),TEXT("Sphere"),Planet,TEXT("Atmosphere"));
    auto* LandLights=Instances(TEXT("EarthLights"),TEXT("Sphere"),Ending::Gold,Planet,TEXT("Glow"));
    FRandomStream R(823);
    for(int I=0;I<1500;++I)
    {
        const float A=R.FRandRange(-PI,PI),Lat=R.FRandRange(-1.15,1.15);
        if(FMath::Sin(A*3+FMath::Sin(Lat*4)*2)+.6f*FMath::Sin(A*7-Lat*5)<.65f)continue;
        const FVector P(FMath::Cos(Lat)*FMath::Cos(A),FMath::Cos(Lat)*FMath::Sin(A),FMath::Sin(Lat));
        LandLights->AddInstance(FTransform(FRotator::ZeroRotator,P*4004,FVector(R.FRandRange(.022f,.06f))));
    }
    Universe=Group(TEXT("UniverseThatBecomesCytoplasm"),FVector::ZeroVector,Sets[3]);
    auto* Stars=Instances(TEXT("Galaxies"),TEXT("Sphere"),FLinearColor(.32f,.5f,.85f),Universe,TEXT("Glow"));
    auto* GoldStars=Instances(TEXT("WarmStars"),TEXT("Sphere"),Ending::Gold,Universe,TEXT("Glow"));
    for(int Cluster=0;Cluster<13;++Cluster)
    {
        const FVector Center=Cluster?R.VRand()*R.FRandRange(7000,21000):FVector::ZeroVector;
        const float Radius=Cluster?R.FRandRange(1100,2800):6200;
        auto* Haze=Shape(TEXT("GalaxyHaze"),Center,FVector(Radius*2,Radius*2,Radius*.25),FLinearColor(.09f,.03f,.18f),TEXT("Sphere"),Universe,TEXT("Haze"));
        Haze->SetRelativeRotation(FRotator(Cluster*19,Cluster*47,0));
        for(int I=0;I<240;++I)
        {
            const float D=R.FRandRange(.03,1)*Radius,A=D/Radius*7+(I%3)*2*PI/3+R.FRandRange(-.18,.18);
            const FVector P=Center+Haze->GetRelativeRotation().RotateVector(FVector(FMath::Cos(A)*D,FMath::Sin(A)*D,R.FRandRange(-.08,.08)*Radius));
            (I%3?Stars:GoldStars)->AddInstance(FTransform(FRotator::ZeroRotator,P,FVector(R.FRandRange(8,24)/100)));
        }
        Shape(TEXT("GalaxyCore"),Center,FVector(Radius*.085),Ending::Gold,TEXT("Sphere"),Universe,TEXT("Glow"));
    }
    auto* FarStars=Instances(TEXT("DistantStars"),TEXT("Sphere"),FLinearColor(.18f,.24f,.37f),Universe,TEXT("Glow"));
    for(int I=0;I<1400;++I)FarStars->AddInstance(FTransform(FRotator::ZeroRotator,R.VRand()*40000,FVector(R.FRandRange(.4,1.3))));
    Cell=Group(TEXT("RevealedCellMembrane"),FVector::ZeroVector,Sets[3]);
    Shape(TEXT("TransparentCell"),FVector::ZeroVector,FVector(3500,4200,3300),Ending::Teal*.28f,TEXT("Sphere"),Cell,TEXT("Atmosphere"));
    Ring(FVector::ZeroVector,1750,Ending::Teal*.6f,Cell,0,13);
    Shape(TEXT("GiantNucleus"),FVector(150,1130,340),FVector(630,570,600),FLinearColor(.20f,.05f,.32f),TEXT("Sphere"),Cell,TEXT("Organic"));
    Shape(TEXT("GiantNucleolus"),FVector(-105,1030,390),FVector(140),Ending::Gold,TEXT("Sphere"),Cell,TEXT("Glow"));
    for(int I=0;I<19;++I)
    {
        const float A=I*2*PI/19;
        auto* C=Shape(TEXT("GiantOrganelle"),FVector(FMath::Sin(A*3)*650,FMath::Cos(A)*1400,FMath::Sin(A)*1280),FVector(110,290,120),I%2?Ending::Rose:Ending::Teal*.35f,TEXT("Sphere"),Cell,TEXT("Organic"));
        C->SetRelativeRotation(FRotator(I*19,I*33,I*17));
    }
    // These structures exist at their final size from the beginning. The camera
    // crosses the enclosing membrane; nothing grows out of the universe's centre.
    Cell->SetRelativeScale3D(FVector(32));
    OtherCells=Group(TEXT("ManyUniversesInsideCells"),FVector::ZeroVector,Sets[3]);
    auto* SmallStars=Instances(TEXT("UniversesInOtherCells"),TEXT("Sphere"),FLinearColor(.32f,.43f,.70f),OtherCells,TEXT("Glow"));
    auto* WarmStars=Instances(TEXT("WarmUniversesInOtherCells"),TEXT("Sphere"),Ending::Gold*.55f,OtherCells,TEXT("Glow"));
    for(int Row=-2;Row<=2;++Row)for(int Col=-2;Col<=2;++Col)
    {
        if(Row==0&&Col==0)continue;
        const FVector Center((Row+Col)%3*35000,Col*164000+(FMath::Abs(Row)%2)*65000,Row*150000);
        const float Radius=R.FRandRange(42000,57000);
        auto* Neighbor=Group(TEXT("NeighborCell"),Center,OtherCells);
        Shape(TEXT("NeighborMembrane"),FVector::ZeroVector,FVector(Radius*2,Radius*2.2,Radius*1.9),Ending::Teal*.21f,TEXT("Sphere"),Neighbor,TEXT("Atmosphere"));
        Ring(FVector::ZeroVector,Radius,Ending::Teal*.35f,Neighbor,0,Radius*.004f);
        Shape(TEXT("NeighborNucleus"),FVector(0,Radius*.60,Radius*.18),FVector(Radius*.30),FLinearColor(.14f,.04f,.23f),TEXT("Sphere"),Neighbor,TEXT("Organic"));
        Shape(TEXT("NeighborGalaxyHaze"),FVector::ZeroVector,FVector(Radius*1.05,Radius*.9,Radius*.13),FLinearColor(.18f,.06f,.29f),TEXT("Sphere"),Neighbor,TEXT("Haze"));
        for(int I=0;I<150;++I)
        {
            const float Dist=R.FRandRange(.015f,.7f)*Radius,A=Dist/Radius*11+(I%3)*2*PI/3;
            const FVector P=Center+FVector(FMath::Cos(A)*Dist*.45,FMath::Sin(A)*Dist,FMath::Cos(A)*Dist*.85);
            (I%3?SmallStars:WarmStars)->AddInstance(FTransform(FRotator::ZeroRotator,P,FVector(R.FRandRange(120,330)/100)));
        }
    }
}

void AEndingEmergence::BuildAfternoon()
{
    auto* Set=Sets[4].Get();
    const FLinearColor Wood(.28f,.13f,.057f),Cream(.69f,.61f,.44f),Ink(.018f,.039f,.045f);
    Shape(TEXT("RoomFloor"),FVector(0,0,-14),FVector(1700,1500,25),FLinearColor(.12f,.14f,.135f),TEXT("Cube"),Set);
    Shape(TEXT("BackWall"),FVector(620,0,240),FVector(25,1500,500),Cream*.7f,TEXT("Cube"),Set);
    Shape(TEXT("WarmWallBehindSpeaker"),FVector(-620,0,240),FVector(25,1500,500),Cream*.62f,TEXT("Cube"),Set);
    Shape(TEXT("SideWall"),FVector(0,680,240),FVector(1250,25,500),Cream*.6f,TEXT("Cube"),Set);
    Shape(TEXT("AfternoonWindow"),FVector(80,662,290),FVector(680,8,280),FLinearColor(.79f,.60f,.33f),TEXT("Cube"),Set,TEXT("Glow"));
    for(int I=-1;I<=1;++I) Shape(TEXT("WindowMullion"),FVector(80+I*220,650,290),FVector(8,10,290),Wood,TEXT("Cube"),Set);
    Shape(TEXT("WindowMullion"),FVector(80,650,290),FVector(690,10,9),Wood,TEXT("Cube"),Set);
    for(int I=0;I<4;++I)
    {
        auto* Patch=Shape(TEXT("SunPatch"),FVector(-100+I*150,100,1),FVector(95,650,2),FLinearColor(.29f,.23f,.12f),TEXT("Cube"),Set);
        Patch->SetRelativeRotation(FRotator(0,24,0));
    }
    Shape(TEXT("SharedTable"),FVector(0,0,78),FVector(310,230,12),Wood,TEXT("Cube"),Set);
    for(int X:{-1,1})for(int Y:{-1,1})Shape(TEXT("TableLeg"),FVector(X*127,Y*88,36),FVector(11,11,74),Wood*.65f,TEXT("Cube"),Set);
    Shape(TEXT("NotebookCover"),FVector(-30,0,86),FVector(78,103,3),Ending::Teal*.3f,TEXT("Cube"),Set);
    Shape(TEXT("NotebookPaper"),FVector(-30,0,88),FVector(74,99,1),FLinearColor(.84f,.78f,.60f),TEXT("Cube"),Set);
    Rod(FVector(-30,-48,89),FVector(-30,48,89),.55,Wood,Set,TEXT("Surface"));
    for(int I=0;I<8;++I) Rod(FVector(-62,-37+I*10,89),FVector(-36,-37+I*10,89),.25,Wood*.45f,Set,TEXT("Surface"));
    // The same five linked openings, now a level sketch in the notebook.
    for(int I=0;I<5;++I)
    {
        const FVector A(-24,-36+I*17,89.7),B(-4,-36+I*17,89.7);
        Sketch.Add(Rod(A,B,.8,Ink,Set,TEXT("Surface")));
        if(I<4) Sketch.Add(Rod(B,B+FVector(0,17,0),.8,Ink,Set,TEXT("Surface")));
        Sketch.Add(Shape(TEXT("SketchNode"),(A+B)*.5,FVector(3,3,.7),Ink,TEXT("Sphere"),Set));
    }
    Pencil=Group(TEXT("Pencil"),FVector(-27,-18,95),Set);
    Rod(FVector::ZeroVector,FVector(-5,-24,27),1.2,Ending::Gold,Pencil,TEXT("Surface"));
    Shape(TEXT("PencilTip"),FVector::ZeroVector,FVector(1.4,1.4,3),Ink,TEXT("Cone"),Pencil);
    Shape(TEXT("LaptopBase"),FVector(66,34,87),FVector(61,80,3),Ink,TEXT("Cube"),Set);
    auto* Screen=Shape(TEXT("LaptopScreen"),FVector(95,34,111),FVector(3,78,48),Ink,TEXT("Cube"),Set);Screen->SetRelativeRotation(FRotator(0,0,-10));
    Shape(TEXT("LaptopDisplay"),FVector(92,34,111),FVector(1,71,40),Ending::Blue*.6f,TEXT("Cube"),Set,TEXT("Glow"));
    for(int I=0;I<5;++I) Shape(TEXT("LaptopLines"),FVector(91,20,99+I*5),FVector(.4,38-I*3,1),Ending::Teal*.5f,TEXT("Cube"),Set,TEXT("Glow"));
    for(int I=0;I<3;++I)
    {
        const FVector P(I==0?-198:I==1?0:180,I==1?-164:25,0);
        auto* Dev=Group(TEXT("Developer"),P,Set);Dev->SetRelativeRotation(FRotator(0,I==0?0:I==1?90:180,0));
        if(I==0)Speaker=Dev;
        Person(FVector::ZeroVector,1,I==0?Ending::Teal*.38f:I==1?FLinearColor(.25f,.15f,.07f):Ending::Blue,Dev);
        const FLinearColor Skin(.6f,.38f,.24f),Dark(.023f,.013f,.012f);
        for(int Side:{-1,1})
        {
            const FVector Eye(17,Side*7,166);
            Shape(TEXT("EyeWhite"),Eye,FVector(3.4,11,7),FLinearColor(.74f,.71f,.61f),TEXT("Sphere"),Dev);
            Shape(TEXT("Iris"),Eye+FVector(2.1,0,0),FVector(1.4,5.7,5.7),FLinearColor(.20f,.09f,.027f),TEXT("Sphere"),Dev,TEXT("Iris"));
            Shape(TEXT("Pupil"),Eye+FVector(2.9,0,0),FVector(.9,2.4,2.9),Dark,TEXT("Sphere"),Dev);
            Shape(TEXT("EyeCatchlight"),Eye+FVector(3.3,-.8,1),FVector(.4,.5,.5),FLinearColor(.78f,.79f,.69f),TEXT("Sphere"),Dev,TEXT("Glow"));
            for(int Upper:{-1,1})
            {
                const FVector Origin=Eye+FVector(4.8,0,Upper*3.5);
                auto* Lid=Shape(TEXT("Eyelid"),Origin,FVector(2.5,12,7),Skin,TEXT("Sphere"),Dev);
                if(I==0){EyeLids.Add(Lid);LidOrigins.Add(Origin);}
                else Lid->SetRelativeLocation(Origin+FVector(-7.5,0,Upper*3.1));
            }
            Rod(FVector(18,Side*7-4,175),FVector(17,Side*7+4,175.8),1.4f,Dark,Dev,TEXT("Surface"));
        }
        for(int Segment=0;Segment<10;++Segment)
        {
            const float A=-1+Segment*.2f,B=A+.2f;
            Rod(FVector(16.5,A*8,150+A*A*3),FVector(16.5,B*8,150+B*B*3),.8f,Dark,Dev,TEXT("Surface"));
        }
        Shape(TEXT("Nose"),FVector(18,0,159),FVector(6,5,8),Skin,TEXT("Sphere"),Dev);
        if(I==0)SpeakerMouth=Shape(TEXT("SpeakingSmile"),FVector(16.8,0,151),FVector(1.4,10,2),Dark,TEXT("Sphere"),Dev);
        Rod(FVector(5,-28,120),FVector(51,-23,84),11,FLinearColor(.48f,.30f,.18f),Dev,TEXT("Surface"));
        Rod(FVector(5,28,120),FVector(51,23,84),11,FLinearColor(.48f,.30f,.18f),Dev,TEXT("Surface"));
        Shape(TEXT("Mug"),FVector(-40+I*76,75-I*70,94),FVector(13,13,19),Cream,TEXT("Cylinder"),Set);
        Shape(TEXT("Coffee"),FVector(-40+I*76,75-I*70,104),FVector(11,11,.5),Wood*.12f,TEXT("Cylinder"),Set);
    }
    // A tiny echo of the opening cave grows out of the drawn path, then settles.
    Idea=Group(TEXT("MiniatureIdea"),FVector(-10,0,90),Set);
    for(int I=0;I<12;++I) Shape(TEXT("LittleMine"),FVector(FMath::Cos(I*.52)*18,FMath::Sin(I*.52)*30,8),FVector(8,9,10),Ending::Teal*.5f,TEXT("Cube"),Idea);
    Shape(TEXT("IdeaSpark"),FVector(0,0,21),FVector(5),Ending::Gold,TEXT("Sphere"),Idea,TEXT("Glow"));
    // Plant, pinboard and loose notes keep the final room recognizably human.
    Shape(TEXT("Pinboard"),FVector(600,-330,235),FVector(12,360,190),Wood*.7f,TEXT("Cube"),Set);
    for(int I=0;I<9;++I)Shape(TEXT("StickyNote"),FVector(591,-460+(I%3)*110,175+(I/3)*55),FVector(2,60,38),I%2?Cream:Ending::Gold*.55f,TEXT("Cube"),Set);
    Shape(TEXT("PlantPot"),FVector(440,460,30),FVector(90,90,60),Wood,TEXT("Cylinder"),Set);
    for(int I=0;I<9;++I)
    {
        auto* Leaf=Shape(TEXT("Leaf"),FVector(440+FMath::Cos(float(I))*55,460+FMath::Sin(float(I))*55,110+I*8),FVector(24,85,20),FLinearColor(.06f,.16f,.08f),TEXT("Sphere"),Set);
        Leaf->SetRelativeRotation(FRotator(I*15,I*40,0));
    }
}

void AEndingEmergence::BuildDemo()
{
    if(!GetWorld()||GetWorld()->IsGameWorld())return;
    for(ADiggableRock* G:Gates)if(IsValid(G))G->Destroy();Gates.Empty();
    TArray<UActorComponent*> Old;GetComponents(Old);
    for(int I=Old.Num()-1;I>=0;--I)if(Old[I]->ComponentHasTag(Ending::Tag)){RemoveInstanceComponent(Old[I]);Old[I]->DestroyComponent();}
    Sets.Empty();Nerves.Empty();Sketch.Empty();Blood.Empty();BloodOrigins.Empty();CrowdOrigins.Empty();EyeLids.Empty();LidOrigins.Empty();NeuronPositions.Empty();NeuronCores.Empty();
    Sets.Add(Group(TEXT("01_PlayableCellsBloodBrain"),FVector::ZeroVector));
    Sets.Add(Group(TEXT("02_CityAndPeople"),Ending::City));
    Sets.Add(Group(TEXT("03_Earth"),Ending::Earth));
    Sets.Add(Group(TEXT("04_UniverseInsideCell"),Ending::Cosmos));
    Sets.Add(Group(TEXT("05_ThatAfternoon"),Ending::Desk));
    BuildJourney();BuildCity();BuildSpace();BuildAfternoon();
    for(int I=1;I<Sets.Num();++I)Sets[I]->SetVisibility(false,true);
    MarkPackageDirty();
}

void AEndingEmergence::BeginPlay()
{
    Super::BeginPlay();
    // Generated shell geometry is saved, but its transient cooked body is not.
    // Recreate collision after loading/PIE duplication before the pawn can move.
    TArray<UDynamicMeshComponent*> Shells;GetComponents(Shells);
    for(auto* ShellMesh:Shells)
    {
        ShellMesh->bUseAsyncCooking=false;
        ShellMesh->SetComplexAsSimpleCollisionEnabled(true,false);
        ShellMesh->GetBodySetup()->bDoubleSidedGeometry=false;
        ShellMesh->UpdateCollision(false);
        ShellMesh->RecreatePhysicsState();
    }
    Camera=GetWorld()->SpawnActor<ACameraActor>();
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount=true;
    Camera->GetCameraComponent()->PostProcessSettings.MotionBlurAmount=0;
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_VignetteIntensity=true;
    Camera->GetCameraComponent()->PostProcessSettings.VignetteIntensity=.3;
}

bool AEndingEmergence::Initialize()
{
    Player=GetWorld()->GetFirstPlayerController();
    if(!Player||!Player->GetPawn()||!Player->InputComponent||Gates.Num()!=1||Sets.Num()!=5||NeuronPositions.Num()!=Ending::NeuronCount||VesselPath.Num()<2)return false;
    Mining=Player->GetPawn()->FindComponentByClass<UMiningComponent>();
    if(!Mining)return false;
    if(auto* PC=Cast<ABrainMiningPlayerController>(Player))
    {
        PC->SetMiningGUIEnabled(false);
        if(PC->InteractionHUD)PC->InteractionHUD->SetVisibility(ESlateVisibility::Collapsed);
    }
    FisheyeMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_Fisheye"));
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_PulseTransit")))
    {TransitMaterial=UMaterialInstanceDynamic::Create(Base,this);Camera->GetCameraComponent()->AddOrUpdateBlendable(TransitMaterial,1);}
    // Local instance only: the authored route is a short, grounded finale.
    if(auto* M=Player->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>())M->FlightSettings.bEnabled=false;
    Mining->OnFieldReset.AddDynamic(this,&AEndingEmergence::RestartDemo);
    if(GEngine&&GEngine->GameViewport)
    {
        SAssignNew(View,SEmergenceEndingView).Owner(this);View->SetVisibility(EVisibility::HitTestInvisible);
        GEngine->GameViewport->AddViewportWidgetContent(View.ToSharedRef(),100);
    }
    SetAmbience(TEXT("Cell"),.34f);return true;
}

void AEndingEmergence::Sound(const TCHAR* Name,float Volume,float Pitch)
{
    if(auto* S=LoadObject<USoundBase>(nullptr,*(FString(TEXT("/Game/EndingEmergence/Audio/S_EE_"))+Name)))UGameplayStatics::PlaySound2D(this,S,Volume,Pitch);
}

void AEndingEmergence::SetAmbience(const TCHAR* Name,float Volume)
{
    if(Ambience){Ambience->Stop();Ambience->DestroyComponent();Ambience=nullptr;}
    if(auto* S=LoadObject<USoundBase>(nullptr,*(FString(TEXT("/Game/EndingEmergence/Audio/S_EE_"))+Name)))
        Ambience=UGameplayStatics::SpawnSound2D(this,S,Volume,1,0,nullptr,false,false);
}

void AEndingEmergence::RestartDemo()
{
    if(bRestoring||!Player||!Mining)return;
    TGuardValue<bool> Guard(bRestoring,true);
    GateIndex=0;Shot=-1;ShotTime=0;Age=0;BeatAt=0;LastCuts=0;bComplete=false;BloodTravel=0;
    CurrentNeuron=TargetNeuron=TravellingToNeuron=INDEX_NONE;PossessionCount=0;VisitedNeurons.Empty();BloodSpeed=480;VesselDistance=150;BloodY=0;PulseAt=-100;bEnteringBrain=false;TransitEffects(0);
    if(DroppedPickaxe){DroppedPickaxe->DestroyComponent();DroppedPickaxe=nullptr;}
    DriftOffset=DriftVelocity=FVector::ZeroVector;
    PullbackBoost=0;PullbackRate=1;
    for(ADiggableRock* G:Gates)if(IsValid(G)){G->ResetRock();G->SetActorHiddenInGame(false);}
    for(int I=0;I<Sets.Num();++I)Sets[I]->SetVisibility(I==0,true);
    if(PulseTrail)PulseTrail->SetVisibility(false);
    for(int I=0;I<NeuronCores.Num();++I)if(auto* M=Cast<UMaterialInstanceDynamic>(NeuronCores[I]->GetMaterial(0)))M->SetVectorParameterValue(TEXT("Tint"),I==NeuronPositions.Num()-1?Ending::Gold*.85f:Ending::Teal*.65f);
    Universe->SetRelativeScale3D(FVector(1));Cell->SetRelativeScale3D(FVector(32));
    if(Speaker)Speaker->SetRelativeRotation(FRotator::ZeroRotator);
    for(UStaticMeshComponent* S:Sketch)S->SetVisibility(false);Idea->SetVisibility(false,true);
    Player->ResetIgnoreMoveInput();Player->ResetIgnoreLookInput();
    Player->GetPawn()->SetActorHiddenInGame(false);
    Player->GetPawn()->SetActorLocation(GetActorTransform().TransformPosition(FVector(100,0,Ending::Ground(100)+110)),false,nullptr,ETeleportType::TeleportPhysics);
    Player->SetControlRotation(GetActorRotation());
    if(auto* C=Cast<ACharacter>(Player->GetPawn())){C->GetCharacterMovement()->StopMovementImmediately();C->GetCharacterMovement()->SetMovementMode(MOVE_Walking);}
    if(auto* C=Player->GetPawn()->FindComponentByClass<UCameraComponent>()){C->SetFieldOfView(90);if(FisheyeMaterial)C->AddOrUpdateBlendable(FisheyeMaterial,0);}
    Player->SetViewTarget(Player->GetPawn());Mining->SetInputSuspended(false);Mining->SetInputContextEnabled(true);
    UpdateNerves();SetAmbience(TEXT("Cell"),.34f);
}

void AEndingEmergence::PreviewFilm()
{
    if(!bReady||Shot>=0)return;
    bEnteringBrain=false;TravellingToNeuron=INDEX_NONE;TransitEffects(0);
    Mining->SetInputSuspended(true);Mining->SetInputContextEnabled(false);
    Player->SetIgnoreMoveInput(true);Player->SetIgnoreLookInput(true);
    if(auto* C=Cast<ACharacter>(Player->GetPawn())){C->GetCharacterMovement()->StopMovementImmediately();C->GetCharacterMovement()->DisableMovement();}
    Player->GetPawn()->SetActorHiddenInGame(true);Player->SetViewTarget(Camera);
    for(ADiggableRock* G:Gates)G->SetActorHiddenInGame(true);
    SetShot(0);UpdateFilm(0);
}

float AEndingEmergence::Duration(int32 Index)
{
    const float Seconds[]={5,20,14,18,27,20,18,.7f,2.4f,12,0};
    return Index>=0&&Index<UE_ARRAY_COUNT(Seconds)?Seconds[Index]:0;
}

void AEndingEmergence::SetShot(int32 Index)
{
    Shot=Index;ShotTime=0;
    const int SetIndex=Shot<=2?1:Shot==3?2:Shot<=6?3:4;
    for(int I=0;I<Sets.Num();++I)Sets[I]->SetVisibility(I==SetIndex,true);
    if(NightContinent)NightContinent->SetVisibility(Shot==2,true);
    if(Shot<=2)
    {
        TArray<USceneComponent*> CityParts;Sets[1]->GetChildrenComponents(false,CityParts);
        for(auto* Part:CityParts)if(Part!=NightContinent)Part->SetVisibility(Shot<2,true);
        EyePerson->SetVisibility(Shot<=1,true);
    }
    if(Shot==4){Universe->SetRelativeScale3D(FVector(1));Cell->SetRelativeScale3D(FVector(32));}
    if(Shot==7)
    {
        for(UStaticMeshComponent* S:Sketch)S->SetVisibility(false);Idea->SetVisibility(false,true);
        Pencil->SetRelativeLocation(FVector(-27,-18,95));DriftOffset=DriftVelocity=FVector::ZeroVector;
        if(Ambience){Ambience->Stop();Ambience->DestroyComponent();Ambience=nullptr;}
    }
    if(Shot==8){Sound(TEXT("Wake"),.55f);SetAmbience(TEXT("Afternoon"),.6f);}
    if(Shot==9){for(UStaticMeshComponent* S:Sketch)S->SetVisibility(true);}
    if(Shot==10){bComplete=true;Idea->SetVisibility(false,true);}
    if(Shot==0)SetAmbience(TEXT("Street"),.48f);
    if(Shot==3)SetAmbience(TEXT("Space"),.32f);
    UE_LOG(LogTemp,Display,TEXT("ENDING_SHOT %d"),Shot);
}

void AEndingEmergence::Pose(FVector Position,FVector LookAt,float Fov,float Roll)
{
    if(Shot>=1&&Shot<=6)Position=LookAt+FRotator(0,DriftOffset.Y,0).RotateVector(Position-LookAt);
    const FTransform T=GetActorTransform();
    FRotator Rot=(T.TransformPosition(LookAt)-T.TransformPosition(Position)).Rotation();Rot.Roll=Roll;
    Camera->SetActorLocationAndRotation(T.TransformPosition(Position),Rot);Camera->GetCameraComponent()->SetFieldOfView(Fov);
}

void AEndingEmergence::UpdateFilm(float Dt)
{
    const float Progress=FMath::Clamp(ShotTime/FMath::Max(1.f,Duration(Shot)),0.f,1.f);
    // Never ease to a stop in the scale journey. Exponential distance increases
    // physical pullback speed as the represented scale grows.
    const float T=Shot>=1&&Shot<=6?Progress:Ending::Ease(Progress);
    const float Zoom=(FMath::Exp(T*FMath::Loge(4.f))-1)/3;
    const float W=bComfortMode?0:FMath::Clamp(Weightlessness,0.f,1.f);
    if(Shot>=1&&Shot<=6)
    {
        // Exponential damping is stable at different frame rates. Input changes velocity,
        // not position, so release visibly coasts. Bound the offset to protect composition.
        const FVector Push(0,(Player->IsInputKeyDown(EKeys::D)?1:0)-(Player->IsInputKeyDown(EKeys::A)?1:0),0);
        const double Decay=FMath::Exp(-2.2*Dt);
        const FVector Target=Push*3.5*W;
        DriftVelocity=DriftVelocity*Decay+Target*(1-Decay);
        DriftOffset=(DriftOffset+DriftVelocity*Dt).GetClampedToMaxSize(4);
        if(W==0){DriftVelocity=DriftOffset=FVector::ZeroVector;}
    }
    switch(Shot)
    {
    case 0: Pose(Ending::City+FMath::Lerp(FVector(38,0,195),FVector(-160,-80,650),T),Ending::City+FVector(0,0,160),75);break;
    case 1:
        EyePerson->SetVisibility(true,true);
        Pose(Ending::City+FMath::Lerp(FVector(-160,-80,650),FVector(-1700,-900,3200),T),Ending::City+FMath::Lerp(FVector(0,0,160),FVector(100,0,75),T),FMath::Lerp(75.f,64.f,T));break;
    case 2:
        Pose(Ending::City+FMath::Lerp(FVector(-110000,-30000,130000),FVector(-260000,-70000,360000),Zoom),Ending::City+FVector(15000,0,-1000),60);break;
    case 3:
        Pose(Ending::Earth+FMath::Lerp(FVector(-5200,-1100,1300),FVector(-15000,-3400,4500),Zoom),Ending::Earth, FMath::Lerp(68.f,58.f,T),FMath::Sin(ShotTime*.12)*1.3*W);break;
    case 4:
    {
        // Drift rotates the rail vector without changing its length. Switching
        // comfort mode can never kick the camera forward along the pullback.
        const FRotator Float(FMath::Sin(ShotTime*.13f)*.4f*W,FMath::Sin(ShotTime*.19f)*.6f*W,0);
        Pose(Ending::Cosmos+Float.RotateVector(FMath::Lerp(FVector(-14500,-1800,4300),FVector(-42000,-4000,10500),Zoom)),Ending::Cosmos,62,FMath::Sin(ShotTime*.16)*3.5*W);break;
    }
    case 5:
    {
        // Cell already surrounds the stars. Pull through the membrane to reveal it.
        const float Decay=1-T;
        const FRotator LastFloat(FMath::Sin(27*.13f)*.4f*W*Decay,FMath::Sin(27*.19f)*.6f*W*Decay,0);
        Pose(Ending::Cosmos+LastFloat.RotateVector(FMath::Lerp(FVector(-42000,-4000,10500),FVector(-5200,-250,900)*32,Zoom)),
            Ending::Cosmos,62,FMath::Sin(27*.16)*3.5*W*Decay);break;
    }
    case 6:
        Pose(Ending::Cosmos+FMath::Lerp(FVector(-5200,-250,900)*32,FVector(-620000,-110000,185000),Zoom),Ending::Cosmos,62,FMath::Sin(ShotTime*.13)*1.2*W);break;
    case 7:
        break;
    case 8:
    case 9:
    case 10:
    {
        const float A=Shot==8?0:Shot==10?1:Ending::Ease(ShotTime/9);
        Pose(Ending::Desk+FMath::Lerp(FVector(-155,18,166),FVector(380,-390,310),A),Ending::Desk+FMath::Lerp(FVector(-178,18,166),FVector(-15,0,125),A),FMath::Lerp(36.f,53.f,A));
        const float Open=Shot==8?Ending::Ease((ShotTime-.06f)/.18f):1;
        for(int I=0;I<EyeLids.Num();++I)EyeLids[I]->SetRelativeLocation(LidOrigins[I]+FVector(-7.5f*Open,0,(I%2?1:-1)*3.1f*Open));
        const bool Talking=Shot==9&&ShotTime>4&&ShotTime<7.2f;
        if(SpeakerMouth)SpeakerMouth->SetRelativeScale3D(FVector(.014,.1,Talking?.018f+.014f*FMath::Abs(FMath::Sin(ShotTime*12)):.018f));
        if(Speaker)Speaker->SetRelativeRotation(FRotator(0,Talking?FMath::Sin((ShotTime-4)*1.3f)*3:0,Talking?FMath::Sin((ShotTime-4)*2)*1.2f:0));
        break;
    }
    }
}

void AEndingEmergence::UpdateNerves()
{
    for(int I=0;I<Nerves.Num();++I)
    {
        const bool On=I<(GateIndex<2?0:(GateIndex-1)*Nerves.Num()/4);
        if(auto* M=Cast<UMaterialInstanceDynamic>(Nerves[I]->GetMaterial(0)))M->SetVectorParameterValue(TEXT("Tint"),On?Ending::Gold*.8f:Ending::Teal*.07f);
    }
}

void AEndingEmergence::EnterBlood()
{
    GateIndex=1;BloodAt=Age;BeatAt=Age+3;BloodSpeed=480;BloodY=0;
    const FVector Eye=Player->PlayerCameraManager->GetCameraLocation();
    const FVector LocalEye=GetActorTransform().InverseTransformPosition(Eye);
    VesselDistance=FMath::Max(150.f,float(LocalEye.X)-1600);BloodStartZ=LocalEye.Z;
    VesselLook=VesselRotation(VesselDistance);
    // A visible copy leaves the hand before the hidden pawn becomes a pulse.
    TArray<UStaticMeshComponent*> Parts;Player->GetPawn()->GetComponents(Parts);
    for(auto* Part:Parts)if(Part->GetName()==TEXT("PickaxeModel")&&Part->GetStaticMesh())
    {
        DroppedPickaxe=NewObject<UStaticMeshComponent>(this,NAME_None,RF_Transient);
        DroppedPickaxe->SetupAttachment(RootComponent);DroppedPickaxe->SetStaticMesh(Part->GetStaticMesh());
        DroppedPickaxe->SetCollisionEnabled(ECollisionEnabled::NoCollision);DroppedPickaxe->SetCastShadow(false);
        for(int I=0;I<Part->GetNumMaterials();++I)DroppedPickaxe->SetMaterial(I,Part->GetMaterial(I));
        DroppedPickaxe->RegisterComponent();DroppedPickaxe->SetWorldTransform(Part->GetComponentTransform());
        // First-person meshes use a separate render transform. Place the dropped
        // world mesh ahead of the eye so it visibly leaves the frame, not behind it.
        const FRotationMatrix ViewAxes(Player->GetControlRotation());
        PickaxeStart=LocalEye+GetActorTransform().InverseTransformVectorNoScale(ViewAxes.GetUnitAxis(EAxis::X)*130+ViewAxes.GetUnitAxis(EAxis::Y)*55-ViewAxes.GetUnitAxis(EAxis::Z)*40);
        DroppedPickaxe->SetRelativeLocation(PickaxeStart);break;
    }
    Mining->SetInputSuspended(true);Mining->SetInputContextEnabled(false);
    Player->SetIgnoreMoveInput(true);Player->GetPawn()->SetActorHiddenInGame(true);
    if(auto* C=Cast<ACharacter>(Player->GetPawn())){C->GetCharacterMovement()->StopMovementImmediately();C->GetCharacterMovement()->DisableMovement();}
    Camera->SetActorLocation(Eye);Camera->SetActorRotation(Player->GetControlRotation());Player->SetViewTarget(Camera);
    SetAmbience(TEXT("Blood"),.42f);Sound(TEXT("Connect"),.45f);
}

FVector AEndingEmergence::NeuronWorldPosition(int32 I) const
{
    return NeuronPositions.IsValidIndex(I)?GetActorTransform().TransformPosition(NeuronPositions[I]):GetActorLocation();
}

bool AEndingEmergence::CanPossess(int32 I) const
{
    if(Shot>=0||GateIndex!=2||TravellingToNeuron!=INDEX_NONE||!NeuronPositions.IsValidIndex(I)||!NeuronPositions.IsValidIndex(CurrentNeuron)||I==CurrentNeuron)return false;
    return FMath::Abs(Ending::Layer(I)-Ending::Layer(CurrentNeuron))==1&&FVector::Dist(NeuronPositions[I],NeuronPositions[CurrentNeuron])<2050;
}

void AEndingEmergence::EnterBrain()
{
    // Leave the open vessel in the same shot; slow down and settle into host 0.
    bEnteringBrain=true;BrainEntryAt=Age;TransitStart=Camera->GetActorLocation();
    TransitEnd=NeuronWorldPosition(0)+GetActorUpVector()*110;TransitLook=Camera->GetActorRotation();
    Player->SetIgnoreLookInput(true);
    if(Ambience){Ambience->bAutoDestroy=true;Ambience->FadeOut(2,0);Ambience=nullptr;}
    SetAmbience(TEXT("Brain"),.4f);if(Ambience)Ambience->FadeIn(2,.4f);
    if(DroppedPickaxe)DroppedPickaxe->SetVisibility(false);
}

void AEndingEmergence::PossessNeuron(int32 I)
{
    if(!CanPossess(I))return;
    TravellingToNeuron=I;TargetNeuron=INDEX_NONE;PulseAt=Age;
    TransitStart=Camera->GetActorLocation();TransitEnd=NeuronWorldPosition(I)+GetActorUpVector()*(I==NeuronPositions.Num()-1?220:110);
    TransitLook=Camera->GetActorRotation();
    TransitDuration=FMath::Clamp(float(FVector::Dist(TransitStart,TransitEnd))/2100.f+.35f,.95f,1.35f);
    Player->SetIgnoreLookInput(true);Sound(TEXT("PulseFlight"),.6f,1.15f/TransitDuration);
}

FVector AEndingEmergence::VesselPoint(float Distance) const
{
    if(VesselPath.Num()<2)return FVector(1600,0,850);
    Distance=FMath::Clamp(Distance,0.f,VesselDistances.Last());
    int Low=0,High=VesselDistances.Num()-1;
    while(High-Low>1){const int Mid=(Low+High)/2;if(VesselDistances[Mid]<Distance)Low=Mid;else High=Mid;}
    const float T=(Distance-VesselDistances[Low])/FMath::Max(1.f,VesselDistances[High]-VesselDistances[Low]);
    return FMath::Lerp(VesselPath[Low],VesselPath[High],T);
}

FRotator AEndingEmergence::VesselRotation(float Distance) const
{
    return (VesselPoint(Distance+35)-VesselPoint(Distance-35)).Rotation();
}

void AEndingEmergence::TransitEffects(float Strength)
{
    Strength*=bComfortMode?.12f:1.f;
    if(TransitMaterial)TransitMaterial->SetScalarParameterValue(TEXT("Strength"),Strength);
    auto& PP=Camera->GetCameraComponent()->PostProcessSettings;
    PP.MotionBlurAmount=.25f*Strength;
    PP.bOverride_SceneFringeIntensity=true;PP.SceneFringeIntensity=1.1f*Strength;
    PP.VignetteIntensity=.3f+.18f*Strength;
}

void AEndingEmergence::BeginThought()
{
    GateIndex=5;BeatAt=Age+4;TargetNeuron=INDEX_NONE;UpdateNerves();
    TakeoffStart=GetActorTransform().InverseTransformPosition(Camera->GetActorLocation());
    TakeoffLook=TakeoffStart+GetActorTransform().InverseTransformVectorNoScale(Player->GetControlRotation().Vector())*850;
    Player->SetIgnoreLookInput(true);Sound(TEXT("Connect"),.65f,.85f);
}

void AEndingEmergence::UpdatePulse(float Dt)
{
    if(GateIndex==1&&!bEnteringBrain)
    {
        const float T=Age-BloodAt;
        BloodSpeed=FMath::FInterpTo(BloodSpeed,Player->IsInputKeyDown(EKeys::W)?950.f:480.f,Dt,3);
        VesselDistance=FMath::Min(VesselDistance+BloodSpeed*Dt,VesselDistances.Last());
        BloodY=FMath::Clamp(BloodY+Dt*150*(float(Player->IsInputKeyDown(EKeys::D))-float(Player->IsInputKeyDown(EKeys::A))),-240.f,240.f);
        const float Z=(BloodStartZ-850)*(1-Ending::Ease(T/1.8f))+(bComfortMode?0:18*FMath::Sin(T*1.6f)*Ending::Ease(T));
        const FRotator Frame=VesselRotation(VesselDistance);
        Camera->SetActorLocation(GetActorTransform().TransformPosition(VesselPoint(VesselDistance)+Frame.RotateVector(FVector(0,BloodY,Z))));
        // Carry the free-look offset around bends without fighting mouse input.
        const FQuat Turn=GetActorQuat()*Frame.Quaternion()*VesselLook.Quaternion().Inverse()*GetActorQuat().Inverse();
        FRotator Look=(Turn*Player->GetControlRotation().Quaternion()).Rotator();Look.Roll=0;Player->SetControlRotation(Look);
        VesselLook=Frame;Look.Roll=bComfortMode?0:FMath::Sin(T*.55f)*2.2f;Camera->SetActorRotation(Look);
        Camera->GetCameraComponent()->SetFieldOfView(FMath::Lerp(90.f,100.f,(BloodSpeed-480)/470));
        if(DroppedPickaxe)
        {
            DroppedPickaxe->SetRelativeLocation(PickaxeStart+FVector(600*T,50*T,120*T-260*T*T));
            DroppedPickaxe->AddLocalRotation(FRotator(155*Dt,30*Dt,65*Dt));
            DroppedPickaxe->SetVisibility(T<2.1f);
        }
        if(VesselDistance>=VesselDistances.Last())EnterBrain();
    }
    if(bEnteringBrain)
    {
        const float T=FMath::Clamp((Age-BrainEntryAt)/3.6f,0.f,1.f);
        // Hermite tangent starts at the final blood speed and ends at rest.
        const FVector StartVelocity=GetActorTransform().TransformVectorNoScale(VesselRotation(VesselDistances.Last()).Vector())*BloodSpeed*3.6f;
        Camera->SetActorLocation(FMath::CubicInterp(TransitStart,StartVelocity,TransitEnd,FVector::ZeroVector,T));
        const FRotator ArriveLook=(NeuronWorldPosition(3)-TransitEnd).Rotation();
        Camera->SetActorRotation(FQuat::Slerp(TransitLook.Quaternion(),ArriveLook.Quaternion(),Ending::Ease(T)));
        Camera->GetCameraComponent()->SetFieldOfView(FMath::Lerp(FMath::Lerp(90.f,100.f,(BloodSpeed-480)/470),94.f,Ending::Ease(T)));
        if(T>=1)
        {
            bEnteringBrain=false;GateIndex=2;CurrentNeuron=0;VisitedNeurons.Add(0);BeatAt=Age+4;
            Player->SetControlRotation(ArriveLook);Player->ResetIgnoreLookInput();Sound(TEXT("Connect"),.3f,1.3f);
        }
    }
    if(GateIndex==2&&TravellingToNeuron!=INDEX_NONE)
    {
        const float T=FMath::Clamp((Age-PulseAt)/TransitDuration,0.f,1.f),Travel=Ending::Ease(T),Envelope=FMath::Square(FMath::Sin(PI*T));
        const FVector Position=FMath::Lerp(TransitStart,TransitEnd,Travel)+GetActorUpVector()*(110*FMath::Sin(PI*Travel));
        Camera->SetActorLocation(Position);
        FRotator Look=FQuat::Slerp(TransitLook.Quaternion(),(TransitEnd-TransitStart).Rotation().Quaternion(),Ending::Ease(T/.3f)).Rotator();
        Look.Roll=bComfortMode?0:2.5f*FMath::Sin(T*2*PI)*Envelope;Camera->SetActorRotation(Look);
        Camera->GetCameraComponent()->SetFieldOfView(94+(bComfortMode?4:22)*Envelope);TransitEffects(Envelope);
        // A short wake trails behind the moving viewpoint, never a full-screen flash.
        const FVector From=GetActorTransform().InverseTransformPosition(TransitStart),To=GetActorTransform().InverseTransformPosition(Position);
        PulseTrail->SetRelativeLocation((From+To)*.5);PulseTrail->SetRelativeScale3D(FVector(.07,.07,(To-From).Size()/100));
        PulseTrail->SetRelativeRotation(FRotationMatrix::MakeFromZ(To-From).Rotator());
        if(T>=1)
        {
            CurrentNeuron=TravellingToNeuron;TravellingToNeuron=INDEX_NONE;++PossessionCount;VisitedNeurons.Add(CurrentNeuron);
            Look.Roll=0;Player->SetControlRotation(Look);Player->ResetIgnoreLookInput();TransitEffects(0);
            Sound(TEXT("Connect"),.28f,1+CurrentNeuron*.018f);
            if(CurrentNeuron==NeuronPositions.Num()-1)BeginThought();
        }
    }
    else if(GateIndex==2)
    {
        Camera->SetActorLocation(NeuronWorldPosition(CurrentNeuron)+GetActorUpVector()*110);
        Camera->SetActorRotation(Player->GetControlRotation());
        Camera->GetCameraComponent()->SetFieldOfView(94);TransitEffects(0);
        TargetNeuron=INDEX_NONE;float Best=.990f;
        for(int I=0;I<NeuronPositions.Num();++I)if(CanPossess(I))
        {
            const float Alignment=FVector::DotProduct(Camera->GetActorForwardVector(),(NeuronWorldPosition(I)-Camera->GetActorLocation()).GetSafeNormal());
            if(Alignment>Best){Best=Alignment;TargetNeuron=I;}
        }
        if(Player->WasInputKeyJustPressed(EKeys::SpaceBar)&&TargetNeuron!=INDEX_NONE)PossessNeuron(TargetNeuron);
    }
    if(PulseTrail)PulseTrail->SetVisibility(TravellingToNeuron!=INDEX_NONE);
    for(int I=0;I<NeuronCores.Num();++I)
    {
        NeuronCores[I]->SetVisibility(I!=CurrentNeuron||GateIndex==5||TravellingToNeuron!=INDEX_NONE);
        if(auto* M=Cast<UMaterialInstanceDynamic>(NeuronCores[I]->GetMaterial(0)))
            M->SetVectorParameterValue(TEXT("Tint"),I==TargetNeuron||I==TravellingToNeuron?Ending::Gold*1.5f:I==NeuronPositions.Num()-1?Ending::Gold*.85f:VisitedNeurons.Contains(I)?Ending::Teal:Ending::Teal*.45f);
    }
}

void AEndingEmergence::Tick(float Dt)
{
    Super::Tick(Dt);
    if(!bReady){bReady=Initialize();if(!bReady)return;RestartDemo();}
    Age+=Dt;
    if(Player->WasInputKeyJustPressed(EKeys::C))bComfortMode=!bComfortMode;
    if(Player->WasInputKeyJustPressed(EKeys::R)){RestartDemo();return;}
    if(Shot<0&&Player->WasInputKeyJustPressed(EKeys::P)){PreviewFilm();return;}
    if(Shot<0)
    {
        const FVector P=GetActorTransform().InverseTransformPosition(Player->GetPawn()->GetActorLocation());
        if(GateIndex==0&&Gates.IsValidIndex(0))
        {
            auto* Gate=Gates[GateIndex].Get();
            if(Gate->SuccessfulCuts!=LastCuts){LastCuts=Gate->SuccessfulCuts;Sound(TEXT("Hit"),.36f,1+GateIndex*.08f);}
            if(Gate->SuccessfulCuts>0 && P.X>Ending::GateX[GateIndex]+150)
            {
                LastCuts=0;EnterBlood();
            }
        }
        if(auto* C=Player->GetPawn()->FindComponentByClass<UCameraComponent>())
        {
            C->SetFieldOfView(FMath::FInterpTo(C->FieldOfView,bComfortMode?90.f:FMath::Lerp(90.f,96.f,BrainLens()),Dt,2));
            if(FisheyeMaterial)C->AddOrUpdateBlendable(FisheyeMaterial,bComfortMode?0:BrainLens()*.22f);
        }
        UpdatePulse(Dt);
        if(GateIndex>=5)
        {
            const float Lift=FMath::Clamp(4-(BeatAt-Age),0.f,4.f);
            const FVector Look=FMath::Lerp(TakeoffLook,TakeoffStart+FVector(80,0,-220),Ending::Ease(Lift/2));
            Pose(TakeoffStart+FVector(-Lift*35,0,Lift*230+Lift*Lift*25),Look,FMath::Lerp(96.f,75.f,Lift/4));
        }
        if(GateIndex>=5&&Age>=BeatAt)PreviewFilm();
    }
    else
    {
        const float PreviousShotTime=ShotTime;
        const bool Pulling=Shot>=1&&Shot<=6;
        // W is intentionally absent: the sequence always moves outward. S boosts
        // the rail clock, increasingly at larger scales, with a short inertial release.
        const float TargetBoost=Pulling&&Player->IsInputKeyDown(EKeys::S)?1.f:0.f;
        PullbackBoost=FMath::Lerp(PullbackBoost,TargetBoost,1-FMath::Exp(-2.4f*Dt));
        const float ScalePhase=Pulling?FMath::Clamp((Shot-1+ShotTime/Duration(Shot))/6,0.f,1.f):0;
        PullbackRate=Pulling?1+PullbackBoost*(1.4f+2.6f*ScalePhase*ScalePhase):1;
        ShotTime+=Dt*FMath::Clamp(FilmSpeed,.25f,4.f)*PullbackRate;
        // Blackout and the eye beat retain real time even in accelerated preview.
        if(Shot==7||Shot==8)ShotTime-=Dt*(FMath::Clamp(FilmSpeed,.25f,4.f)-1);
        if(Shot<10&&ShotTime>=Duration(Shot))SetShot(Shot+1);
        if(Shot==9&&PreviousShotTime<4&&ShotTime>=4)Sound(TEXT("IdeaLine"),.95f,1.03f);
        UpdateFilm(Dt);
    }
    if(Shot<0)
    {
        BloodTravel+=Dt*(510+120*FMath::Sin(Age*5.2f));
        for(int I=0;I<Blood.Num();++I)
        {
            const FVector Origin=BloodOrigins[I];const float D=FMath::Fmod(Origin.X+BloodTravel*(.88f+(I%7)*.04f),VesselDistances.Last());
            const FRotator Frame=VesselRotation(D);
            const FVector Offset(0,Origin.Y+FMath::Sin(Age*1.4f+I)*18,Origin.Z+FMath::Cos(Age*1.8f+I)*22);
            Blood[I]->SetRelativeLocation(VesselPoint(D)+Frame.RotateVector(Offset));
            Blood[I]->SetRelativeRotation(Frame.Quaternion()*FRotator(I*31+FMath::Sin(Age+I)*14,I*43+Age*8,22).Quaternion());
            Blood[I]->SetRelativeScale3D(FVector(2.2,2.1,.62)*Ending::Ease(D/180)*Ending::Ease((VesselDistances.Last()-D)/450));
        }
    }
    if(Shot>=0&&Shot<=2&&Crowd)
    {
        for(int I=0;I<CrowdOrigins.Num();++I)
        {
            FTransform T=CrowdOrigins[I];FVector P=T.GetLocation();
            const float Travel=FMath::Sin(Age*.095f+I*.17f)*180;
            if(I%2)P.Y+=Travel;else P.X+=Travel;
            P.Z+=FMath::Sin(Age*4+I)*2.3;T.SetLocation(P);Crowd->UpdateInstanceTransform(I,T,false,false,true);
            CrowdHeads->UpdateInstanceTransform(I,FTransform(FRotator::ZeroRotator,P+FVector(0,0,69),FVector(.29,.29,.35)),false,false,true);
        }
        Crowd->MarkRenderStateDirty();CrowdHeads->MarkRenderStateDirty();
    }
}

float AEndingEmergence::BrainLens() const
{
    if(!Player||!Player->GetPawn()||Shot>=0)return 0;
    return Ending::Ease((GetActorTransform().InverseTransformPosition(Player->GetPawn()->GetActorLocation()).X-6100)/500.f);
}

FVector AEndingEmergence::GetGateApproach(int32 I) const
{
    I=FMath::Clamp(I,0,4);const float X=Ending::GateX[I]-330,Y=Ending::GateY[I];return GetActorTransform().TransformPosition(FVector(X,Y,Ending::Ground(X,Y)+110));
}

FString AEndingEmergence::Title() const
{
    if(Shot<0)return GateIndex<1?TEXT("01 / 最后一矿"):GateIndex<2?TEXT("02 / 顺着光流"):TEXT("03 / 一个念头正在连接");
    const TCHAR* Names[]={TEXT("04 / 念头，飘起"),TEXT("05 / 我们之中"),TEXT("06 / 万家灯火"),TEXT("07 / 薄薄的大气层"),TEXT("08 / 无重力的远方"),TEXT("09 / 宇宙，细胞质"),TEXT("10 / 每一个，都有宇宙"),TEXT(""),TEXT(""),TEXT("11 / 那个下午"),TEXT("涌现 / 结尾 Demo")};
    return Names[FMath::Clamp(Shot,0,10)];
}

FString AEndingEmergence::Hint() const
{
    if(Shot<0)return GateIndex>=5?TEXT("所有微光，汇成一个念头。"):
        GateIndex==0?TEXT("沿金色痕迹前进，靠近发光薄膜，按住左键挖开，再穿过去。"):
        GateIndex==1?(bEnteringBrain?TEXT("血流散开，一道脉冲正接住你。"):TEXT("让血流托住你。按住 W 加速，松手继续漂浮。")):
        TravellingToNeuron!=INDEX_NONE?TEXT("穿过连接，成为下一束光。"):
        TargetNeuron>=0?(TargetNeuron==NeuronPositions.Num()-1?TEXT("空格 · 汇入最大的神经元"):TEXT("空格 · 穿梭到这个神经元")):
        TEXT("瞄准有标记的神经元，再按空格。不同的路径，都通向那束最大的光。");
    if(Shot>=1&&Shot<=6)return TEXT("S 加速拉远 · A / D 微调方向 · W 无效 · C 平稳镜头");
    if(Shot==10)return TEXT("R 再经历一次");
    return TEXT("");
}

FString AEndingEmergence::Subtitle() const
{
    if(Shot<0)
    {
        if(GateIndex==0&&Age<9)return TEXT("最后一块矿里，好像有光在流动。");
        if(bEnteringBrain)return TEXT("血流的尽头，微光开始彼此连接。");
        if(GateIndex==1&&Age<BeatAt+5)return TEXT("它和更多的微光，汇进了同一条河。");
        if(GateIndex==2&&Age<BeatAt)return TEXT("你不再拿着镐子。你就是那一道脉冲。");
        if(GateIndex>=5)return TEXT("一个念头，出现了。");
        return TEXT("");
    }
    if(Shot==0&&ShotTime>2)return TEXT("一个念头，飘了出来。");
    if(Shot==1&&ShotTime>9&&ShotTime<16)return TEXT("然后，是我们。");
    if((Shot==9&&ShotTime>4)||Shot==10)return TEXT("“欸，要不，我们就做挖！”");
    return TEXT("");
}

float AEndingEmergence::OverlayAlpha() const
{
    if(Shot<0)return GateIndex>=5?Ending::Ease((4-(BeatAt-Age)-1.4f)/2.6f):0;
    // Blink out of the brain, cloud match cut to Earth, then matching stellar light.
    if(Shot==0)return 1-Ending::Ease(ShotTime/1.3f);
    if(Shot==1)return Ending::Ease((ShotTime-(Duration(Shot)-1.2f))/1.2f);
    if(Shot==2)return FMath::Max(1-Ending::Ease(ShotTime/1.2f),Ending::Ease((ShotTime-(Duration(Shot)-.9f))/.9f));
    if(Shot==3)return FMath::Max(1-Ending::Ease(ShotTime/1.1f),Ending::Ease((ShotTime-(Duration(Shot)-.9f))/.9f));
    if(Shot==4)return 1-Ending::Ease(ShotTime/1.1f);
    if(Shot==7)return 1;
    return 0;
}

void AEndingEmergence::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Mining)Mining->OnFieldReset.RemoveDynamic(this,&AEndingEmergence::RestartDemo);
    if(Ambience){Ambience->Stop();Ambience->DestroyComponent();}
    if(View&&GEngine&&GEngine->GameViewport)GEngine->GameViewport->RemoveViewportWidgetContent(View.ToSharedRef());
    View.Reset();if(Camera)Camera->Destroy();
    Super::EndPlay(Reason);
}
