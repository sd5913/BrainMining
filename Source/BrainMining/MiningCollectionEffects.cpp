#include "MiningCollectionEffects.h"
#include "MiningCollection.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/PointLightComponent.h"

AMiningCollectionEffects::AMiningCollectionEffects()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.TickInterval=1.f/30.f;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("EffectsRoot"));
}

void AMiningCollectionEffects::Initialize(int32 EffectKind, AMiningCollectible* Item, int32 ConfettiPerColor)
{
    Kind=EffectKind; SourceItem=Item;
    const FLinearColor Colors[]={FLinearColor(1,.35,.04),FLinearColor(.1,1,.25),FLinearColor(.95,.05,.22),FLinearColor(.12,.55,1),FLinearColor(.9,.7,.08)};
    const bool Spark=Kind==2 || Kind==4;
    for(int32 C=0;C<(Kind==3?5:Spark?2:1);++C)
    {
        auto* Pool=NewObject<UInstancedStaticMeshComponent>(this); AddInstanceComponent(Pool); Pool->SetupAttachment(RootComponent);
        Pool->SetMobility(EComponentMobility::Movable); Pool->SetCollisionEnabled(ECollisionEnabled::NoCollision); Pool->SetCastShadow(false);
        Pool->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Kind==3?TEXT("/Engine/BasicShapes/Cube"):Spark&&C==1?TEXT("/Engine/BasicShapes/Plane"):TEXT("/Engine/BasicShapes/Sphere")));
        auto* Base=LoadObject<UMaterialInterface>(nullptr,Spark?(C==0?TEXT("/Game/CollectionPrototype/Materials/M_CollectionSparkCore"):TEXT("/Game/CollectionPrototype/Materials/M_CollectionSoftGlow")):TEXT("/Game/CollectionPrototype/Materials/M_CollectionTint"));
        if(Base)
        {
            auto* Material=UMaterialInstanceDynamic::Create(Base,this);
            Material->SetVectorParameterValue(TEXT("Tint"),Spark?FLinearColor(.25,1,.07):Kind==3?Colors[C]:FLinearColor(.38,.12,.025));
            Pool->SetMaterial(0,Material);
        }
        Pool->RegisterComponent(); Pools.Add(Pool);
        for(int I=0;I<(Kind==3?FMath::Clamp(ConfettiPerColor,1,256):Kind==2?96:Kind==4?42:90);++I) Pool->AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector));
    }
    Tags.Add(Kind==3?TEXT("CollectionCelebration"):Kind==4?TEXT("CollectionJarSwarm"):Kind==2?TEXT("CollectionGreenGlow"):TEXT("CollectionColaBubbles"));
    if(Kind==4)
    {
        auto* Light=NewObject<UPointLightComponent>(this);AddInstanceComponent(Light);Light->SetupAttachment(RootComponent);Light->RegisterComponent();
        Light->SetIntensityUnits(ELightUnits::Candelas);Light->SetIntensity(35);Light->SetLightColor(FLinearColor(.3,1,.08));Light->SetAttenuationRadius(170);Light->SetCastShadows(false);
    }
}

