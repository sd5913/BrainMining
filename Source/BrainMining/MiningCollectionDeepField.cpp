#include "MiningCollection.h"
#include "MiningSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "DiggableRock.h"
#include "Showcase/MiningTreasure.h"
#include "GameFramework/PlayerController.h"
#include "FogChamberComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Spatial/FastWinding.h"
#include "UDynamicMesh.h"

AActor* AMiningCollectionField::MakeProp(FVector P,FVector Size,FLinearColor Color,bool Round)
{
    auto* A=GetWorld()->SpawnActor<AStaticMeshActor>(P,FRotator::ZeroRotator); Spawned.Add(A);
    auto* C=A->GetStaticMeshComponent(); C->SetMobility(EComponentMobility::Movable);
    C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Round?TEXT("/Engine/BasicShapes/Sphere"):TEXT("/Engine/BasicShapes/Cube")));
    C->SetCollisionEnabled(ECollisionEnabled::NoCollision); A->SetActorScale3D(Size/100);
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionTint")))
    {
        auto* M=UMaterialInstanceDynamic::Create(Base,this); M->SetVectorParameterValue(TEXT("Tint"),Color); C->SetMaterial(0,M);
    }
    return A;
}

void AMiningCollectionField::SpawnDeepField()
{
    Ants.Reset(); AntTrails.Reset(); Birds.Reset(); BirdPath.Reset(); ResidentBird.Reset(); Clues.Reset();
    FlockBursts=0; AmbientAntCount=0; AmbientTime=0; FlightTime=-1; NextFlockCheck=0; BirdObservedCuts=INDEX_NONE;
    ActiveSeed=RandomSeed>0?RandomSeed:FMath::RandRange(1,MAX_int32-1); FRandomStream Random(ActiveSeed);
    const float Depth=FMath::Clamp(ExcavationDepth,650.f,1800.f),Front=460,RoomX=Front+Depth+250,Length=Depth+600;
    int Ids[4]={0,1,2,3}; for(int i=3;i>0;--i) Swap(Ids[i],Ids[Random.RandRange(0,i)]);
    for(int Bay=0;Bay<4;++Bay)
    {
        const int Id=Ids[Bay]; const float Y=-900+Bay*600,BaseZ=Id==3?350:0;
        const bool Cave=Id!=2;
        FTransform Transform(FRotator::ZeroRotator,FVector(Front+Length*.5f,Y,BaseZ+300));
        auto* Rock=GetWorld()->SpawnActorDeferred<ADiggableRock>(ADiggableRock::StaticClass(),Transform);
        Rock->RockSize=FVector(Length,540,600); Rock->RockMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionEarth"));
        if(Cave)
        {
            Rock->FogChamber->bEnabled=true;
            Rock->FogChamber->RoomSize=FVector(560,440,460);
            Rock->FogChamber->RoomOffset=FVector(RoomX-Transform.GetLocation().X,0,-60);
            Rock->FogChamber->FogMaterial=nullptr;
        }
        Rock->FinishSpawning(Transform); Spawned.Add(Rock);
        if(Id==2)
        {
            // The stain is part of the excavated surface, never a floating prop.
            if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionStainedEarth")))
            {
                auto* M=UMaterialInstanceDynamic::Create(Base,Rock);
                M->SetScalarParameterValue(TEXT("TrailY"),Y+GetActorLocation().Y);
                Rock->RockMaterial=M; Rock->RockMesh->SetMaterial(0,M);
            }
        }
        const FVector Position(Cave?RoomX+40:Front+Depth+Random.FRandRange(-35,35),Y+(Cave?0:Random.FRandRange(-45,45)),BaseZ+(Cave?70:150));
        auto* Item=GetWorld()->SpawnActor<AMiningCollectible>(Position,FRotator::ZeroRotator);
        Item->BuildModel(Id); Item->HostRock=Rock; Item->BuriedTransform=Item->GetActorTransform(); Items.Add(Item); Spawned.Add(Item);
        if(Cave)
        {
            Item->bReleased=true; Item->Exposure=1; Item->Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            auto* Lamp=GetWorld()->SpawnActor<AActor>(FVector(RoomX-80,Y,BaseZ+350),FRotator::ZeroRotator); Spawned.Add(Lamp);
            auto* Light=NewObject<UPointLightComponent>(Lamp); Lamp->SetRootComponent(Light); Lamp->AddInstanceComponent(Light); Light->RegisterComponent();
            Light->SetWorldLocation(FVector(RoomX-80,Y,BaseZ+350)); Light->SetAttenuationRadius(450); Light->SetIntensity(180);
            Light->SetLightColor(Id==0?FLinearColor(.45f,.77f,1):Id==1?FLinearColor(.4f,1,.6f):FLinearColor(1,.77f,.4f));
        }
        if(Id==0)
        {
            // A continuous physical clue crosses the earth and enters the room beside the toilet.
            const float PipeY=Y+125,PipeStart=Front-95,PipeEnd=RoomX+110;
            MakeProp(FVector((PipeStart+PipeEnd)*.5f,PipeY,220),FVector(PipeEnd-PipeStart,18,18),FLinearColor(.19f,.34f,.35f));
            MakeProp(FVector(PipeStart,PipeY,187),FVector(18,18,82),FLinearColor(.19f,.34f,.35f));
            for(float X=Front;X<PipeEnd;X+=155) MakeProp(FVector(X,PipeY,220),FVector(13,27,27),FLinearColor(.36f,.44f,.4f));
            MakeProp(FVector(PipeEnd,Y+65,170),FVector(18,130,18),FLinearColor(.19f,.34f,.35f));
            MakeProp(FVector(PipeEnd,PipeY,193),FVector(18,18,70),FLinearColor(.19f,.34f,.35f));
            for(int i=0;i<4;++i) MakeProp(FVector(PipeStart,PipeY,136-i*22),FVector(6,6,12),FLinearColor(.13f,.59f,.76f),true);
            MakeProp(FVector(RoomX,Y,11),FVector(525,405,2),FLinearColor(.07f,.23f,.28f));
        }
        else if(Id==1)
        {
            // Match the campaign: the moving ants themselves are the clue.
            SpawnAntTrail(Rock,FVector(210,Y,95),FVector(RoomX+15,Y,95));
        }
        else if(Id==2)
        {
            // Irregular residue is shaded directly onto the soil mesh above.
        }
        else
        {
            ResidentBird=Item;
            auto AddNestSupport=[&](AActor* A,float AnchorY)
            {
                FCollectionClue C; C.Actor=A; C.Rock=Rock; C.Support=FVector(Front+3,AnchorY,BaseZ+280); Clues.Add(C);
            };
            AddNestSupport(MakeProp(FVector(Front-28,Y,BaseZ+280),FVector(74,120,15),FLinearColor(.24f,.11f,.04f)),Y);
            for(int i=0;i<8;++i)
            {
                auto* Twig=MakeProp(FVector(Front-28,Y-52+i*15,BaseZ+291),FVector(68,7,6),FLinearColor(.37f,.22f,.07f)); Twig->SetActorRotation(FRotator(0,i%2?25:-25,0));
                AddNestSupport(Twig,Y-52+i*15);
                MakeProp(FVector(RoomX+35,Y-50+i*14,BaseZ+17),FVector(90,6,6),FLinearColor(.37f,.22f,.07f))->SetActorRotation(FRotator(0,i%2?25:-25,0));
            }
            // Readable feather silhouettes on the entrance and in deeper soil.
            for(int i=0;i<3;++i) MakeFeather(Rock,FVector(Front+1,Y-60+i*60,BaseZ+200+(i%2)*35),-30+i*28);
            for(int i=1;i<5;++i) MakeFeather(Rock,FVector(Front+i*Depth/5,Y-45+(i%3)*40,BaseZ+195),i%2?22:-18);
        }
    }
    auto* Tutorial=GetWorld()->SpawnActor<AMiningCollectible>(FVector(-300,-330,85),FRotator(0,-15,0));
    Tutorial->BuildModel(2); Tutorial->bReleased=true; Tutorial->Exposure=1; Tutorial->Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Items.Add(Tutorial); Spawned.Add(Tutorial);
}

