#include "MiningAudioComponent.h"
#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningBombToolComponent.h"
#include "MiningWalletSubsystem.h"
#include "Showcase/MiningExhibitHall.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"

namespace
{
	TAutoConsoleVariable<int32> AudioEnabled(TEXT("bm.Audio.Enabled"), 1, TEXT("Enable the mining audio pilot."));
	TAutoConsoleVariable<float> EffectsVolume(TEXT("bm.Audio.SFXVolume"), .7f, TEXT("Audio pilot effects volume, 0 to 1."));
	TAutoConsoleVariable<float> MusicVolume(TEXT("bm.Audio.MusicVolume"), .12f, TEXT("Audio pilot exploration music trim, 0 to 1."));
}

UMiningAudioComponent::UMiningAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	PrimaryComponentTick.TickInterval = .025f;
}

TArray<FName> UMiningAudioComponent::AssetNames()
{
	TArray<FName> Names;
	for (const auto& Prefix : {TEXT("S_DigLight"), TEXT("S_DigDeep"), TEXT("S_Foot_concrete"), TEXT("S_Foot_wood")})
		for (int32 Index = 0; Index < 5; ++Index) Names.Add(FName(*FString::Printf(TEXT("%s_%02d"), Prefix, Index)));
	for (const auto& Prefix : {TEXT("S_DigBreak"), TEXT("S_Swing")})
		for (int32 Index = 0; Index < 3; ++Index) Names.Add(FName(*FString::Printf(TEXT("%s_%02d"), Prefix, Index)));
	for (const auto& Name : {TEXT("S_UI_Open"), TEXT("S_UI_Close"), TEXT("S_UI_Confirm"), TEXT("S_UI_Reject"), TEXT("S_Coin"), TEXT("S_Land"), TEXT("M_DreamyFlashback_Loop")}) Names.Add(FName(Name));
	for (const auto* Name : {TEXT("S_DrillStart"), TEXT("S_DrillIdle_Loop"), TEXT("S_DrillContact_Loop"), TEXT("S_DrillStop"), TEXT("S_DrillLow"), TEXT("S_JetStart"), TEXT("S_Jet_Loop"), TEXT("S_JetStop"), TEXT("S_JetOverheat"), TEXT("S_BombThrow"), TEXT("S_BombStick"), TEXT("S_BombFuse_Loop"), TEXT("S_BombExplosion"), TEXT("S_Collect")}) Names.Add(FName(Name));
	return Names;
}

void UMiningAudioComponent::BeginPlay()
{
	Super::BeginPlay();
	Character = Cast<ACharacter>(GetOwner());
	if (!Character.IsValid() || GetNetMode() == NM_DedicatedServer) { SetComponentTickEnabled(false); return; }
	Mining = GetOwner()->FindComponentByClass<UMiningComponent>();
	if (Mining.IsValid())
	{
		Mining->OnMiningAttempt.AddUniqueDynamic(this, &ThisClass::OnSwing);
		Mining->OnMiningHit.AddUniqueDynamic(this, &ThisClass::OnDrillContact);
		Mining->OnMiningBlocked.AddUniqueDynamic(this, &ThisClass::OnBlockedContact);
		Mining->OnFieldReset.AddUniqueDynamic(this, &ThisClass::StopTools);
		Mining->OnPickaxeContact.AddUObject(this, &ThisClass::OnContact);
		Mining->OnPickaxeBreak.AddUObject(this, &ThisClass::OnBreak);
	}
	if (auto* Instance = GetWorld()->GetGameInstance())
	{
		Wallet = Instance->GetSubsystem<UMiningWalletSubsystem>();
		if (Wallet.IsValid()) Wallet->OnCoinsAdded.AddUObject(this, &ThisClass::OnCoins);
	}
	for (FName Name : AssetNames())
	{
		const FString Path = FString::Printf(TEXT("/Game/Audio/Pilot/%s.%s"), *Name.ToString(), *Name.ToString());
		if (auto* Sound = LoadObject<USoundBase>(nullptr, *Path)) Sounds.Add(Name, Sound);
		else UE_LOG(LogTemp, Warning, TEXT("Audio pilot asset missing: %s"), *Path);
	}
	EffectsConcurrency = NewObject<USoundConcurrency>(this);
	bMainGameAudio = IsMainGameWorld(GetWorld());
	if (bMainGameAudio)
	{
		for (FName Name : MainGameAssetNames())
		{
			const FString Path = FString::Printf(TEXT("/Game/Audio/MainGame/%s.%s"), *Name.ToString(), *Name.ToString());
			if (auto* Sound = LoadObject<USoundBase>(nullptr, *Path)) Sounds.Add(Name, Sound);
			else UE_LOG(LogTemp, Warning, TEXT("MainGame audio asset missing: %s"), *Path);
		}
	}
	EffectsConcurrency->Concurrency.MaxCount = 8;
	EffectsConcurrency->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopOldest;
}

