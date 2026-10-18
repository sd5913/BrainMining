#include "MiningNumberClues.h"
#include "MiningCollection.h"
#include "DiggableRock.h"
#include "GeneratedMiningRock.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MiningCollectionEffects.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"
#include "EchoPassageStage.h"
#include "EngineUtils.h"

void AMiningCollectionField::SpawnAuthoredClues(const FMiningCollectionPlacement& P)
{
    auto* Rock=P.HostRock.Get(); if(!Rock) return;
    const int32 FirstActor=Spawned.Num(),FirstClue=Clues.Num(),FirstAnt=Ants.Num();
    const auto& T=P.ClueTransform;
    if(auto* Generated=Cast<AGeneratedMiningRock>(Rock))Generated->PrepareRuntimeGeometry(T.GetLocation(),450);
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
        // Spawn after the authored clue transform pass below: the route uses world-space samples.
    }
    else if(Id==2)
    {
        if(Cast<AGeneratedMiningRock>(Rock))
        {
            // MainGame uses chunked themed terrain. Local stains preserve its materials.
            const FLinearColor Colors[]={FLinearColor(.035f,.14f,.045f),FLinearColor(.18f,.018f,.025f),FLinearColor(.025f,.065f,.19f)};
            FRandomStream StainRandom(260218);
            for(int I=0;I<18;++I)
            {
                FVector P(0,(I%3-1)*35+StainRandom.FRandRange(-20,20),StainRandom.FRandRange(-45,45));FHitResult Hit;
                if(!Rock->ActorLineTraceSingle(Hit,T.TransformPosition(P-FVector(100,0,0)),T.TransformPosition(P+FVector(100,0,0)),ECC_Visibility,FCollisionQueryParams(NAME_None,true)))continue;
                P=T.InverseTransformPosition(Hit.ImpactPoint);
                auto* Drop=MakeProp(P-FVector(.15,0,0),FVector(.6,StainRandom.FRandRange(16,32),StainRandom.FRandRange(35,75)),Colors[I%3],true);
                Drop->Tags.Add(TEXT("CollectionReagentStain"));
                FCollectionClue C;C.Actor=Drop;C.Rock=Rock;C.Support=P+FVector(2,0,0);Clues.Add(C);
            }
        }
        else
        {
        auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Campaign/Materials/M_CampaignStainedEarth"));
        auto* Stain=UMaterialInstanceDynamic::Create(Base,Rock);
        const FVector F=T.GetRotation().GetForwardVector(),R=T.GetRotation().GetRightVector(),O=T.GetLocation();
        Stain->SetVectorParameterValue(TEXT("Origin"),FLinearColor(O.X,O.Y,O.Z));
        Stain->SetVectorParameterValue(TEXT("Forward"),FLinearColor(F.X,F.Y,F.Z));
        Stain->SetVectorParameterValue(TEXT("Right"),FLinearColor(R.X,R.Y,R.Z));
        Rock->RockMaterial=Stain;Rock->RockMesh->SetMaterial(0,Stain);
        }
    }
    else if(Id==3)
    {
        for(int I=0;I<3;++I)
        {
            FVector Surface(1,-60+I*60,-30+(I%2)*35);FHitResult Hit;
            if(Rock->ActorLineTraceSingle(Hit,T.TransformPosition(Surface-FVector(50,0,0)),T.TransformPosition(Surface+FVector(70,0,0)),ECC_Visibility,FCollisionQueryParams(NAME_None,true)))
                Surface=T.InverseTransformPosition(Hit.ImpactPoint)+FVector(1.8,0,0);
            MakeFeather(Rock,Surface,-30+I*28);
        }
        const FVector End=T.InverseTransformPosition(P.Transform.GetLocation());
        for(int I=1;I<8;++I) MakeFeather(Rock,FMath::Lerp(FVector(3,0,0),End,float(I)/8)+FVector(0,-40+(I%3)*40,-35),I%2?22:-18);
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
    if(Id==4)
    {
        auto* Numbers=GetWorld()->SpawnActor<AMiningNumberClues>();Spawned.Add(Numbers);
        Numbers->Tags.AddUnique(P.AreaTag);Numbers->Tags.AddUnique(TEXT("CampaignExcavationClue"));
        Numbers->Initialize(Rock,P.Transform.GetLocation(),T.GetLocation(),T.GetRotation().GetForwardVector());
    }
    if(Id==1) SpawnAntTrail(Rock,T.TransformPosition(FVector(-170,0,-105)),P.Transform.GetLocation()+FVector(0,0,25));
}

void AMiningCollectionField::SpawnAuthoredPresentation(AMiningCollectible* Item,const FMiningCollectionPlacement& P)
{
    auto Effect=[&](int Kind,FVector Position)
    {
        auto* A=GetWorld()->SpawnActor<AMiningCollectionEffects>(Position,FRotator::ZeroRotator);
        A->Tags.AddUnique(P.AreaTag);A->SetOwner(Item);A->Initialize(Kind,Item);Spawned.Add(A);return A;
    };
    if(P.ItemId==8)
    {
        Item->bReleased=true;Item->Exposure=1;Item->Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        auto* Jet=Effect(0,Item->GetActorLocation()+FVector(-6,0,38));
        Jet->FlowHeight=FMath::Max(140.f,float(P.ClueTransform.GetLocation().Z-Jet->GetActorLocation().Z+120));
        Jet->Tags.Add(TEXT("CollectionVerticalColaBore"));
    }
    else if(P.ItemId==10)
    {
        auto* Trail=Effect(2,P.ClueTransform.TransformPosition(FVector(-12,0,0)));
        Trail->TrailEnd=Item->GetActorLocation()-Trail->GetActorLocation();
        Effect(4,Item->GetActorLocation())->AttachToActor(Item,FAttachmentTransformRules::KeepWorldTransform);
    }
    // Streamed rooms cannot be hard-referenced by the persistent map. Resolve
    // the room at runtime so its original map remains independently editable.
    AActor* Echo=P.EchoSource.Get();
    if(!Echo && (P.ItemId==6 || P.ItemId==7) && !P.AreaTag.IsNone())
        for(TActorIterator<AEchoPassageStage> It(GetWorld());It;++It)
            if(It->ActorHasTag(P.AreaTag) || It->GetComponentsBoundingBox(true).ExpandBy(100).IsInsideOrOn(P.Transform.GetLocation())){Echo=*It;break;}
    if(Echo)
    {
        UMaterialInterface* Surface=nullptr;UMaterialInterface* Contours=nullptr;
        TArray<UInstancedStaticMeshComponent*> Components;Echo->GetComponents(Components);
        for(auto* C:Components)
        {
            if(C->GetFName()==TEXT("CollisionRock"))Surface=C->GetMaterial(0);
            if(C->GetFName()==TEXT("EchoContours"))Contours=C->GetMaterial(0);
        }
        Item->ApplyEchoPresentation(Surface,Contours);
    }
}
