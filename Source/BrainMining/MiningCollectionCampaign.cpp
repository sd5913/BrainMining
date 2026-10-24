#include "MiningNumberClues.h"
#include "MiningCollection.h"
#include "DiggableRock.h"
#include "GeneratedMiningRock.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DecalActor.h"
#include "Components/DecalComponent.h"
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
        // Ants follow the soil surface; no artificial black disc or initial bore.
        // Spawn after the authored clue transform pass below: the route uses world-space samples.
    }
    else if(Id==2)
    {
        if(Cast<AGeneratedMiningRock>(Rock))
        {
            auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionStainOverlay"));
            if(Base)
            {
                ReagentStain=UMaterialInstanceDynamic::Create(Base,this);
                const FVector O=T.GetLocation(),F=T.GetRotation().GetForwardVector(),R=T.GetRotation().GetRightVector();
                ReagentStain->SetVectorParameterValue(TEXT("Origin"),FLinearColor(O.X,O.Y,O.Z));
                ReagentStain->SetVectorParameterValue(TEXT("Forward"),FLinearColor(F.X,F.Y,F.Z));
                ReagentStain->SetVectorParameterValue(TEXT("Right"),FLinearColor(R.X,R.Y,R.Z));
                // Dynamic-mesh scene proxies do not render UMeshComponent's overlay
                // pass. A bounded decal follows both the original and freshly cut faces.
                auto* Stain=GetWorld()->SpawnActor<ADecalActor>();
                Stain->SetActorLocation(FVector(450,0,0));
                Stain->SetActorRotation(FRotator::ZeroRotator);
                Stain->GetDecal()->DecalSize=FVector(500,150,150);
                Stain->GetDecal()->SetDecalMaterial(ReagentStain);
                Stain->GetDecal()->SetFadeScreenSize(0);
                Spawned.Add(Stain);
                Rock->Tags.AddUnique(TEXT("CollectionReagentStain"));
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
        for(int I=0;I<7;++I)
        {
            FVector Surface(1,-90+I*30,-30+(I%3)*22);FHitResult Hit;
            if(Rock->ActorLineTraceSingle(Hit,T.TransformPosition(Surface-FVector(50,0,0)),T.TransformPosition(Surface+FVector(70,0,0)),ECC_Visibility,FCollisionQueryParams(NAME_None,true)))
                Surface=T.InverseTransformPosition(Hit.ImpactPoint)+FVector(1.8,0,0);
            MakeFeather(Rock,Surface,-35+(I*23)%76);
        }
        const FVector End=T.InverseTransformPosition(P.Transform.GetLocation());
        for(int I=1;I<15;++I) MakeFeather(Rock,FMath::Lerp(FVector(3,0,0),End,float(I)/15)+FVector(0,FMath::Sin(I*2.4f)*48,-35+(I%3)*12),I%2?22:-18);
        for(int I=0;I<5;++I)
        {
            auto* Twig=MakeProp(FVector(-20,-45+I*22,-110),FVector(50,7,6),FLinearColor(.37f,.22f,.07f));
            Twig->SetActorRotation(FRotator(0,I%2?25:-25,0));
            FCollectionClue C;C.Actor=Twig;C.Rock=Rock;C.Support=FVector(3,-45+I*22,-110);Clues.Add(C);
        }
    }
    if(Id>=16 && Id<=22)SpawnBranchClues(P);
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
        Jet->FlowHeight=FMath::Max(140.f,float(P.ClueTransform.GetLocation().Z-Jet->GetActorLocation().Z+300));
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
