#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

UCLASS()
class BRAINMINING_API APopupCloseStage : public AActor
{
	GENERATED_BODY()

public:
	APopupCloseStage();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UFUNCTION(BlueprintCallable, Category="PopupClose|Validation") FString ValidateOffice();
	UFUNCTION(BlueprintCallable, Category="PopupClose|Validation") FString GetSessionStatus() const;

	FTransform GetPlayerSpawnTransform() const;

#if WITH_EDITOR
	static void CreateLevel();
	static void UpgradeOffice();
#endif

private:
	enum class EPhase : uint8
	{
		Idle,
		Entering,
		InScreen,
		Spraying,
		Complete
	};

	void ApplyColors();
	void ConfigureOffice();
	void WarmWindowPool(float DeltaSeconds);
	void FitScreenCamera();
	void WriteSessionReport();
	void Paint(UStaticMeshComponent* Mesh, const FLinearColor& Color) const;
	bool IsComputerComponent(const UPrimitiveComponent* Component) const;
	bool CanInteract(APlayerController* Controller) const;
	void SetPromptVisible(APlayerController* Controller, bool bVisible);
	void BeginScreenSession(APlayerController* Controller, APawn* Pawn);
	void TickScreen(float DeltaSeconds);
	void EjectFromScreen();
	void TickSpray(float DeltaSeconds);
	void SpawnFlyingWindow();
	void RestorePlayerControl(bool bRestoreLook);
	float CurrentSpawnInterval() const;

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

	EPhase Phase = EPhase::Idle;
	float EnterTime = 0.f;
	float ScreenTime = 0.f;
	float SpawnDebt = 0.f;
	bool bPopupAccountingValid = true;
	float SprayTime = 0.f;
	float SprayDebt = 0.f;
	float WarmDebt = 0.f;
	int32 PoolCursor = 0;
	int32 LaunchedWindows = 0;
	int32 TotalPopups = 0;
	float FirstSprayDelay = -1.f;
	float SprayPeakFrame = 0.f;
	float EjectionPeakFrame = 0.f;
	double LastTickTime = 0;
	float WallFrameSeconds = 0;
	uint8 SavedMovementMode = 1;
	bool bLookSuppressed = false;
	bool bMoveSuppressed = false;
	TWeakObjectPtr<APawn> LockedPawn;
	TWeakObjectPtr<APlayerController> LockedController;
};
