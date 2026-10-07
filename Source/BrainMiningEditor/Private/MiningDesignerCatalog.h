#pragma once

#include "MiningDesignerLibrary.h"

namespace MiningDesigner
{
	UClass* Family(const UObject* Object);
	TArray<FName> Fields(const UObject* Object, EMiningDesignerSection Section);
	bool Transferable(const FProperty* Property);
	FString Caption(FName Name);
	FString Help(FName Name);
	FString SectionName(EMiningDesignerSection Section);
	FString Effect(EMiningDesignerSection Section);
	FString Status(const UObject* Object);
}
