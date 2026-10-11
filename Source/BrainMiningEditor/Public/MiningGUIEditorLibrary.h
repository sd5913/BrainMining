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
	/** Capture the PIE game viewport, rather than the editor's active viewport. */
	UFUNCTION(BlueprintCallable, Category="Mining GUI|Validation")
	static bool CapturePreview(APlayerController* Controller, const FString& Filename);
	UFUNCTION(BlueprintCallable, Category="Mining GUI|Editor")
	static bool RepairMissingGUIFonts();
	/** Explicit asset edit for this prototype; not run automatically over user edits. */
	UFUNCTION(BlueprintCallable, Category="Mining GUI|Editor")
	static bool ClearGUIPlaceholderNumbers();
	/** Add only the editable cooldown overlay, preserving the existing icon artwork/layout. */
	UFUNCTION(BlueprintCallable, Category="Mining GUI|Editor")
	static bool AddDrillCooldownOverlay();
	/** Adds only the jetpack child to the existing HUD; retains all existing ability/upgrade widgets. */
	UFUNCTION(BlueprintCallable, Category="Mining GUI|Editor")
	static bool AddJetpackHeatOverlay();
};
