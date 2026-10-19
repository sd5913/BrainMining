#include "MiningCavernMotes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

AMiningCavernMotes::AMiningCavernMotes()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=1.f/20;
    Motes=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CavernDust"));SetRootComponent(Motes);
    Motes->SetCollisionEnabled(ECollisionEnabled::NoCollision);Motes->SetCastShadow(false);
    Motes->SetCanEverAffectNavigation(false);Motes->SetNumCustomDataFloats(3);
}

void AMiningCavernMotes::BeginPlay()
{
    Super::BeginPlay();
    Motes->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Drill/M_DrillDust.M_DrillDust")))
    {
        auto* Mat=UMaterialInstanceDynamic::Create(Base,this);
        Mat->SetVectorParameterValue(TEXT("SoilTint"),FLinearColor(.58f,.55f,.72f));
        Mat->SetVectorParameterValue(TEXT("AmbientDustFill"),FLinearColor(.20f,.17f,.28f));
        Motes->SetMaterial(0,Mat);
    }
    FRandomStream R(31417);
    for(int I=0;I<FMath::Clamp(ParticleCount,32,800);++I)
    {
        RestPositions.Add(FVector(R.FRandRange(-1,1),R.FRandRange(-1,1),R.FRandRange(-1,1))*HalfExtent);
        Sizes.Add(R.FRandRange(8,17));
        Motes->AddInstance(FTransform(FQuat::Identity,RestPositions.Last(),FVector(Sizes.Last()/100)));
        Motes->SetCustomDataValue(I,0,R.FRandRange(.38f,.68f));
        Motes->SetCustomDataValue(I,1,(I%2)*.5f);Motes->SetCustomDataValue(I,2,((I/2)%2)*.5f);
    }
    Motes->MarkRenderStateDirty();
}

void AMiningCavernMotes::Tick(float Dt)
{
    Super::Tick(Dt);
    auto* PC=UGameplayStatics::GetPlayerController(this,0);if(!PC || IsHidden())return;
    FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
    Eye=GetActorTransform().InverseTransformPosition(Eye);
    if(Eye.SizeSquared()>FMath::Square(6500.f))return;
    Age+=Dt;
    for(int I=0;I<RestPositions.Num();++I)
    {
        const float Phase=I*2.39996f,Rate=.18f+(I%7)*.025f;
        const FVector P=RestPositions[I]+FVector(FMath::Sin(Age*Rate+Phase)*65,FMath::Cos(Age*Rate*.7f+Phase)*50,FMath::Sin(Age*.13f+Phase)*90);
        const FRotator Facing=FRotationMatrix::MakeFromZ(Eye-P).Rotator();
        Motes->UpdateInstanceTransform(I,FTransform(Facing,P,FVector(Sizes[I]/100)),false,false,true);
    }
    Motes->MarkRenderStateDirty();
}
