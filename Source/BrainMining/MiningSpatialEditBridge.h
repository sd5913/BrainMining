#pragma once

#include "CoreMinimal.h"

class AActor;

namespace MiningSpatialEditing
{
#if WITH_EDITOR
	DECLARE_DELEGATE_RetVal_OneParam(bool, FEditHandler, AActor*);
	extern BRAINMINING_API FEditHandler Handler;
	BRAINMINING_API bool Handle(AActor* Actor);
#endif
}
