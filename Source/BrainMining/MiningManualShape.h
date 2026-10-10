#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningManualShape.generated.h"

class AMiningManualRegion;
class UBoxComponent;

UENUM(BlueprintType)
enum class EMiningManualKind : uint8 { Passage UMETA(DisplayName="通道"), Cavity UMETA(DisplayName="空洞"), SideRoom UMETA(DisplayName="支线房间"), RockBlock UMETA(DisplayName="可挖岩石保留区") };

UENUM(BlueprintType)
enum class EMiningPenMode : uint8 { Corner UMETA(DisplayName="角点"), Smooth UMETA(DisplayName="平滑"), Independent UMETA(DisplayName="独立手柄") };

UENUM(BlueprintType)
enum class EMiningPortalWall : uint8 { PositiveX UMETA(DisplayName="右墙 +X"), NegativeX UMETA(DisplayName="左墙 -X"), PositiveZ UMETA(DisplayName="顶部 +Z"), NegativeZ UMETA(DisplayName="底部 -Z") };

USTRUCT(BlueprintType)
struct FMiningPenPoint
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pen")
	FVector2D Position = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pen")
	FVector2D Arrive = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pen")
	FVector2D Leave = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pen")
	EMiningPenMode Mode = EMiningPenMode::Corner;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pen")
	bool bLocked = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pen", AdvancedDisplay, meta=(DisplayName="整体弯曲绑定位置"))
	float GuideStation = -1;
};

USTRUCT(BlueprintType)
struct FMiningManualPortal
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portal", meta=(DisplayName="连接房间"))
	TObjectPtr<AActor> Room;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portal", meta=(DisplayName="墙面"))
	EMiningPortalWall Wall = EMiningPortalWall::PositiveX;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portal", meta=(DisplayName="沿墙偏移", Units="cm"))
	float Offset = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portal", meta=(DisplayName="入口宽高", ClampMin="100", Units="cm"))
	float Aperture = 500;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portal", meta=(DisplayName="深度方向偏移", Units="cm"))
	float DepthOffset = 0;
};

UCLASS(Blueprintable, HideCategories=(Actor, Collision, Physics, Rendering, Replication, Input))
class BRAINMINING_API AMiningManualShape : public AActor
{
	GENERATED_BODY()
public:
	AMiningManualShape();
	virtual void OnConstruction(const FTransform& Transform) override;
	uint32 GetGeometryHash() const;
	FBox GetRoomBounds() const;
	bool ResolveBoundary(bool bSecond, TArray<FMiningPenPoint>& Points, FString& Error) const;
	bool ResolvePortal(const FMiningManualPortal& Portal, FVector& Center, FVector& Normal, FString& Error) const;
	void Changed();
	UFUNCTION(CallInEditor, BlueprintCallable, Category="编辑", meta=(DisplayName="初始化矩形（替换轮廓）"))
	void ResetRectangle();
	UFUNCTION(CallInEditor, BlueprintCallable, Category="编辑", meta=(DisplayName="初始化椭圆（替换轮廓）"))
	void ResetEllipse();
	UFUNCTION(CallInEditor, BlueprintCallable, Category="编辑", meta=(DisplayName="初始化两房间连接（替换两侧）"))
	bool FitPortals();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="空间", meta=(DisplayName="所属手绘矿区"))
	TObjectPtr<AMiningManualRegion> Region;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="空间", meta=(DisplayName="空间类型"))
	EMiningManualKind Kind = EMiningManualKind::Cavity;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="空间", meta=(DisplayName="启用"))
	bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="空间", meta=(DisplayName="前后深度", Units="cm", ClampMin="100"))
	float Depth = 700;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="空间", meta=(DisplayName="宽高（房间/初始形状）", Units="cm"))
	FVector2D Size = FVector2D(1000, 700);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="空间", meta=(DisplayName="隐藏空洞标记"))
	bool bHiddenCavity = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="轮廓", meta=(DisplayName="边界 A / 闭合轮廓"))
	TArray<FMiningPenPoint> BoundaryA;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="轮廓", meta=(DisplayName="边界 B（通道）"))
	TArray<FMiningPenPoint> BoundaryB;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="入口", meta=(DisplayName="起点入口"))
	FMiningManualPortal StartPortal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="入口", meta=(DisplayName="终点入口"))
	FMiningManualPortal EndPortal;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, NonTransactional, Category="状态", meta=(DisplayName="检查结果"))
	FString ShapeStatus;
	UPROPERTY(VisibleAnywhere, Category="空间")
	TObjectPtr<UBoxComponent> Bounds;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditUndo() override;
#endif
};
