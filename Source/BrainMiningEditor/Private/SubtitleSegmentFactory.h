#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "SubtitleSegmentFactory.generated.h"

UCLASS()
class USubtitleSegmentFactory : public UFactory
{
	GENERATED_BODY()

public:
	USubtitleSegmentFactory();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual FText GetDisplayName() const override;
};
