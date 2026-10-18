#include "MiningNumberClues.h"
#include "DiggableRock.h"
#include "Showcase/MiningTreasure.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AMiningNumberClues::AMiningNumberClues()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.1f;
    Numbers=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("NumberFragments"));SetRootComponent(Numbers);
    Numbers->SetCollisionEnabled(ECollisionEnabled::NoCollision);Numbers->SetCastShadow(false);
    Tags.Add(TEXT("CollectionNumberOne"));
}

void AMiningNumberClues::Initialize(ADiggableRock* Rock,FVector Core,FVector Surface,FVector Inward)
{
    Host=Rock;SetActorLocation(Core);
    Numbers->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/CollectionPrototype/Meshes/SM_NumberFragment")));
    auto* Material=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionGlow")),this);
    Material->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.27f,.34f,.18f));Numbers->SetMaterial(0,Material);
    FRandomStream Random(41027);FVector Right,Up;Inward.FindBestAxisVectors(Right,Up);
    TArray<FVector> Accepted;
    auto Add=[&](FVector Position,FVector Support,float Spacing)
    {
        for(const FVector& P:Accepted)if(FVector::DistSquared(P,Support)<Spacing*Spacing)return false;
        Accepted.Add(Support);Supports.Add(Support-Core);
        // SM_CrystalClue spans 96 cm on its longest axis, scaled .11-.17.
        // Numerals use twice that maximum dimension.
        const float Scale=Random.FRandRange(.22f,.34f);
        const FQuat Facing=FRotationMatrix::MakeFromX(Inward).ToQuat();
        const FQuat Tilt=FRotator(Random.FRandRange(-28,28),Random.FRandRange(-30,30),Random.FRandRange(-80,80)).Quaternion();
        Numbers->AddInstance(FTransform(Facing*Tilt,Position-Core,FVector(Scale)));
        return true;
    };
    // The same compact 135 cm surface patch as campaign mineral inclusions.
    for(int Attempt=0;Attempt<240 && Accepted.Num()<14;++Attempt)
    {
        const float A=Random.FRandRange(0,2*PI),R=Random.FRandRange(20,135);
        const FVector Q=Surface+Right*(R*FMath::Cos(A))+Up*(R*FMath::Sin(A));FHitResult Hit;
        if(Rock->ActorLineTraceSingle(Hit,Q-Inward*110,Q+Inward*160,ECC_Visibility,FCollisionQueryParams(NAME_None,true)))
            Add(Hit.ImpactPoint+Hit.ImpactNormal*1.5,Hit.ImpactPoint-Hit.ImpactNormal*5,20);
    }
    // Batch coverage once: 850 inclusions in the gem's 225 cm core halo, with
    // radial concentration and 17 cm spacing matching RefineCampaignCrystalClues.
    TArray<FVector> Candidates,Probe;
    for(int I=0;I<12000;++I)
    {
        const FVector Q=Core+Random.VRand()*225*FMath::Pow(Random.FRand(),.6f);
        if(FVector::DotProduct(Q-Surface,Inward)<18)continue;
        Candidates.Add(Q);
        for(FVector D:{Inward,-Inward,Right,-Right,Up,-Up})Probe.Add(Q+D*18);
    }
    TArray<bool> Covered;AMiningTreasure::MarkCoveredPoints({Rock},Probe,Covered);
    int Buried=0;
    for(int I=0;I<Candidates.Num() && Buried<850;++I)
    {
        bool Solid=true;for(int J=0;J<6;++J)Solid&=Covered[I*6+J];
        if(Solid && Add(Candidates[I],Candidates[I],17))++Buried;
    }
    ObservedCuts=Rock->SuccessfulCuts;
    Rock->OnTerrainCommitted.AddUObject(this,&AMiningNumberClues::TerrainCommitted);
}

void AMiningNumberClues::TerrainCommitted(const FBox& Bounds)
{
    ObservedCuts=INDEX_NONE;Tick(0); // Remove in the same frame as committed soil.
}

void AMiningNumberClues::Tick(float Dt)
{
    Super::Tick(Dt);
    if(!Host.IsValid() || ObservedCuts==Host->SuccessfulCuts)return;
    ObservedCuts=Host->SuccessfulCuts;
    TArray<FVector> WorldSupports;for(FVector P:Supports)WorldSupports.Add(GetActorTransform().TransformPosition(P));
    TArray<bool> Covered;AMiningTreasure::MarkCoveredPoints({Host.Get()},WorldSupports,Covered);
    // Keep instance/support indices paired under either ISM removal policy.
    for(int I=Supports.Num()-1;I>=0;--I)if(!Covered[I])
    {
        Numbers->RemoveInstance(I);
        if(Numbers->SupportsRemoveSwap())Supports.RemoveAtSwap(I);else Supports.RemoveAt(I);
    }
}
