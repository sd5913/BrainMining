#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MiningDeveloperMapData.h"
#include "MiningDeveloperMapComponent.generated.h"

class AHUD;
class AMiningTunnelGenerator;
class UCanvas;
class UInputComponent;
class APlayerController;

UCLASS()
class BRAINMINING_API UMiningDeveloperMapComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UMiningDeveloperMapComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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
	UFUNCTION(BlueprintCallable, Category="Developer Map")
	void ResetView();
	UFUNCTION(BlueprintCallable, Category="Developer Map")
	void OrbitBy(float YawDegrees, float PitchDegrees);
	UFUNCTION(BlueprintCallable, Category="Developer Map")
	void PanBy(FVector2D Offset);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	bool bVisible = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	bool bExpanded = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	bool bFollowPlayer = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	bool bLabels = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	EMiningMapView View = EMiningMapView::Orbit3D;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	FRotator OrbitRotation = FRotator(-30, -45, 0);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	FVector2D PanOffset = FVector2D::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	bool bControllingMap = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Developer Map")
	int32 DisplayTriangleCount = 0;
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
	struct FProjectedFace
	{
		FVector2D First, Second, Third;
		int32 Region = INDEX_NONE;
		double Depth = 0;
		float Shade = 1;
	};
	struct FProjectedEdge
	{
		FVector2D First, Second;
		int32 Region = INDEX_NONE;
	};
	void BeginMapControl();
	void EndMapControl();
	void Refresh3DProjection();
	void Draw3D(UCanvas* Canvas, const FBox2D& Graph, const FVector2D& Center, double Scale, float UiScale);
	FLinearColor RegionColor(const FMiningMapRegion& Region) const;
	FVector2D ProjectPoint(const FVector& Point) const;
	void RefreshLayout(const FVector& PlayerLocation);
	void RefreshProjection();
	TArray<FMiningMapRegion> Regions;
	TArray<TArray<FVector2D>> ProjectedRegions;
	TArray<MiningDeveloperMap::FSurface> SurfaceRegions;
	TArray<FProjectedFace> ProjectedFaces;
	TArray<FProjectedEdge> ProjectedEdges;
	TWeakObjectPtr<APlayerController> ControlController;
	FRotator CachedRotation = FRotator::ZeroRotator;
	double LastScale = 1;
	FBox2D ProjectedBounds = FBox2D(ForceInit);
	EMiningMapView CachedView = EMiningMapView::SideXZ;
	double NextLayoutSearch = 0;
	bool bInitialized = false;
};
