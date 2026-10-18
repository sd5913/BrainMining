#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PopupClose/PopupOfficeFlood.h"
#include "PopupClose/PopupOfficeExit.h"
#include "PopupCloseStage.generated.h"

class UArrowComponent;
class UBoxComponent;
class UCameraComponent;
class UDirectionalLightComponent;
class UMaterialInterface;
class UPointLightComponent;
class UPopupCloseDesktopWidget;
class UPopupClosePromptWidget;
class UPrimitiveComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UWidgetComponent;
class UWidgetInteractionComponent;
class APopupCloseFlyingWindow;
class APopupOfficeDevice;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPopupOfficeExitEvent);

UCLASS()
class BRAINMINING_API APopupCloseStage : public AActor
{
	GENERATED_BODY()

public:
	APopupCloseStage();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art") bool bInteriorLighting=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art") bool bReplaceCentralDesk=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art",meta=(ClampMin="0")) float InteriorFillIntensity=180.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art") TObjectPtr<UMaterialInterface> FloorArtMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art") TObjectPtr<UMaterialInterface> WallArtMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art") TObjectPtr<UMaterialInterface> CeilingArtMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art") TObjectPtr<UMaterialInterface> DesktopArtMaterial;
 UFUNCTION(BlueprintCallable,CallInEditor,Category="Office Art") void RefreshOfficeArt(){ConfigureOffice();ApplyColors();}
 virtual ~APopupCloseStage() override;
 UPROPERTY(EditAnywhere, Category="Integration") bool bAreaManaged = false;
 UPROPERTY(EditAnywhere, Category="Integration") bool bStartOnAreaEntry = false;
 bool bAreaActive = true;
 void SetAreaActive(bool bActive);
 void CancelSession();
 void ResetCompletedSessions();
 int32 GetCompletedSessions() const { return Exit && Exit->DoorIsOpen() ? 1 : 0; }
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PopupClose|Exit", meta=(ClampMin="15", ClampMax="15", Units="s")) float EruptionBeforeBombSeconds=15.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PopupClose|Exit", meta=(ClampMin="2", Units="s")) float EvacuationBombSeconds=5.f;
 UPROPERTY(BlueprintAssignable, Category="PopupClose|Exit") FPopupOfficeExitEvent OnOfficeEscaped;
 UPROPERTY(BlueprintAssignable, Category="PopupClose|Exit") FPopupOfficeExitEvent OnOfficeDestroyed;
 UFUNCTION(BlueprintPure, Category="PopupClose|Exit") bool IsOfficeEscaped() const { return Exit && Exit->HasEscaped(); }
 UFUNCTION(BlueprintPure, Category="PopupClose|Exit") bool IsOfficeDestroyed() const { return Exit && Exit->IsRuined(); }
 UFUNCTION(BlueprintCallable, Category="PopupClose|Exit") FString GetExitStatus() const;
 bool IsSessionActive() const { return View != EView::Free; }
 FString GetInteractionAction(APlayerController* PC) const;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UFUNCTION(BlueprintCallable, Category="PopupClose|Validation") FString ValidateOffice();
	UFUNCTION(BlueprintCallable, Category="PopupClose|Validation") FString GetSessionStatus() const;
	UFUNCTION(BlueprintCallable, Category="PopupClose") void StartOfficeRound();
	bool HandleInteractionInput(APlayerController* PC);
	bool HandleMobilePrimary(APlayerController* PC);

	UFUNCTION(BlueprintPure, Category="PopupClose") FTransform GetPlayerSpawnTransform() const;

#if WITH_EDITOR
	static void CreateLevel();
	static void UpgradeOffice();
#endif

private:
 friend class FPopupOfficeExit;
 UFUNCTION() void OnPlayerBombExploded(FVector Location);

 enum class EPhase : uint8 { Idle, Running, Spraying, Complete };
 enum class EView : uint8 { Free, Entering, InScreen, Exiting };
 void ApplyColors();
 void ConfigureArtRendering(bool bActive);
 TOptional<int32> SavedOfficeAntiAliasing;
 void ConfigureOffice();
 void Paint(UStaticMeshComponent* Mesh,const FLinearColor& Color) const;
 void GatherDevices();
 APopupOfficeDevice* FocusedDevice(APlayerController* PC) const;
 void HandleInteraction(APlayerController* PC,APopupOfficeDevice* Device);
 void BeginScreenSession(APlayerController* PC,APopupOfficeDevice* Device);
 void ExitScreen();
 void RestorePlayerControl();
 void TickView(float Dt);
 void TickIssues(float Dt);
 void BeginBurst();
 void TickSpray(float Dt);
 void ShowPrompt(APlayerController* PC,const FString& Message);
 void WriteSessionReport();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Floor;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Ceiling;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> WallPositiveY;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> WallNegativeY;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ExitDoorModel;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> TableTop;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Monitor;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Screen;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Stand;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Keyboard;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> AimVolume;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> ScreenCamera;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SpawnPoint;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UArrowComponent> SpawnArrow;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkyLightComponent> Sky;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> RoomLight;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY()
	TObjectPtr<UPopupCloseDesktopWidget> Desktop;

	UPROPERTY()
	TObjectPtr<UPopupClosePromptWidget> Prompt;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetComponent> DesktopSurface;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetInteractionComponent> DesktopInteraction;
	UPROPERTY() TArray<TObjectPtr<APopupCloseFlyingWindow>> WindowPool;

 UPROPERTY() TArray<TObjectPtr<APopupOfficeDevice>> Devices;
 UPROPERTY() TObjectPtr<APopupOfficeDevice> ActiveComputer;
 EPhase Phase=EPhase::Idle;
 EView View=EView::Free;
 float RoundTime=0;
 float ViewTime=0;
 // Independent countdowns in interval units; randomized again after every issue.
 float IssueCountdown[4]={0,0,0,0};
 float SlowPhaseScheduledIssues=0;
 float SprayTime=0;
 float SprayDebt=0;
 double LastTickTime=0;
 float FirstBurstPeakFrame=0;
 bool bPanic=false;
 bool bAccountingValid=true;
 bool bWarningsValid=true;
 bool bIndependentTimersValid=true;
 bool bForcedExit=false;
 uint8 SavedMovementMode=1;
 bool bMoveSuppressed=false;
 bool bLookSuppressed=false;
 TWeakObjectPtr<APawn> LockedPawn;
 TWeakObjectPtr<APlayerController> LockedController;
 TUniquePtr<FPopupOfficeFlood> Flood;
 TUniquePtr<FPopupOfficeExit> Exit;
};
