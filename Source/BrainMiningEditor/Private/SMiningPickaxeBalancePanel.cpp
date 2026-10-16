#include "SMiningPickaxeBalancePanel.h"
#include "MiningComponent.h"
#include "MiningProgressionComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

void SMiningPickaxeBalancePanel::Construct(const FArguments& Arguments)
{
    Draft.Reset(NewObject<UMiningPickaxeSettings>(GetTransientPackage(), NAME_None, RF_Transactional));
    Draft->Levels = GetDefault<UMiningPickaxeSettings>()->Levels;
    Draft->RangeUpgradeMultiplier=GetDefault<UMiningPickaxeSettings>()->RangeUpgradeMultiplier;
    Draft->IntervalUpgradeMultiplier=GetDefault<UMiningPickaxeSettings>()->IntervalUpgradeMultiplier;
    Draft->RefreshDerivedLevels();
    FDetailsViewArgs ViewArgs;
    ViewArgs.bAllowSearch = true;
    ViewArgs.bHideSelectionTip = true;
    ViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
    Details = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(ViewArgs);
    Details->SetObject(Draft.Get());
    const auto Button = [](const TCHAR* Label, TFunction<FReply()> Action)
    {
        return SNew(SButton).Text(FText::FromString(Label)).OnClicked_Lambda(MoveTemp(Action));
    };
    ChildSlot
    [
        SNew(SBorder).Padding(12)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
            [ SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("等级 0 配置基础每击深度、范围边长和挥镐间隔；等级 1～4 的破坏等级自动对应 1～4。2 级起累计范围 ×1.25，3 级起累计间隔 ×0.75，倍率可调整。每击实际深度由破坏等级减岩层硬度修正；差值 ≤-4 时无法破坏。后续等级尺寸自动派生。"))) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
            [
                SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 4))
                + SWrapBox::Slot()[Button(TEXT("保存并应用"), [this]() { return Save(); })]
                + SWrapBox::Slot()[Button(TEXT("重新读取已保存配置"), [this]() { return Reload(); })]
                + SWrapBox::Slot()[Button(TEXT("填入推荐预设（未保存）"), [this]() { return Recommended(); })]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
            [ SNew(STextBlock).Text(this, &SMiningPickaxeBalancePanel::GetValidation).AutoWrapText(true) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
            [ SNew(STextBlock).Text(this, &SMiningPickaxeBalancePanel::GetSummary).AutoWrapText(true) ]
            + SVerticalBox::Slot().FillHeight(1)[Details.ToSharedRef()]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 4)
            [
                SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 4))
                + SWrapBox::Slot()[SNew(STextBlock).Text(FText::FromString(TEXT("目标等级")))]
                + SWrapBox::Slot()[SNew(SSpinBox<int32>).MinValue(0).MaxValue(4).Value_Lambda([this]() { return PreviewLevel; })
                    .OnValueChanged_Lambda([this](int32 Value) { PreviewLevel = Value; })]
                + SWrapBox::Slot()[Button(TEXT("在当前试玩中试用草稿"), [this]() { return Preview(); })]
                + SWrapBox::Slot()[Button(TEXT("恢复已购等级"), [this]() { return Restore(); })]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 4)
            [ SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("试玩时按 Shift+F1 释放鼠标，再点试用。试用不扣矿石、不改购买等级；每次挥镐独立按当前岩层硬度计算深度。保存到 Config/DefaultGame.ini，重启及打包后仍生效。矿镐成长不改变钻机。"))) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 4)
            [ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(Message); }) ]
        ]
    ];
}

bool SMiningPickaxeBalancePanel::IsDirty() const
{
    if(Draft->RangeUpgradeMultiplier!=GetDefault<UMiningPickaxeSettings>()->RangeUpgradeMultiplier ||
       Draft->IntervalUpgradeMultiplier!=GetDefault<UMiningPickaxeSettings>()->IntervalUpgradeMultiplier)return true;
    const auto& Saved = GetDefault<UMiningPickaxeSettings>()->Levels;
    if (Draft->Levels.Num() != Saved.Num()) return true;
    for (int32 Index = 0; Index < Saved.Num(); ++Index)
        if (!FMiningPickaxeLevel::StaticStruct()->CompareScriptStruct(&Draft->Levels[Index], &Saved[Index], 0)) return true;
    return false;
}

FText SMiningPickaxeBalancePanel::GetValidation() const
{
    FString Error;
    if (!Draft->Validate(Error)) return FText::FromString(TEXT("无法保存或试用：") + Error);
    return FText::FromString(IsDirty() ? TEXT("草稿有未保存修改。支持 Ctrl+Z 撤销编辑；关闭窗口会丢弃草稿。") : TEXT("配置有效，与已保存配置一致。"));
}

FText SMiningPickaxeBalancePanel::GetSummary() const
{
    FString Summary = TEXT("等级概览（每击深度继续乘硬度倍率及角色尺寸；范围为洞口边长）");
    for (int32 Index = 0; Index < Draft->Levels.Num(); ++Index)
    {
        const auto Level = Draft->GetLevel(Index);
        Summary += FString::Printf(TEXT("\n等级 %d / 破坏 %d  |  %s  |  边长 %.0f cm  |  每击基础深度 %.1f cm  |  %.3f 秒"),
            Index,Level.DestructionLevel, *Level.GetMaterialCostText(), Level.Width, Level.Depth, Level.Interval);
    }
    return FText::FromString(Summary);
}

