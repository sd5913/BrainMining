#include "MiningCollection.h"
#include "DiggableRock.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/MeshNormals.h"
#include "Spatial/FastWinding.h"
#include "UDynamicMesh.h"

void AMiningCollectionField::MakeFeather(ADiggableRock* Rock,FVector Support,float Angle)
{
    using namespace UE::Geometry;
    auto* A=GetWorld()->SpawnActor<AActor>(); Spawned.Add(A); A->Tags.Add(TEXT("CollectionFeather"));
    auto* Shape=NewObject<UDynamicMeshComponent>(A); A->SetRootComponent(Shape); A->AddInstanceComponent(Shape);
    Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision); Shape->RegisterComponent();
    FDynamicMesh3 Mesh; Mesh.EnableAttributes(); Mesh.Attributes()->EnableMaterialID();
    Mesh.Attributes()->SetNumNormalLayers(1);
    auto Triangle=[&](FVector P,FVector Q,FVector R,int Mat)
    {
        const int I=Mesh.AppendVertex(P),J=Mesh.AppendVertex(Q),K=Mesh.AppendVertex(R);
        const int T=Mesh.AppendTriangle(I,J,K); Mesh.Attributes()->GetMaterialID()->SetValue(T,Mat);
        // Separate back-face vertices keep both sides correctly lit while fluttering.
        const int B=Mesh.AppendTriangle(Mesh.AppendVertex(P),Mesh.AppendVertex(R),Mesh.AppendVertex(Q));
        Mesh.Attributes()->GetMaterialID()->SetValue(B,Mat);
    };
    const FVector2D Edge[]={{0,31},{5,23},{8,15},{6,12},{10,7},{8,3},{9,-3},{6,-10},{4,-16},{0,-24}};
    for(int Side:{-1,1}) for(int i=1;i<UE_ARRAY_COUNT(Edge);++i)
    {
        const FVector P(0,Side*Edge[i-1].X,Edge[i-1].Y),Q(0,Side*Edge[i].X,Edge[i].Y);
        Triangle(FVector(-.25,0,0),P,Q,Side<0?0:1);
    }
    FMeshNormals::InitializeOverlayToPerTriangleNormals(Mesh.Attributes()->PrimaryNormals()); Shape->SetMesh(MoveTemp(Mesh));
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionTint"));
    for(int i=0;i<2;++i)
    {
        auto* M=UMaterialInstanceDynamic::Create(Base,A);
        M->SetVectorParameterValue(TEXT("Tint"),i==0?FLinearColor(.34f,.43f,.48f):FLinearColor(.60f,.65f,.63f)); Shape->SetMaterial(i,M);
    }
    auto* Quill=NewObject<UStaticMeshComponent>(A); A->AddInstanceComponent(Quill); Quill->SetupAttachment(Shape);
    Quill->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));
    Quill->SetCollisionEnabled(ECollisionEnabled::NoCollision); Quill->SetRelativeScale3D(FVector(.009,.012,.64));
    Quill->SetRelativeLocation(FVector(-.45,0,-2));
    auto* Pale=UMaterialInstanceDynamic::Create(Base,A); Pale->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.84f,.77f,.53f)); Quill->SetMaterial(0,Pale); Quill->RegisterComponent();
    A->SetActorLocation(Support-FVector(1.8,0,0)); A->SetActorRotation(FRotator(0,0,Angle));
    FCollectionClue C; C.Actor=A; C.Rock=Rock; C.Support=Support; C.bFeather=true; C.Phase=Clues.Num()*.9f; Clues.Add(C);
}

void AMiningCollectionField::TickClues(float Dt)
{
    using namespace UE::Geometry;
    TSet<ADiggableRock*> Dirty;
    for(const auto& C:Clues) if(C.Actor.IsValid()&&C.Rock.IsValid()&&C.ObservedCuts!=C.Rock->SuccessfulCuts) Dirty.Add(C.Rock.Get());
    // One acceleration structure per edited rock, not one per feather every frame.
    for(ADiggableRock* Rock:Dirty)
    {
        const auto& Mesh=Rock->RockMesh->GetDynamicMesh()->GetMeshRef();
        FDynamicMeshAABBTree3 Tree(&Mesh,true); TFastWindingTree<FDynamicMesh3> Winding(&Tree);
        for(auto& C:Clues) if(C.Actor.IsValid()&&C.Rock.Get()==Rock)
        {
            C.ObservedCuts=Rock->SuccessfulCuts;
            if(C.bFalling) continue;
            bool Supported=Mesh.TriangleCount()>0&&FMath::Abs(Winding.FastWindingNumber(Rock->GetActorTransform().InverseTransformPosition(C.Support)))>.5;
            if(C.bLanded)
            {
                FHitResult Hit; const FVector P=C.Actor->GetActorLocation();
                Supported=GetWorld()->LineTraceSingleByChannel(Hit,P,P-FVector(0,0,12),ECC_Visibility)&&Hit.Normal.Z>.4;
            }
            if(Supported) continue;
            if(!C.bFeather) { C.Actor->Destroy(); continue; }
            C.bFalling=true; C.bLanded=false; C.Speed=8; C.Age=0;
            C.Actor->Tags.Remove(TEXT("FeatherLanded")); C.Actor->Tags.AddUnique(TEXT("FeatherFalling"));
        }
    }
    for(auto& C:Clues) if(C.bFeather&&C.Actor.IsValid()&&(C.bFalling||C.bLanded))
    {
        C.Age+=Dt;
        if(C.Age>12) { C.Actor->Destroy(); continue; }
        if(!C.bFalling) continue;
        C.Speed=FMath::Min(85.f,C.Speed+Dt*65);
        const FVector P=C.Actor->GetActorLocation();
        const FVector Delta(-18*Dt,FMath::Sin(C.Age*3+C.Phase)*22*Dt,-C.Speed*Dt);
        FVector Next=P+Delta; FHitResult Hit;
        if(GetWorld()->SweepSingleByChannel(Hit,P,Next,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(3)))
        {
            Next=Hit.Location+Hit.Normal*1.5;
            if(Hit.Normal.Z>.45)
            {
                C.bFalling=false; C.bLanded=true;
                C.Actor->Tags.Remove(TEXT("FeatherFalling")); C.Actor->Tags.AddUnique(TEXT("FeatherLanded"));
                C.Actor->SetActorRotation(FRotator(90,C.Phase*41,0));
            }
            else Next+=FVector::VectorPlaneProject(Delta,Hit.Normal);
        }
        C.Actor->SetActorLocation(Next);
        if(C.bFalling) C.Actor->SetActorRotation(FRotator(65+FMath::Sin(C.Age*4+C.Phase)*20,C.Phase*41+C.Age*35,FMath::Sin(C.Age*3)*25));
    }
}
