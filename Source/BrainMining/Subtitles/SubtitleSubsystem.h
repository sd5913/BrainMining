#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SubtitleSubsystem.generated.h"

class USubtitleSegment;
class USubtitleWidget;
class UAudioComponent;

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

	UFUNCTION(BlueprintCallable, Category="字幕")
	void StopAll();

	virtual void Deinitialize() override;

private:
	USubtitleSegment* FindSegment(FName SegmentId) const;
	void StartSegment(USubtitleSegment* Segment);
	void ShowCurrentLine();
	void Advance();
	void FinishCurrent();
	void EnsureWidget();

	UPROPERTY()
	TObjectPtr<USubtitleSegment> ActiveSegment;

	UPROPERTY()
	TObjectPtr<USubtitleWidget> Widget;

	UPROPERTY()
	TObjectPtr<UAudioComponent> ActiveAudio;

	TArray<FName> Queue;
	int32 LineIndex = 0;
	FTimerHandle LineTimer;
	bool bPlaying = false;
};
