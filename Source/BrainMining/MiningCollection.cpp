#include "MiningCollection.h"
#include "MiningSaveSubsystem.h"
#include "MiningWalletSubsystem.h"
#include "MiningGUIWidgets.h"
#include "Subtitles/SubtitleSubsystem.h"
#include "MiningHapticsSubsystem.h"
#include "MiningExperienceSubsystem.h"
#include "Subtitles/SubtitleLibrary.h"
#include "Engine/GameInstance.h"
#include "MiningInteractionTrace.h"
#include "Showcase/MiningTreasure.h"
#include "BrainMiningPlayerController.h"
#include "SMiningCollectionView.h"
#include "MiningCollectionWidget.h"
#include "MiningComponent.h"
#include "MiningAudioComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "DiggableRock.h"
#include "HeavenToilet.h"
#include "NiagaraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PostProcessComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/HUD.h"
#include "Materials/MaterialInstanceDynamic.h"

AMiningCollectionGameMode::AMiningCollectionGameMode() { HUDClass=AHUD::StaticClass(); }
AMiningCollectionField::AMiningCollectionField()
{
    PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bTickEvenWhenPaused=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Capture=CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CatalogueCamera")); Capture->SetupAttachment(RootComponent);
    Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false;
    Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->CaptureSource=ESceneCaptureSource::SCS_SceneColorHDR;
    Capture->FOVAngle=40;
    HighlightPostProcess=CreateDefaultSubobject<UPostProcessComponent>(TEXT("CollectionSilhouette"));
    HighlightPostProcess->SetupAttachment(RootComponent); HighlightPostProcess->bUnbound=true; HighlightPostProcess->Priority=100;
    Capture->ShowFlags.SetAtmosphere(false); Capture->ShowFlags.SetFog(false); Capture->ShowFlags.SetMotionBlur(false);
    Capture->PostProcessSettings.bOverride_AutoExposureMinBrightness=true; Capture->PostProcessSettings.AutoExposureMinBrightness=1;
    Capture->PostProcessSettings.bOverride_AutoExposureMaxBrightness=true; Capture->PostProcessSettings.AutoExposureMaxBrightness=1;
    Capture->PostProcessSettings.bOverride_AutoExposureBias=true; Capture->PostProcessSettings.AutoExposureBias=0;
}
bool AMiningCollectionField::InitializePlayer()
{
    Player=GetWorld()->GetFirstPlayerController();
    if(!Player||!Player->GetPawn()) return false;
    auto* Input=Cast<UEnhancedInputComponent>(Player->GetPawn()->InputComponent);
    Mining=Player->GetPawn()->FindComponentByClass<UMiningComponent>();
    if(!Input||!Mining||!Player->GetLocalPlayer()) return false;
    auto* Subsystem=Player->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if(!Subsystem) return false;
    CatalogueKey=EKeys::C;
    CollectionContext=NewObject<UInputMappingContext>(this);
    auto Bind=[&](FKey Key,auto Callback)
    {
        auto* Action=NewObject<UInputAction>(this); Action->bConsumeInput=true; Actions.Add(Action);
        CollectionContext->MapKey(Action,Key); Input->BindAction(Action,ETriggerEvent::Started,this,Callback);
    };
    Bind(EKeys::E,&AMiningCollectionField::CollectTarget);
    Bind(CatalogueKey,&AMiningCollectionField::ToggleCatalogue);
    if(!bUseAuthoredPlacements) Bind(EKeys::R,&AMiningCollectionField::ResetCollection);
    Subsystem->AddMappingContext(CollectionContext,50);
    if(!bAreaManaged && !bCatalogueOnly)
    {
        if (auto* Controller = Cast<ABrainMiningPlayerController>(Player)) Controller->SetMiningGUIEnabled(false);
        if(auto* Move=Player->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>()) { Move->FlightSettings.bEnabled=true; Move->FlightSettings.bEnableHeat=false; }
    }
    StartTransform=Player->GetPawn()->GetActorTransform();
    if (!bSharedAbilities) { Mining->Reach=400; Mining->Radius=115; Mining->DownwardOffset=25; }
    // The field owns fall recovery so inventory and excavation reset together.
    Mining->FallResetDistance=0;
    Inventory.Init(0,FMath::Max(4,CatalogueIds().IsEmpty()?4:CatalogueIds().Last()+1));
    if(auto* Edge=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionSilhouette"))) HighlightPostProcess->Settings.AddBlendable(Edge,1);
    if(ActorHasTag(TEXT("BM_MainGame_Collections")) || ActorHasTag(TEXT("BM_MainGame_SystemIntegration")) || bCatalogueOnly)
        if(auto* Session=GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>(); Session && !Session->CampaignCollection.IsEmpty())Inventory=Session->CampaignCollection;
    Inventory.SetNumZeroed(AMiningCollectible::ItemTypeCount);
    if(!bCatalogueOnly) SpawnField();
    PreviewTexture=NewObject<UTextureRenderTarget2D>(this); PreviewTexture->ClearColor=FLinearColor(0,0,0,1);
    PreviewTexture->InitAutoFormat(1536,1024); Capture->TextureTarget=PreviewTexture;
    if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/UI/Redesign/M_CollectionPreview.M_CollectionPreview")))
    {PreviewMaterial=UMaterialInstanceDynamic::Create(Material,this);PreviewMaterial->SetTextureParameterValue(TEXT("Preview"),PreviewTexture);}
    Preview=GetWorld()->SpawnActor<AMiningCollectible>(FVector(0,0,-10000),FRotator::ZeroRotator);
    Preview->Tags.Add(TEXT("CollectionPreview"));
    Preview->BuildModel(0); Preview->SetActorHiddenInGame(true); Capture->ShowOnlyActors.Add(Preview);
    auto* PresentationSession=GetGameInstance()?GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>():nullptr;
    for(const auto& Placement:AuthoredPlacements)if(Placement.ItemId==0)
    {
        auto* Toilet=Cast<AHeavenToilet>(Placement.PresentationActor);if(!IsValid(Toilet) || !PresentationSession)continue;
        // Retain asset references and scale across map travel, rather than an
        // actor in the departing world. Credits use the same presentation.
        PresentationSession->CampaignToiletScale=Toilet->GetActorScale3D();
        PresentationSession->CampaignToiletBody=Toilet->Body->GetStaticMesh();
        PresentationSession->CampaignToiletLid=Toilet->Lid->GetStaticMesh();
        PresentationSession->CampaignToiletBodyMaterials.Reset();
        for(auto* Material:Toilet->Body->GetMaterials())PresentationSession->CampaignToiletBodyMaterials.Add(Material);
        PresentationSession->CampaignToiletLidMaterials.Reset();
        for(auto* Material:Toilet->Lid->GetMaterials())PresentationSession->CampaignToiletLidMaterials.Add(Material);
        break;
    }
    if(bUseAuthoredPlacements || bCatalogueOnly)
    {
        PreviewToilet=GetWorld()->SpawnActor<AHeavenToilet>(FVector(0,0,-10065),FRotator(0,60,0));
        if(PresentationSession)
        {
            PreviewToilet->SetActorScale3D(PresentationSession->CampaignToiletScale);
            if(PresentationSession->CampaignToiletBody)PreviewToilet->Body->SetStaticMesh(PresentationSession->CampaignToiletBody);
            if(PresentationSession->CampaignToiletLid)PreviewToilet->Lid->SetStaticMesh(PresentationSession->CampaignToiletLid);
            for(int32 I=0;I<PresentationSession->CampaignToiletBodyMaterials.Num();++I)PreviewToilet->Body->SetMaterial(I,PresentationSession->CampaignToiletBodyMaterials[I]);
            for(int32 I=0;I<PresentationSession->CampaignToiletLidMaterials.Num();++I)PreviewToilet->Lid->SetMaterial(I,PresentationSession->CampaignToiletLidMaterials[I]);
        }
        FBox ToiletBounds(ForceInit);
        for(auto* Part:{PreviewToilet->Body.Get(),PreviewToilet->Lid.Get()})
            if(Part->GetStaticMesh())ToiletBounds+=Part->GetStaticMesh()->GetBoundingBox().TransformBy(Part->GetComponentTransform().GetRelativeTransform(PreviewToilet->GetActorTransform()));
        ToiletPreviewCenter=ToiletBounds.IsValid?ToiletBounds.GetCenter():FVector::ZeroVector;
        PreviewToilet->SetActorEnableCollision(false);PreviewToilet->SetActorTickEnabled(false);
        PreviewToilet->WaterJet->DeactivateImmediate();PreviewToilet->OpeningSplash->DeactivateImmediate();
        PreviewToilet->SetActorHiddenInGame(true);Capture->ShowOnlyActors.Add(PreviewToilet);
    }
    for(int i=0;i<2;++i)
    {
        auto* Light=NewObject<UPointLightComponent>(this); AddInstanceComponent(Light); Light->RegisterComponent();
        Light->SetWorldLocation(FVector(-160,i==0?-160:160,-9850)); Light->SetIntensity(i==0?1650:1100);
        Light->SetAttenuationRadius(700); Light->SetLightColor(i==0?FLinearColor(1,.84f,.65f):FLinearColor(.45f,.78f,1));
    }
    if(GEngine&&GEngine->GameViewport)
    {
        SAssignNew(View,SMiningCollectionView).Owner(this); View->SetVisibility(EVisibility::HitTestInvisible);
        GEngine->GameViewport->AddViewportWidgetContent(View.ToSharedRef(),30);
    }
    if(auto* Class=LoadClass<UMiningCollectionWidget>(nullptr,TEXT("/Game/CollectionPrototype/UI/WBP_CollectionInspect.WBP_CollectionInspect_C")))
    {
        CatalogueWidget=CreateWidget<UMiningCollectionWidget>(Player,Class);
        if(CatalogueWidget)
        {
            CatalogueWidget->SetCollection(this);CatalogueWidget->AddToViewport(80);
            CatalogueWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    Toast=bUseAuthoredPlacements?FString::Printf(TEXT("沿水管或矿道寻找藏品，挖开覆盖层后瞄准按 E 收集；%s 打开图鉴。"),*CatalogueKey.GetDisplayName().ToString()):TEXT("先试试左侧已露出的试管架：瞄准它，按 E 收集"); ToastRemaining=10;
    if(bExpandedCollection && !bUseAuthoredPlacements) { Toast=TEXT("右侧通道通往新展区：陈列藏品、四种挖掘线索和红幕布舞台"); ToastRemaining=12; }
    if(bAreaManaged) SetAreaActive(false);
    return true;
}

void AMiningCollectionField::SetAreaActive(bool bActive)
{
    if(!bActive && bCatalogueOpen) SetCatalogueOpen(false);
    bAreaActive=bActive;
    if(Player && Player->GetLocalPlayer() && CollectionContext)
        if(auto* Subsystem=Player->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            if(bActive) Subsystem->AddMappingContext(CollectionContext,50);
            else Subsystem->RemoveMappingContext(CollectionContext);
        }
    if(View) View->SetVisibility(bActive ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
    HighlightPostProcess->bEnabled=bActive;
    if(!bActive && IsValid(Target)) { Target->SetHighlighted(false); Target=nullptr; }
}

void AMiningCollectionField::SpawnField()
{
    if(bUseAuthoredPlacements)
    {
        for(const auto& Placement:AuthoredPlacements)
        {
            if(Placement.ItemId<0 || Placement.ItemId>=AMiningCollectible::ItemTypeCount) continue;
            auto* Item=GetWorld()->SpawnActor<AMiningCollectible>(Placement.Transform.GetLocation(),Placement.Transform.Rotator());
            Item->Tags.Add(Placement.AreaTag);
            Item->BuildModel(Placement.ItemId);
            Item->SetActorScale3D(Placement.Transform.GetScale3D());
            if(IsValid(Placement.PresentationActor))
            {
                Item->PresentationActor=Placement.PresentationActor;
                Item->PresentationActor->SetOwner(Item);
                Item->Model->SetVisibility(false);
            }
            if(bBuriedWithClues && Placement.HostRock)
            {
                Item->HostRock=Placement.HostRock; Item->BuriedTransform=Item->GetActorTransform();
                if(Placement.ItemId==3 && Placement.HostRock->ActorHasTag(TEXT("BM_BombOnly")))Item->Tags.Add(TEXT("BM_BombGateReward"));
                if(Placement.ItemId==3) ResidentBird=Item;
                SpawnAuthoredClues(Placement);
            }
            else { Item->bReleased=true; Item->Exposure=1; Item->Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly); }
            Items.Add(Item); Spawned.Add(Item);
            SpawnAuthoredPresentation(Item,Placement);
        }
        return;
    }
    if(bDeepField) { SpawnDeepField(); if(bExpandedCollection) SpawnExpandedCollection(); RelocateGeneratedField(); return; }
    ActiveSeed=RandomSeed>0?RandomSeed:FMath::RandRange(1,MAX_int32-1);
    FRandomStream Random(ActiveSeed);
    int Ids[4]={0,1,2,3};
    for(int i=3;i>0;--i) Swap(Ids[i],Ids[Random.RandRange(0,i)]);
    auto* Tint=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionTint"));
    auto Prop=[&](FVector P,FVector Size,FLinearColor Color,bool Sphere=false)
    {
        auto* A=GetWorld()->SpawnActor<AStaticMeshActor>(P,FRotator::ZeroRotator); Spawned.Add(A);
        auto* C=A->GetStaticMeshComponent(); C->SetMobility(EComponentMobility::Movable);
        C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Sphere?TEXT("/Engine/BasicShapes/Sphere"):TEXT("/Engine/BasicShapes/Cube")));
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision); A->SetActorScale3D(Size/100);
        if(Tint) { auto* M=UMaterialInstanceDynamic::Create(Tint,this); M->SetVectorParameterValue(TEXT("Tint"),Color); C->SetMaterial(0,M); }
        return A;
    };
    for(int Bay=0;Bay<4;++Bay)
    {
        const int Id=Ids[Bay]; const float Y=-780+Bay*520;
        const float Z=Id==3?550:210;
        FTransform RockTransform(FRotator::ZeroRotator,FVector(680,Y,Z));
        auto* Rock=GetWorld()->SpawnActorDeferred<ADiggableRock>(ADiggableRock::StaticClass(),RockTransform);
        Rock->RockSize=FVector(440,420,420);
        Rock->RockMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CollectionPrototype/Materials/M_CollectionEarth"));
        Rock->FinishSpawning(RockTransform); Spawned.Add(Rock);
        const FVector Position(570+Random.FRandRange(-25,25),Y+Random.FRandRange(-65,65),Z-55+Random.FRandRange(-20,20));
        auto* Item=GetWorld()->SpawnActor<AMiningCollectible>(Position,FRotator::ZeroRotator);
        Item->BuildModel(Id); Item->HostRock=Rock; Item->BuriedTransform=Item->GetActorTransform(); Items.Add(Item); Spawned.Add(Item);
        // Each clue is linked to the shuffled item and protrudes from the front face.
        if(Id==0)
        {
            Prop(FVector(442,Position.Y,Z+45),FVector(80,17,17),FLinearColor(.35f,.49f,.49f));
            Prop(FVector(408,Position.Y,Z+21),FVector(17,17,60),FLinearColor(.35f,.49f,.49f));
            for(int i=0;i<4;++i) Prop(FVector(408,Position.Y,Z-22-i*20),FVector(7,7,12),FLinearColor(.08f,.65f,.85f),true);
        }
        else if(Id==1)
        {
            Prop(FVector(457,Position.Y,Z-35),FVector(7,56,42),FLinearColor(.022f,.017f,.013f),true);
            for(int i=0;i<7;++i)
            {
                const float X=360+i*14,Ay=Position.Y+FMath::Sin(i*1.3f)*12;
                for(int j=0;j<3;++j) Prop(FVector(X+j*4,Ay,8),FVector(5,4,4),FLinearColor(.06f,.025f,.012f),true);
            }
        }
        else if(Id==2)
        {
            const FLinearColor Colors[]={FLinearColor(.62f,.09f,.7f),FLinearColor(.04f,.66f,.45f),FLinearColor(.9f,.58f,.04f)};
            for(int i=0;i<12;++i) Prop(FVector(456,Position.Y+Random.FRandRange(-65,65),Z+Random.FRandRange(-95,65)),FVector(7,Random.FRandRange(10,30),Random.FRandRange(10,35)),Colors[i%3],true);
        }
        else
        {
            Prop(FVector(427,Position.Y,Z+35),FVector(75,105,18),FLinearColor(.25f,.12f,.045f));
            for(int i=0;i<7;++i) { auto* Twig=Prop(FVector(423,Position.Y-45+i*15,Z+49),FVector(65,6,7),FLinearColor(.43f,.26f,.08f)); Twig->SetActorRotation(FRotator(0,(i%2?1:-1)*25,0)); }
        }
    }
    auto* Tutorial=GetWorld()->SpawnActor<AMiningCollectible>(FVector(-300,-330,85),FRotator(0,-15,0));
    Tutorial->BuildModel(2); Tutorial->bReleased=true; Tutorial->Exposure=1;
    Tutorial->Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Items.Add(Tutorial); Spawned.Add(Tutorial);
    RelocateGeneratedField();
}

