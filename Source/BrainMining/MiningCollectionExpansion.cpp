#include "MiningNumberClues.h"
#include "MiningCollection.h"
#include "MiningCollectionEffects.h"
#include "DiggableRock.h"
#include "FogChamberComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/DynamicMeshComponent.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "DynamicMeshEditor.h"
#include "UDynamicMesh.h"

void AMiningCollectionField::SpawnExpandedCollection()
{
    const FLinearColor Dark(.045,.065,.075),Gold(.8,.46,.09),Cream(.8,.83,.72),Red(.38,.008,.025);
    auto Solid=[&](FVector P,FVector Size,FLinearColor Color)
    {
        auto* A=CastChecked<AStaticMeshActor>(MakeProp(P,Size,Color));
        A->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        A->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Block);
        A->Tags.Add(TEXT("CollectionExpansionArchitecture")); return A;
    };
    auto Sign=[&](const FString& Text,FVector P,float Size=22.f,FRotator Rotation=FRotator(0,180,0))
    {
        auto* A=GetWorld()->SpawnActor<AActor>(P,Rotation); Spawned.Add(A);
        auto* C=NewObject<UTextRenderComponent>(A); A->SetRootComponent(C); A->AddInstanceComponent(C);
        C->SetText(FText::FromString(Text)); C->SetWorldSize(Size); C->SetHorizontalAlignment(EHTA_Center);
        C->SetTextRenderColor(FColor(230,211,161)); C->RegisterComponent(); C->SetWorldLocationAndRotation(P,Rotation);
        return A;
    };
    auto Lamp=[&](FVector P,FLinearColor Color,float Intensity,float Radius)
    {
        auto* A=GetWorld()->SpawnActor<AActor>(P,FRotator::ZeroRotator); Spawned.Add(A);
        auto* C=NewObject<UPointLightComponent>(A); A->SetRootComponent(C); A->AddInstanceComponent(C); C->RegisterComponent();
        C->SetWorldLocation(P); C->SetIntensityUnits(ELightUnits::Candelas); C->SetIntensity(Intensity*2); C->SetAttenuationRadius(Radius); C->SetLightColor(Color); C->SetCastShadows(false);
    };
    auto Item=[&](int Id,FVector P,bool Released,ADiggableRock* Rock=nullptr)
    {
        auto* A=GetWorld()->SpawnActor<AMiningCollectible>(P,FRotator::ZeroRotator);
        A->BuildModel(Id); A->HostRock=Rock; A->BuriedTransform=A->GetActorTransform();
        A->bReleased=Released; A->Exposure=Released?1:0;
        if(Released) A->Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        A->Tags.Add(TEXT("CollectionExpansionItem")); Items.Add(A); Spawned.Add(A); return A;
    };
    auto Effect=[&](int Kind,FVector P,AMiningCollectible* Source=nullptr)
    {
        auto* A=GetWorld()->SpawnActor<AMiningCollectionEffects>(P,FRotator::ZeroRotator); A->Initialize(Kind,Source); Spawned.Add(A); return A;
    };

    // A connected north wing. The map upgrade opens the existing wall at the gallery entrance.
    Solid(FVector(375,3100,-40),FVector(3750,3200,80),Dark);
    Solid(FVector(375,4700,430),FVector(3750,70,860),Dark);
    Solid(FVector(-1500,3100,430),FVector(70,3200,860),Dark);
    Solid(FVector(2250,3100,430),FVector(70,3200,860),Dark);
    Solid(FVector(375,3100,900),FVector(3750,3200,60),Dark);
    for(int Bay=0;Bay<4;++Bay)
    {
        const int Id=Bay==0?4:Bay==1?8:Bay==2?9:10;
        const float Y=2000+Bay*700;
        const bool Cave=Id==8,Flower=Id==9;
        const float SoilHeight=Flower?260:Id==8?420:560;
        const FTransform Frame(FRotator::ZeroRotator,FVector(1010,Y,SoilHeight*.5f));
        auto* Rock=GetWorld()->SpawnActorDeferred<ADiggableRock>(ADiggableRock::StaticClass(),Frame);
        Rock->RockSize=FVector(1100,600,SoilHeight);
        Rock->RockMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionEarth"));
        Rock->Tags.Add(TEXT("CollectionExpansionSoil"));
        if(Cave)
        {
            Rock->FogChamber->bEnabled=false; Rock->FogChamber->RoomSize=FVector(64,64,106);
            Rock->FogChamber->RoomOffset=FVector(-250,0,-126); Rock->FogChamber->FogMaterial=nullptr;
        }
        Rock->FinishSpawning(Frame); Spawned.Add(Rock);
        if(Cave)
        {
            // Fog rooms require 100 cm minimum. Author this can-sized pocket
            // directly so its actual collision matches the MainGame cut.
            using namespace UE::Geometry;
            FDynamicMesh3 Mesh=Rock->RockMesh->GetDynamicMesh()->GetMeshRef();
            FMinimalBoxMeshGenerator Box;Box.Box=FOrientedBox3d(FVector3d(-250,0,-126),FVector3d(32,32,53));Box.Generate();
            FDynamicMesh3 Inside(&Box);Inside.ReverseOrientation();
            FDynamicMeshEditor Edit(&Mesh);FMeshIndexMappings Mapping;Edit.AppendMesh(&Inside,Mapping);
            auto* Rest=NewObject<UDynamicMesh>(Rock);Rest->SetMesh(MoveTemp(Mesh));Rock->BakeAuthoredMesh(Rest);
        }
        auto* A=Item(Id,FVector(Flower?665:Id==10?900:760,Y,Flower?175:Cave?80:190),Cave,Rock);
        Solid(FVector(1010,Y-330,280),FVector(1100,35,560),Dark);
        MakeProp(FVector(390,Y,3),FVector(12,570,4),Gold);
        Sign(FString::Printf(TEXT("%02d  /  %s"),Id+1,Id==4?TEXT("MORE THAN TWO"):Id==8?TEXT("COLA SPRING"):Id==9?TEXT("DEEP ROOTS"):TEXT("LITTLE LIGHT")),FVector(420,Y,650),26);
        Lamp(FVector(60,Y,620),Cream,650,1100);
        if(Id==4)
        {
            auto* Numbers=GetWorld()->SpawnActor<AMiningNumberClues>();Spawned.Add(Numbers);
            Numbers->Initialize(Rock,A->GetActorLocation(),FVector(460,Y,190),FVector::ForwardVector);
        }
        else if(Id==8)
        {
            // A continuous vertical bore connects the underground cavity to the soil surface.
            for(float Z=120;Z<=SoilHeight+15;Z+=14) Rock->MineSphere(FVector(754,Y,Z),15);
            auto* Jet=Effect(0,A->GetActorLocation()+FVector(-6,0,38),A);
            Jet->FlowHeight=SoilHeight-A->GetActorLocation().Z+135;
            Jet->Tags.Add(TEXT("CollectionVerticalColaBore"));
            Lamp(FVector(760,Y,185),FLinearColor(1,.49,.18),90,300);
        }
        else if(Id==10)
        {
            auto* Trail=Effect(2,FVector(442,Y,190),A);Trail->TrailEnd=FVector(458,0,0);
            auto* Glow=Effect(4,A->GetActorLocation(),A);Glow->AttachToActor(A,FAttachmentTransformRules::KeepWorldTransform);
            // The swarm owns the local light, so collection stops light and particles together.
        }
    }
    Solid(FVector(1010,4430,280),FVector(1100,35,560),Dark);
    for(int I=0;I<3;++I)
    {
        const float Y=1880+I*490;
        Solid(FVector(-690,Y,52),FVector(190,190,104),Dark);
        MakeProp(FVector(-690,Y,107),FVector(196,196,6),Gold);
        auto* A=Item(5+I,FVector(-690,Y,I==0?139:167),true);
        A->SetActorRotation(FRotator(0,180,0));
        Sign(I==0?TEXT("06 / TAPE"):I==1?TEXT("07 / NOTE"):TEXT("08 / FRAME"),FVector(-580,Y,95),17);
        Lamp(FVector(-500,Y,400),Cream,350,650);
    }
    Sign(TEXT("NEW FINDS   >"),FVector(-1100,1530,300),32,FRotator(0,-90,0));

    // An enclosed theatre, separated from excavation bays by a solid side wall.
    Solid(FVector(130,3960,390),FVector(50,1400,780),Dark);
    Solid(FVector(-650,4650,390),FVector(1550,60,780),Dark);
    Solid(FVector(-650,4300,24),FVector(1450,650,48),FLinearColor(.17,.065,.035));
    Solid(FVector(-650,3930,8),FVector(900,100,16),Dark);
    Solid(FVector(-650,3990,16),FVector(900,100,32),Dark);
    auto* Stage=Effect(3,FVector(-650,3300,0));
    for(int Side:{-1,1})
    {
        auto* Curtain=MakeProp(FVector(-650+Side*270,3300,300),FVector(540,20,600),Red);
        Curtain->Tags.Add(TEXT("CollectionRedCurtain")); Stage->Curtains.Add(Curtain);
        // Visible pleats travel with the curtain as it opens.
        for(int J=0;J<12;++J)
        {
            auto* Pleat=MakeProp(FVector(-650+Side*270-247+J*45,3284,300),FVector(20,24,590),FLinearColor(.52,.015,.035),true);
            Pleat->AttachToActor(Curtain,FAttachmentTransformRules::KeepWorldTransform);
        }
        Solid(FVector(-650+Side*700,3300,350),FVector(70,70,700),Gold);
        auto* Cannon=MakeProp(FVector(-650+Side*480,3950,90),FVector(35,35,120),Gold);
        CastChecked<AStaticMeshActor>(Cannon)->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder")));
        Cannon->SetActorRotation(FRotator(Side*35,0,0)); Cannon->Tags.Add(TEXT("CollectionRibbonCannon"));
        Solid(FVector(-650+Side*480,3950,55),FVector(90,90,15),Dark);
        const FVector LightPosition(-650+Side*380,4050,700);
        auto* Fixture=MakeProp(LightPosition,FVector(65,65,40),Dark); Fixture->Tags.Add(TEXT("CollectionStageLight"));
        CastChecked<AStaticMeshActor>(Fixture)->GetStaticMeshComponent()->SetCastShadow(false);
        auto* Spot=NewObject<USpotLightComponent>(Fixture); Fixture->AddInstanceComponent(Spot); Spot->SetupAttachment(Fixture->GetRootComponent()); Spot->RegisterComponent();
        Spot->SetWorldLocation(LightPosition-FVector(0,0,35)); Spot->SetWorldRotation((FVector(-650,4280,50)-LightPosition).Rotation());
        Spot->SetIntensityUnits(ELightUnits::Candelas); Spot->SetIntensity(1800); Spot->SetAttenuationRadius(1400); Spot->SetOuterConeAngle(37); Spot->SetInnerConeAngle(20); Spot->SetLightColor(Side<0?FLinearColor(1,.72,.38):FLinearColor(.42,.65,1));
    }
    MakeProp(FVector(-650,3300,650),FVector(1470,50,100),Red);
    Sign(TEXT("A MOMENT FOR YOU"),FVector(-650,3270,640),30,FRotator(0,-90,0));
    Sign(TEXT("CELEBRATE"),FVector(-650,4610,420),55,FRotator(0,-90,0));
    auto* Ribbon=Item(11,FVector(-650,4230,57),true);
    Ribbon->SetActorRotation(FRotator(0,30,0));
    Lamp(FVector(-650,4250,390),FLinearColor(1,.65,.3),350,850);
    Lamp(FVector(-650,3120,440),Cream,300,700);
}