bool UMiningAudioComponent::CanPlay() const
{
	return IsActive() && AudioEnabled.GetValueOnGameThread() != 0 && Character.IsValid() && Character->IsLocallyControlled() && !UGameplayStatics::IsGamePaused(this);
}

void UMiningAudioComponent::Play(FName Name, float Gain, float Pitch, bool bUI)
{
	if (!CanPlay()) return;
	const float Volume = Gain * FMath::Clamp(EffectsVolume.GetValueOnGameThread(), 0.f, 1.f);
	if (Volume <= 0) return;
	if (const auto* Sound = Sounds.Find(Name))
	{
		LastPlayedSound = Name;
		UGameplayStatics::PlaySound2D(this, Sound->Get(), Volume, Pitch, 0, EffectsConcurrency, GetOwner(), bUI);
	}
}

void UMiningAudioComponent::PlayVariant(const FString& Prefix, int32 Count, float Gain)
{
	if (!CanPlay() || Count <= 0) return;
	const int32* Previous = PreviousVariants.Find(Prefix);
	int32 Index = FMath::RandRange(0, Count - 1);
	if (Previous && Count > 1 && Index == *Previous) Index = (Index + FMath::RandRange(1, Count - 1)) % Count;
	PreviousVariants.Add(Prefix, Index);
	Play(FName(*FString::Printf(TEXT("%s_%02d"), *Prefix, Index)), Gain, FMath::FRandRange(.96f, 1.04f));
}

void UMiningAudioComponent::OnSwing(float Interval)
{
	if (Mining.IsValid() && !Mining->bDrillMode) PlayVariant(TEXT("S_Swing"), 3, .32f);
}

void UMiningAudioComponent::OnContact(const FHitResult& Hit, int32 Stage)
{
	++ContactCount;
	LastContactSurface = ResolveSurface(Hit);
	if (bMainGameAudio && LastContactSurface == EMiningAudioSurface::Dirt)
	{
		Play(TEXT("S_DirtDig"), Stage <= 1 ? .5f : .65f, FMath::FRandRange(.94f, 1.06f));
		return;
	}
	PlayVariant(Stage <= 1 ? TEXT("S_DigLight") : TEXT("S_DigDeep"), 5, Stage <= 1 ? .75f : .9f);
}

void UMiningAudioComponent::OnBreak(const FVector& Position)
{
	PlayVariant(TEXT("S_DigBreak"), 3, .8f);
}

void UMiningAudioComponent::OnCoins(int64 Amount)
{
	if (!CanPlay() || Amount <= 0 || GetWorld()->GetTimeSeconds() < NextCoinTime) return;
	NextCoinTime = GetWorld()->GetTimeSeconds() + .2;
	Play(TEXT("S_Coin"), .28f);
}

void UMiningAudioComponent::PlayPanel(bool bOpen) { Play(bOpen ? TEXT("S_UI_Open") : TEXT("S_UI_Close"), .45f, 1.f, true); }
void UMiningAudioComponent::PlayPurchase(bool bSuccess) { Play(bSuccess ? TEXT("S_UI_Confirm") : TEXT("S_UI_Reject"), .6f, 1.f, true); }

bool UMiningAudioComponent::IsMiningArea() const
{
	if (Hall.IsValid()) return Hall->bReady && Hall->Areas.IsValidIndex(Hall->ActiveArea) && Hall->AreaKind(Hall->ActiveArea) == EMiningExperienceKind::Mining;
	return UGameplayStatics::GetCurrentLevelName(this, true) == TEXT("Lvl_ExternalMining_Demo");
}

bool UMiningAudioComponent::HasFootsteps() const
{
	if (IsMiningArea()) return true;
	if (!Hall.IsValid() || !Hall->bReady || !Hall->Areas.IsValidIndex(Hall->ActiveArea)) return false;
	const auto Kind = Hall->AreaKind(Hall->ActiveArea);
	return Kind == EMiningExperienceKind::Office || Kind == EMiningExperienceKind::Museum;
}

