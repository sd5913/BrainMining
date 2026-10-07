#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MiningDesignerLibrary.generated.h"

UENUM(BlueprintType)
enum class EMiningDesignerSection : uint8
{
	Gameplay, Mining, Terrain, Visual, Advanced
};

UCLASS()
class BRAINMININGEDITOR_API UMiningDesignerLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Mining Designer")
	static void OpenDesigner();
	UFUNCTION(BlueprintCallable, Category="Mining Designer")
	static FString CapturePreset(UObject* Target, EMiningDesignerSection Section);
	UFUNCTION(BlueprintCallable, Category="Mining Designer")
	static bool ApplyPreset(UObject* Target, EMiningDesignerSection Section, const FString& Json, FString& Error);
	UFUNCTION(BlueprintCallable, Category="Mining Designer")
	static bool ResetSection(UObject* Target, EMiningDesignerSection Section, FString& Error);
	UFUNCTION(BlueprintCallable, Category="Mining Designer")
	static bool SavePreset(UObject* Target, EMiningDesignerSection Section, const FString& Name, bool bOverwrite, FString& Error);
	UFUNCTION(BlueprintCallable, Category="Mining Designer")
	static bool LoadPreset(UObject* Target, EMiningDesignerSection Section, const FString& Name, FString& Error);
	UFUNCTION(BlueprintCallable, Category="Mining Designer")
	static TArray<FString> ListPresets(UObject* Target, EMiningDesignerSection Section);
	UFUNCTION(BlueprintPure, Category="Mining Designer")
	static bool CanEdit(UObject* Target);
};
