#include "MiningCollection.h"
#include "DiggableRock.h"
#include "Showcase/MiningTreasure.h"
#include "Components/BoxComponent.h"
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
        void Rod(FVector A,FVector B,float Radius,int Mat)
        {
            const FVector Axis=(B-A).GetSafeNormal();
            const FVector U=FVector::CrossProduct(Axis,FVector(0,0,1)).GetSafeNormal(UE_SMALL_NUMBER,FVector(0,1,0));
            const FVector V=FVector::CrossProduct(Axis,U);
            int Ends[2][8];
            for(int E=0;E<2;++E) for(int I=0;I<8;++I)
                Ends[E][I]=Mesh.AppendVertex((E?B:A)+Radius*(U*FMath::Cos(I*PI/4)+V*FMath::Sin(I*PI/4)));
            for(int I=0;I<8;++I) { const int N=(I+1)%8; Tri(Ends[0][I],Ends[1][I],Ends[1][N],Mat); Tri(Ends[0][I],Ends[1][N],Ends[0][N],Mat); }
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
        void Cylinder(FVector P,float Radius,float Height,int Mat,int Sides=40)
        {
            int Bottom=Mesh.AppendVertex(P-FVector(0,0,Height*.5f)),Top=Mesh.AppendVertex(P+FVector(0,0,Height*.5f));
            TArray<int> Lo,Hi;
            for(int I=0;I<Sides;++I) { const float A=I*2*PI/Sides; const FVector R(Radius*FMath::Cos(A),Radius*FMath::Sin(A),0); Lo.Add(Mesh.AppendVertex(P+R-FVector(0,0,Height*.5f))); Hi.Add(Mesh.AppendVertex(P+R+FVector(0,0,Height*.5f))); }
            for(int I=0;I<Sides;++I) { int J=(I+1)%Sides; Tri(Lo[I],Lo[J],Hi[J],Mat); Tri(Lo[I],Hi[J],Hi[I],Mat); Tri(Bottom,Lo[J],Lo[I],Mat); Tri(Top,Hi[I],Hi[J],Mat); }
        }
        void Curve(FVector A,FVector ControlA,FVector ControlB,FVector End,float Radius,int Mat,int Steps=18)
        {
            FVector Prev=A;
            for(int I=1;I<=Steps;++I) { const float T=float(I)/Steps,S=1-T; const FVector P=S*S*S*A+3*S*S*T*ControlA+3*S*T*T*ControlB+T*T*T*End; Rod(Prev,P,FMath::Lerp(Radius,.22f,T),Mat); Prev=P; }
        }
        #include "MiningCollectibleDetails.inl"
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
    const TCHAR* Names[]={TEXT("来自天堂的马桶"),TEXT("电脑里的蚂蚁窝"),TEXT("化学试剂"),TEXT("肉鸽"),TEXT("奇怪的计算器"),TEXT("损坏的磁带"),TEXT("音符"),TEXT("老旧的相片"),TEXT("喷发中的可乐"),TEXT("「GPT」之花"),TEXT("同步闪光的萤火虫"),TEXT("散落的彩带")};
    return Id>=0&&Id<ItemTypeCount?Names[Id]:TEXT("未知藏品");
}
FString AMiningCollectible::Description(int32 Id)
{
    const TCHAR* Lines[]={TEXT("不觉得这很神圣吗？"),TEXT("一台正在运行一个游戏的电脑……这是什么游戏？"),TEXT("加点这个……再加点那个……"),TEXT("好肥的鸟！"),
        TEXT("这个年代计算器也有自我意识了吗？"),
        TEXT("小时候最喜欢的一集。"),
        TEXT("长得很像蝌蚪，它会变成青蛙吗？"),
        TEXT("记忆是很宝贵的。"),
        TEXT("曼妥思VS可乐，你们知道吗？"),
        TEXT("AI大人牛逼。"),
        TEXT("科学小知识，最具教育意义的一集。"),
        TEXT("恭喜！")};
    return Id>=0&&Id<ItemTypeCount?Lines[Id]:FString();
}
FString AMiningCollectible::Clue(int32 Id)
{
    const TCHAR* Lines[]={TEXT("滴水的管道"),TEXT("沿土层爬向蚁穴的蚂蚁"),TEXT("彩色液滴"),TEXT("高处的鸟巢"),TEXT("土里错落埋藏的数字 1"),TEXT(""),TEXT(""),TEXT(""),TEXT("连通地下空腔、持续向上冒泡的孔洞"),TEXT("土面上的一簇微光小花"),TEXT("沿土层延伸的闪烁光点"),TEXT("红幕布后的庆祝舞台")};
    return Id>=0&&Id<ItemTypeCount?Lines[Id]:FString();
}

