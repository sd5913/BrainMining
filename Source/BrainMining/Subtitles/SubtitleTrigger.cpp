#include "Subtitles/SubtitleTrigger.h"

#include "Subtitles/SubtitleLibrary.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"

ASubtitleTrigger::ASubtitleTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(140.f, 140.f, 100.f));
	Box->SetCollisionProfileName(TEXT("Trigger"));
	Box->SetGenerateOverlapEvents(true);
	Box->SetHiddenInGame(true);
#if WITH_EDITORONLY_DATA
	Box->ShapeColor = FColor(90, 170, 255);
#endif
	Box->OnComponentBeginOverlap.AddDynamic(this, &ASubtitleTrigger::OnBoxOverlap);
}

void ASubtitleTrigger::SetSegmentId(FName InSegmentId)
{
	SegmentId = InSegmentId;
}

void ASubtitleTrigger::OnBoxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bPlayed || SegmentId.IsNone())
	{
		return;
	}
	// The card introduction follows successful matchmaking, never the approach volume.
	if (SegmentId == TEXT("卡牌玩法1")) return;

	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	bPlayed = true;
	USubtitleLibrary::PlaySubtitleSegment(this, SegmentId);
}
