#include "MiningPickaxeSettings.h"

FString FMiningPickaxeLevel::GetMaterialCostText() const
{
    TArray<FString> Parts;
    for (const auto& Cost : MaterialCosts)
        Parts.Add(FString::Printf(TEXT("%s ×%d"), *FMiningMineralCost::Name(Cost.Mineral), Cost.Count));
    return FString::Join(Parts, TEXT("  "));
}

bool FMiningPickaxeLevel::Validate(FString& Error) const
{
    const auto InRange = [](float Value, float Minimum, float Maximum)
    {
        return FMath::IsFinite(Value) && Value >= Minimum && Value <= Maximum;
    };
    if (DestructionLevel<0 || DestructionLevel>4 || Price < 0 || Price > 1000000000 || !InRange(Width, 20, 2000) || !InRange(Depth, 10, 2000) ||
        !InRange(Interval, .01f, 5) || !InRange(Roughness, 0, 1))
    {
        Error = TEXT("破坏等级须为 0～4；价格须为 0～10 亿；边长 20～2000 cm；深度 10～2000 cm；最终间隔 0.01～5 秒；随机强度 0～1。");
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
    if (bUseHardness) return FText::FromString(FString::Printf(TEXT("破坏等级 %d · 范围边长 %.0f cm · 每击基础深度 %.0f cm（按岩层硬度修正）· 间隔 %.3f 秒"),
        DestructionLevel, Width, Depth, Interval));
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
    const int32 Costs[5][4] = {{0,0,0,0},{4,2,0,0},{6,4,0,0},{8,6,2,0},{10,8,4,2}};
    const EMiningMineral Types[] = {EMiningMineral::Copper, EMiningMineral::Iron, EMiningMineral::Malachite, EMiningMineral::Diamond};
    for (int32 Index = 0; Index < 5; ++Index)
    {
        auto& Level = Result.AddDefaulted_GetRef();
        Level.Name = FString::Printf(TEXT("等级 %d%s"), Index, Index == 0 ? TEXT(" · 初始") : TEXT(""));
        Level.Price = Prices[Index];
        for (int32 Kind = 0; Kind < 4; ++Kind) if (Costs[Index][Kind] > 0)
        {
            auto& Cost = Level.MaterialCosts.AddDefaulted_GetRef();
            Cost.Mineral = Types[Kind]; Cost.Count = Costs[Index][Kind];
        }
        Level.DestructionLevel = Index;
        Level.bUseHardness = true;
        Level.Width = Index >= 2 ? 400.f : 320.f;
        Level.Depth = 208.f;
        Level.Interval = Index >= 3 ? .225f : .3f;
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
    if (Settings->Validate(Error)) return Settings->GetLevel(Index);
    return MakeRecommendedLevels()[Index];
}

bool UMiningPickaxeSettings::Validate(FString& Error) const
{
    if (!FMath::IsFinite(RangeUpgradeMultiplier) || RangeUpgradeMultiplier<1 || RangeUpgradeMultiplier>4 ||
        !FMath::IsFinite(IntervalUpgradeMultiplier) || IntervalUpgradeMultiplier<.1f || IntervalUpgradeMultiplier>1)
    {Error=TEXT("范围升级倍率须为 1～4，间隔升级倍率须为 0.1～1。");return false;}
    if (Levels.Num() != 5)
    {
        Error = TEXT("必须保留等级 0～4，共五档配置。");
        return false;
    }
    for (int32 Index = 0; Index < Levels.Num(); ++Index)
    {
        const auto& Costs = Levels[Index].MaterialCosts;
        if ((Index == 0 && !Costs.IsEmpty()) || (Index > 0 && Costs.IsEmpty()))
        { Error = TEXT("等级 0 升级材料须留空；等级 1～4 须填写升级材料。"); return false; }
        TSet<EMiningMineral> Seen;
        for (const auto& Cost : Costs)
        {
            if (!FMiningMineralCost::IsSupported(Cost.Mineral) || Cost.Count < 1 || Cost.Count > 1000000 || Seen.Contains(Cost.Mineral))
            { Error = TEXT("升级材料仅支持铜矿、铁矿、孔雀石、钻石；不可重复，数量须为 1～1000000。"); return false; }
            Seen.Add(Cost.Mineral);
        }
        if (!Levels[Index].Validate(Error))
        {
            Error = FString::Printf(TEXT("等级 %d：%s"), Index, *Error);
            return false;
        }
        if (!GetLevel(Index).Validate(Error)) return false;
        if ((Index == 0 && Levels[Index].Price != 0) || (Index > 0 && Levels[Index].Price <= 0))
        {
            Error = TEXT("等级 0 价格须为 0，等级 1～4 价格须大于 0。");
            return false;
        }
    }
    Error.Reset();
    return true;
}

FMiningPickaxeLevel UMiningPickaxeSettings::GetLevel(int32 Level) const
{
    const int32 Index=FMath::Clamp(Level,0,4);
    FMiningPickaxeLevel Result=Levels.IsValidIndex(Index)?Levels[Index]:MakeRecommendedLevels()[Index];
    const auto Base=Levels.IsEmpty()?MakeRecommendedLevels()[0]:Levels[0];
    Result.DestructionLevel=Index;Result.bUseHardness=true;
    Result.Width=Base.Width*(Index>=2?RangeUpgradeMultiplier:1);
    Result.Depth=Base.Depth;Result.Interval=Base.Interval*(Index>=3?IntervalUpgradeMultiplier:1);
    Result.Stages.SetNum(1);Result.Stages[0]=FMiningPickaxeStage();
    return Result;
}

void UMiningPickaxeSettings::RefreshDerivedLevels()
{
    for(int32 Index=0;Index<Levels.Num() && Index<5;++Index)Levels[Index]=GetLevel(Index);
}

float UMiningPickaxeSettings::DepthFactor(int32 DestructionLevel,int32 HardnessLevel)
{
    static const float Factors[]={0,.25f,.5f,.75f,1,1.5f,2,2.5f,3};
    const int64 Difference=static_cast<int64>(DestructionLevel)-HardnessLevel;
    return Factors[static_cast<int32>(FMath::Clamp<int64>(Difference,-4,4))+4];
}

#if WITH_EDITOR
void UMiningPickaxeSettings::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    RefreshDerivedLevels();Super::PostEditChangeProperty(Event);
}
#endif
