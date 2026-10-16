#include "EndingComicRock.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "Generators/BoxSphereGenerator.h"
#include "UDynamicMesh.h"

using namespace UE::Geometry;

void EndingComic::InkFacets(FDynamicMesh3& Mesh)
{
    // Encode actual crease edges, not a repeating painted pattern. Suppress
    // coplanar boolean diagonals and tiny edges so the face does not become a grid.
    Mesh.DiscardAttributes();Mesh.EnableAttributes();
    FMeshNormals::InitializeOverlayToPerTriangleNormals(Mesh.Attributes()->PrimaryNormals());
    Mesh.Attributes()->EnablePrimaryColors();
    auto* UV=Mesh.Attributes()->PrimaryUV();auto* Colors=Mesh.Attributes()->PrimaryColors();
    TArray<FVector3d> Normals;Normals.SetNum(Mesh.MaxTriangleID());
    for(int T:Mesh.TriangleIndicesItr())Normals[T]=Mesh.GetTriNormal(T);
    for(int T:Mesh.TriangleIndicesItr())
    {
        const FIndex3i Edges=Mesh.GetTriEdges(T);
        FVector4f Ink(0,0,0,.8f+.2f*FMath::Frac(FMath::Abs(FMath::Sin(T*1.735f))*17.13f));
        for(int I=0;I<3;++I)
        {
            const int E=Edges[(I+1)%3];const FIndex2i Adj=Mesh.GetEdgeT(E),V=Mesh.GetEdgeV(E);
            const int Other=Adj.A==T?Adj.B:Adj.A;
            const double Bend=Other<0?1:1-Normals[T].Dot(Normals[Other]);
            Ink[I]=(Mesh.GetVertex(V.A)-Mesh.GetVertex(V.B)).SquaredLength()>100
                ?FMath::Clamp(float((Bend-.045)/.30),0.f,.95f):0.f;
        }
        UV->SetTriangle(T,FIndex3i(UV->AppendElement(FVector2f(1,0)),UV->AppendElement(FVector2f(0,1)),UV->AppendElement(FVector2f(0,0))));
        Colors->SetTriangle(T,FIndex3i(Colors->AppendElement(Ink),Colors->AppendElement(Ink),Colors->AppendElement(Ink)));
    }
}

FDynamicMesh3 EndingComic::Boulder(FVector Size,int32 Seed)
{
    // A cube-sphere distributes vertices evenly; latitude spheres leave a very
    // visible fan of long triangles at their poles when inked.
    FBoxSphereGenerator G;G.Radius=1;G.Box=FOrientedBox3d(FVector3d::Zero(),FVector3d(1));G.EdgeVertices=FIndex3i(6,6,6);
    G.Generate();FDynamicMesh3 Mesh(&G);Mesh.DiscardAttributes();
    FRandomStream R(Seed+731);
    const FQuat Tilt=FRotator(13,Seed*23%90,17).Quaternion();
    for(int V:Mesh.VertexIndicesItr())
    {
        FVector D=(Tilt.RotateVector(FVector(Mesh.GetVertex(V)))+R.VRand()*.045).GetSafeNormal();
        const double Largest=D.GetAbsMax();
        D/=Largest;
        Mesh.SetVertex(V,FVector3d(D*Size*.5*R.FRandRange(.72f,1.28f)));
    }
    return Mesh;
}

FDynamicMesh3 EndingComic::Volume(FVector Size)
{
    // Shared end planes meet exactly; all variation is made by the ending cutter.
    FGridBoxMeshGenerator G;G.Box=FOrientedBox3d(FVector3d::Zero(),FVector3d(Size*.5));G.EdgeVertices=FIndex3i(3,7,7);
    G.Generate();FDynamicMesh3 Mesh(&G);Mesh.DiscardAttributes();
    for(int V:Mesh.VertexIndicesItr())
    {
        FVector3d P=Mesh.GetVertex(V);
        const double Y=P.Y/(Size.Y*.5),Z=P.Z/(Size.Z*.5);
        // Map the square cross-section onto a disk without a long triangle fan.
        P.Y*=FMath::Sqrt(1-Z*Z*.5);P.Z*=FMath::Sqrt(1-Y*Y*.5);
        // Identical relief on both ends keeps neighbouring chunks watertight.
        P.X+=(FMath::Sin(Y*7.3+Z*4.1)*.55+FMath::Cos(Y*11.7-Z*8.3)*.45)*32*(1-FMath::Max(FMath::Abs(Y),FMath::Abs(Z)));
        Mesh.SetVertex(V,P);
    }
    return Mesh;
}