void UMiningAudioComponent::UpdateMusic(float DeltaTime)
{
	if (bMainGameAudio) { UpdateMainGameSoundscape(DeltaTime); return; }
	const float Target = CanPlay() && IsMiningArea() ? FMath::Clamp(MusicVolume.GetValueOnGameThread(), 0.f, 1.f) : 0.f;
	MusicGain = FMath::FInterpConstantTo(MusicGain, Target, DeltaTime, .25f);
	if (Target > 0 && !Music)
	{
		if (const auto* Sound = Sounds.Find(TEXT("M_DreamyFlashback_Loop")))
		{
			Music = NewObject<UAudioComponent>(GetOwner());
			Music->bAutoActivate = false;
			Music->bAutoDestroy = false;
			Music->bAllowSpatialization = false;
			Music->SetSound(Sound->Get());
			Music->RegisterComponent();
		}
	}
	if (Music)
	{
		Music->SetVolumeMultiplier(MusicGain);
		if (Target > 0 && !Music->IsPlaying()) Music->Play();
		if (MusicGain <= 0 && Music->IsPlaying()) Music->Stop();
	}
}

void UMiningAudioComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (!Hall.IsValid() && GetWorld()->GetTimeSeconds() >= NextHallSearch)
	{
		Hall = AMiningExhibitHall::Find(GetWorld());
		NextHallSearch = GetWorld()->GetTimeSeconds() + 1;
	}
	UpdateMusic(DeltaTime);
	UpdateTools(DeltaTime);
	if (!Character.IsValid()) return;
	const auto* Movement = Character->GetCharacterMovement();
	const FVector Location = Character->GetActorLocation();
	const bool bGrounded = Movement->IsMovingOnGround();
	const bool bAllowed = CanPlay() && HasFootsteps() && !(Hall.IsValid() && Hall->IsInteractionOpen()) && !(Mining.IsValid() && Mining->IsInputSuspended());
	const float Distance = FVector::Dist2D(Location, PreviousLocation);
	const bool bContinuous = bTrackingMovement && FVector::Dist(Location, PreviousLocation) < FMath::Max(150.f, Movement->GetMaxSpeed() * DeltaTime * 2.f);
	if (bAllowed && bContinuous)
	{
		if (bGrounded && !bWasGrounded && PreviousVerticalSpeed < -200)
		{
			Play(TEXT("S_Land"), bMainGameAudio ? FMath::Clamp(-PreviousVerticalSpeed / 1400.f, .2f, .65f) : .65f);
			if (bMainGameAudio) PlayFootstep(Movement->CurrentFloor.HitResult, .15f);
			StepDistance = 0;
		}
		if (bGrounded && bWasGrounded && Movement->Velocity.SizeSquared2D() > 400)
		{
			StepDistance += Distance;
			if (StepDistance >= 165.f)
			{
				StepDistance = FMath::Fmod(StepDistance, 165.f);
				PlayFootstep(Movement->CurrentFloor.HitResult, .25f);
			}
		}
		else StepDistance = 0;
	}
	else StepDistance = 0;
	PreviousLocation = Location;
	PreviousVerticalSpeed = Movement->Velocity.Z;
	bWasGrounded = bGrounded;
	bTrackingMovement = bAllowed;
}

void UMiningAudioComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Mining.IsValid())
	{
		Mining->OnMiningAttempt.RemoveDynamic(this, &ThisClass::OnSwing);
		Mining->OnMiningHit.RemoveDynamic(this, &ThisClass::OnDrillContact);
		Mining->OnMiningBlocked.RemoveDynamic(this, &ThisClass::OnBlockedContact);
		Mining->OnFieldReset.RemoveDynamic(this, &ThisClass::StopTools);
		Mining->OnPickaxeContact.RemoveAll(this);
		Mining->OnPickaxeBreak.RemoveAll(this);
	}
	if (Wallet.IsValid()) Wallet->OnCoinsAdded.RemoveAll(this);
	StopTools();
	StopSoundscape();
	if (Music) { Music->Stop(); Music->DestroyComponent(); Music = nullptr; }
	Super::EndPlay(Reason);
}

void UMiningAudioComponent::PlayCollect()
{
	if (!CanPlay() || GetWorld()->GetTimeSeconds() < NextCollectTime) return;
	NextCollectTime = GetWorld()->GetTimeSeconds() + .2;
	NextCoinTime = NextCollectTime;
	Play(TEXT("S_Collect"), .5f);
}

void UMiningAudioComponent::OnDrillContact(const FHitResult& Hit)
{
	if (Mining.IsValid() && Mining->bDrillMode && Mining->IsMiningActive())
	{
		++ContactCount;
		LastContactSurface = ResolveSurface(Hit);
		DrillContactUntil = GetWorld()->GetTimeSeconds() + FMath::Clamp(Mining->GetMiningInterval() * 1.5f, .08f, .3f);
	}
}

