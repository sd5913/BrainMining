#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CollectionUMGEditorLibrary.generated.h"
class UMiningCollectionWidget;
class UWidget;
UCLASS()
class BRAINMININGEDITOR_API UCollectionUMGEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Creates the initial designer tree once. Never replaces a user's edited widget. */
    UFUNCTION(BlueprintCallable,Category="Collection|Editor") static bool CreateCollectionInspectWidget();
    /** Explicit redesign command; authoring script backs up the previous asset first. */
    UFUNCTION(BlueprintCallable,Category="Collection|Editor") static bool ApplyCollectionInspectRedesign();
    /** Editor QA dispatches widget pointer handlers using the current designer geometry. */
    UFUNCTION(BlueprintCallable,Category="Collection|Validation") static bool ExerciseInspectPointer(UMiningCollectionWidget* Widget, bool bZoom);
    UFUNCTION(BlueprintPure,Category="Collection|Validation") static UWidget* FindInspectControl(UMiningCollectionWidget* Widget,FName Name);
};
