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
#include "Showcase/MiningTreasure.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/PlayerController.h"

void AMiningCollectionField::SpawnAntTrail(ADiggableRock* Rock,FVector Start,FVector End)
{
    FCollectionAntTrail Trail;Trail.Rock=Rock;Trail.Start=Start;Trail.End=End;
    const int TrailIndex=AntTrails.Add(Trail);
    for(int I=0;I<42;++I)
    {
        auto* Root=GetWorld()->SpawnActor<AActor>();Spawned.Add(Root);Root->Tags.Add(TEXT("CollectionAmbientAnt"));Root->Tags.Add(TEXT("CollectionSoilAnt"));Root->SetOwner(Rock);
        auto* Scene=NewObject<USceneComponent>(Root);Root->SetRootComponent(Scene);Root->AddInstanceComponent(Scene);Scene->RegisterComponent();
        FCollectionAnt Ant;Ant.Actor=Root;Ant.TrailIndex=TrailIndex;Ant.Length=FVector::Distance(Start,End);Ant.Phase=I/42.f;
        const FLinearColor Shell(.028,.014,.008);
        auto Part=[&](FVector P,FVector Size){auto* A=MakeProp(P,Size,Shell,true);A->AttachToActor(Root,FAttachmentTransformRules::KeepRelativeTransform);return A;};
        Part(FVector(-5.5,0,2.8),FVector(7.3,4.5,4.6)); // Gaster
        Part(FVector(-1.3,0,2.6),FVector(1.5,1.4,1.5)); // Narrow petiole
        Part(FVector(1,0,2.7),FVector(3.6,2.6,3));      // Thorax
        Part(FVector(4.4,0,3),FVector(4.2,3.8,3.4));   // Head faces travel
        auto Rod=[&](FVector A,FVector B,float Width,USceneComponent* Parent)
        {
            auto* Segment=MakeProp((A+B)*.5,FVector((B-A).Size(),Width,Width),Shell);
            Segment->SetActorRotation((B-A).Rotation());
            Segment->AttachToComponent(Parent,FAttachmentTransformRules::KeepRelativeTransform);
        };
        for(int Side:{-1,1})
        {
            Rod(FVector(5.4,Side*1.1,3.7),FVector(7.7,Side*2.1,4.5),.45,Scene);
            Rod(FVector(7.7,Side*2.1,4.5),FVector(9.3,Side*3.8,3.8),.35,Scene);
            for(int J=0;J<3;++J)
            {
                auto* Joint=NewObject<USceneComponent>(Root);Root->AddInstanceComponent(Joint);Joint->SetupAttachment(Scene);Joint->RegisterComponent();
                Joint->SetRelativeLocation(FVector(2-J*1.6,Side*.8,2.4));
                const float Sweep=2.8f-J*2.8f;
                const FVector Knee(Sweep,Side*3.2,1.2),Foot(Sweep*1.8,Side*5.2,-2.2);
                Rod(FVector::ZeroVector,Knee,.55,Joint);Rod(Knee,Foot,.4,Joint);
                Rod(Foot,Foot+FVector(1,Side*.7,0),.3,Joint);Ant.Legs.Add(Joint);
            }
        }
        Root->SetActorScale3D(FVector(.45f));
        Ants.Add(Ant);
    }
    AmbientAntCount=Ants.Num();
}

void AMiningCollectionField::UpdateAntTrails()
{
    for(auto& Trail:AntTrails)
    {
        auto* Rock=Trail.Rock.Get();if(!Rock || Trail.ObservedCuts==Rock->SuccessfulCuts)continue;
        Trail.ObservedCuts=Rock->SuccessfulCuts;
        const int Samples=FMath::Clamp(FMath::CeilToInt(FVector::Distance(Trail.Start,Trail.End)/24),8,140);
        TArray<FVector> Centers;for(int I=0;I<=Samples;++I)Centers.Add(FMath::Lerp(Trail.Start,Trail.End,float(I)/Samples));
        TArray<TObjectPtr<ADiggableRock>> Rocks;Rocks.Add(Rock);TArray<bool> Covered;
        AMiningTreasure::MarkCoveredPoints(Rocks,Centers,Covered);
        Trail.Points=Centers;Trail.Normals.Init(FVector::UpVector,Centers.Num());Trail.Exposed.Init(false,Centers.Num());
        const FVector Side=FVector::CrossProduct((Trail.End-Trail.Start).GetSafeNormal(),FVector::UpVector).GetSafeNormal();
        FCollisionQueryParams Query(SCENE_QUERY_STAT(CollectionAntSurface),true);if(Player&&Player->GetPawn())Query.AddIgnoredActor(Player->GetPawn());
        for(int I=0;I<Centers.Num();++I)
        {
            if(Covered[I])continue; // Still buried; this part becomes visible as excavation advances.
            float Nearest=280;
            for(FVector D:{-FVector::UpVector,Side,-Side,FVector::UpVector})
            {
                FHitResult Hit;
                if(GetWorld()->LineTraceSingleByChannel(Hit,Centers[I],Centers[I]+D*280,ECC_Visibility,Query)&&Hit.Distance<Nearest&&FVector::DotProduct(Hit.ImpactNormal,-D)>.1)
                { Nearest=Hit.Distance;Trail.Points[I]=Hit.ImpactPoint;Trail.Normals[I]=Hit.ImpactNormal;Trail.Exposed[I]=true; }
            }
        }
    }
}

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
        TArray<FVector> Supports;
        for(const auto& C:Clues)if(C.Actor.IsValid()&&C.Rock.Get()==Rock)Supports.Add(C.Support);
        TArray<bool> Covered;AMiningTreasure::MarkCoveredPoints({Rock},Supports,Covered);
        int32 SupportIndex=0;
        for(auto& C:Clues) if(C.Actor.IsValid()&&C.Rock.Get()==Rock)
        {
            C.ObservedCuts=Rock->SuccessfulCuts;
            bool Supported=Covered[SupportIndex++];
            if(C.bFalling) continue;
            if(C.bLanded)
            {
                FHitResult Hit; const FVector P=C.Actor->GetActorLocation();
                Supported=GetWorld()->LineTraceSingleByChannel(Hit,P,P-FVector(0,0,12),ECC_Visibility)&&Hit.Normal.Z>.4;
            }
            if(Supported) continue;
            if(!C.bFeather)
            {
                TArray<AActor*> Parts;C.Actor->GetAttachedActors(Parts,true,true);
                for(AActor* Part:Parts)if(IsValid(Part))Part->Destroy();
                C.Actor->Destroy();continue;
            }
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
