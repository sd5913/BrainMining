#include "MiningCollection.h"
#include "DiggableRock.h"
#include "Showcase/MiningTreasure.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Spatial/FastWinding.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "UDynamicMesh.h"

using namespace UE::Geometry;
namespace
{
    struct FCollectibleMesh
    {
        FDynamicMesh3 Mesh;
        FCollectibleMesh() { Mesh.EnableAttributes(); Mesh.Attributes()->EnableMaterialID(); }
        void Tri(int A,int B,int C,int Mat)
        {
            // UE uses clockwise winding / left-handed triangle normals.
            const int Id=Mesh.AppendTriangle(A,C,B);
            if(Id>=0) Mesh.Attributes()->GetMaterialID()->SetValue(Id,Mat);
        }
        void Box(FVector P,FVector Half,int Mat)
        {
            int V[8];
            for(int i=0;i<8;++i) V[i]=Mesh.AppendVertex(P+FVector((i&1)?Half.X:-Half.X,(i&2)?Half.Y:-Half.Y,(i&4)?Half.Z:-Half.Z));
            const int Faces[][4]={{0,4,6,2},{1,3,7,5},{0,1,5,4},{2,6,7,3},{0,2,3,1},{4,5,7,6}};
            for(const auto& F:Faces) { Tri(V[F[0]],V[F[1]],V[F[2]],Mat); Tri(V[F[0]],V[F[2]],V[F[3]],Mat); }
        }
        void Ellipsoid(FVector P,FVector Radius,int Mat,int Sides=10,int Rings=6)
        {
            const int Top=Mesh.AppendVertex(P+FVector(0,0,Radius.Z));
            TArray<int> V;
            for(int r=1;r<Rings;++r) for(int s=0;s<Sides;++s)
            {
                const double T=PI*r/Rings,A=2*PI*s/Sides;
                V.Add(Mesh.AppendVertex(P+Radius*FVector(FMath::Sin(T)*FMath::Cos(A),FMath::Sin(T)*FMath::Sin(A),FMath::Cos(T))));
            }
            const int Bottom=Mesh.AppendVertex(P-FVector(0,0,Radius.Z));
            for(int s=0;s<Sides;++s)
            {
                int N=(s+1)%Sides;
                Tri(Top,V[s],V[N],Mat);
                for(int r=0;r<Rings-2;++r)
                {
                    const int A=r*Sides+s,B=r*Sides+N,C=(r+1)*Sides+s,D=(r+1)*Sides+N;
                    Tri(V[A],V[C],V[D],Mat); Tri(V[A],V[D],V[B],Mat);
                }
                Tri(Bottom,V[(Rings-2)*Sides+N],V[(Rings-2)*Sides+s],Mat);
            }
        }
        void Ring(FVector P,FVector Radius,float Tube,int Mat)
        {
            int V[12][4];
            for(int i=0;i<12;++i) for(int j=0;j<4;++j)
            {
                const double A=2*PI*i/12,B=2*PI*j/4;
                V[i][j]=Mesh.AppendVertex(P+FVector((Radius.X+Tube*FMath::Cos(B))*FMath::Cos(A),(Radius.Y+Tube*FMath::Cos(B))*FMath::Sin(A),Tube*FMath::Sin(B)));
            }
            for(int i=0;i<12;++i) for(int j=0;j<4;++j) { int N=(i+1)%12,K=(j+1)%4; Tri(V[i][j],V[N][j],V[N][K],Mat); Tri(V[i][j],V[N][K],V[i][K],Mat); }
        }
    };
}

AMiningCollectible::AMiningCollectible()
{
    Body=CreateDefaultSubobject<UBoxComponent>(TEXT("CollectibleBody")); SetRootComponent(Body);
    Body->SetBoxExtent(FVector(49,45,61)); Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetCollisionObjectType(ECC_PhysicsBody); Body->SetCollisionResponseToAllChannels(ECR_Block);
    Body->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
    Body->SetLinearDamping(1.4f); Body->SetAngularDamping(3.5f);
    Model=CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("LowpolyModel")); Model->SetupAttachment(Body);
    Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Outline=CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("WhiteSilhouette")); Outline->SetupAttachment(Body);
    Outline->SetCollisionEnabled(ECollisionEnabled::NoCollision); Outline->SetCastShadow(false); Outline->SetVisibility(false);
}

