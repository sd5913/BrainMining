#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MainGameSubtitleDirector.generated.h"

class USubtitleSegment;
class ASubtitleTrigger;

// Lives in the subtitle sublevel so the narrative wiring travels with it.
UCLASS()
class BRAINMINING_API AMainGameSubtitleDirector : public AActor
{
    GENERATED_BODY()
public:
    AMainGameSubtitleDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    void PlayOnce(FName SegmentId);
    UFUNCTION(BlueprintPure, Category="Subtitles") bool HasTriggered(FName SegmentId) const { return Played.Contains(SegmentId); }
    UPROPERTY(EditAnywhere, Category="Subtitles") TArray<TObjectPtr<USubtitleSegment>> Segments;
    UPROPERTY(EditAnywhere, Category="Subtitles") TObjectPtr<ASubtitleTrigger> MuseumEntry;
    UPROPERTY(EditAnywhere, Category="Subtitles") TObjectPtr<ASubtitleTrigger> MuseumEnd;
    UPROPERTY(EditAnywhere, Category="Subtitles") FBox MuseumBounds;
    UPROPERTY(EditAnywhere, Category="Subtitles") float MuseumHintSeconds = 300.f;
private:
    UFUNCTION() void OnCardsCompleted(FName RoomId);
    TSet<FName> Played;
    float MuseumSeconds = 0;
};
