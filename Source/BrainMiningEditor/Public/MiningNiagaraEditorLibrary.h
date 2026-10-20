#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MiningNiagaraEditorLibrary.generated.h"
class UNiagaraSystem;
/** Scoped authoring access to the engine's Niagara stack editor. Editor only. */
UCLASS()
class UMiningNiagaraEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable) static FString InspectObject(UObject* Object);
    UFUNCTION(BlueprintCallable) static bool SetObjectProperty(UObject* Object,FName Property,const FString& Value);
    UFUNCTION(BlueprintCallable) static FString InspectStack(UNiagaraSystem* System,FName Emitter=NAME_None,FName Script=NAME_None,FName Module=NAME_None);
    UFUNCTION(BlueprintCallable) static bool SetStackValue(UNiagaraSystem* System,FName Emitter,FName Script,FName Module,const TArray<FName>& Inputs,const FString& StructPath,const FString& Json);
    UFUNCTION(BlueprintCallable) static bool CompileSystem(UNiagaraSystem* System);
};
