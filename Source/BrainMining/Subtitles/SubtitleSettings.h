#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SubtitleSettings.generated.h"

/** Shared visual settings for every subtitle segment and editor preview. */
UCLASS(Config=Game, DefaultConfig)
class BRAINMINING_API USubtitleSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category="通用设置", meta=(DisplayName="字体大小", ClampMin="12", ClampMax="72", ToolTip="所有字幕共用的字号，按界面 DPI 缩放。原字号为 22。"))
	int32 FontSize = 28;

	UPROPERTY(Config, EditAnywhere, Category="通用设置", meta=(DisplayName="色散强度", ClampMin="0", ClampMax="3", Delta="0.05", ToolTip="统一控制红青色边缘的偏移与透明度。0 关闭，1 为原效果。"))
	float ChromaticStrength = 1.4f;

	UPROPERTY(Config, EditAnywhere, Category="通用设置", meta=(DisplayName="正弦漂浮强度", ClampMin="0", ClampMax="3", Delta="0.05", ToolTip="统一控制正弦波的上下、左右漂浮幅度和轻微摆动。0 静止，1 为原效果；不改变漂浮速度。"))
	float FloatStrength = 1.4f;

	UPROPERTY(Config, EditAnywhere, Category="通用设置", meta=(DisplayName="同段字幕间隔", ClampMin="0", ClampMax="2", Delta="0.05", ToolTip="同一个字幕段的相邻字幕行之间增加的停顿时间（秒）。"))
	float LineGap = 0.35f;

	UPROPERTY(Config, EditAnywhere, Category="通用设置", meta=(DisplayName="字幕底部留白", ClampMin="0", ClampMax="500", Delta="5", ToolTip="字幕距屏幕底部的距离（界面 DPI 单位）。增大可避开喷气背包热量条。"))
	float BottomMargin = 150.f;

};
