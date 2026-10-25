#include "MiningAudioComponent.h"
#include "MiningComponent.h"
#include "Showcase/MiningExhibitHall.h"
#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"

bool UMiningAudioComponent::IsMainGameWorld(const UWorld* World)
{
	return World && UGameplayStatics::GetCurrentLevelName(World, true) == TEXT("Lvl_MainGame");
}

TArray<FName> UMiningAudioComponent::MainGameAssetNames()
{
	TArray<FName> Names;
	for (const auto* Name : {TEXT("S_PickaxePickup"), TEXT("S_JetpackEquip"), TEXT("S_DirtDig"),
		TEXT("S_Foot_dirt_00"), TEXT("S_Foot_gravel_00"), TEXT("S_BedrockContact"),
		TEXT("S_DrillStart"), TEXT("S_DrillIdle_Loop"), TEXT("S_DrillContact_Loop"), TEXT("S_DrillDirt_Loop"), TEXT("S_DrillStop"),
		TEXT("S_JetStart"), TEXT("S_Jet_Loop"), TEXT("S_JetStop"), TEXT("S_BombExplosion"),
		TEXT("A_MineWind_Loop"), TEXT("A_Cavern_Loop"), TEXT("A_Drips_Loop"), TEXT("M_CaveTheme_Loop")}) Names.Add(FName(Name));
	return Names;
}

EMiningAudioSurface UMiningAudioComponent::ResolveSurface(const FHitResult& Hit)
{
	const auto* Component = Hit.GetComponent();
	const auto* Actor = Hit.GetActor();
	auto HasTag = [Actor, Component](FName Tag)
	{
		return (Actor && Actor->ActorHasTag(Tag)) || (Component && Component->ComponentHasTag(Tag));
	};
	if (HasTag(TEXT("BM_Audio_Wood"))) return EMiningAudioSurface::Wood;
	if (HasTag(TEXT("BM_Audio_Dirt"))) return EMiningAudioSurface::Dirt;
	if (HasTag(TEXT("BM_Audio_Gravel"))) return EMiningAudioSurface::Gravel;
	if (!Component) return EMiningAudioSurface::Stone;
	int32 Section = 0;
	const auto* Material = Hit.FaceIndex != INDEX_NONE ? Component->GetMaterialFromCollisionFaceIndex(Hit.FaceIndex, Section) : nullptr;
	if (!Material) Material = Component->GetMaterial(0);
	if (!Material) return EMiningAudioSurface::Stone;
	const FString Name = Material->GetName();
	if (Name.Contains(TEXT("Oak")) || Name.Contains(TEXT("Wood"))) return EMiningAudioSurface::Wood;
	if (Name.Contains(TEXT("Gravel"))) return EMiningAudioSurface::Gravel;
	float Core = 0;
	Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("MudCore")), Core);
	if (Core >= .5f) return EMiningAudioSurface::Dirt;
	float Count = 0;
	float Reach = 240;
	Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("MudCount")), Count);
	Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("MudReach")), Reach);
	for (int32 Index = 0; Index < FMath::Clamp(FMath::RoundToInt(Count), 0, 32); ++Index)
	{
		FLinearColor Start = FLinearColor::Transparent;
		FLinearColor End = FLinearColor::Transparent;
		if (!Material->GetVectorParameterValue(FMaterialParameterInfo(*FString::Printf(TEXT("MudStart%d"), Index)), Start) ||
			!Material->GetVectorParameterValue(FMaterialParameterInfo(*FString::Printf(TEXT("MudEnd%d"), Index)), End)) continue;
		const FVector StartPosition(Start.R, Start.G, Start.B);
		const FVector Axis = FVector(End.R, End.G, End.B) - StartPosition;
		const double Length = FMath::Max(Axis.Size(), .01);
		const FVector Direction = Axis / Length;
		const FVector Offset = Hit.ImpactPoint - StartPosition;
		const double Along = FVector::DotProduct(Offset, Direction);
		const double Radial = (Offset - Direction * Along).Size();
		const double Radius = FMath::Lerp(double(Start.A), double(End.A), FMath::Clamp(Along / Length, 0., 1.));
		const double Outside = FMath::Max(FMath::Max(-Along, Along - Length), 0.);
		const double Weight = (1. - FMath::SmoothStep(0., FMath::Max(double(Reach), 1.), Outside)) *
			(1. - FMath::SmoothStep(Radius + 40., Radius + 200., Radial));
		if (Weight >= .5) return EMiningAudioSurface::Dirt;
	}
	return EMiningAudioSurface::Stone;
}

void UMiningAudioComponent::PlayStarterPickup(bool bJetpack)
{
	if (bMainGameAudio) Play(bJetpack ? TEXT("S_JetpackEquip") : TEXT("S_PickaxePickup"), .6f);
}

void UMiningAudioComponent::OnBlockedContact(const FHitResult& Hit)
{
	if (!bMainGameAudio || !CanPlay() || !Hit.bBlockingHit || GetWorld()->GetTimeSeconds() < NextBlockedTime) return;
	NextBlockedTime = GetWorld()->GetTimeSeconds() + .45;
	Play(TEXT("S_BedrockContact"), .25f);
}

void UMiningAudioComponent::PlayFootstep(const FHitResult& Hit, float Gain)
{
	const auto Surface = bMainGameAudio ? ResolveSurface(Hit) : EMiningAudioSurface::Stone;
	switch (Surface)
	{
	case EMiningAudioSurface::Wood: PlayVariant(TEXT("S_Foot_wood"), 5, Gain); break;
	case EMiningAudioSurface::Dirt: PlayVariant(TEXT("S_Foot_dirt"), 1, Gain); break;
	case EMiningAudioSurface::Gravel: PlayVariant(TEXT("S_Foot_gravel"), 1, Gain); break;
	default: PlayVariant(TEXT("S_Foot_concrete"), 5, Gain); break;
	}
}

