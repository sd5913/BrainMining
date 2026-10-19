#include "StartRoom/MiningStartRoom.h"
#include "MiningInteractionTrace.h"
#include "MiningAudioComponent.h"
#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningToolEffectsComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "StartRoom/MiningOpening.h"
#include "Subtitles/SubtitleLibrary.h"
#include "DiggableRock.h"
#include "Components/LightComponent.h"

AMiningStarterPickup::AMiningStarterPickup()
{
    InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
    SetRootComponent(InteractionBox);
    InteractionBox->SetBoxExtent(FVector(48, 38, 22));
    InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    InteractionBox->SetGenerateOverlapEvents(false);
}

void AMiningStarterPickup::Collect()
{
    bCollected = true;
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    TArray<AActor*> VisualChildren;
    GetAttachedActors(VisualChildren, true, true);
    for (AActor* Child : VisualChildren)
    {
        Child->SetActorHiddenInGame(true);
        Child->SetActorEnableCollision(false);
    }
}

AMiningStartRoom::AMiningStartRoom()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.TickInterval=.1f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}
void AMiningStartRoom::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!Player.IsValid() || bHeadlampUnlocked)return;
    const FTransform Frame=GetActorTransform();
    for(TActorIterator<ADiggableRock> Rock(GetWorld());Rock;++Rock)
        if(Rock->ActorHasTag(TEXT("BM_StartRoom_ExitBlocker")) && Rock->SuccessfulCuts>0)
        {
            FHitResult Hit;
            FCollisionQueryParams Q(SCENE_QUERY_STAT(StartRoomBreach),true,Player.Get());
            const bool bBlocked=GetWorld()->LineTraceSingleByChannel(Hit,Frame.TransformPosition(FVector(425,0,95)),Frame.TransformPosition(FVector(650,0,95)),ECC_Visibility,Q);
            if(!bBlocked || Frame.InverseTransformPosition(Player->GetActorLocation()).X>650)bHeadlampUnlocked=true;
            break;
        }
    TArray<ULightComponent*> Lights;Player->GetComponents(Lights);
    for(auto* Light:Lights)
        if(Light->GetFName()==TEXT("MiningLight") || Light->GetFName()==TEXT("MiningFillLight"))Light->SetVisibility(bHeadlampUnlocked);
}

FName AMiningStartRoom::GetViewedProp(APlayerController* Controller) const
{
    if(!Controller || !Controller->GetPawn() || Controller->GetPawn()!=Player.Get() || Controller->IsPaused() || Controller->bShowMouseCursor)return NAME_None;
    FVector Eye;FRotator View;Controller->GetPlayerViewPoint(Eye,View);
    const auto Segment=[](AActor* Actor)->FName
    {
        for(;Actor;Actor=Actor->GetAttachParentActor())
            for(FName Tag:Actor->Tags)if(Tag.ToString().StartsWith(TEXT("StartRoomView_")))return Tag;
        return NAME_None;
    };
    FHitResult Hit;
    if(MiningInteraction::Trace(GetWorld(),Hit,Eye,View,InteractionReach,FCollisionQueryParams(SCENE_QUERY_STAT(StartRoomView),true,Controller->GetPawn()),[&](const FHitResult& H){return !Segment(H.GetActor()).IsNone();}))return Segment(Hit.GetActor());
    return NAME_None;
}

AMiningStartRoom* AMiningStartRoom::Find(UWorld* World)
{
    if (World) for (TActorIterator<AMiningStartRoom> Room(World); Room; ++Room) return *Room;
    return nullptr;
}

void AMiningStartRoom::ReleasePlayer()
{
    if (bOwnMiningBlock && Mining.IsValid()) Mining->PopEncounterBlock();
    if (bOwnFlightBlock && Flight.IsValid()) Flight->PopFlightBlock();
    if (Effects.IsValid()) Effects->bEnabled = bPreviousEffectsEnabled;
    bOwnMiningBlock = bOwnFlightBlock = false;
    Player.Reset(); Mining.Reset(); Flight.Reset(); Effects.Reset();
}

void AMiningStartRoom::InitializePlayer(APawn* Pawn)
{
    if (!Pawn || Player.Get() == Pawn) return;
    ReleasePlayer();
    Player = Pawn;
    // The skeletal computer has no reliable complex trace surface. A query-only
    // target follows its visible bounds and never blocks the player or physics.
    for(TActorIterator<AActor> Actor(GetWorld());Actor;++Actor)
        if(Actor->ActorHasTag(TEXT("StartRoomView_Computer")) && !Actor->FindComponentByClass<UBoxComponent>())
        {
            const FBox Bounds=Actor->GetComponentsBoundingBox(true);
            Actor->SetActorEnableCollision(true);
            if(!Bounds.IsValid)continue;
            auto* Target=NewObject<UBoxComponent>(*Actor,TEXT("StartRoomComputerViewTarget"));
            Actor->AddInstanceComponent(Target);Target->SetupAttachment(Actor->GetRootComponent());
            Target->SetBoxExtent(Bounds.GetExtent().ComponentMax(FVector(3)));
            Target->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Target->SetCollisionResponseToAllChannels(ECR_Ignore);
            Target->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
            Target->SetGenerateOverlapEvents(false);Target->SetCanEverAffectNavigation(false);
            Target->RegisterComponent();Target->SetWorldScale3D(FVector::OneVector);
            Target->SetWorldLocationAndRotation(Bounds.GetCenter(),FQuat::Identity);
        }
    Mining = Pawn->FindComponentByClass<UMiningComponent>();
    Flight = Pawn->FindComponentByClass<UMiningCharacterMovementComponent>();
    Effects = Pawn->FindComponentByClass<UMiningToolEffectsComponent>();
    if (Mining.IsValid() && !bPickaxeUnlocked) { Mining->PushEncounterBlock(); bOwnMiningBlock = true; }
    if (Flight.IsValid() && !bJetpackUnlocked) { Flight->PushFlightBlock(); bOwnFlightBlock = true; }
    if (Effects.IsValid())
    {
        bPreviousEffectsEnabled = Effects->bEnabled;
        if (!bPickaxeUnlocked) Effects->bEnabled = false;
    }
}

