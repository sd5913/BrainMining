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
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
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
        return Pickup->Equipment == EMiningStarterEquipment::Pickaxe ? TEXT("拿起镐子 · 解锁挖掘") : TEXT("穿戴喷气背包 · 解锁飞行");
    return FString();
}

bool AMiningStartRoom::Interact(APlayerController* Controller)
{
    auto* Pickup = GetTarget(Controller);
    if (!Pickup || Controller->bShowMouseCursor) return false;
    if (Pickup->Equipment == EMiningStarterEquipment::Pickaxe)
    {
        if (bPickaxeUnlocked || !Mining.IsValid()) return false;
        bPickaxeUnlocked = true;
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
    bPickaxeUnlocked = bJetpackUnlocked = true;
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
