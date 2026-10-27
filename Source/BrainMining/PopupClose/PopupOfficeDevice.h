#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraTypes.h"
#include "PopupOfficeDevice.generated.h"
class UBoxComponent;
class UCameraComponent;
class UInstancedStaticMeshComponent;
class UWidgetComponent;
class UWidgetInteractionComponent;
class UPopupCloseDesktopWidget;
class UPopupCloseWarningWidget;
class APopupCloseFlyingWindow;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class BRAINMINING_API APopupOfficeDevice : public AActor
{
 GENERATED_BODY()
public:
 APopupOfficeDevice();
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 void ResetSession();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 UPROPERTY(EditAnywhere,Category="Office",meta=(ClampMin="0",ClampMax="3")) int32 DeviceIndex=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art") TObjectPtr<UStaticMesh> ReplacementModel;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art") FTransform ReplacementTransform;
 // Optional render-target material for curved CRT glass; interaction stays on Surface.
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art") TObjectPtr<UMaterialInterface> ScreenProjectionMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art") TArray<TObjectPtr<UMaterialInterface>> ModelMaterialOverrides;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art") FVector PaperOutputPosition=FVector(-49,0,112);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art") FVector PaperOutputDirection=FVector(-1,0,0);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art") float PaperOutputYaw=0.f;
 UFUNCTION(BlueprintCallable,CallInEditor,Category="Office Art") void RefreshOfficeArt(){BuildModel();}
 bool IsPrinter() const { return DeviceIndex>=2; }
 bool IsLocked() const { return bLocked; }
 int32 Pending() const;
 int32 Created() const { return ProblemsCreated; }
 int32 Handled() const;
 int32 PoolSize() const { return Pool.Num(); }
 int32 Emitted() const { return PoolCursor; }
 bool WarningVisible() const;
 bool AccountingValid() const;
 bool CanInteract(APlayerController* PC) const;
 FVector InteractionPoint() const;
 void AddProblem();
 bool ProcessPaper();
 void RefreshWarning();
 void Lock();
 void HideWarning();
 void SetComputerView(bool bEnabled);
 void WarmPool(float Dt);
 void Emit();
 void CleanupOnePopup();
 void SetScreenInteractive(bool bEnabled);
 void TickScreen(float Dt,APlayerController* PC);
 void HandleGamepadPointer(bool bPressed);
 void FitCamera();
 void BeginCameraTransition(APlayerController* PC,float Duration,bool bEntering);
 void EndCameraTransition(){bCameraTransition=false;}
 virtual void CalcCamera(float DeltaTime,FMinimalViewInfo& OutResult) override;
 bool ClickReplyForValidation();
	bool ClickReplyForMobile();
 UPopupCloseDesktopWidget* GetDesktop() const { return Desktop; }
 UWidgetComponent* GetSurface() const { return Surface; }
private:
 void BuildModel();
 bool TraceDesktopGlass(const FVector& Origin,const FVector& Direction,FHitResult& WidgetHit) const;
 UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> SceneRoot;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Office Art",meta=(AllowPrivateAccess="true")) TObjectPtr<UStaticMeshComponent> ArtShell;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Aim;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetComponent> Surface;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetComponent> Warning;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetInteractionComponent> Pointer;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> PaperStack;
 UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Model;
 UPROPERTY() TObjectPtr<UPopupCloseDesktopWidget> Desktop;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ProjectedDesktop;
 UPROPERTY() TObjectPtr<UPopupCloseWarningWidget> WarningUI;
 UPROPERTY() TArray<TObjectPtr<APopupCloseFlyingWindow>> Pool;
 float WarmDebt=0;
 int32 PoolCursor=0;
 int32 ProblemsCreated=0;
 int32 PapersHandled=0;
 bool bLocked=false;
 bool bComputerView=false;
 bool bCameraTransition=false,bRaisedCameraPath=false;
 double CameraTransitionStart=0;
 float CameraTransitionDuration=1.f,CameraClearanceZ=0.f;
 FMinimalViewInfo CameraFrom,CameraTo;
};
