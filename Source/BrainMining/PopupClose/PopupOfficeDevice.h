#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PopupOfficeDevice.generated.h"
class UBoxComponent;
class UCameraComponent;
class UInstancedStaticMeshComponent;
class UWidgetComponent;
class UWidgetInteractionComponent;
class UPopupCloseDesktopWidget;
class UPopupCloseWarningWidget;
class APopupCloseFlyingWindow;

UCLASS()
class BRAINMINING_API APopupOfficeDevice : public AActor
{
 GENERATED_BODY()
public:
 APopupOfficeDevice();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 UPROPERTY(EditAnywhere,Category="Office",meta=(ClampMin="0",ClampMax="3")) int32 DeviceIndex=0;
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
 void WarmPool(float Dt);
 void Emit();
 void CleanupOnePopup();
 void SetScreenInteractive(bool bEnabled);
 void TickScreen(float Dt,APlayerController* PC);
 void FitCamera();
 bool ClickReplyForValidation();
 UPopupCloseDesktopWidget* GetDesktop() const { return Desktop; }
 UWidgetComponent* GetSurface() const { return Surface; }
private:
 void BuildModel();
 UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> SceneRoot;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Aim;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetComponent> Surface;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetComponent> Warning;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetInteractionComponent> Pointer;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> PaperStack;
 UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Model;
 UPROPERTY() TObjectPtr<UPopupCloseDesktopWidget> Desktop;
 UPROPERTY() TObjectPtr<UPopupCloseWarningWidget> WarningUI;
 UPROPERTY() TArray<TObjectPtr<APopupCloseFlyingWindow>> Pool;
 float WarmDebt=0;
 int32 PoolCursor=0;
 int32 ProblemsCreated=0;
 int32 PapersHandled=0;
 bool bLocked=false;
};
