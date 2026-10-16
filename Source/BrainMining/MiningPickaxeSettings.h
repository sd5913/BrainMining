#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MiningMineralInventory.h"
#include "MiningPickaxeSettings.generated.h"

USTRUCT(BlueprintType)
struct BRAINMINING_API FMiningPickaxeStage
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stage", meta=(DisplayName="累计深度（%）", ClampMin="1", ClampMax="100"))
    float DepthPercent = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stage", meta=(DisplayName="累计洞口边长（%）", ClampMin="1", ClampMax="100"))
    float WidthPercent = 100;
};

USTRUCT(BlueprintType)
struct BRAINMINING_API FMiningPickaxeLevel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level", meta=(DisplayName="等级名称"))
    FString Name;

    // Retained for legacy demonstration maps; MainGame uses MaterialCosts exclusively.
    UPROPERTY()
    int64 Price = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level", meta=(DisplayName="升级材料", TitleProperty="Mineral", ToolTip="升级到此等级需要的矿石。每种矿石只填一次，数量为正整数；等级 0 留空。"))
    TArray<FMiningMineralCost> MaterialCosts;

    FString GetMaterialCostText() const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Level", meta=(DisplayName="镐子破坏等级"))
    int32 DestructionLevel = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Level", meta=(DisplayName="按硬度逐击挖掘"))
    bool bUseHardness = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level", meta=(DisplayName="挖掘范围边长（厘米）", EditCondition="DestructionLevel == 0", ClampMin="20", ClampMax="2000", ToolTip="在等级 0 配置基础边长；等级 2 起乘范围升级倍率，继续乘人物缩放和挖掘尺寸倍率。"))
    float Width = 320;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level", meta=(DisplayName="每击基础挖掘深度（厘米）", EditCondition="DestructionLevel == 0", ClampMin="10", ClampMax="2000", ToolTip="在等级 0 配置。每击实际深度还要乘破坏等级与硬度差的深度倍率，以及人物尺寸倍率。"))
    float Depth = 208;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level", meta=(DisplayName="挥镐间隔（秒）", EditCondition="DestructionLevel == 0", ClampMin="0.1", ClampMax="5"))
    float Interval = .3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level", meta=(DisplayName="轮廓随机强度", ClampMin="0", ClampMax="1", ToolTip="0 为规则轮廓，1 为当前完整随机强度。同一个坑的随机形状保持固定。"))
    float Roughness = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level", meta=(DisplayName="商店说明（留空自动生成）", MultiLine="true"))
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Level", meta=(DisplayName="旧版逐击阶段", EditCondition="!bUseHardness", ToolTip="仅旧版固定击数模式使用。硬度模式每击独立计算深度。"))
    TArray<FMiningPickaxeStage> Stages;

    bool Validate(FString& Error) const;
    FText GetDescription() const;
};

UCLASS(Config=Game, DefaultConfig)
class BRAINMINING_API UMiningPickaxeSettings : public UObject
{
    GENERATED_BODY()
public:
    UMiningPickaxeSettings();

    UPROPERTY(Config, EditAnywhere, EditFixedSize, Category="矿镐等级", meta=(DisplayName="等级 0～4（等级 0 配置基础数值）", TitleProperty="Name", EditFixedOrder))
    TArray<FMiningPickaxeLevel> Levels;

    UPROPERTY(Config, EditAnywhere, Category="升级倍率", meta=(DisplayName="2 级起范围倍率", ClampMin="1", ClampMax="4"))
    float RangeUpgradeMultiplier = 1.25f;
    UPROPERTY(Config, EditAnywhere, Category="升级倍率", meta=(DisplayName="3 级起间隔倍率", ClampMin="0.1", ClampMax="1"))
    float IntervalUpgradeMultiplier = .75f;

    FMiningPickaxeLevel GetLevel(int32 Level) const;
    void RefreshDerivedLevels();
    static float DepthFactor(int32 DestructionLevel, int32 HardnessLevel);
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

    static TArray<FMiningPickaxeLevel> MakeRecommendedLevels();
    static FMiningPickaxeLevel ResolveLevel(int32 Level);
    bool Validate(FString& Error) const;
};
