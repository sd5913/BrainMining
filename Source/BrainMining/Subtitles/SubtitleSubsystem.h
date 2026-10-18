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

	UPROPERTY(Transient)
	TObjectPtr<USoundWaveProcedural> ActiveGeneratedVoice;

	TArray<FName> Queue;
	int32 LineIndex = 0;
	FTimerHandle LineTimer;
	bool bPlaying = false;
};
