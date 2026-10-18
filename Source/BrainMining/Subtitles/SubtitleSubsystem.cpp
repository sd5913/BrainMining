#include "Subtitles/SubtitleSubsystem.h"
#include "MiningAudioSettings.h"

#include "Subtitles/SubtitleSegment.h"
#include "Subtitles/SubtitleWidget.h"
#include "Subtitles/SubtitleVoice.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWaveProcedural.h"
#include "TimerManager.h"
#include "Components/AudioComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubtitles, Log, All);

namespace
{
	void StopLineAudio(TObjectPtr<UAudioComponent>& Audio)
	{
		if (IsValid(Audio)) Audio->Stop();
		Audio = nullptr;
	}
}

USubtitleSubsystem* USubtitleSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USubtitleSubsystem>() : nullptr;
}

void USubtitleSubsystem::Deinitialize()
{
	StopAll();
	Super::Deinitialize();
}

void USubtitleSubsystem::StopAll()
{
	Queue.Reset();
	bPlaying = false;
	ActiveSegment = nullptr;
	LineIndex = 0;
	StopLineAudio(ActiveAudio);
	ActiveGeneratedVoice = nullptr;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LineTimer);
	}
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
}

void USubtitleSubsystem::PlaySegment(FName SegmentId)
{
	if (SegmentId.IsNone())
	{
		return;
	}

	if (bPlaying)
	{
		Queue.Add(SegmentId);
		return;
	}

	USubtitleSegment* Segment = FindSegment(SegmentId);
	if (!Segment)
	{
		UE_LOG(LogSubtitles, Warning, TEXT("找不到字幕段 %s。"), *SegmentId.ToString());
		return;
	}
	StartSegment(Segment);
}

USubtitleSegment* USubtitleSubsystem::FindSegment(FName SegmentId) const
{
	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FARFilter Filter;
	Filter.ClassPaths.Add(USubtitleSegment::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> Assets;
	AssetRegistry.Get().GetAssets(Filter, Assets);
	for (const FAssetData& Asset : Assets)
	{
		FString TagValue;
		if (!Asset.GetTagValue(FName(TEXT("SegmentId")), TagValue))
		{
			if (const USubtitleSegment* Loaded = Cast<USubtitleSegment>(Asset.GetAsset()))
			{
				TagValue = Loaded->SegmentId.ToString();
			}
		}
		TagValue.TrimQuotesInline();
		if (TagValue == SegmentId.ToString())
		{
			return Cast<USubtitleSegment>(Asset.GetAsset());
		}
	}
	return nullptr;
}

void USubtitleSubsystem::StartSegment(USubtitleSegment* Segment)
{
	if (!Segment || Segment->Lines.IsEmpty())
	{
		UE_LOG(LogSubtitles, Warning, TEXT("字幕段 %s 没有可播放的文本。"), Segment ? *Segment->SegmentId.ToString() : TEXT(""));
		FinishCurrent();
		return;
	}

	ActiveSegment = Segment;
	LineIndex = 0;
	bPlaying = true;
	EnsureWidget();
	ShowCurrentLine();
}

void USubtitleSubsystem::EnsureWidget()
{
	if (Widget || !GetWorld())
	{
		return;
	}

	APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	if (!Controller)
	{
		return;
	}

	Widget = CreateWidget<USubtitleWidget>(Controller);
	if (!Widget)
	{
		return;
	}

	Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
	Widget->AddToViewport(40);
	Widget->SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
	Widget->SetAlignmentInViewport(FVector2D::ZeroVector);
}

void USubtitleSubsystem::ShowCurrentLine()
{
	UWorld* World = GetWorld();
	if (!World || !ActiveSegment || !ActiveSegment->Lines.IsValidIndex(LineIndex))
	{
		FinishCurrent();
		return;
	}

	const FSubtitleLine& Line = ActiveSegment->Lines[LineIndex];
	StopLineAudio(ActiveAudio);
	ActiveGeneratedVoice = nullptr;
	EnsureWidget();
	if (Widget)
	{
		Widget->SetLine(Line.Text);
	}

	const FString Text = Line.Text.ToString();
	float Duration = SubtitleVoice::TextDuration(Text);
	USoundBase* Sound = Line.Audio;
	if (!Sound)
	{
		auto Clip = SubtitleVoice::Generate(Text, FMath::Rand());
		if (!Clip.Samples.IsEmpty())
		{
			ActiveGeneratedVoice = NewObject<USoundWaveProcedural>(this, NAME_None, RF_Transient);
			ActiveGeneratedVoice->SetSampleRate(Clip.SampleRate);
			ActiveGeneratedVoice->SetNumFrames(Clip.Samples.Num());
			ActiveGeneratedVoice->NumChannels = 1;
			ActiveGeneratedVoice->Duration = Clip.Duration;
			ActiveGeneratedVoice->SoundGroup = SOUNDGROUP_Voice;
			ActiveGeneratedVoice->QueueAudio(reinterpret_cast<const uint8*>(Clip.Samples.GetData()), Clip.Samples.Num() * sizeof(int16));
			Sound = ActiveGeneratedVoice;
		}
	}
	if (Sound)
	{
		ActiveAudio = UGameplayStatics::CreateSound2D(World, Sound);
		if (ActiveAudio)
		{
			if (auto* Instance = World->GetGameInstance()) ActiveAudio->SoundClassOverride = Instance->GetSubsystem<UMiningAudioSettings>()->GetClass(4);
			ActiveAudio->Play();
		}
	}
	if (Line.Audio)
	{
		const float SoundDuration = Line.Audio->GetDuration();
		if (SoundDuration > 0.05f)
		{
			Duration = SoundDuration;
		}
	}

	World->GetTimerManager().SetTimer(LineTimer, this, &USubtitleSubsystem::Advance, Duration, false);
}

void USubtitleSubsystem::Advance()
{
	++LineIndex;
	if (!ActiveSegment || !ActiveSegment->Lines.IsValidIndex(LineIndex))
	{
		FinishCurrent();
		return;
	}
	ShowCurrentLine();
}

void USubtitleSubsystem::FinishCurrent()
{
	StopLineAudio(ActiveAudio);
	ActiveGeneratedVoice = nullptr;
	bPlaying = false;
	ActiveSegment = nullptr;
	LineIndex = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LineTimer);
	}

	if (!Queue.IsEmpty())
	{
		const FName NextId = Queue[0];
		Queue.RemoveAt(0);
		PlaySegment(NextId);
		return;
	}

	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
}
