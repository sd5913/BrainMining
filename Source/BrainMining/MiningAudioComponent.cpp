#include "MiningAudioComponent.h"
#include "MiningComponent.h"
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
	TAutoConsoleVariable<float> MusicVolume(TEXT("bm.Audio.MusicVolume"), .16f, TEXT("Audio pilot exploration music volume, 0 to 1."));
}

UMiningAudioComponent::UMiningAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
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
	EffectsConcurrency->Concurrency.MaxCount = 8;
	EffectsConcurrency->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopOldest;
}

bool UMiningAudioComponent::CanPlay() const
{
	return AudioEnabled.GetValueOnGameThread() != 0 && Character.IsValid() && Character->IsLocallyControlled();
}

void UMiningAudioComponent::Play(FName Name, float Gain, float Pitch, bool bUI)
{
	if (!CanPlay()) return;
	const float Volume = Gain * FMath::Clamp(EffectsVolume.GetValueOnGameThread(), 0.f, 1.f);
	if (Volume <= 0) return;
	if (const auto* Sound = Sounds.Find(Name))
		UGameplayStatics::PlaySound2D(this, Sound->Get(), Volume, Pitch, 0, EffectsConcurrency, GetOwner(), bUI);
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
	PlayVariant(Stage <= 1 ? TEXT("S_DigLight") : TEXT("S_DigDeep"), 5, Stage <= 1 ? .85f : 1.f);
}

void UMiningAudioComponent::OnBreak(const FVector& Position)
{
	PlayVariant(TEXT("S_DigBreak"), 3, .9f);
}

void UMiningAudioComponent::OnCoins(int64 Amount)
{
	if (!CanPlay() || Amount <= 0 || GetWorld()->GetTimeSeconds() < NextCoinTime) return;
	NextCoinTime = GetWorld()->GetTimeSeconds() + .12;
	Play(TEXT("S_Coin"), .45f);
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
	if (!Character.IsValid()) return;
	const auto* Movement = Character->GetCharacterMovement();
	const FVector Location = Character->GetActorLocation();
	const bool bGrounded = Movement->IsMovingOnGround();
	const bool bAllowed = CanPlay() && HasFootsteps() && !(Hall.IsValid() && Hall->IsInteractionOpen()) && !(Mining.IsValid() && Mining->IsInputSuspended());
	const float Distance = FVector::Dist2D(Location, PreviousLocation);
	const bool bContinuous = bTrackingMovement && FVector::Dist(Location, PreviousLocation) < FMath::Max(150.f, Movement->GetMaxSpeed() * DeltaTime * 2.f);
	if (bAllowed && bContinuous)
	{
		if (bGrounded && !bWasGrounded && PreviousVerticalSpeed < -200) { Play(TEXT("S_Land"), .65f); StepDistance = 0; }
		if (bGrounded && bWasGrounded && Movement->Velocity.SizeSquared2D() > 400)
		{
			StepDistance += Distance;
			if (StepDistance >= 165.f)
			{
				StepDistance = FMath::Fmod(StepDistance, 165.f);
				PlayVariant(TEXT("S_Foot_concrete"), 5, .35f);
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
		Mining->OnPickaxeContact.RemoveAll(this);
		Mining->OnPickaxeBreak.RemoveAll(this);
	}
	if (Wallet.IsValid()) Wallet->OnCoinsAdded.RemoveAll(this);
	if (Music) { Music->Stop(); Music->DestroyComponent(); Music = nullptr; }
	Super::EndPlay(Reason);
}