FDynamicMesh3 EndingComic::Tunnel(int32 Stage)
{
    FDynamicMesh3 Mesh;FRandomStream R(426+Stage*79);
    constexpr int Rows=25,Sides=18;
    for(int Row=0;Row<=Rows;++Row)for(int J=0;J<Sides;++J)
    {
        const double A=J*2*PI/Sides+(Row%2?.075:-.045)+(Stage>=3?Row*.12:0);
        const double Base=235;
        const double Radius=Base+R.FRandRange(-34,34)+FMath::Sin(Row*.7+J*1.7)*16;
        const double X=Stage*1300+Row*1300.0/Rows+(Row>0&&Row<Rows?R.FRandRange(-15,15):0);
        Mesh.AppendVertex(FVector3d(X,FMath::Cos(A)*Radius,FMath::Sin(A)*Radius));
    }
    for(int Row=0;Row<Rows;++Row)for(int J=0;J<Sides;++J)
    {
        const int A=Row*Sides+J,B=Row*Sides+(J+1)%Sides,C=A+Sides,D=B+Sides;
        if((Row+J)%2){Mesh.AppendTriangle(A,C,B);Mesh.AppendTriangle(B,C,D);}
        else{Mesh.AppendTriangle(A,C,D);Mesh.AppendTriangle(A,D,B);}
    }
    // Irregular jutting blocks and long crystal-like spurs make parallax visible.
    for(int I=0;I<28;++I)
    {
        const double X=Stage*1300+40+I*46,A=I*2.39996+(Stage>=3?I*.12:0);
        const FVector Radial(0,FMath::Cos(A),FMath::Sin(A));
        const FVector Size=Stage==1?FVector(55,65,170):Stage==2?FVector(145,60,90):FVector(130,95,100);
        FDynamicMesh3 Chunk=Boulder(Size,Stage*100+I);
        const FQuat Q=FQuat::FindBetweenNormals(FVector::UpVector,Radial)*FRotator(0,0,I*19).Quaternion();
        const FVector Center=FVector(X,0,0)+Radial*(Stage==1?300:250);
        TArray<int> Map;Map.SetNum(Chunk.MaxVertexID());
        for(int V:Chunk.VertexIndicesItr())Map[V]=Mesh.AppendVertex(FVector3d(Center+Q.RotateVector(FVector(Chunk.GetVertex(V)))));
        for(int T:Chunk.TriangleIndicesItr()){const FIndex3i V=Chunk.GetTriangle(T);Mesh.AppendTriangle(Map[V.A],Map[V.B],Map[V.C]);}
    }
    InkFacets(Mesh);return Mesh;
}

AEndingComicRock::AEndingComicRock(){bUseProceduralCutFallback=true;}

void AEndingComicRock::ResetRock()
{
    FDynamicMesh3 Mesh=bContinuousVolume?EndingComic::Volume(RockSize):EndingComic::Boulder(RockSize,VisualSeed);EndingComic::InkFacets(Mesh);CommitMesh(MoveTemp(Mesh));
    if(RockMaterial)RockMesh->SetMaterial(0,RockMaterial);
    SuccessfulCuts=0;LastStatus=TEXT("Ending faceted rock ready");
}

bool AEndingComicRock::MineSphere(FVector WorldCenter,float WorldRadius)
{
    if(WorldCenter.ContainsNaN()||!FMath::IsFinite(WorldRadius)||WorldRadius<=0)return false;
    // Same shared boolean/validation/collision path, with an ending-only cutter.
    FBoxSphereGenerator G;G.Radius=1;G.Box=FOrientedBox3d(FVector3d::Zero(),FVector3d(1));G.EdgeVertices=FIndex3i(5,5,5);
    G.Generate();FDynamicMesh3 Cutter(&G);Cutter.DiscardAttributes();
    FRandomStream R(VisualSeed*97+SuccessfulCuts*31+529);
    const FQuat Q=FRotator(R.FRandRange(-90,90),R.FRandRange(-90,90),R.FRandRange(-90,90)).Quaternion();
    for(int V:Cutter.VertexIndicesItr())
    {
        FVector Direction=Q.RotateVector(FVector(Cutter.GetVertex(V)));
        if(bContinuousVolume)
        {
            FVector Local=GetActorQuat().UnrotateVector(Direction);Local.X*=.26;
            Direction=GetActorQuat().RotateVector(Local);
        }
        Cutter.SetVertex(V,FVector3d(WorldCenter+Direction*WorldRadius*R.FRandRange(.90f,1.13f)));
    }
    bool Cut=CutLocal(WorldCenter,WorldRadius,Cutter);
    // The same cutter crosses a chunk seam; no intact cap is left between volumes.
    if(bContinuousVolume)for(auto Neighbor:{Previous,Next})if(Neighbor.IsValid())
    {
        const FVector P=Neighbor->GetActorTransform().InverseTransformPosition(WorldCenter);
        if(FMath::Abs(P.X)<Neighbor->RockSize.X*.5+WorldRadius*.32)
            Cut=Neighbor->CutLocal(WorldCenter,WorldRadius,Cutter)||Cut;
    }
    return Cut;
}

bool AEndingComicRock::CutLocal(FVector WorldCenter,float WorldRadius,const FDynamicMesh3& Cutter)
{
    if(!MineCutter(WorldCenter,WorldRadius*1.15f,&Cutter))return false;
    RockMesh->GetDynamicMesh()->EditMesh([](FDynamicMesh3& Mesh){EndingComic::InkFacets(Mesh);},EDynamicMeshChangeType::AttributeEdit,
        EDynamicMeshAttributeChangeFlags::NormalsTangents|EDynamicMeshAttributeChangeFlags::VertexColors|EDynamicMeshAttributeChangeFlags::UVs);
    return true;
}