int32 UMiningAudioComponent::FindAudioRegion() const
{
	if (!Hall.IsValid() || !Hall->bReady || !Hall->Areas.IsValidIndex(Hall->ActiveArea) || !Character.IsValid()) return INDEX_NONE;
	const auto& Area = Hall->Areas[Hall->ActiveArea];
	if (Area.Kind != EMiningExperienceKind::Mining) return INDEX_NONE;
	int32 MiningRegion = 0;
	for (int32 Index = 0; Index <= Hall->ActiveArea; ++Index)
		if (Hall->Areas[Index].Kind == EMiningExperienceKind::Mining) ++MiningRegion;
	if (MiningRegion == 1 && Area.SourceLevel)
	{
		const FVector Local = Area.SourceLevel->LevelTransform.InverseTransformPosition(Character->GetActorLocation());
		const float Margin = AudioRegion == 0 ? 70.f : 0.f;
		if (FMath::Abs(Local.X) < 500 + Margin && FMath::Abs(Local.Y) < 400 + Margin && Local.Z > -100 && Local.Z < 450)
			return 0;
	}
	return MiningRegion;
}

void UMiningAudioComponent::SetSoundscapeLayer(FName Name, float Target, float DeltaTime)
{
	auto* Existing = Soundscape.Find(Name);
	if (!Existing && Target > 0)
	{
		const auto* Sound = Sounds.Find(Name);
		if (!Sound) return;
		auto* Layer = NewObject<UAudioComponent>(GetOwner());
		Layer->bAutoActivate = false;
		Layer->bAutoDestroy = false;
		Layer->bAllowSpatialization = false;
		Layer->SetSound(Sound->Get());
		Layer->SetVolumeMultiplier(0.f);
		Layer->RegisterComponent();
		Soundscape.Add(Name, Layer);
		Existing = Soundscape.Find(Name);
	}
	if (!Existing) return;
	auto* Layer = Existing->Get();
	const bool bRoomMusic = Name.ToString().StartsWith(TEXT("M_Room_"));
	const float FadeSpeed = bRoomMusic ? .09f : Name.ToString().StartsWith(TEXT("M_")) ? .28f : .045f;
	const float Gain = FMath::FInterpConstantTo(Layer->VolumeMultiplier, Target, DeltaTime, FadeSpeed);
	Layer->SetVolumeMultiplier(Gain);
	if (Target > 0 && !Layer->IsPlaying()) Layer->Play();
	// Keep the exploration loop's playback timeline alive underneath room music.
	// Re-entering a mine only fades it back in, rather than restarting a stopped voice.
	else if (Gain <= 0 && Layer->IsPlaying() && Name != TEXT("M_DreamyFlashback_Loop")) Layer->Stop();
	if (bRoomMusic && Target <= 0 && Gain <= 0)
	{
		Layer->DestroyComponent();
		Soundscape.Remove(Name);
	}
}

void UMiningAudioComponent::UpdateMainGameSoundscape(float DeltaTime)
{
	if (!CanPlay()) { StopSoundscape(); return; }
	const int32 Region = FindAudioRegion();
	if (Region != PendingAudioRegion) { PendingAudioRegion = Region; RegionStableSeconds = 0; }
	else RegionStableSeconds += DeltaTime;
	if (AudioRegion == INDEX_NONE || RegionStableSeconds >= .5f) AudioRegion = PendingAudioRegion;
	static const auto* MainGameMusicVolume = IConsoleManager::Get().FindConsoleVariable(TEXT("bm.Audio.MusicVolume"));
	const float Trim = MainGameMusicVolume ? FMath::Clamp(MainGameMusicVolume->GetFloat(), 0.f, 1.f) : .12f;
	const bool bBusy = bJetRunning || (bDrillRunning && Mining.IsValid() && Mining->IsMiningActive());
	const float MusicTarget = Trim * (bBusy ? .65f : 1.f);
	UpdateRoomMusic(DeltaTime, MusicTarget);
	const bool bMine = AudioRegion >= 1 && AudioRegion <= 4;
	const float Bed = bBusy ? .55f : 1.f;
	SetSoundscapeLayer(TEXT("M_DreamyFlashback_Loop"), AudioRegion == 0 ? MusicTarget * .6f : bMine ? MusicTarget : 0.f, DeltaTime);
	SetSoundscapeLayer(TEXT("A_MineWind_Loop"), bMine ? (AudioRegion == 1 ? .075f : AudioRegion == 4 ? .045f : .015f) * Bed : 0.f, DeltaTime);
	SetSoundscapeLayer(TEXT("A_Cavern_Loop"), AudioRegion >= 2 && AudioRegion <= 4 ? (AudioRegion == 2 ? .065f : .035f) * Bed : 0.f, DeltaTime);
	SetSoundscapeLayer(TEXT("A_Drips_Loop"), AudioRegion == 3 ? .04f * Bed : 0.f, DeltaTime);
}

void UMiningAudioComponent::StopSoundscape()
{
	for (auto& Entry : Soundscape)
		if (Entry.Value) { Entry.Value->Stop(); Entry.Value->DestroyComponent(); }
	Soundscape.Empty();
	AudioRegion = PendingAudioRegion = INDEX_NONE;
	RegionStableSeconds = 0;
}