void UMiningAudioComponent::SetToolLoop(FName Name, bool bActive, float Gain, float Pitch)
{
	if (!bActive)
	{
		if (auto* Existing = ToolLoops.Find(Name)) { (*Existing)->Stop(); (*Existing)->DestroyComponent(); ToolLoops.Remove(Name); }
		return;
	}
	auto* Existing = ToolLoops.Find(Name);
	if (!Existing)
	{
		const auto* Sound = Sounds.Find(Name);
		if (!Sound) return;
		auto* Loop = NewObject<UAudioComponent>(GetOwner());
		Loop->bAutoActivate = false;
		Loop->bAutoDestroy = false;
		Loop->bAllowSpatialization = false;
		Loop->SetSound(Sound->Get());
		Loop->RegisterComponent();
		Loop->SetVolumeMultiplier(Gain * FMath::Clamp(EffectsVolume.GetValueOnGameThread(), 0.f, 1.f));
		Loop->SetPitchMultiplier(Pitch);
		Loop->FadeIn(.06f, Loop->VolumeMultiplier);
		ToolLoops.Add(Name, Loop);
	}
	else
	{
		(*Existing)->SetVolumeMultiplier(Gain * FMath::Clamp(EffectsVolume.GetValueOnGameThread(), 0.f, 1.f));
		(*Existing)->SetPitchMultiplier(Pitch);
	}
}

void UMiningAudioComponent::StopTools()
{
	for (auto& Entry : ToolLoops) if (Entry.Value) { Entry.Value->Stop(); Entry.Value->DestroyComponent(); }
	ToolLoops.Empty();
	bDrillRunning = bJetRunning = false;
	DrillContactUntil = 0;
}

void UMiningAudioComponent::Deactivate()
{
	StopTools();
	StopSoundscape();
	if (Music) Music->Stop();
	MusicGain = 0;
	Super::Deactivate();
}

void UMiningAudioComponent::UpdateTools(float DeltaTime)
{
	const bool bAllowed = CanPlay() && Mining.IsValid() && !Mining->IsInputSuspended() && !(Hall.IsValid() && Hall->IsInteractionOpen());
	if (!bAllowed) { StopTools(); return; }
	const auto* Bomb = GetOwner()->FindComponentByClass<UMiningBombToolComponent>();
	const bool bDrill = IsMiningArea() && !Mining->IsEncounterBlocked() && Mining->bDrillMode && !(Bomb && Bomb->bEquipped);
	const bool bContact = bDrill && Mining->IsMiningActive() && GetWorld()->GetTimeSeconds() < DrillContactUntil;
	if (bDrill != bDrillRunning) Play(bDrill ? TEXT("S_DrillStart") : TEXT("S_DrillStop"), .4f);
	SetToolLoop(TEXT("S_DrillIdle_Loop"), bDrill, bContact ? .12f : Mining->IsMiningActive() ? .3f : .12f, Mining->IsMiningActive() ? 1.12f : .9f);
	const bool bDirtContact = bMainGameAudio && LastContactSurface == EMiningAudioSurface::Dirt;
	SetToolLoop(TEXT("S_DrillContact_Loop"), bContact && !bDirtContact, .38f);
	if (bMainGameAudio) SetToolLoop(TEXT("S_DrillDirt_Loop"), bContact && bDirtContact, .3f);
	const bool bLow = Mining->bUseDrillEnergy && !Mining->bInfiniteDrillEnergy && Mining->DrillEnergy <= Mining->MaxDrillEnergy * .15f;
	if (bLow && !bLowDrill && (bDrill || bDrillRunning)) Play(TEXT("S_DrillLow"), .5f);
	if (!bLow || bDrill || bDrillRunning) bLowDrill = bLow;
	bDrillRunning = bDrill;
	const auto* Flight = Cast<UMiningCharacterMovementComponent>(Character->GetCharacterMovement());
	const bool bJet = Flight && Flight->IsFlightAvailable() && (Flight->IsFlightThrustActive() || (Flight->bCreativeFlight && Flight->IsFalling()));
	if (bJet != bJetRunning) Play(bJet ? TEXT("S_JetStart") : TEXT("S_JetStop"), .4f);
	SetToolLoop(TEXT("S_Jet_Loop"), bJet, .3f);
	const bool bHot = Flight && Flight->IsJetpackOverheated();
	if (bHot && !bOverheated) Play(TEXT("S_JetOverheat"), .5f);
	bOverheated = bHot;
	bJetRunning = bJet;
}
