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

UCLASS()
class BRAINMINING_API APopupCloseStage : public AActor
{
	GENERATED_BODY()

public:
	APopupCloseStage();
    UPROPERTY(EditAnywhere, Category="Integration") bool bAreaManaged = false;
    bool bAreaActive = true;
    void SetAreaActive(bool bActive);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	void CancelSession();
	int32 GetCompletedSessions() const { return CompletedSessions; }
    void ResetCompletedSessions() { CompletedSessions = 0; }
	bool IsSessionActive() const { return Phase != EPhase::Idle; }

	FTransform GetPlayerSpawnTransform() const;

#if WITH_EDITOR
	static void CreateLevel();
#endif

private:
	enum class EPhase : uint8
	{
		Idle,
		Entering,
		InScreen,
		Spraying
	};

	void ApplyColors();
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

	EPhase Phase = EPhase::Idle;
	int32 CompletedSessions = 0;
	float EnterTime = 0.f;
	float ScreenTime = 0.f;
	float SpawnDebt = 0.f;
	float ClimaxTime = 0.f;
	float SprayTime = 0.f;
	float SprayDebt = 0.f;
	bool bLookSuppressed = false;
	bool bMoveSuppressed = false;
	TWeakObjectPtr<APawn> LockedPawn;
	TWeakObjectPtr<APlayerController> LockedController;
};