FReply SMiningPickaxeBalancePanel::Save()
{
    if (!Draft->Validate(Message)) return FReply::Handled();
    auto* Settings = GetMutableDefault<UMiningPickaxeSettings>();
    const auto Previous = Settings->Levels;
    const float OldRange=Settings->RangeUpgradeMultiplier,OldInterval=Settings->IntervalUpgradeMultiplier;
    Settings->RangeUpgradeMultiplier=Draft->RangeUpgradeMultiplier;Settings->IntervalUpgradeMultiplier=Draft->IntervalUpgradeMultiplier;
    Settings->Levels = Draft->Levels;
    if (!Settings->TryUpdateDefaultConfigFile())
    {
        Settings->Levels = Previous;
        Settings->RangeUpgradeMultiplier=OldRange;Settings->IntervalUpgradeMultiplier=OldInterval;
        Message = TEXT("保存失败，请检查 Config/DefaultGame.ini 是否可写。草稿已保留。");
        return FReply::Handled();
    }
    const int32 Count = ApplySavedToPIE();
    Message = FString::Printf(TEXT("已保存。当前试玩中 %d 个采矿角色已按各自已购等级应用；新开游戏自动读取。"), Count);
    return FReply::Handled();
}

FReply SMiningPickaxeBalancePanel::Reload()
{
    const FScopedTransaction Transaction(FText::FromString(TEXT("重新读取矿镐配置")));
    Draft->Modify();
    Draft->Levels = GetDefault<UMiningPickaxeSettings>()->Levels;
    Draft->RangeUpgradeMultiplier=GetDefault<UMiningPickaxeSettings>()->RangeUpgradeMultiplier;
    Draft->IntervalUpgradeMultiplier=GetDefault<UMiningPickaxeSettings>()->IntervalUpgradeMultiplier;
    Draft->RefreshDerivedLevels();
    Details->ForceRefresh();
    Message = TEXT("已重新读取已保存配置。可用 Ctrl+Z 恢复刚才的草稿；试玩中的临时属性需点恢复已购等级。");
    return FReply::Handled();
}

FReply SMiningPickaxeBalancePanel::Recommended()
{
    const FScopedTransaction Transaction(FText::FromString(TEXT("填入矿镐推荐配置")));
    Draft->Modify();
    Draft->Levels = UMiningPickaxeSettings::MakeRecommendedLevels();
    Draft->RangeUpgradeMultiplier=1.25f;Draft->IntervalUpgradeMultiplier=.75f;Draft->RefreshDerivedLevels();
    Details->ForceRefresh();
    Message = TEXT("推荐配置已填入草稿，尚未保存。可先在试玩中试用。");
    return FReply::Handled();
}

FReply SMiningPickaxeBalancePanel::SetStages(int32 Count)
{
    if (!Draft->Levels.IsValidIndex(PreviewLevel)) return FReply::Handled();
    const FScopedTransaction Transaction(FText::FromString(TEXT("设置矿镐逐击曲线")));
    Draft->Modify();
    Draft->Levels[PreviewLevel].Stages = UMiningPickaxeSettings::MakeRecommendedLevels()[Count == 2 ? 4 : 0].Stages;
    Details->ForceRefresh();
    Message = FString::Printf(TEXT("等级 %d 已填入 %d 击曲线，尚未保存。"), PreviewLevel, Count);
    return FReply::Handled();
}

FReply SMiningPickaxeBalancePanel::Preview()
{
    if (!Draft->Validate(Message)) return FReply::Handled();
    int32 Count = 0;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        UWorld* World = Context.World();
        if (!World || World->WorldType != EWorldType::PIE) continue;
        for (TActorIterator<AActor> Actor(World); Actor; ++Actor)
            if (auto* Mining = Actor->FindComponentByClass<UMiningComponent>())
                Count += Mining->ApplyPickaxeBalance(Draft->GetLevel(PreviewLevel)) ? 1 : 0;
    }
    Message = Count > 0 ? FString::Printf(TEXT("当前试玩的 %d 个采矿角色正在试用等级 %d 草稿，升级等级和背包未改变。"), Count, PreviewLevel)
        : TEXT("请先在编辑器内开始试玩，等待角色出现，再点试用草稿。");
    return FReply::Handled();
}

int32 SMiningPickaxeBalancePanel::ApplySavedToPIE()
{
    int32 Count = 0;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        UWorld* World = Context.World();
        if (!World || World->WorldType != EWorldType::PIE) continue;
        for (TActorIterator<AActor> Actor(World); Actor; ++Actor)
        {
            auto* Mining = Actor->FindComponentByClass<UMiningComponent>();
            if (!Mining) continue;
            const auto* Progression = Actor->FindComponentByClass<UMiningProgressionComponent>();
            Mining->ApplyPickaxeBalance(UMiningPickaxeSettings::ResolveLevel(Progression ? Progression->GetLevel(0) : 0));
            ++Count;
        }
    }
    return Count;
}

FReply SMiningPickaxeBalancePanel::Restore()
{
    Message = FString::Printf(TEXT("已恢复当前试玩中 %d 个采矿角色的已保存、已购等级属性。"), ApplySavedToPIE());
    return FReply::Handled();
}
