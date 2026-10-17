#include "HeavenToiletWater.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "DiggableRock.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/MeshNormals.h"
#include "Spatial/FastWinding.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "UDynamicMesh.h"

using namespace UE::Geometry;
namespace
{
    struct FWaterMesh
    {
        FDynamicMesh3 Mesh;
        FWaterMesh() { Mesh.EnableAttributes(); }
        void Quad(FVector A,FVector B,FVector C,FVector D)
        {
            const int V0=Mesh.AppendVertex(A),V1=Mesh.AppendVertex(B),V2=Mesh.AppendVertex(C),V3=Mesh.AppendVertex(D);
            Mesh.AppendTriangle(V0,V2,V1); Mesh.AppendTriangle(V0,V3,V2);
        }
        void Disk(FVector Center,float RadiusX,float RadiusY,int Rings=32,int Sides=128)
        {
            // Shared rings keep the tessellated animated surface seamless.
            const int CenterId=Mesh.AppendVertex(Center);
            TArray<int> V;
            for(int r=1;r<=Rings;++r) for(int i=0;i<Sides;++i)
            {
                const float A=2*PI*i/Sides,F=float(r)/Rings;
                V.Add(Mesh.AppendVertex(Center+FVector(F*RadiusX*FMath::Cos(A),F*RadiusY*FMath::Sin(A),0)));
            }
            for(int i=0;i<Sides;++i)
            {
                const int N=(i+1)%Sides;
                Mesh.AppendTriangle(CenterId,V[N],V[i]);
                for(int r=0;r<Rings-1;++r)
                {
                    const int A=r*Sides+i,B=r*Sides+N,C=(r+1)*Sides+i,D=(r+1)*Sides+N;
                    Mesh.AppendTriangle(V[A],V[B],V[D]); Mesh.AppendTriangle(V[A],V[D],V[C]);
                }
            }
        }
        void Apply(UDynamicMeshComponent* Component)
        {
            FMeshNormals::InitializeOverlayToPerTriangleNormals(Mesh.Attributes()->PrimaryNormals());
            Component->SetMesh(MoveTemp(Mesh));
        }
    };
}

AHeavenToiletWater::AHeavenToiletWater()
{
    PrimaryActorTick.bCanEverTick=true;
    Basin=CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("ShallowWaterSurface"));
    SetRootComponent(Basin);
    Spill=CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("ExcavatedOutletWater"));
    Spill->SetupAttachment(Basin);
    OutletSpray=CreateDefaultSubobject<UNiagaraComponent>(TEXT("RealOutletSpray"));
    OutletSpray->SetupAttachment(Basin);OutletSpray->SetAutoActivate(false);
    OutletSpray->SetAbsolute(false,false,true);
    for(auto* C:{Basin.Get(),Spill.Get()})
    {
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetGenerateOverlapEvents(false); C->SetCastShadow(false);
        C->SetEnableRaytracing(false); C->SetCanEverAffectNavigation(false);
    }
}

void AHeavenToiletWater::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform); RebuildWater();
}

void AHeavenToiletWater::RebuildWater()
{
    FWaterMesh Surface;
    Surface.Disk(FVector(0,0,WaterHeight),FMath::Max(100.f,Radius-2),FMath::Max(100.f,Radius-2));
    Surface.Apply(Basin);
    Basin->SetMaterial(0,WaterMaterial);
    Spill->SetMaterial(0,SpillMaterial);
    FWaterMesh Empty; Empty.Apply(Spill);
    Spill->SetVisibility(false);
    bSpilling=false; ConnectedCells=0; BreachCount=0; SpillStrength=0; SpillAge=0; ObservedCuts.Reset();
    OutletSpray->SetAsset(OutletSpraySystem);OutletSpray->DeactivateImmediate();
}

