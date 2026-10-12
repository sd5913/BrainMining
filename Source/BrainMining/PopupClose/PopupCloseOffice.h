#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PopupCloseOffice.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class BRAINMINING_API APopupCloseOffice : public AActor
{
 GENERATED_BODY()
public:
 APopupCloseOffice();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 int32 GetDetailCount() const;
private:
 void BuildOffice();
 UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Groups;
 UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Surfaces;
};