bool AMiningCollectionField::FindBirdExit(TArray<FVector>& Path) const
{
    if(!ResidentBird.IsValid()||!ResidentBird->HostRock) return false;
    ADiggableRock* Rock=ResidentBird->HostRock;
    if(bUseAuthoredPlacements && Player)
    {
        FVector Eye;FRotator EyeRotation;Player->GetPlayerViewPoint(Eye,EyeRotation);
        const FVector Start=ResidentBird->GetActorLocation()+FVector(0,0,25);
        const FVector Toward=(Eye-Start).GetSafeNormal();
        FCollisionQueryParams Query(SCENE_QUERY_STAT(AuthoredBirdFlight),true,ResidentBird.Get());
        if(Player->GetPawn())Query.AddIgnoredActor(Player->GetPawn());
        FHitResult Hit;
        const FVector End=Eye+FVector(0,0,35);
        if(GetWorld()->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(14),Query))return false;
        TArray<bool> Covered;AMiningTreasure::MarkCoveredPoints({Rock},{Start},Covered);
        if(Covered[0])return false;
        Path={Start,FMath::Lerp(Start,End,.5f),End};
        const FVector Beyond=End+Toward*240+FVector(0,0,120);
        if(!GetWorld()->SweepSingleByChannel(Hit,End,Beyond,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(14),Query))Path.Add(Beyond);
        return true;
    }
    const auto& Mesh=Rock->RockMesh->GetDynamicMesh()->GetMeshRef();
    UE::Geometry::FDynamicMeshAABBTree3 Tree(&Mesh,true);
    UE::Geometry::TFastWindingTree<UE::Geometry::FDynamicMesh3> Winding(&Tree);
    Path.Reset();
    const FTransform Frame=Rock->GetActorTransform();
    const FVector Origin=Frame.InverseTransformPosition(ResidentBird->GetActorLocation())+FVector(-60,0,45),Center=FVector::ZeroVector,Half=Rock->RockSize*.5f;
    const float Front=Center.X-Half.X;
    auto Air=[&](FVector P)
    {
        if(P.Z<Center.Z-Half.Z+30||P.Z>Center.Z+Half.Z-30||FMath::Abs(P.Y-Center.Y)>Half.Y-30||P.X>Center.X+Half.X-30||P.X<Front-110) return false;
        for(FVector Offset:{FVector::ZeroVector,FVector(24,0,0),FVector(-24,0,0),FVector(0,24,0),FVector(0,-24,0),FVector(0,0,24),FVector(0,0,-24)})
            if(Mesh.TriangleCount()>0&&FMath::Abs(Winding.FastWindingNumber(P+Offset))>.5) return false;
        return true;
    };
    if(!Air(Origin)) return false;
    TArray<FIntVector> Queue; TArray<int32> Parent; TSet<FIntVector> Visited;
    Queue.Add(FIntVector::ZeroValue); Parent.Add(INDEX_NONE); Visited.Add(FIntVector::ZeroValue);
    int End=INDEX_NONE;
    for(int Head=0;Head<Queue.Num()&&Head<6000;++Head)
    {
        const FVector P=Origin+FVector(Queue[Head])*65;
        if(P.X<Front-25) { End=Head; break; }
        for(const FIntVector Step:{FIntVector(-1,0,0),FIntVector(0,-1,0),FIntVector(0,1,0),FIntVector(0,0,1),FIntVector(0,0,-1),FIntVector(1,0,0)})
        {
            const FIntVector Key=Queue[Head]+Step;
            if(Visited.Contains(Key)) continue;
            const FVector Next=Origin+FVector(Key)*65;
            if(!Air(Next)) { Visited.Add(Key); continue; }
            if(!Air((P+Next)*.5)) continue;
            Visited.Add(Key); Queue.Add(Key); Parent.Add(Head);
        }
    }
    if(End==INDEX_NONE) return false;
    for(int Id=End;Id!=INDEX_NONE;Id=Parent[Id]) Path.Insert(Origin+FVector(Queue[Id])*65,0);
    for(FVector& P:Path) P=Frame.TransformPosition(P);
    const FVector Exit=Path.Last();
    if(!bBuriedWithClues)
    {Path.Add(Exit+Frame.TransformVector(FVector(-350,0,130)));Path.Add(Exit+Frame.TransformVector(FVector(-850,0,850)));}
    else
    {
        // The campaign is an enclosed cave. Clip the flight to clear air instead of flying through the opposite wall.
        FCollisionQueryParams Query(SCENE_QUERY_STAT(CampaignBirdFlight),true,ResidentBird.Get());
        if(Player && Player->GetPawn())Query.AddIgnoredActor(Player->GetPawn());
        for(FVector Delta:{Frame.TransformVector(FVector(-150,0,15)),FVector(280,0,35),FVector(260,0,25)})
        {
            FHitResult Hit;const FVector Start=Path.Last(),Destination=Start+Delta;
            const bool Blocked=GetWorld()->SweepSingleByChannel(Hit,Start,Destination,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(22),Query);
            const FVector Next=Blocked?Start+Delta*FMath::Max(0.f,Hit.Time-.08f):Destination;
            if(FVector::Distance(Start,Next)>30)Path.Add(Next);
        }
    }
    return true;
}