void AMiningCollectionField::RelocateGeneratedField()
{
    const FVector Offset=GetActorLocation();
    if(Offset.IsNearlyZero()) return;
    for(AActor* Actor:Spawned)
        if(IsValid(Actor) && !Actor->GetAttachParentActor()) Actor->AddActorWorldOffset(Offset,false,nullptr,ETeleportType::TeleportPhysics);
    for(AMiningCollectible* Item:Items) if(IsValid(Item)) Item->BuriedTransform.AddToTranslation(Offset);
    for(auto& Ant:Ants) Ant.Start+=Offset;
    for(auto& Trail:AntTrails) { Trail.Start+=Offset;Trail.End+=Offset;Trail.ObservedCuts=INDEX_NONE; }
    for(auto& Clue:Clues) Clue.Support+=Offset;
}

void AMiningCollectionField::Tick(float Dt)
{
    Super::Tick(Dt);
    if(!bInitialized) { bInitialized=InitializePlayer(); return; }
    if(bAreaManaged && !bAreaActive && !bCatalogueOpen) return;
    ToastRemaining=FMath::Max(0.f,ToastRemaining-Dt);
    if(!Player||!Player->GetPawn()) return;
    if(bCatalogueOpen)
    {
        PreviewAnimationTime+=Dt;PreviewCaptureAge+=Dt;
        if(AMiningCollectible::IsMineralReward(SelectedId) && SelectedId!=14 && PreviewCaptureAge>=1.f/30.f)
        {Preview->AnimateEmergence(PreviewAnimationTime);Capture->CaptureScene();PreviewCaptureAge=0;}
        else if(PreviewRefreshFrames>0) { Capture->CaptureScene(); --PreviewRefreshFrames; }
        return;
    }
    if(bDeepField || bBuriedWithClues) TickAmbient(Dt);
    if(!bUseAuthoredPlacements && Player->GetPawn()->GetActorLocation().Z<StartTransform.GetLocation().Z-2000) { ResetCollection(); return; }
    RefreshTarget();
    PollTime+=Dt; if(PollTime<.12f) return; PollTime=0;
    for(AMiningCollectible* Item:Items)
    {
        if(!IsValid(Item)||Item->bCollected||Item->bReleased||!Item->HostRock) continue;
        if(Item->LastObservedCuts==Item->HostRock->SuccessfulCuts) continue;
        Item->LastObservedCuts=Item->HostRock->SuccessfulCuts;
        Item->Exposure=Item->MeasureExposure();
        // Exposure triggers release; only released items become interaction targets.
        if(Item->CanRelease(AMiningCollectible::PickupExposure))
        {
            Item->Release();
            if(Item->bReleased) { Toast=AMiningCollectible::ItemName(Item->ItemId)+TEXT(" 已松动掉落，瞄准后按 E 收集"); ToastRemaining=5; }
        }
    }
}
void AMiningCollectionField::RefreshTarget()
{
    AMiningCollectible* NewTarget=nullptr;
    FVector P; FRotator R; Player->GetPlayerViewPoint(P,R);
    FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(CollectionAim),true,Player->GetPawn());
    if(MiningInteraction::Trace(GetWorld(),Hit,P,R,400,Q,[](const FHitResult& Candidate)
    {
        auto* Item=Cast<AMiningCollectible>(Candidate.GetActor());
        if(!Item && Candidate.GetActor()) Item=Cast<AMiningCollectible>(Candidate.GetActor()->GetOwner());
        return Item && Item->IsCollectible();
    }))
    {
        auto* Item=Cast<AMiningCollectible>(Hit.GetActor());
        if(!Item && Hit.GetActor()) Item=Cast<AMiningCollectible>(Hit.GetActor()->GetOwner());
        if(Item&&Item->IsCollectible()) NewTarget=Item;
    }
    if(Target!=NewTarget)
    {
        if(IsValid(Target)) Target->SetHighlighted(false);
        Target=NewTarget;
        if(IsValid(Target)) Target->SetHighlighted(true);
    }
}
bool AMiningCollectionField::RegisterExternalCollectible(AMiningCollectible* Item)
{
    if(!bInitialized || !IsValid(Item) || Item->ItemId<0 || Item->ItemId>=AMiningCollectible::ItemTypeCount)return false;
    Items.AddUnique(Item);
    if(Inventory.Num()<=Item->ItemId)Inventory.SetNumZeroed(Item->ItemId+1);
    return true;
}
void AMiningCollectionField::CollectTarget()
{
    TryCollectTarget();
}
bool AMiningCollectionField::TryCollectTarget()
{
    if(bCatalogueOpen||!Player || (bAreaManaged && !bAreaActive)) return false;
    RefreshTarget();
    if(!IsValid(Target)||!Target->IsCollectible()||!Inventory.IsValidIndex(Target->ItemId)) return false;
    const bool bFirstCollection=UniqueCount()==0;
    AMiningCollectible* Item=Target; Item->SetHighlighted(false); Item->bCollected=true;
    Item->Body->SetSimulatePhysics(false); Item->SetActorEnableCollision(false); Item->SetActorHiddenInGame(true);
    if(IsValid(Item->PresentationActor)) { Item->PresentationActor->Destroy(); Item->PresentationActor=nullptr; }
    ++Inventory[Item->ItemId];
    QueuePickup(Item->ItemId);
    GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>()->RecordCollectible(Item->ItemId);
    UMiningHapticsSubsystem::Emit(Player, EMiningHapticCue::Collectible);
    if(Player->GetPawn())
        if(auto* Audio=Player->GetPawn()->FindComponentByClass<UMiningAudioComponent>())Audio->PlayCollectible(Item->ItemId);
    // All collectibles, including the room-owned stage ribbon, reach this
    // successful pickup path. Queue the editable segment only after collection.
    USubtitleLibrary::PlaySubtitleSegment(this,FName(*FString::Printf(TEXT("收集物拾取_%02d"),Item->ItemId)));
    if(bFirstCollection)USubtitleLibrary::PlaySubtitleSegment(this,TEXT("首次收藏提示"));
    if(ActorHasTag(TEXT("BM_MainGame_Collections")) || ActorHasTag(TEXT("BM_MainGame_SystemIntegration")))if(auto* Session=GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>())Session->CampaignCollection=Inventory;
    SelectedId=Item->ItemId; Target=nullptr;
    Toast=FString::Printf(TEXT("已收集 · %s     [ %s ] 打开图鉴"),*AMiningCollectible::ItemName(Item->ItemId),*CatalogueKey.GetDisplayName().ToString()); ToastRemaining=5;
    return true;
}
int32 AMiningCollectionField::UniqueCount() const { int32 N=0; for(int32 Count:Inventory) N+=Count>0; return N; }
TArray<int32> AMiningCollectionField::CatalogueIds() const
{
    TArray<int32> Result;
    // MainGame's shared interaction field also accepts room-owned collectibles;
    // an empty spawn list must still show the full locked catalogue.
    if(bCatalogueOnly || (bUseAuthoredPlacements && AuthoredPlacements.IsEmpty())){for(int32 I=0;I<AMiningCollectible::ItemTypeCount;++I)Result.Add(I);return Result;}
    if(bUseAuthoredPlacements)
    {
        for(const auto& Placement:AuthoredPlacements)
            if(Placement.ItemId>=0 && Placement.ItemId<AMiningCollectible::ItemTypeCount) Result.AddUnique(Placement.ItemId);
        // The authored TV room owns this reward; show its locked entry before escaping.
        if(ActorHasTag(TEXT("BM_MainGame_Collections")))
            for(int32 Id=11;Id<AMiningCollectible::ItemTypeCount;++Id)Result.AddUnique(Id);
    }
    else for(int32 Id=0;Id<(bDeepField && bExpandedCollection?AMiningCollectible::WorldItemTypeCount:4);++Id) Result.Add(Id);
    for(const auto& Item:Items)if(IsValid(Item))Result.AddUnique(Item->ItemId);
    Result.Sort();
    return Result;
}
void AMiningCollectionField::ToggleCatalogue()
{
    // A pause-menu catalogue may be created and opened before its first tick.
    if(!bInitialized)bInitialized=InitializePlayer();
    if(bInitialized)SetCatalogueOpen(!bCatalogueOpen);
}
void AMiningCollectionField::SetCatalogueOpen(bool Open)
{
    if(Open==bCatalogueOpen||!Player||!Player->GetPawn()) return;
    bCatalogueOpen=Open;
    Mining->CancelMining(); Mining->SetComponentTickEnabled(!Open);
    if(Open)
    {
        if(IsValid(Target)) Target->SetHighlighted(false); Target=nullptr;
        Player->SetIgnoreLookInput(true); Player->SetIgnoreMoveInput(true);
        if(auto* Move=Player->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>()) { Move->ResetFlightInput(); Move->StopMovementImmediately(); }
        Player->GetPawn()->DisableInput(Player);
        Player->SetPause(true);
        FInputModeUIOnly Mode;
        if(CatalogueWidget)
        {
            if(View)View->SetVisibility(EVisibility::Collapsed);
            CatalogueWidget->SetVisibility(ESlateVisibility::Visible);
            Mode.SetWidgetToFocus(CatalogueWidget->TakeWidget());
        }
        else if(View){View->SetVisibility(EVisibility::Visible);Mode.SetWidgetToFocus(View);}
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);Player->SetInputMode(Mode);
        Player->bShowMouseCursor=true;
        if(!Inventory.IsValidIndex(SelectedId)||Inventory[SelectedId]==0) { SelectedId=INDEX_NONE; for(int i=0;i<Inventory.Num();++i) if(Inventory[i]>0) { SelectedId=i; break; } }
        if(SelectedId==INDEX_NONE){const auto Ids=CatalogueIds();if(!Ids.IsEmpty())SelectedId=Ids[0];}
        SelectItem(SelectedId);
        if(CatalogueWidget)CatalogueWidget->RevealCollection();
        if(UniqueCount()==0)if(auto* Subtitles=USubtitleSubsystem::Get(this))
            Subtitles->PlayInteractionLines(TEXT("EmptyCollection"),{FText::FromString(TEXT("挖掘就能找到灵感啦~"))});
    }
    else
    {
        if(auto* Subtitles=USubtitleSubsystem::Get(this))Subtitles->StopInteraction(TEXT("EmptyCollection"));
        Player->SetPause(false); Player->ResetIgnoreLookInput(); Player->ResetIgnoreMoveInput();
        Player->GetPawn()->EnableInput(Player); Player->bShowMouseCursor=false; Player->SetInputMode(FInputModeGameOnly());
        if(CatalogueWidget)CatalogueWidget->SetVisibility(ESlateVisibility::Collapsed);
        if(View) View->SetVisibility(EVisibility::HitTestInvisible);
        if(Preview) Preview->SetActorHiddenInGame(true);
        if(PreviewToilet)PreviewToilet->SetActorHiddenInGame(true);
    }
}
void AMiningCollectionField::SelectItem(int32 Id)
{
    SelectedId=Id;PreviewAnimationTime=0;
    if(bCatalogueOpen && AMiningCollectible::IsMineralReward(Id))
        if(auto* Subtitles=USubtitleSubsystem::Get(this))Subtitles->StopInteraction(TEXT("EmptyCollection"));
    if(!Preview) return;
    const bool Owned=Inventory.IsValidIndex(Id)&&Inventory[Id]>0;
    Preview->SetActorHiddenInGame(!Owned || (PreviewToilet && Id==0));
    if(PreviewToilet)PreviewToilet->SetActorHiddenInGame(!Owned || Id!=0);
    if(Owned) Preview->BuildModel(Id);
    ResetPreview();
    if(CatalogueWidget)CatalogueWidget->RefreshCollection();
}
void AMiningCollectionField::ResetPreview() { PreviewYaw=-30; PreviewPitch=-8; PreviewDistance=SelectedId==9?850:(SelectedId==0?480:330); RotatePreview(0,0); }
void AMiningCollectionField::RotatePreview(float Yaw,float Pitch)
{
    if(!Preview) return;
    PreviewYaw=FRotator::NormalizeAxis(PreviewYaw+Yaw); PreviewPitch=FMath::Clamp(PreviewPitch+Pitch,-80.f,80.f);
    Preview->SetActorRotation(FRotator(PreviewPitch,PreviewYaw,0));
    if(PreviewToilet)
    {
        const FQuat Rotation=FRotator(PreviewPitch,PreviewYaw+90,0).Quaternion();
        PreviewToilet->SetActorLocationAndRotation(FVector(0,0,-9990)-Rotation.RotateVector(ToiletPreviewCenter*PreviewToilet->GetActorScale3D()),Rotation);
    }
    Capture->SetWorldLocation(FVector(-PreviewDistance,0,SelectedId==9?-10125:-9990)); Capture->SetWorldRotation(FRotator(0,0,0));
    Capture->CaptureScene();
    PreviewRefreshFrames=3;
}
void AMiningCollectionField::ZoomPreview(float Steps) { PreviewDistance=FMath::Clamp(PreviewDistance-Steps*22,SelectedId==9?650.f:270.f,SelectedId==9?1200.f:550.f); RotatePreview(0,0); }
void AMiningCollectionField::ResetCollection()
{
    if(bUseAuthoredPlacements) return; // Campaign retries must never erase global inventory.
    if(!bInitialized||bCatalogueOpen) return;
    if(!bAreaManaged) Mining->ResetField();
    else Mining->CancelMining();
    Target=nullptr;
    for(AActor* A:Spawned) if(IsValid(A)) A->Destroy();
    Spawned.Reset(); Items.Reset(); Inventory.Init(0,FMath::Max(4,CatalogueIds().IsEmpty()?4:CatalogueIds().Last()+1)); SelectedId=INDEX_NONE;
    SpawnField();
    Toast=TEXT("已开始新一轮，收集进度清空；寻找墙面线索与新展区"); ToastRemaining=6;
}
void AMiningCollectionField::EndPlay(const EEndPlayReason::Type Reason)
{
    if(bCatalogueOpen) SetCatalogueOpen(false);
    if(CatalogueWidget){CatalogueWidget->RemoveFromParent();CatalogueWidget=nullptr;}
    if(Player&&Player->GetLocalPlayer()) if(auto* S=Player->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()) S->RemoveMappingContext(CollectionContext);
    if(View&&GEngine&&GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(View.ToSharedRef());
    View.Reset(); Super::EndPlay(Reason);
    for (AActor* Actor : Spawned) if (IsValid(Actor)) Actor->Destroy();
    Spawned.Reset();
    if (IsValid(Preview)) Preview->Destroy();
    if(IsValid(PreviewToilet))PreviewToilet->Destroy();PreviewToilet=nullptr;
    Preview = nullptr;
}
