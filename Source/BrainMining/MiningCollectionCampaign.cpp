#include "MiningCollection.h"
#include "DiggableRock.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInstanceDynamic.h"

void AMiningCollectionField::SpawnAuthoredClues(const FMiningCollectionPlacement& P)
{
    auto* Rock=P.HostRock.Get(); if(!Rock) return;
    const int32 FirstActor=Spawned.Num(),FirstClue=Clues.Num(),FirstAnt=Ants.Num();
    const auto& T=P.ClueTransform;
    const int Id=P.ItemId;
    // Coordinates describe a soil bank: clues at X=0, treasure deeper inside the wall.
    if(Id==0)
    {
        const FLinearColor Pipe(.19f,.34f,.35f);
        MakeProp(FVector(390,115,5),FVector(1000,18,18),Pipe);
        MakeProp(FVector(-105,115,-45),FVector(18,18,115),Pipe);
        for(int I=0;I<7;++I) MakeProp(FVector(I*140,115,5),FVector(13,27,27),FLinearColor(.36f,.44f,.4f));
        MakeProp(FVector(840,50,5),FVector(18,145,18),Pipe);
        for(int I=0;I<4;++I) MakeProp(FVector(-105,115,-112-I*20),FVector(6,6,12),FLinearColor(.13f,.59f,.76f),true);
        MakeProp(FVector(-110,115,-216),FVector(120,110,3),FLinearColor(.07f,.23f,.28f),true);
    }
    else if(Id==1)
    {
        MakeProp(FVector(-2,0,-190),FVector(5,42,46),FLinearColor(.015f,.017f,.013f),true);
        Rock->MineSphere(T.TransformPosition(FVector(2,0,-190)),24);
        for(int I=0;I<12;++I)
        {
            auto* Root=GetWorld()->SpawnActor<AActor>();Spawned.Add(Root);Root->Tags.Add(TEXT("CollectionAmbientAnt"));Root->Tags.Add(TEXT("CampaignWallAnt"));Root->SetOwner(Rock);
            auto* Scene=NewObject<USceneComponent>(Root);Root->SetRootComponent(Scene);Root->AddInstanceComponent(Scene);Scene->RegisterComponent();
            FCollectionAnt Ant;Ant.Actor=Root;Ant.Start=FVector(0,-255,-190);Ant.Length=285;Ant.Phase=I/12.f;
            for(int J=0;J<3;++J)MakeProp(FVector(-6+J*6,0,3),FVector(J==0?9:6,5,5),FLinearColor(.055f,.02f,.008f),true)->AttachToActor(Root,FAttachmentTransformRules::KeepRelativeTransform);
            for(int Side:{-1,1}) for(int J=0;J<3;++J)
            {
                auto* Leg=Cast<AStaticMeshActor>(MakeProp(FVector(-5+J*5,Side*5,1),FVector(2,10,1.6),FLinearColor(.035f,.012f,.008f)));
                Leg->AttachToActor(Root,FAttachmentTransformRules::KeepRelativeTransform);Ant.Legs.Add(Leg->GetStaticMeshComponent());
            }
            Ants.Add(Ant);
        }
        AmbientAntCount=Ants.Num();
    }
    else if(Id==2)
    {
        auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Campaign/Materials/M_CampaignStainedEarth"));
        auto* Stain=UMaterialInstanceDynamic::Create(Base,Rock);
        const FVector F=T.GetRotation().GetForwardVector(),R=T.GetRotation().GetRightVector(),O=T.GetLocation();
        Stain->SetVectorParameterValue(TEXT("Origin"),FLinearColor(O.X,O.Y,O.Z));
        Stain->SetVectorParameterValue(TEXT("Forward"),FLinearColor(F.X,F.Y,F.Z));
        Stain->SetVectorParameterValue(TEXT("Right"),FLinearColor(R.X,R.Y,R.Z));
        Rock->RockMaterial=Stain;Rock->RockMesh->SetMaterial(0,Stain);
    }
    else
    {
        for(int I=0;I<3;++I)
        {
            FVector Surface(1,-60+I*60,-30+(I%2)*35);FHitResult Hit;
            if(Rock->ActorLineTraceSingle(Hit,T.TransformPosition(Surface-FVector(50,0,0)),T.TransformPosition(Surface+FVector(70,0,0)),ECC_Visibility,FCollisionQueryParams(NAME_None,true)))
                Surface=T.InverseTransformPosition(Hit.ImpactPoint)+FVector(1.8,0,0);
            MakeFeather(Rock,Surface,-30+I*28);
        }
        for(int I=1;I<8;++I) MakeFeather(Rock,FVector(I*120,-40+(I%3)*40,-35),I%2?22:-18);
        for(int I=0;I<5;++I)
        {
            auto* Twig=MakeProp(FVector(-20,-45+I*22,-110),FVector(50,7,6),FLinearColor(.37f,.22f,.07f));
            Twig->SetActorRotation(FRotator(0,I%2?25:-25,0));
            FCollectionClue C;C.Actor=Twig;C.Rock=Rock;C.Support=FVector(3,-45+I*22,-110);Clues.Add(C);
        }
    }
    for(int I=FirstActor;I<Spawned.Num();++I)
    {
        auto* A=Spawned[I].Get(); A->Tags.AddUnique(P.AreaTag);A->Tags.AddUnique(TEXT("CampaignExcavationClue"));
        if(!A->GetAttachParentActor())
        {A->SetActorLocation(T.TransformPosition(A->GetActorLocation()));A->SetActorRotation(T.GetRotation()*A->GetActorQuat());}
    }
    for(int I=FirstClue;I<Clues.Num();++I)Clues[I].Support=T.TransformPosition(Clues[I].Support);
    for(int I=FirstAnt;I<Ants.Num();++I)Ants[I].Frame=T;
}