void AMiningCollectible::BuildModel(int32 Id,bool Flying)
{
    ItemId=FMath::Clamp(Id,0,ItemTypeCount-1);
    Body->SetBoxExtent(ItemId==9?FVector(190,190,330):ItemId==11?FVector(12,54,9):FVector(49,45,61));
    FCollectibleMesh B;
    // Ivory, charcoal, turquoise, purple, gold, timber, pigeon-grey, coral.
    FLinearColor Colors[]={{.85f,.87f,.77f},{.035f,.06f,.07f},{.06f,.65f,.43f},{.48f,.12f,.65f},{.95f,.59f,.07f},{.3f,.12f,.045f},{.36f,.44f,.54f},{.86f,.27f,.15f}};
    if(ItemId==1 || ItemId==4 || ItemId==8)
    {
        Colors[0]=FLinearColor(.78f,.76f,.65f);Colors[1]=FLinearColor(.018f,.024f,.027f);
        Colors[2]=ItemId==4?FLinearColor(.24f,.34f,.22f):FLinearColor(.035f,.19f,.12f);
        Colors[5]=FLinearColor(.13f,.055f,.022f);Colors[6]=FLinearColor(.37f,.42f,.43f);
        Colors[7]=FLinearColor(.63f,.018f,.035f);
    }
    if(ItemId==11){Colors[4]=FLinearColor(1,.045f,.35f);Colors[7]=FLinearColor(1,.35f,.65f);}
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
        B.DetailedComputer();
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
    else if(ItemId==3)
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
    else if(ItemId==4)
    {
        B.DetailedCalculator();
    }
    else if(ItemId==5)
    {
        B.Box(FVector::ZeroVector,FVector(12,44,29),1);
        B.Box(FVector(-13,0,5),FVector(1,39,19),0);
        B.Box(FVector(-15,0,-1),FVector(1,31,10),6);
        for(int Side:{-1,1})
        {
            B.Ellipsoid(FVector(-17,Side*21,-1),FVector(2,10,10),1,20,10);
            B.Ellipsoid(FVector(-20,Side*21,-1),FVector(1,4,4),0,12,6);
            for(int J=0;J<6;++J) { const float A=J*PI/3; B.Rod(FVector(-21,Side*21+4*FMath::Cos(A),-1+4*FMath::Sin(A)),FVector(-21,Side*21+8*FMath::Cos(A),-1+8*FMath::Sin(A)),1,0); }
        }
        B.Box(FVector(-16,0,18),FVector(1,34,2),7);
        B.Box(FVector(-15,0,-23),FVector(1,25,5),6);
        for(int Side:{-1,1}) B.Ellipsoid(FVector(-17,Side*16,-24),FVector(1,3,3),1);
    }
    else if(ItemId==6)
    {
        B.Box(FVector(0,0,-53),FVector(23,39,5),1);
        for(int Side:{-1,1})
        {
            B.Ellipsoid(FVector(0,Side*25,-29+Side*5),FVector(10,17,11),4,20,10);
            B.Rod(FVector(0,Side*25+11,-29+Side*5),FVector(0,Side*25+11,47+Side*5),4,4);
        }
        B.Rod(FVector(0,-14,43),FVector(0,36,53),6,4);
        B.Rod(FVector(0,-14,29),FVector(0,36,39),4,4);
    }
    else if(ItemId==7)
    {
        B.Box(FVector(5,0,0),FVector(4,42,54),5);
        B.Box(FVector(-1,0,0),FVector(2,35,47),2);
        for(int Side:{-1,1})
        {
            B.Box(FVector(-4,Side*40,0),FVector(6,5,56),4);
            B.Box(FVector(-4,0,Side*51),FVector(6,40,5),4);
        }
        // Two quiet figures leaning together, fine charcoal contours on ivory paper.
        B.Box(FVector(-3.2,0,0),FVector(.3,35,47),0);
        auto Line=[&](FVector2D A,FVector2D C,FVector2D D,FVector2D E,float W=.55f)
        { B.Curve(FVector(-4.2,A.X,A.Y),FVector(-4.2,C.X,C.Y),FVector(-4.2,D.X,D.Y),FVector(-4.2,E.X,E.Y),W,1,24); };
        Line({-22,17},{-23,38},{-2,39},{-3,18});
        Line({-22,17},{-20,6},{-6,6},{-3,18});
        Line({-20,27},{-17,35},{-7,36},{-3,24},.8);
        Line({-17,10},{-18,2},{-27,3},{-29,-12});
        Line({-7,10},{-5,1},{4,2},{7,-14});
        Line({-29,-12},{-30,-23},{-29,-30},{-27,-35});
        Line({-27,-35},{-12,-38},{-2,-36},{3,-33});
        Line({3,18},{1,34},{19,34},{21,20});
        Line({3,18},{4,7},{18,6},{21,20});
        Line({4,25},{1,39},{23,36},{24,10},.85);
        Line({24,10},{26,2},{21,0},{18,5},.7);
        Line({8,9},{6,0},{5,-5},{5,-17});
        Line({18,8},{28,3},{29,-6},{28,-16});
        Line({28,-16},{30,-25},{29,-32},{26,-35});
        Line({26,-35},{18,-37},{9,-36},{3,-33});
        // A relaxed arm rests across the other's shoulder, with no exaggerated faces.
        Line({23,-5},{14,3},{-2,2},{-10,-4},.65);
        Line({-23,-10},{-22,-21},{-17,-27},{-10,-29});
        B.Rod(FVector(9,0,30),FVector(30,0,-53),3,5);
    }
    else if(ItemId==8)
    {
        B.DetailedCola();
    }
    else if(ItemId==9)
    {
        FRandomStream Roots(92174);
        // Unequal, tapered cubic curves with branches growing from the parent curve.
        for(int R=0;R<15;++R)
        {
            const float Angle=Roots.FRandRange(0,2*PI),Reach=Roots.FRandRange(55,115);
            const FVector A(Roots.FRandRange(-5,5),Roots.FRandRange(-5,5),12);
            const FVector End(FMath::Cos(Angle)*Reach,FMath::Sin(Angle)*Reach,Roots.FRandRange(-245,-165));
            const FVector C1=A+FVector(Roots.FRandRange(-25,25),Roots.FRandRange(-25,25),-80);
            const FVector C2=End+FVector(Roots.FRandRange(-23,23),Roots.FRandRange(-23,23),Roots.FRandRange(45,95));
            B.Curve(A,C1,C2,End,Roots.FRandRange(2.5,4.6),5,26);
            for(int J=0;J<5;++J)
            {
                const float T=Roots.FRandRange(.28,.85),S=1-T;
                const FVector P=S*S*S*A+3*S*S*T*C1+3*S*T*T*C2+T*T*T*End;
                FVector Tip=P+FVector(Roots.FRandRange(-30,30),Roots.FRandRange(-30,30),Roots.FRandRange(-38,-12));
                Tip.X=FMath::Clamp(Tip.X,-94.,94.);Tip.Y=FMath::Clamp(Tip.Y,-94.,94.);Tip.Z=FMath::Max(Tip.Z,-270.);
                B.Curve(P,P+FVector(Roots.FRandRange(-12,12),Roots.FRandRange(-12,12),-7),Tip+FVector(8,-5,12),Tip,Roots.FRandRange(.8,1.7),5,15);
            }
        }
        for(int V:B.Mesh.VertexIndicesItr())
        {
            FVector3d P=B.Mesh.GetVertex(V);P.X*=1.65;P.Y*=1.65;P.Z=12+(P.Z-12)*1.55;
            B.Mesh.SetVertex(V,P);
        }
        for(int F=0;F<7;++F)
        {
            const float Angle=F*2.39996f;
            const FVector Head(FMath::Sin(Angle)*13,FMath::Cos(Angle)*(F?29:0),F==0?119:94+(F%3)*8);
            const float Size=F==0?12:8.5f+(F%2)*1.5f;
            B.Curve(FVector(0,0,7),FVector(0,Head.Y*.3,55),Head-FVector(4,4,23),Head,1.7,2,20);
            B.Ellipsoid(Head-FVector(0,Head.Y>0?6:-6,31),FVector(2,9,3.5),2);
            B.Ellipsoid(Head,FVector(2.3,Size,Size),9,24,12);
            for(int L=0;L<6;++L) for(int J=0;J<24;++J)
            {
                auto Knot=[&](int K) { const float A=L*PI/3,T=K*2*PI/24,U=Size*(.47f+.39f*FMath::Cos(T)),V=Size*.34f*FMath::Sin(T);return Head+FVector(-3-.4f*FMath::Sin(T*2),U*FMath::Cos(A)-V*FMath::Sin(A),U*FMath::Sin(A)+V*FMath::Cos(A)); };
                B.Rod(Knot(J),Knot(J+1),.65f,0);
            }
        }
    }
    else if(ItemId==10)
    {
        // Transparent cylindrical glass, a screw-top lid, and many tiny luminous insects.
        B.Cylinder(FVector(0,0,-1),27,78,8,48);
        B.Ring(FVector(0,0,-40),FVector(26,26,0),1.6,0);
        B.Cylinder(FVector(0,0,41),28,6,6,40);
        for(int Z:{37,40,44}) B.Ring(FVector(0,0,Z),FVector(27,27,0),1,4);
        FRandomStream Flies(1042);
        for(int I=0;I<46;++I)
        {
            const float A=Flies.FRandRange(0,2*PI),Radius=Flies.FRandRange(2,21);
            const FVector P(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,Flies.FRandRange(-32,30));
            B.Ellipsoid(P,FVector(Flies.FRandRange(.55,1.25)),9,8,5);
            if(I%4==0) B.Ellipsoid(P+FVector(0,0,1.5),FVector(.5,.7,1),1,6,4);
        }
    }
    else if(ItemId==11)
    {
        // One continuous ribbon; the surrounding falling confetti is not collectible.
        for(int J=0;J<40;++J)
        {
            const float Y=-50+J*2.5f,Next=Y+2.5f;
            const FVector A(FMath::Sin(Y*.09f)*7,Y,FMath::Cos(Y*.12f)*5),C(FMath::Sin(Next*.09f)*7,Next,FMath::Cos(Next*.12f)*5);
            B.Rod(A,C,2.6f,4);
            B.Rod(A+FVector(3,0,0),C+FVector(3,0,0),1.2f,7);
        }
    }
    FMeshNormals::InitializeOverlayToPerTriangleNormals(B.Mesh.Attributes()->PrimaryNormals());
    if(ItemId==9)
    {
        FVector Extent=FVector::ZeroVector;
        for(int V:B.Mesh.VertexIndicesItr())Extent=Extent.ComponentMax(FVector(B.Mesh.GetVertex(V)).GetAbs());
        Body->SetBoxExtent(Extent+FVector(2));
    }
    Model->SetMesh(FDynamicMesh3(B.Mesh)); Outline->SetMesh(MoveTemp(B.Mesh));
    auto* Base=LoadObject<UMaterialInterface>(nullptr,(ItemId==1||ItemId==4||ItemId==8)?TEXT("/Game/MainGame/Materials/ScenePolish/M_CollectibleDetail"):TEXT("/Game/CollectionPrototype/Materials/M_CollectionTint"));
    auto* Edge=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionOutline"));
    for(int i=0;i<UE_ARRAY_COUNT(Colors);++i)
    {
        if(Base) { auto* Mat=UMaterialInstanceDynamic::Create(Base,this); Mat->SetVectorParameterValue(TEXT("Tint"),Colors[i]);
            if(ItemId==8 && i==6){Mat->SetScalarParameterValue(TEXT("Metallic"),.82f);Mat->SetScalarParameterValue(TEXT("Roughness"),.28f);}
            if(ItemId==1 && i==2){Mat->SetScalarParameterValue(TEXT("Glow"),.35f);Mat->SetScalarParameterValue(TEXT("Roughness"),.25f);}
            Model->SetMaterial(i,Mat); }
        if(Edge) Outline->SetMaterial(i,Edge);
    }
    if(ItemId==9 || ItemId==10)
    {
        const TCHAR* Path=ItemId==9?TEXT("/Game/CollectionPrototype/Materials/M_CollectionFlowerGlow"):TEXT("/Game/CollectionPrototype/Materials/M_CollectionSparkCore");
        if(auto* Glow=LoadObject<UMaterialInterface>(nullptr,Path)) Model->SetMaterial(9,Glow);
        if(Edge) { Outline->SetMaterial(8,Edge); Outline->SetMaterial(9,Edge); }
    }
    if(ItemId==10)
        if(auto* Glass=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionGlass"))) Model->SetMaterial(8,Glass);
}

void AMiningCollectible::SetHighlighted(bool Enabled)
{
    // The postprocess draws only outside the visible custom-depth silhouette.
    // Keep the old inverted hull disabled: it can cover surfaces or outline internal seams.
    Outline->SetVisibility(bEchoPresentation&&!bCollected);
    Model->SetRenderCustomDepth(Enabled&&!bCollected);
}
void AMiningCollectible::ApplyEchoPresentation(UMaterialInterface* Surface,UMaterialInterface* Contours)
{
    if(!Surface || !Contours) return;
    bEchoPresentation=true;
    FCollectibleMesh Edges;
    const auto& Mesh=Model->GetDynamicMesh()->GetMeshRef();
    for(int Edge:Mesh.EdgeIndicesItr())
    {
        const auto T=Mesh.GetEdgeT(Edge);
        // Suppress coplanar triangulation diagonals, retain the model's actual contours.
        if(T.B>=0 && FVector3d::DotProduct(Mesh.GetTriNormal(T.A),Mesh.GetTriNormal(T.B))>.94) continue;
        const auto V=Mesh.GetEdgeV(Edge);
        Edges.Rod(FVector(Mesh.GetVertex(V.A)),FVector(Mesh.GetVertex(V.B)),.55f,0);
    }
    FMeshNormals::InitializeOverlayToPerTriangleNormals(Edges.Mesh.Attributes()->PrimaryNormals());
    Outline->SetMesh(MoveTemp(Edges.Mesh));Outline->SetMaterial(0,Contours);
    for(int Slot=0;Slot<Model->GetNumMaterials();++Slot)Model->SetMaterial(Slot,Surface);
    Outline->SetVisibility(true);Model->SetRenderCustomDepth(false);
}
float AMiningCollectible::MeasureExposure() const
{
    if(!IsValid(HostRock)||HostRock->TriangleCount==0) return 1;
    if(ItemId==9)
    {
        // Sample the crown and deep root mass: exposing only the flowers is insufficient.
        TArray<FVector> Crown;
        for(int Z=0;Z<6;++Z)for(int X=-1;X<=1;++X)for(int Y=-1;Y<=1;++Y)
            Crown.Add(GetActorTransform().TransformPosition(FVector(X*60,Y*60,-35-Z*55)));
        TArray<bool> Covered;AMiningTreasure::MarkCoveredPoints({HostRock},Crown,Covered);
        int Free=0;for(bool Solid:Covered)Free+=!Solid;
        return float(Free)/Crown.Num();
    }
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
    return !bReleased&&!bCollected&&Exposure>=FMath::Clamp(Threshold,ItemId==9?.8f:.1f,1.f);
}
void AMiningCollectible::Release()
{
    if(bReleased||bCollected) return;
    if(ItemId==4 || ItemId==9)
    {
        if(MeasureExposure()<(ItemId==9?.8f:.65f))return;
        // Keep the excavated calculator facing the player, with no Chaos impulse
        // that can throw it through a terrain chunk awaiting collision cooking.
        bReleased=true;
        if(ItemId==9)Body->SetBoxExtent(FVector(48,48,125));
        Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        return;
    }
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
                Clear &= Air(Candidate+GetActorQuat().RotateVector((Body->GetScaledBoxExtent()+FVector(2))*FVector(X,Y,Z)));
            if(Clear) { Position=Candidate; break; }
        }
    }
    if(!Clear) return;
    SetActorLocation(Position); bReleased=true;
    Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Body->SetSimulatePhysics(true); Body->SetMassOverrideInKg(NAME_None,3,true);
    Body->AddImpulse(-GetActorForwardVector()*55+FVector(0,0,35),NAME_None,true);
}
