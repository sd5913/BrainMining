#include "MiningHapticsSubsystem.h"
#include "MiningGamepadSubsystem.h"
#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "Showcase/MiningExhibitHall.h"
#include "EchoPassageStage.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/ActorComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/IInputInterface.h"
#include "Misc/Paths.h"

namespace
{
    template <typename Callback>
    void ForPlayers(const UObject* Source, bool bSpatial, Callback Action)
    {
        UWorld* World = Source ? Source->GetWorld() : nullptr;
        if (!World || !World->IsGameWorld()) return;
        const AActor* Actor = Cast<AActor>(Source);
        if (const auto* Component = Cast<UActorComponent>(Source)) Actor = Component->GetOwner();
        const auto* Pawn = Cast<APawn>(Actor);
        const auto* OwnerPlayer = Pawn ? Cast<APlayerController>(Pawn->GetController()) : Cast<APlayerController>(Actor);
        for (auto Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
        {
            auto* Player = Iterator->Get();
            if (!Player || !Player->IsLocalController() || (!bSpatial && OwnerPlayer && OwnerPlayer != Player)) continue;
            if (!bSpatial && Pawn && !OwnerPlayer) continue;
            if (auto* Haptics = UMiningHapticsSubsystem::Find(Player)) Action(Haptics, Player);
        }
    }
}

UMiningHapticsSubsystem* UMiningHapticsSubsystem::Find(const APlayerController* Player)
{
    auto* Local = Player ? Player->GetLocalPlayer() : nullptr;
    return Local ? Local->GetSubsystem<UMiningHapticsSubsystem>() : nullptr;
}

APlayerController* UMiningHapticsSubsystem::Controller() const
{
    return GetLocalPlayer() ? GetLocalPlayer()->GetPlayerController(GetWorld()) : nullptr;
}

void UMiningHapticsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UMiningGamepadSubsystem>();
    LoadPreferences(FPaths::ProjectSavedDir() / TEXT("Config/BrainMiningHaptics.ini"));
    EchoHandle = AEchoPassageStage::OnFeedback.AddUObject(this, &ThisClass::OnEcho);
}

void UMiningHapticsSubsystem::Deinitialize()
{
    AEchoPassageStage::OnFeedback.Remove(EchoHandle);
    Stop();
    Super::Deinitialize();
}

bool UMiningHapticsSubsystem::CanPlay() const
{
    auto* Player = Controller();
    if (!bEnabled || !FMath::IsFinite(Strength) || Strength <= 0 || !Player || !Player->IsLocalController()
        || !Player->GetPawn() || !GetWorld() || !GetWorld()->IsGameWorld() || Player->IsPaused()
        || !FSlateApplication::IsInitialized() || !FSlateApplication::Get().IsActive()) return false;
    auto* Input = UMiningGamepadSubsystem::Find(Player);
    if (!Input || !UMiningGamepadSubsystem::UsesGamepad(Player)) return false;
    const auto Context = Input->GetContext();
    return Context != EMiningInputContext::Cards && Context != EMiningInputContext::Menu && Context != EMiningInputContext::Catalogue;
}

void UMiningHapticsSubsystem::SyncIdentity()
{
    auto* Player = Controller();
    APawn* Pawn = Player ? Player->GetPawn() : nullptr;
    auto* Hall = GetWorld() ? AMiningExhibitHall::Find(GetWorld()) : nullptr;
    const int32 Area = Hall ? Hall->ActiveArea : INDEX_NONE;
    auto* Input = UMiningGamepadSubsystem::Find(Player);
    const int32 InputContext = Input ? int32(Input->GetContext()) : INDEX_NONE;
    if (Player == LastController.Get() && Pawn == LastPawn.Get() && GetWorld() == LastWorld.Get() && Area == LastArea && InputContext == LastInputContext) return;
    Stop();
    LastController = Player; LastPawn = Pawn; LastWorld = GetWorld(); LastArea = Area; LastInputContext = InputContext;
    bSamplesReady = false;
}

void UMiningHapticsSubsystem::Emit(const UObject* Source, EMiningHapticCue Cue, float Gain)
{
    ForPlayers(Source, false, [&](auto* Haptics, auto*) { Haptics->Play(Cue, Gain); });
}

