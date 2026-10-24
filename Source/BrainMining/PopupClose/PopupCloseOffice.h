#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PopupCloseOffice.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class URectLightComponent;

UCLASS()
class BRAINMINING_API APopupCloseOffice : public AActor
{
 GENERATED_BODY()
public:
 APopupCloseOffice();
 UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bWalkThroughConnections=false;
 virtual void OnConstruction(const FTransform& Transform) override;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art") bool bReplacePerimeterWorkstations=false;
 // The imported modular shell owns window openings, floors, ceiling tiles and fixtures.
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art") bool bUseImportedArchitecture=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art") TArray<TObjectPtr<UMaterialInterface>> SurfaceOverrides;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art") TObjectPtr<UMaterialInterface> CarpetMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art") TObjectPtr<UMaterialInterface> CeilingTileMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art",meta=(ClampMin="0")) float InteriorLightLumens=1500.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art",meta=(ClampMin="0")) float CornerLightLumens=800.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Office Art",meta=(ClampMin="1000",ClampMax="12000")) float InteriorLightTemperature=4700.f;
 virtual void BeginPlay() override;
 UFUNCTION(BlueprintCallable,CallInEditor,Category="Office Art") void RefreshOfficeArt(){BuildOffice();}
 int32 GetDetailCount() const;
private:
 void BuildOffice();
 UPROPERTY() TObjectPtr<UMaterialInterface> CeilingWithoutGI;
 // Native components are assembled by the constructor; old maps must not reload a shorter registry.
 UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Groups;
 UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Surfaces;
 UPROPERTY(VisibleAnywhere,Category="Office Art") TArray<TObjectPtr<URectLightComponent>> CeilingLights;
};