void AHeavenToiletWater::BeginPlay()
{
    Super::BeginPlay(); RebuildWater();
    if(WaterMaterial) { BasinMID=UMaterialInstanceDynamic::Create(WaterMaterial,this); Basin->SetMaterial(0,BasinMID); }
    if(SpillMaterial) { SpillMID=UMaterialInstanceDynamic::Create(SpillMaterial,this); Spill->SetMaterial(0,SpillMID); }
    RefreshBreach();
}

void AHeavenToiletWater::RefreshBreach()
{
    // Query only when terrain changes, using its real post-boolean mesh. A high
    // tunnel, blind pocket, or mere cut count must never trigger leakage.
    ObservedCuts.Reset();
    const float Step=20, Y0=GetActorLocation().Y-CorridorHalfWidth;
    const int NX=FMath::CeilToInt((InnerX-OuterX)/Step)+1,NY=FMath::CeilToInt(2*CorridorHalfWidth/Step)+1;
    if(NX<2 || NY<2 || NX*NY>20000) return;
    TArray<uint8> Air; Air.Init(1,NX*NY);
    auto SurfaceZ=[&](float X) { return GetActorLocation().Z+WaterHeight; };
    // Test at the basin level; the short exterior sheet falls to the dry floor.
    auto Point=[&](int X,int Y) { const float WX=OuterX+X*Step; return FVector(WX,Y0+Y*Step,SurfaceZ(WX)); };
    for(ADiggableRock* Rock:RetainingSoil)
    {
        ObservedCuts.Add(IsValid(Rock)?Rock->SuccessfulCuts:INDEX_NONE);
        if(!IsValid(Rock)) continue;
        const auto& Mesh=Rock->RockMesh->GetDynamicMesh()->GetMeshRef();
        if(Mesh.TriangleCount()==0) continue;
        FDynamicMeshAABBTree3 Tree(&Mesh,true);
        TFastWindingTree<FDynamicMesh3> Winding(&Tree);
        const FTransform Frame=Rock->GetActorTransform();
        const FBox Bounds=Rock->GetComponentsBoundingBox();
        for(int X=0;X<NX;++X) for(int Y=0;Y<NY;++Y)
        {
            const int I=X*NY+Y;
            if(!Air[I]) continue;
            const FVector P=Point(X,Y);
            for(FVector Offset:{FVector(0,0,-3),FVector(8,0,-3),FVector(-8,0,-3),FVector(0,8,-3),FVector(0,-8,-3)})
            {
                if(Bounds.IsInsideOrOn(P+Offset) && FMath::Abs(Winding.FastWindingNumber(Frame.InverseTransformPosition(P+Offset)))>.5)
                { Air[I]=0; break; }
            }
        }
    }
    TArray<uint8> Wet; Wet.Init(0,NX*NY);
    TArray<int> Queue;
    for(int Y=0;Y<NY;++Y) if(Air[(NX-1)*NY+Y]) { const int I=(NX-1)*NY+Y; Wet[I]=1; Queue.Add(I); }
    int Exit=INDEX_NONE;
    for(int Head=0;Head<Queue.Num();++Head)
    {
        const int I=Queue[Head],X=I/NY,Y=I%NY;
        if(X==0 && (Exit==INDEX_NONE || FMath::Abs(Y-NY/2)<FMath::Abs(Exit%NY-NY/2))) Exit=I;
        for(FIntPoint D:{FIntPoint(-1,0),FIntPoint(1,0),FIntPoint(0,-1),FIntPoint(0,1)})
        {
            const int A=X+D.X,B=Y+D.Y,N=A*NY+B;
            if(A<0||A>=NX||B<0||B>=NY||Wet[N]||!Air[N]) continue;
            FHitResult Hit;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(HeavenWaterConnectivity),true,this);
            if(GetWorld()->LineTraceSingleByChannel(Hit,Point(X,Y)-FVector(0,0,3),Point(A,B)-FVector(0,0,3),ECC_Visibility,Params)) continue;
            Wet[N]=1; Queue.Add(N);
        }
    }
    ConnectedCells=Queue.Num();
    const bool Open=Exit!=INDEX_NONE && !RetainingSoil.IsEmpty();
    if(Open && !bSpilling) { ++BreachCount; SpillAge=0; }
    bSpilling=Open; Spill->SetVisibility(Open);
    if(!Open) { SpillStrength=0; OutletSpray->Deactivate(); return; }
    FWaterMesh Flow;
    const FTransform Frame=GetActorTransform();
    auto Local=[&](FVector P) { return Frame.InverseTransformPosition(P); };
    for(int X=0;X<NX-1;++X) for(int Y=0;Y<NY-1;++Y)
    {
        if(OuterX+(X+1)*Step>MouthX) continue; // no coplanar overlap with basin
        const int I=X*NY+Y;
        if(Wet[I] && Wet[I+1] && Wet[I+NY] && Wet[I+NY+1])
            Flow.Quad(Local(Point(X,Y)),Local(Point(X+1,Y)),Local(Point(X+1,Y+1)),Local(Point(X,Y+1)));
    }
    const float ExitY=Point(0,Exit%NY).Y;
    OutletSpray->SetWorldLocation(FVector(OuterX,ExitY,GetActorLocation().Z+WaterHeight+1));
    if(OutletSpraySystem && !OutletSpray->IsActive()) OutletSpray->Activate(true);
    // Short descending sheet and a small persistent puddle, not a draining simulation.
    for(int i=0;i<12;++i)
    {
        const float X0=OuterX-i*15,X1=X0-15;
        const float W0=22+i*3,W1=W0+3;
        const float Z0=GetActorLocation().Z+FMath::Lerp(WaterHeight,1.5f,float(i)/12),Z1=GetActorLocation().Z+FMath::Lerp(WaterHeight,1.5f,float(i+1)/12);
        Flow.Quad(Local(FVector(X0,ExitY-W0,Z0)),Local(FVector(X1,ExitY-W1,Z1)),Local(FVector(X1,ExitY+W1,Z1)),Local(FVector(X0,ExitY+W0,Z0)));
    }
    Flow.Disk(Local(FVector(OuterX-170,ExitY,GetActorLocation().Z+1.5)),110,85,10,64);
    Flow.Apply(Spill);
}