void AMiningCollectionField::TickAmbient(float Dt)
{
    AmbientTime+=Dt;
    TickClues(Dt);
    UpdateAntTrails();
    for(FCollectionAnt& Ant:Ants) if(Ant.Actor.IsValid())
    {
        const bool ComputerCollected=GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>()->WasCollectedThisRun(1);
        if(ComputerCollected)
        {
            Ant.Fade=FMath::Max(0.f,Ant.Fade-Dt/2.5f);
            if(Ant.FadeMaterial.IsValid())Ant.FadeMaterial->SetVectorParameterValue(TEXT("Fade"),FLinearColor(Ant.Fade,Ant.Fade,Ant.Fade));
            if(Ant.Fade<=0)
            {
                TArray<AActor*> Parts;Ant.Actor->GetAttachedActors(Parts,true,true);
                for(AActor* Part:Parts)Part->Destroy();
                Ant.Actor->Destroy();continue;
            }
        }
        const float T=FMath::Fmod(AmbientTime*32/Ant.Length+Ant.Phase,1.f);
        if(AntTrails.IsValidIndex(Ant.TrailIndex))
        {
            const auto& Trail=AntTrails[Ant.TrailIndex];
            if(Trail.Points.Num()<2)continue;
            const float Along=T*(Trail.Points.Num()-1);const int I=FMath::Min(FMath::FloorToInt(Along),Trail.Points.Num()-2);const float Blend=Along-I;
            const bool Visible=Trail.Exposed[I]&&Trail.Exposed[I+1];
            if(Ant.Actor->IsHidden()==Visible)
            {
                Ant.Actor->SetActorHiddenInGame(!Visible);
                TArray<AActor*> Parts;Ant.Actor->GetAttachedActors(Parts);
                for(AActor* Part:Parts)Part->SetActorHiddenInGame(!Visible);
            }
            Ant.Actor->SetActorLocation(FMath::Lerp(Trail.Points[I],Trail.Points[I+1],Blend));
            if(Visible)
            {
                const FVector N=FMath::Lerp(Trail.Normals[I],Trail.Normals[I+1],Blend).GetSafeNormal();
                const FVector Heading=FVector::VectorPlaneProject(Trail.End-Trail.Start,N).GetSafeNormal();
                Ant.Actor->SetActorLocation(FMath::Lerp(Trail.Points[I],Trail.Points[I+1],Blend)+N*.25f);
                Ant.Actor->SetActorRotation(FRotationMatrix::MakeFromXZ(Heading,N).ToQuat());
            }
        }
        else if(Ant.Actor->ActorHasTag(TEXT("CampaignWallAnt")))
        {
            auto* Surface=Cast<ADiggableRock>(Ant.Actor->GetOwner());FHitResult Hit;
            const FVector Local=Ant.Start+FVector(0,T*Ant.Length,FMath::Sin(T*PI*4+Ant.Phase)*4);
            const bool Supported=Surface && Surface->ActorLineTraceSingle(Hit,Ant.Frame.TransformPosition(Local-FVector(50,0,0)),Ant.Frame.TransformPosition(Local+FVector(50,0,0)),ECC_Visibility,FCollisionQueryParams(NAME_None,true));
            Ant.Actor->SetActorHiddenInGame(!Supported);
            if(Supported)
            {
                Ant.Actor->SetActorLocation(Hit.ImpactPoint+Hit.ImpactNormal*1.5);
                Ant.Actor->SetActorRotation(FRotationMatrix::MakeFromXZ(Ant.Frame.TransformVector(FVector(0,1,0)),Hit.ImpactNormal).ToQuat());
            }
        }
        else
        {
            Ant.Actor->SetActorLocation(Ant.Frame.TransformPosition(Ant.Start+FVector(T*Ant.Length,FMath::Sin(T*PI*4+Ant.Phase)*5,.4f*FMath::Sin(AmbientTime*20))));
            Ant.Actor->SetActorRotation(Ant.Frame.GetRotation()*FRotator(0,FMath::Cos(T*PI*4+Ant.Phase)*8,0).Quaternion());
        }
        for(int i=0;i<Ant.Legs.Num();++i) if(Ant.Legs[i].IsValid()) Ant.Legs[i]->SetRelativeRotation(FRotator(0,FMath::Sin(AmbientTime*18+i*PI)*27,0));
    }
    if(ResidentBird.IsValid()&&!ResidentBird->bCollected)
        ResidentBird->Model->SetRelativeRotation(FRotator(0,0,FMath::Sin(AmbientTime*.7f)*5));
    if(FlockBursts==0&&ResidentBird.IsValid()&&ResidentBird->HostRock&&AmbientTime>=NextFlockCheck)
    {
        NextFlockCheck=AmbientTime+.3f;
        const int Cuts=ResidentBird->HostRock->SuccessfulCuts;
        if(Cuts>0)
        {
            BirdObservedCuts=Cuts;
            if(Cuts>0&&FindBirdExit(BirdPath))
            {
                FlockBursts=1; FlightTime=0;
                for(int i=0;i<8;++i)
                {
                    auto* Bird=GetWorld()->SpawnActor<AMiningCollectible>(BirdPath[0],FRotator::ZeroRotator); Bird->Tags.Add(TEXT("CollectionFlyingBird")); Spawned.Add(Bird);
                    Bird->BuildModel(3,true); Bird->SetActorScale3D(FVector(.25f)); Bird->SetActorEnableCollision(false);
                    FCollectionBird Flight; Flight.Actor=Bird; Flight.Delay=i*.16f;
                    for(int Side:{-1,1})
                    {
                        auto* Wing=NewObject<UStaticMeshComponent>(Bird); Bird->AddInstanceComponent(Wing); Wing->SetupAttachment(Bird->GetRootComponent());
                        Wing->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"))); Wing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                        Wing->SetMaterial(0,Bird->Model->GetMaterial(6)); Wing->SetRelativeLocation(FVector(4,Side*32,-2)); Wing->SetRelativeScale3D(FVector(.42,.7,.045)); Wing->RegisterComponent();
                        if(Side<0) Flight.LeftWing=Wing; else Flight.RightWing=Wing;
                    }
                    Birds.Add(Flight);
                }
                Toast=TEXT("鸟群飞出了洞穴，巢里还留着一只歪头的小家伙"); ToastRemaining=6;
            }
        }
    }
    if(FlightTime<0) return;
    FlightTime+=Dt;
    for(int i=0;i<Birds.Num();++i)
    {
        auto& B=Birds[i]; if(!B.Actor.IsValid()) continue;
        const float Age=FlightTime-B.Delay;
        B.Actor->SetActorHiddenInGame(Age<0); if(Age<0) continue;
        float Distance=Age*340; int Segment=1;
        while(Segment<BirdPath.Num()&&Distance>FVector::Distance(BirdPath[Segment-1],BirdPath[Segment])) { Distance-=FVector::Distance(BirdPath[Segment-1],BirdPath[Segment]); ++Segment; }
        if(Segment>=BirdPath.Num()) { B.Actor->Destroy(); continue; }
        const FVector Start=BirdPath[Segment-1],End=BirdPath[Segment],Direction=(End-Start).GetSafeNormal();
        B.Actor->SetActorLocation(Start+Direction*Distance+FVector(0,FMath::Sin(Age*3+i)*4,FMath::Sin(Age*5+i)*5));
        B.Actor->SetActorRotation((-Direction).Rotation());
        const float Flap=FMath::Sin(Age*23)*55;
        if(B.LeftWing.IsValid()) B.LeftWing->SetRelativeRotation(FRotator(0,0,Flap));
        if(B.RightWing.IsValid()) B.RightWing->SetRelativeRotation(FRotator(0,0,-Flap));
    }
}