AMiningStarterPickup* AMiningStartRoom::GetTarget(APlayerController* Controller) const
{
    if (!Controller || !Controller->GetPawn() || Controller->GetPawn() != Player.Get() || Controller->IsPaused()) return nullptr;
    FVector Location; FRotator Rotation;
    Controller->GetPlayerViewPoint(Location, Rotation);
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(StartRoomPickup), false, Controller->GetPawn());
    if (!MiningInteraction::Trace(GetWorld(),Hit,Location,Rotation,InteractionReach,Params,[](const FHitResult& Candidate)
        {
            for (AActor* Actor=Candidate.GetActor(); Actor; Actor=Actor->GetAttachParentActor())
                if (auto* Pickup=Cast<AMiningStarterPickup>(Actor)) return !Pickup->bCollected;
            return false;
        })) return nullptr;
    for (AActor* Actor = Hit.GetActor(); Actor; Actor = Actor->GetAttachParentActor())
        if (auto* Pickup = Cast<AMiningStarterPickup>(Actor)) return Pickup->bCollected ? nullptr : Pickup;
    return nullptr;
}

FString AMiningStartRoom::GetInteractionAction(APlayerController* Controller) const
{
    if (const auto* Pickup = GetTarget(Controller))
        return Pickup->Equipment == EMiningStarterEquipment::Pickaxe ? TEXT("镐子") : TEXT("喷气背包");
    if(!GetViewedProp(Controller).IsNone())return TEXT("查看");
    return FString();
}

bool AMiningStartRoom::Interact(APlayerController* Controller)
{
    auto* Pickup = GetTarget(Controller);
    if(!Pickup && Controller && !Controller->bShowMouseCursor)
    {
        const FName Segment=GetViewedProp(Controller);
        if(!Segment.IsNone()){USubtitleLibrary::PlaySubtitleSegment(this,Segment);return true;}
    }
    if (!Pickup || Controller->bShowMouseCursor) return false;
    if (Pickup->Equipment == EMiningStarterEquipment::Pickaxe)
    {
        if (bPickaxeUnlocked || !Mining.IsValid()) return false;
        bPickaxeUnlocked = true;
        for(TActorIterator<AMiningOpening> Opening(GetWorld());Opening;++Opening)Opening->BeginTitleToWall(nullptr);
        if (bOwnMiningBlock) { Mining->PopEncounterBlock(); bOwnMiningBlock = false; }
        Mining->SelectPickaxe();
        if (Effects.IsValid()) Effects->bEnabled = bPreviousEffectsEnabled;
    }
    else
    {
        if (bJetpackUnlocked || !Flight.IsValid()) return false;
        bJetpackUnlocked = true;
        if (bOwnFlightBlock) { Flight->PopFlightBlock(); bOwnFlightBlock = false; }
    }
    Pickup->Collect();
    if (auto* Audio = Controller->GetPawn()->FindComponentByClass<UMiningAudioComponent>())
        Audio->PlayStarterPickup(Pickup->Equipment == EMiningStarterEquipment::Jetpack);
    return true;
}

void AMiningStartRoom::UnlockDeveloperEquipment()
{
    if (bOwnMiningBlock && Mining.IsValid()) { Mining->PopEncounterBlock(); bOwnMiningBlock = false; }
    if (bOwnFlightBlock && Flight.IsValid()) { Flight->PopFlightBlock(); bOwnFlightBlock = false; }
    if (Effects.IsValid()) Effects->bEnabled = bPreviousEffectsEnabled;
    bPickaxeUnlocked = bJetpackUnlocked = bHeadlampUnlocked = true;
    if(Player.IsValid())
    {
        TArray<ULightComponent*> Lights;Player->GetComponents(Lights);
        for(auto* Light:Lights)if(Light->GetFName()==TEXT("MiningLight")||Light->GetFName()==TEXT("MiningFillLight"))Light->SetVisibility(true);
    }
    for (TActorIterator<AMiningStarterPickup> Pickup(GetWorld()); Pickup; ++Pickup)
        if (!Pickup->bCollected) Pickup->Collect();
}

void AMiningStartRoom::EndPlay(const EEndPlayReason::Type Reason)
{
    ReleasePlayer();
    Super::EndPlay(Reason);
}

void AMiningStartRoomController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if (auto* Room = AMiningStartRoom::Find(GetWorld())) Room->InitializePlayer(InPawn);
}

void AMiningStartRoomController::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Room = AMiningStartRoom::Find(GetWorld())) Room->InitializePlayer(GetPawn());
}

bool AMiningStartRoomController::InputKey(const FInputKeyEventArgs& Params)
{
    if (auto* Room = AMiningStartRoom::Find(GetWorld()))
    {
        if (Params.Key == EKeys::E && Params.Event == IE_Pressed && !bUpgradeMenuHeld)
            if (Room->Interact(this)) return true;
        // This introductory level supplies only these two tools. Prevent alternate tools/UI bypassing the lesson.
        if (Params.Key == EKeys::G || Params.Key == EKeys::V || Params.Key == EKeys::Tab) return true;
    }
    return Super::InputKey(Params);
}

AMiningStartRoomGameMode::AMiningStartRoomGameMode()
{
    PlayerControllerClass = AMiningStartRoomController::StaticClass();
}
