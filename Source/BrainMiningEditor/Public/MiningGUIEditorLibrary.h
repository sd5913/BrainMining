#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MiningGUIEditorLibrary.generated.h"

class APlayerController;
class UButton;

/** One-time asset creation, not runtime layout. Existing user-edited assets are preserved. */
UCLASS()
class BRAINMININGEDITOR_API UMiningGUIEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Mining GUI|Editor")
	static bool CreateMiningGUIAssets();
	/** Editor-only validation through the game input and actual Slate button delegates. */
	UFUNCTION(BlueprintCallable, Category="Mining GUI|Validation")
	static void SendPreviewKey(APlayerController* Controller, FName Key, bool bPressed);
	UFUNCTION(BlueprintCallable, Category="Mining GUI|Validation")
	static bool SendPreviewFocusedKey(APlayerController* Controller, FName Key, bool bPressed);
	UFUNCTION(BlueprintCallable, Category="Mining GUI|Validation")
	static void SendPreviewMouseAxis(APlayerController* Controller, float DeltaX, float DeltaY);
	UFUNCTION(BlueprintCallable, Category="Mining GUI|Validation")
	static bool SimulatePreviewButton(UButton* Button, bool bPressed);
	UFUNCTION(BlueprintCallable, Category="Mining GUI|Validation")
	static void StartFloatingPreview();
	UFUNCTION(BlueprintCallable, Category="Mining GUI|Editor")
	static bool RepairMissingGUIFonts();
	/** Explicit asset edit for this prototype; not run automatically over user edits. */
	UFUNCTION(BlueprintCallable, Category="Mining GUI|Editor")
	static bool ClearGUIPlaceholderNumbers();
};