void AMiningCollectionEffects::Tick(float Dt)
{
    Super::Tick(Dt);
    if(SourceItem.IsValid() && SourceItem->bCollected) { SetActorHiddenInGame(true); SetActorTickEnabled(false); return; }
    if(Kind==3)
    {
        if(auto* Pawn=UGameplayStatics::GetPlayerPawn(this,0))
        {
            const FVector P=GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation());
            if(FMath::Abs(P.X)<570 && P.Y>-200 && P.Y<1350 && FMath::Abs(P.Z)<800)
            {
                bCurtainOpen=true;
                if(!bSeparateStageTriggers || P.Y>CelebrationForward)bStageActive=true;
            }
        }
        bCurtainOpen|=bStageActive;
        CurtainOpening=FMath::FInterpTo(CurtainOpening,bCurtainOpen?1.f:0.f,Dt,2.5f);
        if(CurtainClosedLocations.Num()!=Curtains.Num())
        {CurtainClosedLocations.Reset();for(const auto& Curtain:Curtains)CurtainClosedLocations.Add(IsValid(Curtain)?Curtain->GetActorLocation():FVector::ZeroVector);}
        for(int I=0;I<Curtains.Num();++I) if(IsValid(Curtains[I]))
            Curtains[I]->SetActorLocation(CurtainClosedLocations[I]+GetActorTransform().TransformVector(FVector((I==0?-1:1)*CurtainOpening*CurtainSlideDistance,0,0)));
        if(!bStageActive) return;
    }
    Age+=Dt;
    FVector Eye=GetActorLocation()+FVector(-300,0,0); FRotator EyeRotation;
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->GetPlayerViewPoint(Eye,EyeRotation);
    for(int C=0;C<Pools.Num();++C)
    {
        auto* Pool=Pools[C].Get();
        for(int I=0;I<Pool->GetInstanceCount();++I)
        {
            const float Seed=I*1.713f+(Kind==3?C*2.47f:0);
            FVector P,Scale; FRotator Rotation=FRotator::ZeroRotator;
            if(Kind==3)
            {
                // Stagger launches over the entire flight cycle: a continuous shower.
                const float Delay=4.3f*(I+float(C)/Pools.Num())/Pool->GetInstanceCount();
                if(bSeparateStageTriggers && Age<Delay){Pool->UpdateInstanceTransform(I,FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector),false,false,true);continue;}
                const float T=FMath::Fmod(bSeparateStageTriggers?FMath::Max(0.f,Age-Delay):Age+I*.067f+C*.031f,4.3f),Side=I%2?1.f:-1.f;
                const float Speed=150+45*FMath::Sin(Seed);
                P=FVector(Side*(480-Speed*T)+18*FMath::Sin(T*4+Seed),650+T*(35+22*FMath::Sin(Seed)),FMath::Max(52.f,145+410*T-105*T*T));
                Rotation=FRotator(T*170+Seed*50,T*110+Seed*90,T*210);
                Scale=FVector(.035,.16+(I%4)*.03,.009);
            }
            else if(Kind==2 || Kind==4)
            {
                const float Pulse=.25f+.75f*FMath::Pow(.5f+.5f*FMath::Sin(Age*(2+I%4*.4f)+Seed),3);
                if(Kind==4) P=FVector(FMath::Sin(Age*.9f+Seed)*19,FMath::Cos(Age*.7f+Seed)*19,FMath::Sin(Age*.8f+Seed*2)*29);
                else
                {
                    FRandomStream R(10427+I*7919);
                    const float T=R.FRandRange(-.12f,1.04f),Phase=R.FRandRange(0,2*PI),Speed=R.FRandRange(.35f,1.15f);
                    FVector Side,Up;TrailEnd.GetSafeNormal().FindBestAxisVectors(Side,Up);
                    const float Spread=R.FRandRange(18,65),Offset=R.FRandRange(-30,30);
                    P=TrailEnd*T+Side*(FMath::Sin(T*8.7f+1.3f)*37+FMath::Cos(Phase+Age*Speed)*Spread)
                        +Up*(FMath::Sin(T*5.1f-.7f)*28+FMath::Sin(Phase*1.7f+Age*Speed*.71f)*Spread+Offset)
                        +TrailEnd.GetSafeNormal()*FMath::Sin(Age*Speed*.6f+Phase)*R.FRandRange(3,18);
                }
                Scale=FVector((C==0?.012f:Kind==4?.11f:.22f)*Pulse);
                if(C==1) Rotation=FRotationMatrix::MakeFromZ(GetActorTransform().InverseTransformPosition(Eye)-P).Rotator();
            }
            else
            {
                const float T=FMath::Fmod(Age*95/FMath::Max(100.f,FlowHeight)+float(I)/Pool->GetInstanceCount(),1.f);
                P=FVector(FMath::Sin(Seed+Age)*12,FMath::Cos(Seed+Age*.7f)*12,T*FlowHeight);
                Scale=FVector(.018f+.036f*T);
            }
            Pool->UpdateInstanceTransform(I,FTransform(Rotation,P,Scale),false,false,true);
        }
        Pool->MarkRenderStateDirty();
    }
}
