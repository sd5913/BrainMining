#include "Subtitles/SubtitleSubsystem.h"
#include "MiningLocalizationSubsystem.h"
#include "MiningAudioSettings.h"

#include "Subtitles/SubtitleSegment.h"
#include "Subtitles/SubtitleSettings.h"
#include "Subtitles/SubtitleWidget.h"
#include "Subtitles/SubtitleVoice.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWaveProcedural.h"
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
	FTSTicker::GetCoreTicker().RemoveTicker(PlaybackTicker);
	if (auto* Localization = GetGameInstance()->GetSubsystem<UMiningLocalizationSubsystem>())
		Localization->OnLanguageChanged.RemoveAll(this);
	StopAll();
	Super::Deinitialize();
}

void USubtitleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UMiningLocalizationSubsystem>();
	GetGameInstance()->GetSubsystem<UMiningLocalizationSubsystem>()->OnLanguageChanged.AddUObject(this, &USubtitleSubsystem::RefreshLanguage);
	// Collection and pause menus freeze world timers. Dialogue uses real time,
	// like its 2D voice, so it can finish while the catalogue remains open.
	PlaybackTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &USubtitleSubsystem::TickPlayback));
}

void USubtitleSubsystem::ScheduleLine(float Duration)
{
	LineSecondsRemaining = FMath::Max(.001f, Duration);
	PlaybackWorld = GetWorld();
}

bool USubtitleSubsystem::TickPlayback(float DeltaSeconds)
{
	if (!bPlaying) return true;
	if (!PlaybackWorld.IsValid() || PlaybackWorld.Get() != GetWorld())
	{
		StopAll();
		return true;
	}
	LineSecondsRemaining -= FMath::Max(0.f, DeltaSeconds);
	if (LineSecondsRemaining <= 0)
	{
		if (bInLineGap) ShowCurrentLine();
		else Advance();
	}
	return true;
}

void USubtitleSubsystem::RefreshLanguage(const FString& Culture)
{
	if (!bPlaying || bInLineGap || !GetWorld()) return;
	ShowCurrentLine();
}
void USubtitleSubsystem::SuppressForWorld(UWorld* World) { StopAll(); SuppressedWorld=World; }

void USubtitleSubsystem::StopAll()
{
	Queue.Reset();
	bInLineGap = false;
	StatusDuration = 0;
	bPlaying = false;
	ActiveSegment = nullptr;
	LineIndex = 0;
	StopLineAudio(ActiveAudio);
	ActiveGeneratedVoice = nullptr;
	LineSecondsRemaining = 0;
	PlaybackWorld.Reset();
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
}

void USubtitleSubsystem::PlaySegment(FName SegmentId)
{
    if(bPlaying && PlaybackWorld.Get()!=GetWorld())StopAll();
    if(GetWorld() && SuppressedWorld.Get()==GetWorld())return;
	if (SegmentId.IsNone())
	{
		return;
	}

	if (bPlaying)
	{
		if (!ActiveSegment || ActiveSegment->SegmentId != SegmentId) Queue.AddUnique(SegmentId);
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

void USubtitleSubsystem::PlayInteractionLines(FName Id, const TArray<FText>& Lines)
{
    if(bPlaying && PlaybackWorld.Get()!=GetWorld())StopAll();
    if (!GetWorld() || SuppressedWorld.Get()==GetWorld() || Lines.IsEmpty()) return;
    if (bPlaying && ActiveSegment && ActiveSegment->SegmentId==Id) return;
    StopAll();
    auto* Segment=NewObject<USubtitleSegment>(this);
    Segment->SegmentId=Id;
    for (const FText& Text:Lines) Segment->Lines.AddDefaulted_GetRef().Text=Text;
    StartSegment(Segment);
}

void USubtitleSubsystem::StopInteraction(FName Id)
{
    if(ActiveSegment && ActiveSegment->SegmentId==Id)StopAll();
}

FText USubtitleSubsystem::GetActiveText() const
{
    return ActiveSegment && ActiveSegment->Lines.IsValidIndex(LineIndex) ? ActiveSegment->Lines[LineIndex].Text : FText::GetEmpty();
}

void USubtitleSubsystem::PlayStatusLine(FText Text, float Duration)
{
    if(bPlaying && PlaybackWorld.Get()!=GetWorld())StopAll();
    if(GetWorld() && SuppressedWorld.Get()==GetWorld())return;
	if(Text.IsEmpty() || !GetWorld())return;
	// Brief matching/status messages must not erase an authored narrative queue.
	if(bPlaying && ActiveSegment && ActiveSegment->SegmentId != TEXT("GameplayStatus")) return;
	StopAll();
	auto* Segment=NewObject<USubtitleSegment>(this);
	Segment->SegmentId=TEXT("GameplayStatus");
	Segment->Lines.AddDefaulted_GetRef().Text=Text;
	StartSegment(Segment);
	StatusDuration = FMath::Max(.1f, Duration);
	ScheduleLine(StatusDuration);
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
	StatusDuration = 0;
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
	Widget->AddToViewport(90);
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
	bInLineGap = false;
	StopLineAudio(ActiveAudio);
	ActiveGeneratedVoice = nullptr;
	EnsureWidget();
	if (Widget)
	{
		Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
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
		ActiveAudio = UGameplayStatics::CreateSound2D(World, Sound, GetDefault<USubtitleSettings>()->VoiceVolume);
		if (ActiveAudio)
		{
			if (auto* Instance = World->GetGameInstance()) ActiveAudio->SoundClassOverride = Instance->GetSubsystem<UMiningAudioSettings>()->GetClass(4);
			ActiveAudio->Play();
		}
	}
	if (Line.Audio)
	{
		const float SoundDuration = Line.Audio->GetDuration();
		if (!Line.Audio->IsLooping() && FMath::IsFinite(SoundDuration) && SoundDuration > 0.05f && SoundDuration < 3600.f)
		{
			Duration = SoundDuration;
		}
	}

	ScheduleLine(StatusDuration > 0 ? StatusDuration : Duration);
}

void USubtitleSubsystem::Advance()
{
	++LineIndex;
	if (!ActiveSegment || !ActiveSegment->Lines.IsValidIndex(LineIndex))
	{
		FinishCurrent();
		return;
	}
	StopLineAudio(ActiveAudio);
	ActiveGeneratedVoice = nullptr;
	if (Widget) Widget->SetVisibility(ESlateVisibility::Hidden);
	const float Gap = FMath::Clamp(GetDefault<USubtitleSettings>()->LineGap, 0.f, 2.f);
	if (Gap > 0.f)
	{
		bInLineGap = true;
		ScheduleLine(Gap);
	}
	else ShowCurrentLine();
}

void USubtitleSubsystem::FinishCurrent()
{
	bInLineGap = false;
	StatusDuration = 0;
	StopLineAudio(ActiveAudio);
	ActiveGeneratedVoice = nullptr;
	bPlaying = false;
	ActiveSegment = nullptr;
	LineIndex = 0;
	LineSecondsRemaining = 0;
	PlaybackWorld.Reset();

	// Always retire the finished line, even if a queued asset was not cooked or
	// has been removed. A failed lookup must not leave its predecessor on screen.
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
	while (!Queue.IsEmpty())
	{
		const FName NextId = Queue[0];
		Queue.RemoveAt(0);
		PlaySegment(NextId);
		if (bPlaying) return;
	}
}