FString AMiningCollectible::ItemName(int32 Id)
{
    const TCHAR* Names[]={TEXT("天堂马桶"),TEXT("蚂蚁的电脑"),TEXT("化学试管架"),TEXT("歪头肉鸽")};
    return Id>=0&&Id<4?Names[Id]:TEXT("未知藏品");
}
FString AMiningCollectible::Description(int32 Id)
{
    const TCHAR* Lines[]={TEXT("藏在洞穴深处的白瓷马桶，\n像一小片安静的天堂。"),TEXT("蚂蚁们偷偷收藏的旧电脑，\n屏幕还在发出温柔的绿光。"),TEXT("四种颜色，四个未知答案。\n小心摆放，暂时别混在一起。"),TEXT("鸟群飞远了，它却留在巢里，\n歪着脑袋，认真地看着你。")};
    return Id>=0&&Id<4?Lines[Id]:FString();
}
FString AMiningCollectible::Clue(int32 Id)
{
    const TCHAR* Lines[]={TEXT("滴水的管道"),TEXT("钻入土里的蚂蚁"),TEXT("彩色液滴"),TEXT("高处的鸟巢")};
    return Id>=0&&Id<4?Lines[Id]:FString();
}

void AMiningCollectible::BuildModel(int32 Id,bool Flying)
{
    ItemId=FMath::Clamp(Id,0,3);
    FCollectibleMesh B;
    // Ivory, charcoal, turquoise, purple, gold, timber, pigeon-grey, coral.
    const FLinearColor Colors[]={{.85f,.87f,.77f},{.035f,.06f,.07f},{.06f,.65f,.43f},{.48f,.12f,.65f},{.95f,.59f,.07f},{.3f,.12f,.045f},{.36f,.44f,.54f},{.86f,.27f,.15f}};
    if(ItemId==0)
    {
        B.Box(FVector(13,0,-44),FVector(23,24,10),0);
        B.Ellipsoid(FVector(2,0,-26),FVector(23,22,24),0);
        B.Ellipsoid(FVector(-9,0,-3),FVector(36,30,25),0);
        B.Ellipsoid(FVector(-12,0,13),FVector(25,20,3),1);
        B.Ellipsoid(FVector(-12,0,16),FVector(19,14,2),2);
        B.Ring(FVector(-9,0,19),FVector(29,24,0),5,0);
        B.Box(FVector(30,0,20),FVector(12,29,32),0);
        B.Box(FVector(30,0,55),FVector(14,31,4),0);
        B.Box(FVector(17,-24,36),FVector(5,3,4),4);
    }
    else if(ItemId==1)
    {
        B.Box(FVector(9,0,13),FVector(26,40,32),0);
        B.Box(FVector(-19,0,14),FVector(3,33,25),1);
        B.Box(FVector(-23,0,16),FVector(1,29,20),2);
        B.Box(FVector(-25,-13,16),FVector(1,4,5),0);
        B.Box(FVector(-25,7,5),FVector(1,14,2),1);
        B.Box(FVector(7,0,-27),FVector(10,12,10),6);
        B.Box(FVector(0,0,-38),FVector(24,30,4),0);
        B.Box(FVector(-28,0,-49),FVector(20,42,6),6);
        for(int r=0;r<3;++r) for(int c=0;c<8;++c) B.Box(FVector(-42+r*12,-33+c*9,-41),FVector(4,3,2),0);
        B.Ellipsoid(FVector(-25,19,28),FVector(2,3,2),1,6,4);
    }
    else if(ItemId==2)
    {
        B.Box(FVector(0,0,-45),FVector(23,44,6),5);
        for(int S:{-1,1}) B.Box(FVector(0,S*40,-8),FVector(20,4,36),5);
        B.Box(FVector(0,0,10),FVector(21,43,4),5);
        for(int i=0;i<4;++i)
        {
            const float Y=-29+i*19;
            B.Ellipsoid(FVector(0,Y,-14),FVector(9,8,18),2+i%3,8,4);
            B.Box(FVector(0,Y,18),FVector(6,6,25),0);
            B.Box(FVector(-7,Y,5),FVector(1,5,21),2+i%3);
            B.Box(FVector(0,Y,46),FVector(8,8,5),5);
        }
    }
    else
    {
        B.Ellipsoid(FVector(1,0,-12),FVector(32,28,34),6);
        B.Ellipsoid(FVector(-14,0,16),FVector(21,21,27),2);
        B.Ellipsoid(FVector(-19,-3,39),FVector(22,20,21),6);
        if(!Flying)
        {
            B.Ellipsoid(FVector(5,-25,-6),FVector(25,7,22),0);
            B.Ellipsoid(FVector(5,25,-6),FVector(25,7,22),0);
        }
        B.Ellipsoid(FVector(-33,-18,43),FVector(6,3,6),4,8,4);
        B.Ellipsoid(FVector(-35,-20,43),FVector(3,2,4),1,8,4);
        B.Ellipsoid(FVector(-33,14,43),FVector(6,3,6),4,8,4);
        B.Ellipsoid(FVector(-35,16,43),FVector(3,2,4),1,8,4);
        B.Ellipsoid(FVector(-41,-3,32),FVector(10,8,5),0,6,3);
        B.Ellipsoid(FVector(31,0,-24),FVector(18,14,8),1,6,3);
        for(int S:{-1,1}) { B.Box(FVector(0,S*13,-45),FVector(3,3,9),7); B.Box(FVector(-6,S*13,-53),FVector(12,7,3),7); }
    }
    FMeshNormals::InitializeOverlayToPerTriangleNormals(B.Mesh.Attributes()->PrimaryNormals());
    Model->SetMesh(FDynamicMesh3(B.Mesh)); Outline->SetMesh(MoveTemp(B.Mesh));
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionTint"));
    auto* Edge=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionOutline"));
    for(int i=0;i<UE_ARRAY_COUNT(Colors);++i)
    {
        if(Base) { auto* Mat=UMaterialInstanceDynamic::Create(Base,this); Mat->SetVectorParameterValue(TEXT("Tint"),Colors[i]); Model->SetMaterial(i,Mat); }
        if(Edge) Outline->SetMaterial(i,Edge);
    }
}

