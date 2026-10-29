#include "MiningRoomEscape.h"
#include "MiningCollection.h"
#include "MiningCollectionEffects.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void AMiningRoomEscape::BuildExitStage()
{
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder"));
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionTint"));
    if(!Base)Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    auto Material=[&](FLinearColor Tint)
    {
        auto* M=UMaterialInstanceDynamic::Create(Base,this);M->SetVectorParameterValue(TEXT("Tint"),Tint);return M;
    };
    auto* Red=Material(FLinearColor(.38f,.008f,.025f));
    auto* Pleat=Material(FLinearColor(.6f,.018f,.035f));
    auto* Gold=Material(FLinearColor(.65f,.38f,.07f));
    auto* Dark=Material(FLinearColor(.035f,.025f,.025f));
    auto Shape=[&](FVector P,FVector Size,UMaterialInterface* M,FName Tag)
    {
        auto* C=NewObject<UStaticMeshComponent>(this);AddInstanceComponent(C);C->SetupAttachment(RootComponent);
        C->SetStaticMesh(Cube);C->SetMaterial(0,M);C->SetMobility(EComponentMobility::Movable);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->ComponentTags.Add(Tag);
        C->RegisterComponent();C->SetWorldLocation(P);C->SetWorldScale3D(Size/100);return C;
    };
    const FVector Deck(31900+SourceLevelOffset.X,ComputerEntry.Y,9900+SourceLevelOffset.Z);
    StageCelebration=GetWorld()->SpawnActor<AMiningCollectionEffects>(Deck-FVector(900,0,0),FRotator(0,-90,0));
    StageCelebration->CelebrationCue=TEXT("S_TVEscapeVictory");
    StageCelebration->SetOwner(this);StageCelebration->SetActorScale3D(FVector(1.5));
    StageCelebration->bSeparateStageTriggers=true;StageCelebration->CurtainSlideDistance=530;StageCelebration->CelebrationForward=440;
    StageCelebration->Initialize(3,nullptr,128);StageCelebration->SetActorTickEnabled(false);
    GeneratedActors.Add(StageCelebration);
    auto* ExposureBounds=NewObject<UBoxComponent>(this);AddInstanceComponent(ExposureBounds);ExposureBounds->SetupAttachment(RootComponent);
    ExposureBounds->SetBoxExtent(FVector(1500,1300,950));ExposureBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ExposureBounds->SetCollisionResponseToAllChannels(ECR_Ignore);ExposureBounds->SetGenerateOverlapEvents(false);
    ExposureBounds->RegisterComponent();ExposureBounds->SetWorldLocation(Deck+FVector(0,0,450));
    auto* Exposure=NewObject<UPostProcessComponent>(this);AddInstanceComponent(Exposure);Exposure->SetupAttachment(ExposureBounds);
    Exposure->bUnbound=false;Exposure->Priority=21;Exposure->BlendRadius=100;
    Exposure->Settings.bOverride_AutoExposureMethod=true;Exposure->Settings.AutoExposureMethod=AEM_Manual;
    Exposure->Settings.bOverride_AutoExposureBias=true;Exposure->Settings.AutoExposureBias=-1.5f;
    Exposure->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Exposure->Settings.AutoExposureApplyPhysicalCameraExposure=false;
    Exposure->Settings.bOverride_BloomIntensity=true;Exposure->Settings.BloomIntensity=.1f;
    Exposure->Settings.bOverride_MotionBlurAmount=true;Exposure->Settings.MotionBlurAmount=0;Exposure->RegisterComponent();
    // Reuse collection theatre opening: each leaf and its pleats move as one group.
    const FVector Curtain(31070+SourceLevelOffset.X,ComputerEntry.Y,10470+SourceLevelOffset.Z);
    for(float Side:{-1.f,1.f})
    {
        auto* Leaf=GetWorld()->SpawnActor<AActor>(Curtain+FVector(0,Side*450,0),FRotator::ZeroRotator);
        auto* LeafRoot=NewObject<USceneComponent>(Leaf);Leaf->SetRootComponent(LeafRoot);Leaf->AddInstanceComponent(LeafRoot);LeafRoot->RegisterComponent();Leaf->SetActorLocation(Curtain+FVector(0,Side*450,0));
        Leaf->SetOwner(this);GeneratedActors.Add(Leaf);StageCelebration->Curtains.Insert(Leaf,0);
        Shape(Curtain+FVector(0,Side*450,0),FVector(28,900,1140),Red,TEXT("EscapeStageCurtain"))->AttachToComponent(LeafRoot,FAttachmentTransformRules::KeepWorldTransform);
        for(int J=0;J<14;++J)
            Shape(Curtain+FVector(-20,Side*450-420+J*64,0),FVector(35,25,1120),Pleat,TEXT("EscapeStageCurtain"))->AttachToComponent(LeafRoot,FAttachmentTransformRules::KeepWorldTransform);
        Shape(Curtain+FVector(0,Side*930,0),FVector(65,65,1200),Gold,TEXT("EscapeStageFrame"));
        const FVector LampPosition=Deck+FVector(250,Side*580,1050);
        // Keep the stage lighting without the floating placeholder lamp boxes.
        auto* Spot=NewObject<USpotLightComponent>(this);AddInstanceComponent(Spot);Spot->SetupAttachment(RootComponent);
        Spot->SetIntensityUnits(ELightUnits::Candelas);Spot->SetIntensity(1200);
        Spot->SetAttenuationRadius(2500);Spot->SetOuterConeAngle(44);Spot->SetInnerConeAngle(25);
        Spot->SetLightColor(Side<0?FLinearColor(1,.72f,.38f):FLinearColor(.42f,.65f,1));
        Spot->SetCastShadows(false);Spot->RegisterComponent();Spot->SetWorldLocation(LampPosition-FVector(0,0,40));
        Spot->SetWorldRotation((Deck+FVector(0,0,35)-LampPosition).Rotation());
    }
    for(int32 I=0;I<3;++I)
    {
        const FTransform T=StageCelebration->GetActorTransform();
        const FVector Muzzle=AMiningCollectionEffects::WelcomeCannonMuzzle(I);
        const FVector Axis=AMiningCollectionEffects::WelcomeCannonDirection(I);
        auto* Barrel=Shape(T.TransformPosition(Muzzle-Axis*60),FVector(52,52,180),Gold,TEXT("CollectionRibbonCannon"));
        Barrel->SetStaticMesh(Cylinder);Barrel->SetWorldRotation(FRotationMatrix::MakeFromZ(T.TransformVectorNoScale(Axis)).Rotator());
        FVector BasePosition=Muzzle-Axis*120;
        const float StandHeight=BasePosition.Z;BasePosition.Z=StandHeight*.5f;
        Shape(T.TransformPosition(BasePosition),FVector(135,135,StandHeight*T.GetScale3D().Z+8),Dark,TEXT("EscapeStageCannonBase"));
    }
    Shape(Curtain+FVector(0,0,610),FVector(60,1940,140),Red,TEXT("EscapeStageCurtain"));
    auto* CurtainLight=NewObject<UPointLightComponent>(this);AddInstanceComponent(CurtainLight);CurtainLight->SetupAttachment(RootComponent);
    CurtainLight->SetIntensity(120000);CurtainLight->SetAttenuationRadius(1900);CurtainLight->SetLightColor(FLinearColor(1,.5f,.3f));
    CurtainLight->SetCastShadows(false);CurtainLight->RegisterComponent();CurtainLight->SetWorldLocation(Curtain+FVector(-450,0,200));
    // Carry light through the exit corridor after leaving the spotlit stage.
    for(int I=0;I<6;++I)
    {
        auto* ExitLight=NewObject<UPointLightComponent>(this);AddInstanceComponent(ExitLight);ExitLight->SetupAttachment(RootComponent);
        ExitLight->SetIntensityUnits(ELightUnits::Candelas);ExitLight->SetIntensity(I==1?220.f:400.f);ExitLight->SetAttenuationRadius(1700);
        ExitLight->SetLightColor(FLinearColor(.84f,.88f,1));ExitLight->SetCastShadows(false);ExitLight->RegisterComponent();
        ExitLight->SourceRadius=60;ExitLight->SoftSourceRadius=90;
        ExitLight->SetWorldLocation(Deck+FVector(I==1?1050.f:500.f+I*750.f,0,230));
    }
    // The collectible leaves the middle cannon in the opening salvo.
    StageRibbon=GetWorld()->SpawnActor<AMiningCollectible>(Deck+FVector(0,0,14),FRotator(0,30,0));
    StageRibbon->BuildModel(11);StageRibbon->Exposure=1;StageRibbon->SetActorHiddenInGame(true);
    StageRibbon->Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);StageRibbon->SetOwner(this);
    StageRibbon->Tags.Add(TEXT("CardEscapeStageRibbon"));GeneratedActors.Add(StageRibbon);
}

