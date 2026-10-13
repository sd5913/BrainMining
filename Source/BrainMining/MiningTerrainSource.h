#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MiningTerrainSource.generated.h"

class UDynamicMesh;

UCLASS()
class BRAINMINING_API UMiningTerrainSource : public UDataAsset
{
	GENERATED_BODY()
public:
	virtual void Serialize(FArchive& Ar) override;
	UPROPERTY(VisibleAnywhere, Instanced)
	TObjectPtr<UDynamicMesh> Preview;
	UPROPERTY(VisibleAnywhere, Instanced)
	TArray<TObjectPtr<UDynamicMesh>> Meshes;
};