void AHeavenToiletWater::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    CheckTime+=DeltaSeconds;
    if(CheckTime>=.25f)
    {
        CheckTime=0;
        bool Dirty=ObservedCuts.Num()!=RetainingSoil.Num();
        for(int i=0;i<RetainingSoil.Num()&&!Dirty;++i)
            Dirty=ObservedCuts[i]!=(IsValid(RetainingSoil[i])?RetainingSoil[i]->SuccessfulCuts:INDEX_NONE);
        if(Dirty) RefreshBreach();
    }
    if(bSpilling)
    {
        SpillAge+=DeltaSeconds;
        SpillStrength=.35f+.65f*FMath::Exp(-SpillAge/5.f);
        if(SpillMID) SpillMID->SetScalarParameterValue(TEXT("FlowStrength"),SpillStrength);
    }
    if(BasinMID) if(APawn* Pawn=UGameplayStatics::GetPlayerPawn(this,0))
    {
        const FVector P=Pawn->GetActorLocation();
        const float Speed=FVector::Dist2D(P,LastPawnPosition)/FMath::Max(DeltaSeconds,.001f);
        const auto* Capsule=Pawn->FindComponentByClass<UCapsuleComponent>();
        const float FeetZ=P.Z-(Capsule?Capsule->GetScaledCapsuleHalfHeight():90.f);
        const bool InWater=FeetZ<GetActorLocation().Z+WaterHeight && P.Z>GetActorLocation().Z && FVector::DistSquared2D(P,GetActorLocation())<Radius*Radius;
        BasinMID->SetVectorParameterValue(TEXT("Wader"),FLinearColor(P.X,P.Y,0,0));
        BasinMID->SetScalarParameterValue(TEXT("WakeStrength"),InWater?FMath::Clamp(Speed/250.f,0.f,1.f):0.f);
        LastPawnPosition=P;
    }
}
