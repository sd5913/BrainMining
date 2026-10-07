#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MiningDeveloperMapData.h"
#include "MiningDeveloperMapComponent.generated.h"

class AHUD;
class AMiningTunnelGenerator;
class UCanvas;
class UInputComponent;

UCLASS()
class BRAINMINING_API UMiningDeveloperMapComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UMiningDeveloperMapComponent();
	void BindInput(UInputComponent* Input);
	void Draw(AHUD* HUD, UCanvas* Canvas);

	UFUNCTION(BlueprintCallable, Category="Developer Map")
	void ToggleMap();
	UFUNCTION(BlueprintCallable, Category="Developer Map")
	void ToggleExpanded();
	UFUNCTION(BlueprintCallable, Category="Developer Map")
	void CycleView();
	UFUNCTION(BlueprintCallable, Category="Developer Map")
	void ToggleFollow();
	UFUNCTION(BlueprintCallable, Category="Developer Map")
	void ToggleLabels();
	UFUNCTION(BlueprintCallable, Category="Developer Map")
	void ZoomIn();
	UFUNCTION(BlueprintCallable, Category="Developer Map")
	void ZoomOut();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	bool bVisible = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	bool bExpanded = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	bool bFollowPlayer = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	bool bLabels = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	EMiningMapView View = EMiningMapView::SideXZ;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	float Zoom = 1;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	FString LocationLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	int32 CurrentRegionIndex = INDEX_NONE;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	int32 RegionCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	int32 SnapshotRevision = INDEX_NONE;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	bool bLegacySnapshot = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	bool bPlayerOffscreen = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	FVector2D PlayerMapPosition = FVector2D::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	FVector2D MapMinimum = FVector2D::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	FVector2D MapMaximum = FVector2D::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	TObjectPtr<AMiningTunnelGenerator> Layout;

private:
	void RefreshLayout(const FVector& PlayerLocation);
	void RefreshProjection();
	TArray<FMiningMapRegion> Regions;
	TArray<TArray<FVector2D>> ProjectedRegions;
	FBox2D ProjectedBounds = FBox2D(ForceInit);
	EMiningMapView CachedView = EMiningMapView::SideXZ;
	double NextLayoutSearch = 0;
	bool bInitialized = false;
};
