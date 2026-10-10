#include "SubtitleSegmentFactory.h"

#include "Subtitles/SubtitleSegment.h"

USubtitleSegmentFactory::USubtitleSegmentFactory()
{
	SupportedClass = USubtitleSegment::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* USubtitleSegmentFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<USubtitleSegment>(InParent, InClass ? InClass : USubtitleSegment::StaticClass(), InName, Flags);
}

FText USubtitleSegmentFactory::GetDisplayName() const
{
	return FText::FromString(TEXT("字幕段"));
}
