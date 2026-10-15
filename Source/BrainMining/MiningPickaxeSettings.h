#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level", meta=(DisplayName="购买此等级的价格", ClampMin="0", ClampMax="1000000000"))
    int64 Price = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level", meta=(DisplayName="洞口边长（基础厘米）", ClampMin="20", ClampMax="2000", ToolTip="各级最终基础值，不叠乘升级。实际尺寸继续乘关卡人物缩放和挖掘尺寸倍率。"))
    float Width = 320;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level", meta=(DisplayName="完整挖掘深度（基础厘米）", ClampMin="10", ClampMax="2000", ToolTip="与边长独立。实际尺寸继续乘关卡人物缩放和挖掘尺寸倍率。"))
    float Depth = 208;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level", meta=(DisplayName="敲击间隔（秒）", ClampMin="0.1", ClampMax="5"))
    float Interval = .3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level", meta=(DisplayName="轮廓随机强度", ClampMin="0", ClampMax="1", ToolTip="0 为规则轮廓，1 为当前完整随机强度。同一个坑的随机形状保持固定。"))
    float Roughness = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level", meta=(DisplayName="商店说明（留空自动生成）", MultiLine="true"))
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level", meta=(DisplayName="逐击阶段（行数就是击数）", ToolTip="按敲击顺序填写累计百分比，不是本击增量。支持 1～6 击；末行必须为 100 / 100。"))
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

    UPROPERTY(Config, EditAnywhere, EditFixedSize, Category="矿镐等级", meta=(DisplayName="等级 0～4（最终数值，不叠乘）", TitleProperty="Name", EditFixedOrder))
    TArray<FMiningPickaxeLevel> Levels;

    static TArray<FMiningPickaxeLevel> MakeRecommendedLevels();
    static FMiningPickaxeLevel ResolveLevel(int32 Level);
    bool Validate(FString& Error) const;
};
