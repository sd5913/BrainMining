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
    if(ItemId==9)
    {
        auto* Plane=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane"));
        auto* Logo=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionFlowerLogo"));
        if(Plane && Logo)for(int F=0;F<7;++F)
        {
            const float A=F*2.39996f,Size=F==0?26:18.f+(F%2)*3.f;
            auto* P=NewObject<UStaticMeshComponent>(this);AddInstanceComponent(P);P->SetupAttachment(Model);
            P->SetStaticMesh(Plane);P->SetMaterial(0,Logo);P->SetCollisionEnabled(ECollisionEnabled::NoCollision);P->SetCastShadow(false);
            P->SetRelativeLocation(FVector(FMath::Sin(A)*24-2.8f,FMath::Cos(A)*(F?55:0),F==0?145:112+(F%3)*13));
            // Plane local X runs across the flower; its local Y runs vertically.
            P->SetRelativeRotation(FRotationMatrix::MakeFromXZ(FVector(0,1,0),FVector(-1,0,0)).Rotator());
            P->SetRelativeScale3D(FVector(Size/50));P->RegisterComponent();EmergenceParts.Add(P);EmergenceHomes.Add(P->GetRelativeTransform());
        }
        return;
    }
    const int32 Count=ItemId==12?3:ItemId==13?25:ItemId==15?24:ItemId==16?6:ItemId==17?16:ItemId==18?5:ItemId==19?7:ItemId==21?108:ItemId==22?30:(ItemId==2||ItemId==8||ItemId==10)?8:0;
    if(!Count)return;
    // Reuse four materials; transform existing components during animation,
    // never rebuild dynamic meshes or allocate materials every frame.
    TArray<UMaterialInstanceDynamic*> Mats;
    const FLinearColor Colors[]={FLinearColor(.28,.82,1),FLinearColor(.83,.36,1),FLinearColor(1,.72,.18),FLinearColor(.22,1,.66)};
    for(int32 I=0;I<4;++I)
    {
        auto* M=UMaterialInstanceDynamic::Create(Base,this);
        M->SetVectorParameterValue(TEXT("Tint"),ItemId==10?FLinearColor(.3f,1,.06f):ItemId==16?FLinearColor(.62,.57,.48):ItemId==18?FLinearColor(1,.64,.11):ItemId==21?(I==0?FLinearColor(.68,.45,.3):Colors[I]):ItemId==22?(I==0?FLinearColor(.82,.91,.95):Colors[I]):ItemId==13?FLinearColor(.62,.70,.76):ItemId==12?FLinearColor(1,.69,.15):Colors[I]);
        M->SetScalarParameterValue(TEXT("Glow"),ItemId==10?2.5f:ItemId==13?.45f:ItemId==12?1.3f:.45f);M->SetScalarParameterValue(TEXT("Metallic"),.7f);M->SetScalarParameterValue(TEXT("Roughness"),.25f);Mats.Add(M);
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
            Solid->SetStaticMesh((ItemId==12||ItemId==2||ItemId==8||ItemId==10||ItemId==16||ItemId==17||ItemId==18||ItemId==19||(ItemId==21 && I%3==0))?Sphere:Cube);Solid->SetMaterial(0,Mats[ItemId==21?(I%3==0?0:1+(I/3)%3):ItemId==22?(I%6==0?1+(I/6)%3:0):I%4]);Solid->SetCollisionEnabled(ECollisionEnabled::NoCollision);Solid->SetCastShadow(false);
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
        else if(ItemId==17)
        {
            P={-5.f,0.f,float(-10+(I%4)*9)};Scale=I<8?(I%2?FVector(.025,.045,.08):FVector(.045,.13,.06)):FVector(.017);
        }
        else if(ItemId==21)
        {
            // Thirty-six low-poly passengers: head, coat and two joined legs.
            P={-5,0,float(I%3==0?-4:I%3==1?-13:-24)};
            Scale=I%3==0?FVector(.06):I%3==1?FVector(.06,.08,.12):FVector(.045,.06,.10);
        }
        else if(ItemId==22)
        {
            const int J=I%6;P={-9.f,0.f,0.f};
            if(J==0){P.X=-8.4;Scale={.008,.53,.115};}
            else if(J==1){P.Y=-21;Scale={.012,.055,.055};}
            else{P.Y=-4+(J==5?7:0);P.Z=7-J*2.6;Scale={.01,J==5?.15:.30,.012};}
        }
        else if(ItemId==18){P={0,0,40};Scale={.075,.075,.017};}
        else if(ItemId==16){P={-8.f,float(I*8-20),20};Scale={.018,.025,.055};}
        else if(ItemId==19){P={0,0,22};Scale=FVector(.018);}
        else if(ItemId==2||ItemId==8||ItemId==10){P={0,0,0};Scale=FVector(.018);}
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
    // Visual-only motion keeps the interaction body and saved world transform stable.
    const bool Gentle=!IsMineralReward(ItemId)||ItemId==14;
    if(Gentle)
    {
        Model->SetRelativeLocation(FVector(0,0,FMath::Sin(Time*1.35f+ItemId)*1.4f));
        Model->SetRelativeRotation(FRotator(FMath::Sin(Time*.7f)*1.2f,0,FMath::Sin(Time*.9f+ItemId)*1.6f));
        Outline->SetRelativeLocationAndRotation(Model->GetRelativeLocation(),Model->GetRelativeRotation());
    }
    if(ItemId==3)
    {
        const bool Flying=!ActorHasTag(TEXT("CollectionPreview"));
        const float Beat=Time*2*PI*2.6f;
        if(Flying)
        {
            Model->SetRelativeLocation(FVector(0,0,3.5f*FMath::Sin(Beat-.6f)+1.5f*FMath::Sin(Time*1.4f)));
            Model->SetRelativeRotation(FRotator(3+2*FMath::Sin(Beat-.4f),0,1.5f*FMath::Sin(Time*1.7f)));
            Outline->SetRelativeLocationAndRotation(Model->GetRelativeLocation(),Model->GetRelativeRotation());
        }
        for(int I=0;I<EmergenceParts.Num();++I)
        {
            // Both shoulders stay attached; mirrored rolls lift and lower the feathers together.
            const float Side=I==0?-1.f:1.f;
            EmergenceParts[I]->SetRelativeRotation(FRotator(0,0,Side*(Flying?12.f+48.f*FMath::Sin(Beat):-72.f)));
        }
        return;
    }
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
        else if(ItemId==17)
        {
            if(I<8)
            {
                const int Fish=I/2;const float A=Time*(.55f+Fish*.08f)+Fish*1.8f;
                const FVector P(-7+Fish*7,25*FMath::Sin(A),-9+Fish*8+4*FMath::Sin(A*1.4f));
                Part->SetRelativeLocation(P+FVector(0,I%2?-9.f*(FMath::Cos(A)>0?1:-1):0,0));
                Part->SetRelativeRotation(FRotator(0,I%2?FMath::Sin(Time*9+Fish)*22:0,0));
            }
            else {const float T=FMath::Fmod(Time*.21f+I*.137f,1.f);Part->SetRelativeLocation(FVector(15+3*FMath::Sin(Time+I),-26+I%3*9,-28+T*60));Part->SetRelativeScale3D(Home.GetScale3D()*FMath::Sin(T*PI));}
        }
        else if(ItemId==21)
        {
            const int Person=I/3;const float T=FMath::Fmod(Time*.16f+Person/36.f,1.f);
            const float Exit=FMath::SmoothStep(.08f,.8f,T);
            const FVector P(-4-Exit*80,((Person%5)-2)*4+Exit*((Person%7)-3)*12,Home.GetLocation().Z+FMath::Sin(Time*11+Person)*.7f);
            Part->SetRelativeLocation(P);Part->SetRelativeScale3D(Home.GetScale3D()*FMath::Min(FMath::SmoothStep(0.f,.08f,T),1-FMath::SmoothStep(.83f,1.f,T)));
        }
        else if(ItemId==22)
        {
            const float T=FMath::Fmod(Time*.10f+(I/6)*.2f,1.f);
            Part->SetRelativeLocation(Home.GetLocation()+FVector(0,0,-29+T*67));
            Part->SetRelativeScale3D(Home.GetScale3D()*FMath::Min(FMath::SmoothStep(0.f,.08f,T),1-FMath::SmoothStep(.9f,1.f,T)));
        }
        else if(ItemId==18)
        {
            const float T=FMath::Fmod(Time*.3f+I*.2f,1.f),A=I*2.4f;
            Part->SetRelativeLocation(FVector(18*FMath::Cos(A),18*FMath::Sin(A),40+25*FMath::Sin(T*PI)));
            Part->SetRelativeRotation(FRotator(T*360,0,0));
        }
        else if(ItemId==16||ItemId==19||ItemId==2||ItemId==8)
        {
            const float T=FMath::Fmod(Time*.25f+I*.163f,1.f),A=Time+I*2.4f;
            const float Z=ItemId==16?20:ItemId==19?23:ItemId==8?48:0;
            Part->SetRelativeLocation(FVector(FMath::Sin(A)*(4+T*8),FMath::Cos(A)*(5+T*12),Z+T*(ItemId==16?42:20)));
            Part->SetRelativeScale3D(Home.GetScale3D()*FMath::Sin(T*PI)*(1+T));
        }
        else if(ItemId==10)
            Part->SetRelativeLocation(FVector(18*FMath::Sin(Time*.8f+I),18*FMath::Cos(Time*.7f+I*2),28*FMath::Sin(Time*.5f+I*3)));
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
    if(ItemId==19)
        if(auto* Liquid=Cast<UMaterialInstanceDynamic>(Model->GetMaterial(10)))Liquid->SetScalarParameterValue(TEXT("AnimationTime"),Time);
    if(ItemId==12)
        if(auto* Filament=Cast<UMaterialInstanceDynamic>(Model->GetMaterial(4)))
            Filament->SetScalarParameterValue(TEXT("Glow"),FMath::Lerp(.15f,2.f,FMath::SmoothStep(.5f,2.f,Time)));
}
