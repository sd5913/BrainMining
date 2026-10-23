#include "MiningCollection.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"

void AMiningCollectible::BuildEmergenceParts()
{
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"));
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere"));
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MainGame/Materials/ScenePolish/M_CollectibleDetail"));
    if(!Cube || !Sphere || !Base)return;
    const int32 Count=ItemId==12?3:ItemId==13?25:ItemId==15?24:0;
    // Reuse four materials; transform existing components during animation,
    // never rebuild dynamic meshes or allocate materials every frame.
    TArray<UMaterialInstanceDynamic*> Mats;
    const FLinearColor Colors[]={FLinearColor(.28,.82,1),FLinearColor(.83,.36,1),FLinearColor(1,.72,.18),FLinearColor(.22,1,.66)};
    for(int32 I=0;I<4;++I)
    {
        auto* M=UMaterialInstanceDynamic::Create(Base,this);
        M->SetVectorParameterValue(TEXT("Tint"),ItemId==13?FLinearColor(.62,.70,.76):ItemId==12?FLinearColor(1,.69,.15):Colors[I]);
        M->SetScalarParameterValue(TEXT("Glow"),ItemId==13?.45f:ItemId==12?1.3f:.45f);M->SetScalarParameterValue(TEXT("Metallic"),.7f);M->SetScalarParameterValue(TEXT("Roughness"),.25f);Mats.Add(M);
    }
    for(int32 I=0;I<Count;++I)
    {
        USceneComponent* Part=nullptr;
        if(ItemId==15)
        {
            auto* Crystal=NewObject<UDynamicMeshComponent>(this);Part=Crystal;
            UE::Geometry::FDynamicMesh3 Mesh;Mesh.EnableAttributes();
            for(FVector3d P:{FVector3d(50,0,0),FVector3d(-50,0,0),FVector3d(0,50,0),FVector3d(0,-50,0),FVector3d(0,0,50),FVector3d(0,0,-50)})Mesh.AppendVertex(P);
            for(int X:{0,1})for(int Y:{2,3})for(int Z:{4,5})
            {int T=Mesh.AppendTriangle(X,Y,Z);if(Mesh.GetTriNormal(T).Dot(Mesh.GetTriCentroid(T))<0)Mesh.ReverseTriOrientation(T);}
            UE::Geometry::FMeshNormals::InitializeOverlayToPerTriangleNormals(Mesh.Attributes()->PrimaryNormals());
            Crystal->SetMesh(MoveTemp(Mesh));Crystal->SetMaterial(0,Mats[(I/6)%4]);Crystal->SetCollisionEnabled(ECollisionEnabled::NoCollision);Crystal->SetCastShadow(false);
        }
        else
        {
            auto* Solid=NewObject<UStaticMeshComponent>(this);Part=Solid;
            Solid->SetStaticMesh(ItemId==12?Sphere:Cube);Solid->SetMaterial(0,Mats[I%4]);Solid->SetCollisionEnabled(ECollisionEnabled::NoCollision);Solid->SetCastShadow(false);
        }
        AddInstanceComponent(Part);Part->SetupAttachment(Model);
        FVector P,Scale;FRotator R=FRotator::ZeroRotator;
        if(ItemId==12){P={-10,0,15};Scale=FVector(.028f);}
        else if(ItemId==13)
        {
            // Rows of short nails form a recognisable arrow, including barbs.
            if(I<13){P={-9,0.f,-27.f+I*4.f};Scale={.018f,.027f,.047f};}
            else {const int32 J=(I-13)/2;const float Side=(I%2?1:-1);P={-9,Side*(2+J*2.7f),24-J*2.7f};Scale={.018f,.026f,.055f};R.Roll=Side*45;}
        }
        else
        {
            const float A=(I%6)*PI/3,Radius=13+(I/6)*7;
            P={-46.f-(I/6)*.4f,Radius*FMath::Cos(A),Radius*FMath::Sin(A)};
            Scale={.012f,.065f,.12f};R.Roll=FMath::RadiansToDegrees(A)+45;
        }
        Part->SetRelativeTransform(FTransform(R,P,Scale));Part->RegisterComponent();
        EmergenceParts.Add(Part);EmergenceHomes.Add(Part->GetRelativeTransform());
    }
    AnimateEmergence(0);
}

void AMiningCollectible::AnimateEmergence(float Time)
{
    for(int32 I=0;I<EmergenceParts.Num();++I)
    {
        auto* Part=EmergenceParts[I].Get();const auto& Home=EmergenceHomes[I];
        if(ItemId==12)
        {
            const float A=Time*2.8f+I*2*PI/3;
            Part->SetRelativeLocation(FVector(FMath::Cos(A)*24,FMath::Sin(A)*37,-43+FMath::Sin(A*3)*5));
        }
        else if(ItemId==13)
        {
            const float Cycle=FMath::Fmod(Time,9.f);
            const float Blend=FMath::SmoothStep(.2f,3.f,Cycle)*(1-FMath::SmoothStep(7.f,9.f,Cycle));
            const float A=I*2.39996f,Radius=10+(I%4)*5;
            const FVector Scatter(-9,Radius*FMath::Cos(A),Radius*FMath::Sin(A));
            Part->SetRelativeLocation(FMath::Lerp(Scatter,Home.GetLocation(),Blend));
            const FQuat Random=FRotator(0,0,I*61+Time*26).Quaternion();
            Part->SetRelativeRotation(FQuat::Slerp(Random,Home.GetRotation(),Blend));
        }
        else if(ItemId==15)
        {
            const int32 Ring=I/6;
            const float Angle=(I%6)*PI/3+Time*(Ring%2?.14f:-.11f);
            const float R=13+Ring*7+2*FMath::Sin(Time*.8f+Ring);
            Part->SetRelativeLocation(FVector(Home.GetLocation().X,R*FMath::Cos(Angle),R*FMath::Sin(Angle)));
            Part->SetRelativeRotation(FRotator(0,0,FMath::RadiansToDegrees(Angle)+45+12*FMath::Sin(Time*.9f)));
            Part->SetRelativeScale3D(Home.GetScale3D()*FVector(1,1+.3f*FMath::Sin(Time+Ring),1+.2f*FMath::Cos(Time+Ring)));
        }
    }
    if(ItemId==12)
        if(auto* Filament=Cast<UMaterialInstanceDynamic>(Model->GetMaterial(4)))
            Filament->SetScalarParameterValue(TEXT("Glow"),FMath::Lerp(.15f,2.f,FMath::SmoothStep(.5f,2.f,Time)));
}
