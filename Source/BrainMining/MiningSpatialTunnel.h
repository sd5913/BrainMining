#pragma once

#include "CoreMinimal.h"
#include "MiningManualShape.h"
#include "MiningManualRegion.h"
#include "Components/SceneComponent.h"
#include "MiningSpatialTunnel.generated.h"

UENUM(BlueprintType)
enum class EMiningSpatialWall : uint8 { PositiveX, NegativeX, PositiveY, NegativeY, PositiveZ, NegativeZ };

USTRUCT(BlueprintType)
struct FMiningSpatialKnot
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="三维节点", meta=(DisplayName="位置"))
	FVector Position = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="三维节点", meta=(DisplayName="进入手柄"))
	FVector Arrive = FVector(-200, 0, 0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="三维节点", meta=(DisplayName="离开手柄"))
	FVector Leave = FVector(200, 0, 0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="三维节点", meta=(DisplayName="累计扭转角", Units="deg"))
	float Roll = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="三维节点", meta=(DisplayName="截面宽高倍率", ClampMin="0.05"))
	FVector2D SectionScale = FVector2D(1, 1);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="三维节点", meta=(DisplayName="截面钢笔轮廓"))
	TArray<FMiningPenPoint> Profile;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="三维节点", meta=(DisplayName="锁定"))
	bool bLocked = false;
};

USTRUCT(BlueprintType)
struct FMiningSpatialPortal
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="入口", meta=(DisplayName="连接房间（可空）"))
	TObjectPtr<AActor> Room;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="入口", meta=(DisplayName="连接墙面"))
	EMiningSpatialWall Wall = EMiningSpatialWall::PositiveX;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="入口", meta=(DisplayName="墙面二维偏移", Units="cm"))
	FVector2D Offset = FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="入口", meta=(DisplayName="入口方向对齐墙面"))
	bool bAlignToWall = false;
};

UCLASS()
class BRAINMINING_API UMiningSpatialPathComponent : public USceneComponent
{
	GENERATED_BODY()
};

class AMiningSpatialTunnel;

UCLASS(Blueprintable, HideCategories=(Passage, Plugs))
class BRAINMINING_API AMiningSpatialRegion : public AMiningManualRegion
{
	GENERATED_BODY()
public:
	TArray<AMiningSpatialTunnel*> GetTunnels() const;
	virtual uint32 GetGeometryHash() const override;
	virtual FBox GetRockBounds() const override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="自动基岩", meta=(DisplayName="自动包裹可挖岩体", ToolTip="沿通道、房间和空洞自动生成外围，不再受手动岩石范围限制。关闭可使用旧版固定范围。"))
	bool bAutomaticBedrock = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="自动基岩", meta=(DisplayName="可挖余量", Units="cm", ClampMin="10", EditCondition="bAutomaticBedrock"))
	float ExcavationMargin = 400;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="自动基岩", meta=(DisplayName="外围基岩厚度", Units="cm", ClampMin="30", ClampMax="500", EditCondition="bAutomaticBedrock"))
	float AutomaticBedrockThickness = 100;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="编辑方式", meta=(DisplayName="自由编辑（保留未完成草稿）", ToolTip="生成失败时保留控制点，不自动回退或缩小造型；保存草稿与生成地形分别显示。"))
	bool bFreeEditing = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="三维矿区", meta=(DisplayName="三维主通道"))
	TObjectPtr<AMiningSpatialTunnel> MainTunnel;
private:
	mutable uint32 PreviewBoundsHash = 0;
	mutable FBox PreviewBounds = FBox(ForceInit);
};

UCLASS(Blueprintable, HideCategories=(Actor, Collision, Physics, Rendering, Replication, Input))
class BRAINMINING_API AMiningSpatialTunnel : public AActor
{
	GENERATED_BODY()
public:
	AMiningSpatialTunnel();
	uint32 GetGeometryHash() const;
	bool ResolveKnots(TArray<FMiningSpatialKnot>& Resolved, FString& Error) const;
	bool ResolvePortal(const FMiningSpatialPortal& Portal, FTransform& RoomTransform, FVector& HalfSize, FVector& Center, FVector& Normal, FString& Error) const;
	void Changed();
	UFUNCTION(CallInEditor, BlueprintCallable, Category="编辑", meta=(DisplayName="两房间间初始化直线（替换节点）"))
	bool FitPortals();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="三维隧道")
	TObjectPtr<UMiningSpatialPathComponent> Path;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="三维隧道", meta=(DisplayName="所属三维矿区"))
	TObjectPtr<AMiningSpatialRegion> Region;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="三维隧道", meta=(DisplayName="类型"))
	EMiningManualKind Kind = EMiningManualKind::Passage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="三维隧道", meta=(DisplayName="启用"))
	bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="三维隧道", meta=(DisplayName="隐藏空洞"))
	bool bHiddenCavity = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="三维隧道", meta=(DisplayName="曲线节点与截面"))
	TArray<FMiningSpatialKnot> Knots;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="三维隧道", meta=(DisplayName="初始向上方向"))
	FVector InitialUp = FVector::UpVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="三维隧道", meta=(DisplayName="支线房间尺寸", Units="cm", ClampMin="250"))
	FVector RoomSize = FVector(1000, 1000, 700);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="入口", meta=(DisplayName="起点入口"))
	FMiningSpatialPortal StartPortal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="入口", meta=(DisplayName="终点入口"))
	FMiningSpatialPortal EndPortal;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, NonTransactional, Category="状态", meta=(DisplayName="检查结果"))
	FString ShapeStatus;
#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
	virtual void PostEditUndo() override;
#endif
};