void AMiningRoomEscape::TickExitStage()
{
    if(!StageCelebration || !StageRibbon)return;
    StageCelebration->SetActorTickEnabled(bEnteredComputer || Phase==EMiningEscapePhase::Escaped);
    if(!StageRibbon->bReleased)
    {
        const float T=StageCelebration->GetCelebrationAge();
        if(T<0 || !StageCelebration->bStageActive)return;
        StageRibbon->SetActorHiddenInGame(false);
        constexpr float LandingTime=.9f;
        const float Flight=FMath::Min(T,LandingTime);
        const FVector P=AMiningCollectionEffects::WelcomeCannonMuzzle(2)+FVector(0,-260,180)*Flight
            +FVector(0,0,-(145.f+180.f*LandingTime-9.4f)/(LandingTime*LandingTime)*Flight*Flight);
        StageRibbon->SetActorLocation(StageCelebration->GetActorTransform().TransformPosition(P));
        StageRibbon->SetActorRotation(FRotator(T<LandingTime?T*170:0,Flight*45,T<LandingTime?T*210:0));
        if(T<LandingTime)return;
        StageRibbon->bReleased=true;
        StageRibbon->Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }
    if(Phase!=EMiningEscapePhase::Escaped)return;
    if(!StageCollection.IsValid())
    {
        if(TActorIterator<AMiningCollectionField> It(GetWorld());It)StageCollection=*It;
        if(!StageCollection.IsValid())
        {
            auto* Field=GetWorld()->SpawnActorDeferred<AMiningCollectionField>(AMiningCollectionField::StaticClass(),FTransform::Identity,this);
            Field->bUseAuthoredPlacements=true;Field->bAreaManaged=true;Field->bSharedAbilities=true;
            Field->bShowExplorationHUD=false;Field->CatalogueKey=EKeys::C;
            Field->FinishSpawning(FTransform::Identity);StageCollection=Field;bOwnsStageCollection=true;GeneratedActors.Add(Field);
        }
    }
    if(StageCollection->RegisterExternalCollectible(StageRibbon))
    {
        if(!StageCollection->IsAreaActive())StageCollection->SetAreaActive(true);
        // The room does not announce unrelated collection-field tutorial text.
        if(StageCollection->ToastRemaining>5)StageCollection->ToastRemaining=0;
    }
}
