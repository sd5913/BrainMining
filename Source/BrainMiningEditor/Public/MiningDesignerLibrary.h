#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MiningDesignerLibrary.generated.h"

class AActor;

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
    UFUNCTION(BlueprintCallable, Category="Mining Designer|Showcase")
    static TArray<AActor*> CopyExperienceActors(const FString& SourceMap, FVector Offset, FName AreaTag, TSubclassOf<AActor> ActorClass = nullptr);
    UFUNCTION(BlueprintCallable, Category="Mining Designer|Showcase")
    static bool BuildTreasureMeshes();
    UFUNCTION(BlueprintCallable, Category="Mining Designer|Showcase")
    static TArray<AActor*> ImportShowcaseRoom(const FString& SourceMap, FVector Offset, const TArray<FString>& ExcludedLabels);
	UFUNCTION(BlueprintCallable, Category="Mining Designer")
	static void OpenDesigner();
	UFUNCTION(BlueprintCallable, Category="Mining Designer")
	static void OpenManualEditor();
	UFUNCTION(BlueprintCallable, Category="Mining Designer")
	static void OpenSpatialEditor();
	UFUNCTION(BlueprintCallable, Category="Mining Designer|Spatial")
	static bool CheckSpatialEdit(AActor* Actor);
	UFUNCTION(BlueprintCallable, Category="Mining Designer|Spatial")
	static bool SmoothSpatialIssue();
	UFUNCTION(BlueprintPure, Category="Mining Designer|Spatial")
	static FString SpatialEditingStatus();
	UFUNCTION(BlueprintCallable, Category="Mining|Spatial")
	static FString SpatialDragPerformance();
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
