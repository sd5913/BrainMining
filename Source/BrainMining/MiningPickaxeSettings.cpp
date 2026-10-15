#include "MiningPickaxeSettings.h"

bool FMiningPickaxeLevel::Validate(FString& Error) const
{
    const auto InRange = [](float Value, float Minimum, float Maximum)
    {
        return FMath::IsFinite(Value) && Value >= Minimum && Value <= Maximum;
    };
    if (Price < 0 || Price > 1000000000 || !InRange(Width, 20, 2000) || !InRange(Depth, 10, 2000) ||
        !InRange(Interval, .1f, 5) || !InRange(Roughness, 0, 1))
    {
        Error = TEXT("价格须为 0～10 亿；边长 20～2000 cm；深度 10～2000 cm；间隔 0.1～5 秒；随机强度 0～1。");
        return false;
    }
    if (Stages.Num() < 1 || Stages.Num() > 6)
    {
        Error = TEXT("每级必须包含 1～6 个逐击阶段，阶段行数就是所需击数。");
        return false;
    }
    float PreviousDepth = 0;
    float PreviousWidth = 0;
    for (int32 Index = 0; Index < Stages.Num(); ++Index)
    {
        const auto& Stage = Stages[Index];
        if (!InRange(Stage.DepthPercent, 1, 100) || !InRange(Stage.WidthPercent, 1, 100) ||
            Stage.DepthPercent <= PreviousDepth || Stage.WidthPercent < PreviousWidth)
        {
            Error = FString::Printf(TEXT("第 %d 击：百分比必须在 1～100 内，深度逐击递增，边长不能回退。"), Index + 1);
            return false;
        }
        PreviousDepth = Stage.DepthPercent;
        PreviousWidth = Stage.WidthPercent;
    }
    if (Stages.Last().DepthPercent != 100 || Stages.Last().WidthPercent != 100)
    {
        Error = TEXT("最后一击的累计深度和洞口边长必须都是 100%。");
        return false;
    }
    Error.Reset();
    return true;
}

FText FMiningPickaxeLevel::GetDescription() const
{
    if (!Description.IsEmpty()) return FText::FromString(Description);
    return FText::FromString(FString::Printf(TEXT("%d 击挖开 · 基础边长 %.0f cm · 基础深度 %.0f cm · 间隔 %.3f 秒"),
        Stages.Num(), Width, Depth, Interval));
}

UMiningPickaxeSettings::UMiningPickaxeSettings()
{
    Levels = MakeRecommendedLevels();
}

TArray<FMiningPickaxeLevel> UMiningPickaxeSettings::MakeRecommendedLevels()
{
    TArray<FMiningPickaxeLevel> Result;
    const int64 Prices[] = {0, 50, 1000, 5000, 50000};
    for (int32 Index = 0; Index < 5; ++Index)
    {
        auto& Level = Result.AddDefaulted_GetRef();
        Level.Name = FString::Printf(TEXT("等级 %d%s"), Index, Index == 0 ? TEXT(" · 初始") : TEXT(""));
        Level.Price = Prices[Index];
        Level.Width = Index >= 2 ? 384.f : 320.f;
        Level.Depth = Index >= 1 ? 280.8f : 208.f;
        Level.Interval = Index >= 3 ? .255f : .3f;
        const float Depths[] = {25, 60, 100};
        const float Widths[] = {80, 95, 100};
        for (int32 Strike = 0; Strike < (Index == 4 ? 2 : 3); ++Strike)
        {
            auto& Stage = Level.Stages.AddDefaulted_GetRef();
            Stage.DepthPercent = Index == 4 ? (Strike == 0 ? 55 : 100) : Depths[Strike];
            Stage.WidthPercent = Index == 4 ? (Strike == 0 ? 90 : 100) : Widths[Strike];
        }
    }
    return Result;
}

FMiningPickaxeLevel UMiningPickaxeSettings::ResolveLevel(int32 Level)
{
    const int32 Index = FMath::Clamp(Level, 0, 4);
    const auto* Settings = GetDefault<UMiningPickaxeSettings>();
    FString Error;
    if (Settings->Validate(Error)) return Settings->Levels[Index];
    return MakeRecommendedLevels()[Index];
}

bool UMiningPickaxeSettings::Validate(FString& Error) const
{
    if (Levels.Num() != 5)
    {
        Error = TEXT("必须保留等级 0～4，共五档配置。");
        return false;
    }
    for (int32 Index = 0; Index < Levels.Num(); ++Index)
    {
        if (!Levels[Index].Validate(Error))
        {
            Error = FString::Printf(TEXT("等级 %d：%s"), Index, *Error);
            return false;
        }
        if ((Index == 0 && Levels[Index].Price != 0) || (Index > 0 && Levels[Index].Price <= 0))
        {
            Error = TEXT("等级 0 价格须为 0，等级 1～4 价格须大于 0。");
            return false;
        }
    }
    Error.Reset();
    return true;
}
