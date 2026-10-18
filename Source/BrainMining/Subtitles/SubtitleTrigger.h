#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SubtitleTrigger.generated.h"

class UBoxComponent;

UCLASS(meta=(DisplayName="字幕触发盒"))
class BRAINMINING_API ASubtitleTrigger : public AActor
{
	GENERATED_BODY()

public:
	ASubtitleTrigger();

	void SetSegmentId(FName InSegmentId);
	bool HasPlayed() const { return bPlayed; }

protected:
	UPROPERTY(VisibleAnywhere, Category="字幕")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="字幕", meta=(DisplayName="字幕段ID"))
	FName SegmentId;

	UFUNCTION()
	void OnBoxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	bool bPlayed = false;
};