void UMiningHapticsSubsystem::EmitAt(const UObject* Source, EMiningHapticCue Cue, FVector Location, float Radius, float Gain)
{
    if (Location.ContainsNaN()) return;
    ForPlayers(Source, true, [&](auto* Haptics, auto* Player)
    {
        if (APawn* Pawn = Player->GetPawn())
        {
            float Attenuation = FMiningHapticMixer::DistanceGain(FVector::Distance(Location, Pawn->GetActorLocation()), Radius);
            if (Attenuation > 0 && (Cue == EMiningHapticCue::Explosion || Cue == EMiningHapticCue::DoorBreak))
            {
                FCollisionQueryParams Query(SCENE_QUERY_STAT(HapticOcclusion), false, Pawn);
                if (const auto* Actor = Cast<AActor>(Source)) Query.AddIgnoredActor(Actor);
                FHitResult Hit;
                if (Player->GetWorld()->LineTraceSingleByChannel(Hit, Location, Pawn->GetActorLocation(), ECC_Visibility, Query)) Attenuation *= .35f;
            }
            Haptics->Play(Cue, Gain * Attenuation);
        }
    });
}

bool UMiningHapticsSubsystem::Play(EMiningHapticCue Cue, float Gain)
{
    SyncIdentity();
    if (!CanPlay() || (!bOptional && FMiningHapticMixer::Pattern(Cue).bOptional)) return false;
    const bool bAccepted = Mixer.Play(Cue, Gain);
    if (bAccepted) ++AcceptedCueCount;
    return bAccepted;
}

void UMiningHapticsSubsystem::MiningContact(UMiningComponent* Mining)
{
    if (!Mining || !Mining->bDrillMode) return;
    ForPlayers(Mining, false, [&](auto* Haptics, auto*)
    {
        Haptics->SyncIdentity();
        if (Haptics->CanPlay()) Haptics->DrillContactRemaining = FMath::Clamp(Mining->GetMiningInterval() * 1.5f, .08f, .3f);
    });
}

void UMiningHapticsSubsystem::FinaleThrust(const UObject* Source, bool bActive, bool bComfortMode)
{
    ForPlayers(Source, false, [&](auto* Haptics, auto*)
    {
        Haptics->SyncIdentity();
        Haptics->FinaleRemaining = bActive && Haptics->CanPlay() ? .15f : 0;
        Haptics->FinaleGain = bComfortMode ? .4f : 1.f;
    });
}

void UMiningHapticsSubsystem::OnEcho(AEchoPassageStage* Stage, FName Event, FVector Location)
{
    if (!Stage || Stage->GetWorld() != GetWorld() || !Controller() || !Controller()->GetPawn()) return;
    const float Distance = FVector::Distance(Location, Controller()->GetPawn()->GetActorLocation());
    if (Distance > 4000) return;
    const auto Cue = Event == TEXT("Knock") ? EMiningHapticCue::EchoKnock : Event == TEXT("Landing") ? EMiningHapticCue::EchoLanding
        : Event == TEXT("Collapse") ? EMiningHapticCue::EchoCollapse : EMiningHapticCue::PuzzleComplete;
    Play(Cue);
}