void AMiningCollectible::SetHighlighted(bool Enabled)
{
    // The postprocess draws only outside the visible custom-depth silhouette.
    // Keep the old inverted hull disabled: it can cover surfaces or outline internal seams.
    Outline->SetVisibility(false);
    Model->SetRenderCustomDepth(Enabled&&!bCollected);
    if(IsValid(PresentationActor))
    {
        Model->SetRenderCustomDepth(false);
        TArray<UPrimitiveComponent*> Components;PresentationActor->GetComponents(Components);
        for(auto* Component:Components) if(Component->IsA<UStaticMeshComponent>())
        {
            Component->SetCustomDepthStencilValue(1);
            Component->SetRenderCustomDepth(Enabled&&!bCollected);
        }
    }
}
float AMiningCollectible::MeasureExposure() const
{
    if(!IsValid(HostRock)||HostRock->TriangleCount==0) return 1;
    // Sample the actual model surface, independent of attempted/failed cutting spheres.
    const FDynamicMesh3& Art=Model->GetDynamicMesh()->GetMeshRef();
    TArray<FVector> Samples;const int Stride=FMath::Max(1,Art.VertexCount()/100);
    for(int V:Art.VertexIndicesItr()) if(V%Stride==0)
    {
        const FVector World=Model->GetComponentTransform().TransformPosition(Art.GetVertex(V));
        Samples.Add(World);
    }
    TArray<bool> Covered;AMiningTreasure::MarkCoveredPoints({HostRock},Samples,Covered);
    int Free=0;for(bool Solid:Covered)Free+=!Solid;
    return Covered.Num()>0?float(Free)/Covered.Num():0;
}
bool AMiningCollectible::CanRelease(float Threshold) const
{
    return !bReleased&&!bCollected&&Exposure>=FMath::Clamp(Threshold,.1f,1.f);
}
void AMiningCollectible::Release()
{
    if(bReleased||bCollected) return;
    // Before enabling physics, require a clear body volume and an air path to it.
    // This prevents a partly buried rigid body being explosively ejected by Chaos.
    FVector Position=GetActorLocation(); bool Clear=!IsValid(HostRock)||HostRock->TriangleCount==0;
    if(!Clear)
    {
        auto Air=[&](FVector P)
        {
            TArray<bool> Covered;AMiningTreasure::MarkCoveredPoints({HostRock},{P},Covered);
            return !Covered[0];
        };
        const FVector Start=Position;
        for(int Step=0;Step<=10;++Step)
        {
            const FVector Candidate=Start-GetActorForwardVector()*(Step*20);
            if(!Air(Candidate)) break;
            Clear=true;
            for(int X=-1;X<=1;++X) for(int Y=-1;Y<=1;++Y) for(int Z=-1;Z<=1;++Z)
                Clear &= Air(Candidate+GetActorQuat().RotateVector(FVector(X*51,Y*47,Z*63)));
            if(Clear) { Position=Candidate; break; }
        }
    }
    if(!Clear) return;
    SetActorLocation(Position); bReleased=true;
    Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Body->SetSimulatePhysics(true); Body->SetMassOverrideInKg(NAME_None,3,true);
    Body->AddImpulse(-GetActorForwardVector()*55+FVector(0,0,35),NAME_None,true);
}
