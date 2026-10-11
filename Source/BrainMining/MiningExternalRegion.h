#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningExternalRegion.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UBoxComponent;
class AGeneratedMiningRock;
class ADynamicMeshActor;

USTRUCT(BlueprintType)
struct FMiningExternalCutter
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutter", meta=(DisplayName="通路模型"))
	TObjectPtr<UStaticMesh> Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutter", meta=(DisplayName="矿区内变换", ToolTip="相对于矿区中心；坐标单位 cm。使用正缩放。"))
	FTransform LocalTransform = FTransform::Identity;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cutter", meta=(DisplayName="参与切割"))
	bool bEnabled = true;
};

USTRUCT(BlueprintType)
struct FMiningExternalGuideLayer
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Guide", meta=(DisplayName="阻塞块模型"))
	TObjectPtr<UStaticMesh> Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Guide", meta=(DisplayName="模型附加变换", ToolTip="相对于共用的引导层变换；与通路共用原点的 FBX 保持默认值。"))
	FTransform LocalTransform = FTransform::Identity;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Guide", meta=(DisplayName="参与生成"))
	bool bEnabled = true;
};

// No dependency on MiningTunnelGenerator or the retired designer workflow.
UCLASS(Blueprintable, HideCategories=(Replication, Input, Actor, HLOD, Cooking), meta=(DisplayName="外部通路矿区"))
class BRAINMINING_API AMiningExternalRegion : public AActor
{
	GENERATED_BODY()
public:
	AMiningExternalRegion();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Destroyed() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="矿区", meta=(DisplayName="可挖岩层尺寸", Units="cm", ClampMin="100"))
	FVector RockSize = FVector(6000, 4000, 3000);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="矿区", meta=(DisplayName="基岩外壳厚度", Units="cm", ClampMin="10", ClampMax="3000"))
	float ShellThickness = 200;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="通路", meta=(DisplayName="Blender 通路切割体", TitleProperty="Mesh"))
	TArray<FMiningExternalCutter> Cutters;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="通路", meta=(DisplayName="显示切割体预览"))
	bool bShowCutterPreview = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="材质", meta=(DisplayName="可挖岩石材质"))
	TObjectPtr<UMaterialInterface> RockMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="材质", meta=(DisplayName="不可挖基岩材质"))
	TObjectPtr<UMaterialInterface> ShellMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="分块", meta=(DisplayName="分块边长", Units="cm", ClampMin="400", ClampMax="3000"))
	float ChunkLength = 1600;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="分块", meta=(DisplayName="每块三角形预算", ClampMin="1000", ClampMax="100000"))
	int32 ChunkTriangleBudget = 100000;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="分块", meta=(DisplayName="后台保守简化"))
	bool bSimplifyChunks = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="引导可挖层", meta=(DisplayName="阻塞块模型列表", TitleProperty="Mesh"))
	TArray<FMiningExternalGuideLayer> GuideLayers;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="引导可挖层", meta=(DisplayName="跟随通路变换", ToolTip="与通路共用原点的阻塞 FBX 启用此项；移动通路切割体后需重新生成引导层。"))
	bool bGuideFollowCutterTransform = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="引导可挖层", meta=(DisplayName="对应切割体索引", ClampMin="0", EditCondition="bGuideFollowCutterTransform", ToolTip="从 0 开始，默认对应第一个通路切割体。"))
	int32 GuideCutterIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="引导可挖层", meta=(DisplayName="共用引导层变换", EditCondition="!bGuideFollowCutterTransform", ToolTip="不跟随通路时使用，坐标相对于矿区中心，单位 cm。"))
	FTransform GuideTransform = FTransform::Identity;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="引导可挖层", meta=(DisplayName="引导层材质", ToolTip="未指定时使用阻塞模型的首个材质；模型无材质则使用可挖岩石材质。"))
	TObjectPtr<UMaterialInterface> GuideMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="生成结果", meta=(DisplayName="生成状态"))
	FString BuildStatus = TEXT("设置岩层和通路模型后，点击生成矿区。无切割体时生成实心矿区。");
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="生成结果", meta=(DisplayName="可挖岩层"))
	TObjectPtr<AGeneratedMiningRock> GeneratedRock;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="生成结果", meta=(DisplayName="不可挖外壳"))
	TObjectPtr<ADynamicMeshActor> GeneratedShell;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="生成结果", meta=(DisplayName="岩层范围"))
	TObjectPtr<UBoxComponent> BoundsPreview;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="生成结果", meta=(DisplayName="引导层生成状态"))
	FString GuideBuildStatus = TEXT("添加阻塞块模型后，点击生成引导可挖层。");
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="生成结果", meta=(DisplayName="引导可挖层"))
	TArray<TObjectPtr<AGeneratedMiningRock>> GeneratedGuideLayers;

	UFUNCTION(BlueprintCallable, CallInEditor, Category="矿区操作", meta=(DisplayName="生成 / 重新生成矿区"))
	bool GenerateRegion();
	UFUNCTION(BlueprintCallable, CallInEditor, Category="矿区操作", meta=(DisplayName="清除本矿区生成结果"))
	void ClearGeneratedRegion();
	UFUNCTION(BlueprintCallable, CallInEditor, Category="矿区操作", meta=(DisplayName="恢复初始可挖岩层"))
	void ResetExcavation();
	UFUNCTION(BlueprintCallable, CallInEditor, Category="矿区操作", meta=(DisplayName="生成 / 重新生成引导可挖层"))
	bool GenerateGuideLayers();
	UFUNCTION(BlueprintCallable, CallInEditor, Category="矿区操作", meta=(DisplayName="清除引导可挖层"))
	void ClearGeneratedGuideLayers();
	UFUNCTION(BlueprintCallable, CallInEditor, Category="矿区操作", meta=(DisplayName="恢复初始引导可挖层"))
	void ResetGuideExcavation();

private:
#if WITH_EDITOR
	void RefreshPreview();
	void RemoveOwnedActor(AActor* Actor);
#endif
};
