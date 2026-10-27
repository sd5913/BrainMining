#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SubtitleSubsystem.generated.h"

class USubtitleSegment;
class USubtitleWidget;
class UAudioComponent;
class USoundWaveProcedural;

UCLASS()
class BRAINMINING_API USubtitleSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static USubtitleSubsystem* Get(const UObject* WorldContext);

	UFUNCTION(BlueprintCallable, Category="字幕")
	void PlaySegment(FName SegmentId);
	// Time-sensitive gameplay status uses the same subtitle widget and styling.
	UFUNCTION(BlueprintCallable, Category="字幕")
	void PlayStatusLine(FText Text, float Duration);
	// Immediate interaction feedback replaces older interaction lines without a queue.
	void PlayInteractionLines(FName Id, const TArray<FText>& Lines);
	void StopInteraction(FName Id);
	bool IsPlaying() const { return bPlaying; }
    UFUNCTION(BlueprintPure) FText GetActiveText() const;
    UFUNCTION(BlueprintPure) int32 GetPendingSegmentCount() const { return Queue.Num(); }

	UFUNCTION(BlueprintCallable, Category="字幕")
	void StopAll();
    void SuppressForWorld(UWorld* World);

	virtual void Deinitialize() override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
	USubtitleSegment* FindSegment(FName SegmentId) const;
	void StartSegment(USubtitleSegment* Segment);
	void ShowCurrentLine();
	void Advance();
	void FinishCurrent();
	void EnsureWidget();
	void RefreshLanguage(const FString& Culture);

	UPROPERTY()
	TObjectPtr<USubtitleSegment> ActiveSegment;

	UPROPERTY()
	TObjectPtr<USubtitleWidget> Widget;

	UPROPERTY()
	TObjectPtr<UAudioComponent> ActiveAudio;

	UPROPERTY(Transient)
	TObjectPtr<USoundWaveProcedural> ActiveGeneratedVoice;

	TArray<FName> Queue;
	int32 LineIndex = 0;
	FTimerHandle LineTimer;
	bool bPlaying = false;
	bool bInLineGap = false;
	float StatusDuration = 0;
    TWeakObjectPtr<UWorld> SuppressedWorld;
};
