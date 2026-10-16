#include "EndingEmergence.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "Materials/MaterialInterface.h"

void AEndingEmergence::BuildBrainSurface()
{
    using namespace UE::Geometry;
    FDynamicMesh3 Mesh(true,true,false,false);
    constexpr int Rows=256,Columns=384;
    auto Vertex=[&](float Latitude,float Longitude)
    {
        const FVector D(FMath::Sin(Latitude)*FMath::Cos(Longitude),FMath::Sin(Latitude)*FMath::Sin(Longitude),FMath::Cos(Latitude));
        const FVector P=D*10500;
        // Broad winding gyri, interrupted by smaller sulci; the midline is a
        // recessed longitudinal fissure rather than another drawn stripe.
        const float Flow=P.Y*.0065f+2.3f*FMath::Sin(P.X*.0014f)+.6f*FMath::Sin(P.X*.003f+P.Z*.001f);
        const float Ridge=.5f+.5f*FMath::Cos(Flow);
        const float Cross=.87f+.13f*FMath::Sin(P.X*.003f+FMath::Sin(P.Y*.002f)*1.5f);
        const float Fissure=FMath::Exp(-FMath::Square(P.Y/170.f));
        const float Relief=-190+240*Ridge*Cross-300*Fissure;
        const int Id=Mesh.AppendVertex(FVector3d(D*(10500+Relief)));
        Mesh.SetVertexColor(Id,FVector3f(FMath::Clamp((Relief+380)/430,0.f,1.f),.5f,1.f));
        return Id;
    };
    const int North=Vertex(0,0);
    for(int Row=1;Row<Rows;++Row)for(int Col=0;Col<Columns;++Col)Vertex(PI*Row/Rows,2*PI*Col/Columns);
    const int South=Vertex(PI,0);
    for(int Col=0;Col<Columns;++Col)
    {
        const int Next=(Col+1)%Columns;
        Mesh.AppendTriangle(North,1+Col,1+Next);
        for(int Row=0;Row<Rows-2;++Row)
        {
            const int A=1+Row*Columns+Col,B=1+Row*Columns+Next;
            Mesh.AppendTriangle(A,A+Columns,B);Mesh.AppendTriangle(B,A+Columns,B+Columns);
        }
        Mesh.AppendTriangle(1+(Rows-2)*Columns+Next,1+(Rows-2)*Columns+Col,South);
    }
    FMeshNormals::QuickComputeVertexNormals(Mesh);Mesh.EnableAttributes();
    FMeshNormals::InitializeOverlayToPerVertexNormals(Mesh.Attributes()->PrimaryNormals());
    Mesh.Attributes()->EnablePrimaryColors();
    auto* Colors=Mesh.Attributes()->PrimaryColors();
    for(int V:Mesh.VertexIndicesItr())Colors->AppendElement(FVector4f(Mesh.GetVertexColor(V),1));
    for(int T:Mesh.TriangleIndicesItr())Colors->SetTriangle(T,Mesh.GetTriangle(T));
    auto* Surface=NewObject<UDynamicMeshComponent>(this,TEXT("SculptedCerebralCortex"),RF_Transactional);
    AddInstanceComponent(Surface);Surface->ComponentTags.Add(TEXT("EmergenceEndingGenerated"));
    Surface->SetupAttachment(Sets[0]);Surface->SetRelativeLocation(FVector(19700,0,-8146.21125));
    Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);Surface->SetCastShadow(false);Surface->SetEnableRaytracing(false);
    Surface->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndingEmergence/Materials/M_EE_Cortex")));
    Surface->SetMesh(MoveTemp(Mesh));Surface->RegisterComponent();
}
