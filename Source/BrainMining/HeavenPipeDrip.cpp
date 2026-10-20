#include "HeavenPipeDrip.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

AHeavenPipeDrip::AHeavenPipeDrip()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=1.f/30;
    Droplets=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FallingDrops"));SetRootComponent(Droplets);
    Splashes=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ImpactSplashes"));Splashes->SetupAttachment(Droplets);
    for(auto* C:{Droplets.Get(),Splashes.Get()})
    {
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(false);C->SetCanEverAffectNavigation(false);
    }
}

void AHeavenPipeDrip::BeginPlay()
{
    Super::BeginPlay();
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MainGame/Materials/ScenePolish/M_CollectibleDetail.M_CollectibleDetail"));
    auto* Material=UMaterialInstanceDynamic::Create(Base,this);
    Material->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.30f,.66f,.82f));
    Material->SetScalarParameterValue(TEXT("Roughness"),.12f);Material->SetScalarParameterValue(TEXT("Glow"),.65f);
    for(auto* C:{Droplets.Get(),Splashes.Get()}){C->SetStaticMesh(Mesh);C->SetMaterial(0,Material);}
    for(int I=0;I<22;++I)Droplets->AddInstance(FTransform(FVector::ZeroVector));
    for(int I=0;I<18;++I)Splashes->AddInstance(FTransform(FVector::ZeroVector));
    Tick(0);
}

void AHeavenPipeDrip::Tick(float Dt)
{
    Super::Tick(Dt);if(IsHidden()||Droplets->GetInstanceCount()==0)return;
    Age+=Dt;
    const float Depth=FMath::Max(10.f,FallDistance),Flight=FMath::Sqrt(2*Depth/980.f);
    for(int I=0;I<Droplets->GetInstanceCount();++I)
    {
        const float T=FMath::Fmod(Age+I*Flight/22,Flight),Phase=I*2.39996f;
        const FVector P(FMath::Sin(Phase)*(1+T*3),FMath::Cos(Phase)*(1+T*3),-490*T*T);
        Droplets->UpdateInstanceTransform(I,FTransform(FQuat::Identity,P,FVector(.020f,.020f,.045f+T*.025f)),false,false,true);
    }
    for(int I=0;I<Splashes->GetInstanceCount();++I)
    {
        const float T=FMath::Fmod(Age+I*.019f,.34f),Phase=I*2.39996f;
        const FVector P(FMath::Sin(Phase)*T*50,FMath::Cos(Phase)*T*50,-Depth+1+60*T-176*T*T);
        Splashes->UpdateInstanceTransform(I,FTransform(FQuat::Identity,P,FVector(.018f*(1-T/.34f))),false,false,true);
    }
    Droplets->MarkRenderStateDirty();Splashes->MarkRenderStateDirty();
}
