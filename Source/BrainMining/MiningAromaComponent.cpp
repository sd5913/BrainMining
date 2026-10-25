#include "MiningAromaComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "UDynamicMesh.h"

UMiningAromaComponent::UMiningAromaComponent()
{PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickInterval=1.f/30.f;}
void UMiningAromaComponent::BeginPlay()
{
    Super::BeginPlay();
    Plume=NewObject<UDynamicMeshComponent>(GetOwner());GetOwner()->AddInstanceComponent(Plume);
    Plume->SetupAttachment(GetOwner()->GetRootComponent());Plume->SetCollisionEnabled(ECollisionEnabled::NoCollision);Plume->SetCastShadow(false);
    Plume->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_BranchAroma")));
    UE::Geometry::FDynamicMesh3 Mesh;Mesh.EnableAttributes();auto* UV=Mesh.Attributes()->PrimaryUV();
    for(int I=0;I<64;++I)
    {
        for(int K=0;K<4;++K){Mesh.AppendVertex(FVector3d(K%2,K/2,I*.01));UV->AppendElement(FVector2f(K%2,K/2));}
        for(UE::Geometry::FIndex3i V:{UE::Geometry::FIndex3i(I*4,I*4+2,I*4+1),UE::Geometry::FIndex3i(I*4+1,I*4+2,I*4+3)})
        {int T=Mesh.AppendTriangle(V);UV->SetTriangle(T,V);}
    }
    UE::Geometry::FMeshNormals::InitializeOverlayToPerTriangleNormals(Mesh.Attributes()->PrimaryNormals());
    Plume->SetMesh(MoveTemp(Mesh));Plume->RegisterComponent();
}
void UMiningAromaComponent::TickComponent(float Dt,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(Dt,TickType,TickFunction);Age+=Dt;
    auto* PC=GetWorld()->GetFirstPlayerController();if(!PC||!Plume)return;
    FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
    const FTransform Frame=GetOwner()->GetActorTransform();if(FVector::DistSquared(Eye,Frame.GetLocation())>FMath::Square(2500.f))return;
    const FVector Source=Frame.TransformPosition(SourceLocal),Mouth=Frame.GetLocation();
    const FVector Out=-Frame.GetUnitAxis(EAxis::X),Side=Frame.GetUnitAxis(EAxis::Y);
    const FVector Right=FRotationMatrix(View).GetUnitAxis(EAxis::Y),Up=FRotationMatrix(View).GetUnitAxis(EAxis::Z);
    Plume->GetDynamicMesh()->EditMesh([&](UE::Geometry::FDynamicMesh3& Mesh)
    {
        for(int I=0;I<64;++I)
        {
            const float T=FMath::Frac(Age/4.8f+I/64.f),Phase=I*2.39996f;
            // Exit perpendicular to the bank before rising in WORLD up, even on sloped walls.
            const float Exit=FMath::Clamp((T-.30f)/.7f,0.f,1.f);
            FVector Center=T<.30f?FMath::Lerp(Source,Mouth,T/.30f):Mouth+Out*(Exit*150)+FVector::UpVector*(Exit*Exit*155);
            Center+=Side*(FMath::Sin(Phase+Age*.8f)*(3+Exit*15))+Out*(FMath::Cos(Phase)*Exit*6);
            const float Size=(9+Exit*23)*FMath::SmoothStep(0.f,.06f,T)*(1-FMath::SmoothStep(.72f,1.f,T));
            const float A=Phase+Age*.12f;const FVector R=Right*FMath::Cos(A)+Up*FMath::Sin(A),U=Up*FMath::Cos(A)-Right*FMath::Sin(A);
            for(int K=0;K<4;++K)Mesh.SetVertex(I*4+K,FVector3d(Frame.InverseTransformPosition(Center+(R*(K%2?1:-1)+U*(K/2?1:-1))*Size)));
        }
    },EDynamicMeshChangeType::MeshVertexChange,EDynamicMeshAttributeChangeFlags::VertexPositions,false);
}
