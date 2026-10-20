#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProjectTypographyEditorLibrary.generated.h"

UCLASS()
class UProjectTypographyEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Import the project OTFs and migrate every project widget blueprint. Does not save maps. */
    UFUNCTION(BlueprintCallable, Category="Project|Typography")
    static bool InstallProjectFonts();
    /** Embed the two supplied ending faces without changing project-wide typography. */
    UFUNCTION(BlueprintCallable, Category="Project|Typography")
    static bool InstallCreditsFonts();
};