void UMiningHapticsSubsystem::SampleGameplay(float DeltaTime, bool bAllowed)
{
    auto* Pawn = LastPawn.Get();
    auto* Character = Cast<ACharacter>(Pawn);
    auto* Move = Character ? Character->GetCharacterMovement() : nullptr;
    auto* Flight = Cast<UMiningCharacterMovementComponent>(Move);
    auto* Mining = Pawn ? Pawn->FindComponentByClass<UMiningComponent>() : nullptr;
    auto* Input = UMiningGamepadSubsystem::Find(Controller());
    const bool bGameplay = bAllowed && Input && Input->GetContext() == EMiningInputContext::Exploration
        && !Controller()->IsMoveInputIgnored() && !Controller()->IsLookInputIgnored()
        && (!Mining || (!Mining->IsInputSuspended() && !Mining->IsEncounterBlocked()));
    const bool bDrill = Mining && Mining->bDrillMode;
    const bool bLow = Mining && Mining->bUseDrillEnergy && !Mining->bInfiniteDrillEnergy && Mining->DrillEnergy <= Mining->MaxDrillEnergy * .15f;
    const bool bEmpty = bLow && Mining->DrillEnergy <= UE_SMALL_NUMBER;
    const bool bJet = Flight && Flight->IsFlightAvailable() && (Flight->IsFlightThrustActive() || (Flight->bCreativeFlight && Flight->IsFalling()));
    const bool bHot = Flight && Flight->IsJetpackOverheated();
    const bool bGrounded = Move && Move->IsMovingOnGround();
    const FVector Location = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
    if (bSamplesReady && bGameplay)
    {
        if (bDrill && !bWasDrill) Play(EMiningHapticCue::DrillStart);
        if (bEmpty && !bWasEmpty && bWasDrill) Play(EMiningHapticCue::DrillEmpty);
        else if (bLow && !bWasLow && (bDrill || bWasDrill)) Play(EMiningHapticCue::DrillLow);
        if (bJet && !bWasJet) Play(EMiningHapticCue::JetStart);
        if (bHot && !bWasHot) Play(EMiningHapticCue::JetOverheat);
        if (bGrounded && !bWasGrounded && PreviousVerticalSpeed < -200 && Move
            && FVector::Distance(Location, PreviousLocation) < FMath::Max(150.f, Move->Velocity.Size() * DeltaTime * 2.f))
            Play(EMiningHapticCue::Landing, FMath::Clamp((-PreviousVerticalSpeed - 150.f) / 1000.f, .2f, 1.f));
    }
    bWasDrill = bDrill; bWasLow = bLow; bWasEmpty = bEmpty; bWasJet = bJet; bWasHot = bHot;
    bWasGrounded = bGrounded; PreviousLocation = Location; PreviousVerticalSpeed = Move ? Move->Velocity.Z : 0;
    bSamplesReady = Pawn != nullptr;
    FVector2D Target = FVector2D::ZeroVector;
    if (bContinuous && bGameplay)
    {
        if (bDrill && Mining->IsMiningActive() && DrillContactRemaining > 0)
            Target = FVector2D(.17, .26) * (.9f + .1f * FMath::Sin(AnimationTime * 32));
        else if (bDrill && bOptional) Target = FVector2D(.03, .05);
        if (bJet) { Target.X = FMath::Max(Target.X, .10); Target.Y = FMath::Max(Target.Y, .05); }
    }
    if (bContinuous && bAllowed && FinaleRemaining > 0)
    {
        Target.X = FMath::Max(Target.X, .16 * FinaleGain);
        Target.Y = FMath::Max(Target.Y, .12 * FinaleGain);
    }
    Continuous = FMath::Vector2DInterpTo(Continuous, Target, DeltaTime, 25.f);
    if (Target.IsNearlyZero() && Continuous.Size() < .002) Continuous = FVector2D::ZeroVector;
}

void UMiningHapticsSubsystem::Tick(float DeltaTime)
{
    SyncIdentity();
    const float Step = FMath::IsFinite(DeltaTime) ? FMath::Max(0.f, DeltaTime) : 0;
    AnimationTime = FMath::Fmod(AnimationTime + Step, 1000.f);
    const bool bAllowed = CanPlay();
    SampleGameplay(Step, bAllowed);
    DrillContactRemaining = FMath::Max(0.f, DrillContactRemaining - Step);
    FinaleRemaining = FMath::Max(0.f, FinaleRemaining - Step);
    if (!bAllowed) { Stop(); return; }
    Output = Mixer.Advance(Step, Continuous) * FMath::Clamp(Strength, 0.f, 1.f);
    WriteOutput();
}

void UMiningHapticsSubsystem::WriteOutput()
{
    auto* Player = LastController.Get();
    if (!Player) return;
    const auto Write = [&](float Value, bool bLow, uint64& Handle)
    {
        if (Value <= .001f)
        {
            if (Handle) Player->PlayDynamicForceFeedback(0, 0, bLow, !bLow, bLow, !bLow, EDynamicForceFeedbackAction::Stop, Handle);
            Handle = 0;
        }
        else Handle = Player->PlayDynamicForceFeedback(Value, .12f, bLow, !bLow, bLow, !bLow,
            Handle ? EDynamicForceFeedbackAction::Update : EDynamicForceFeedbackAction::Start, Handle);
    };
    Write(Output.X, true, LowHandle);
    Write(Output.Y, false, HighHandle);
}

void UMiningHapticsSubsystem::Stop()
{
    const bool bHadOutput = LowHandle || HighHandle || !Output.IsNearlyZero();
    Output = Continuous = FVector2D::ZeroVector;
    Mixer.Reset(); DrillContactRemaining = FinaleRemaining = 0;
    WriteOutput();
    if (bHadOutput && GetLocalPlayer() && FSlateApplication::IsInitialized())
        if (auto Interface = FSlateApplication::Get().GetInputInterface()) Interface->SetForceFeedbackChannelValues(GetLocalPlayer()->GetControllerId(), FForceFeedbackValues());
}
