#pragma once

#include "CoreMinimal.h"
#include "MiningDeveloperMapData.generated.h"

UENUM(BlueprintType)
enum class EMiningMapView : uint8 { SideXZ, TopXY, SideYZ };

UENUM(BlueprintType)
enum class EMiningMapRegionKind : uint8 { Room, Tunnel, Space, Branch, HiddenEntrance, RockRegion, GuidePlug };

USTRUCT(BlueprintType)
struct FMiningDeveloperMapSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Developer Map")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Developer Map")
	bool bShowOnStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Developer Map", meta=(ClampMin="320", ClampMax="800"))
	float PanelWidth = 420;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Developer Map", meta=(ClampMin="260", ClampMax="600"))
	float PanelHeight = 320;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Developer Map", meta=(ClampMin="1000", ClampMax="20000", Units="cm"))
	float FollowWidth = 6000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Developer Map", meta=(ClampMin="0.3", ClampMax="1"))
	float BackgroundOpacity = 0.94f;
};

USTRUCT(BlueprintType)
struct FMiningMapRegion
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Map")
	FString Name;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Map")
	EMiningMapRegionKind Kind = EMiningMapRegionKind::Room;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Map")
	int32 GroupIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Map")
	bool bRockFilled = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Map")
	bool bEllipsoid = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Map", AdvancedDisplay)
	FBox Bounds = FBox(ForceInit);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Map", AdvancedDisplay)
	TArray<FVector> Vertices;

	UPROPERTY()
	TArray<FPlane> Planes;

	bool Contains(const FVector& Position) const;
};

namespace MiningDeveloperMap
{
	FMiningMapRegion Convex(const FString& Name, EMiningMapRegionKind Kind, int32 Group, const TArray<FVector>& Vertices);
	FMiningMapRegion Box(const FString& Name, EMiningMapRegionKind Kind, int32 Group, const FBox& Bounds);
	FVector2D Project(const FVector& Point, EMiningMapView View);
	TArray<FVector2D> Hull(TArray<FVector2D> Points);
	TArray<FVector2D> Outline(const FMiningMapRegion& Region, EMiningMapView View);
	TArray<FVector2D> ClipPolygon(TArray<FVector2D> Points, const FBox2D& Clip);
	bool ClipLine(FVector2D& Start, FVector2D& Finish, const FBox2D& Clip);
	int32 FindRegion(const TArray<FMiningMapRegion>& Regions, const FVector& Position);
	double FitScale(const FBox2D& WorldBounds, const FVector2D& ScreenSize);
}
