#include "MiningExperienceSubsystem.h"
#include "Showcase/MiningExhibitHall.h"
#include "MiningComponent.h"
#include "MiningCharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace MiningExperiences
{
    struct FEntry { const TCHAR* Title; const TCHAR* Detail; const TCHAR* Map; };
    const FEntry Entries[] = {
        {TEXT("连续展示与宝物"), TEXT("七个展区 · 双钻机 · 背包热量 · 宝物金币"), TEXT("/Game/Showcase/Maps/Lvl_ContinuousShowcase")},
        {TEXT("主矿区"), TEXT("外部通路 · 引导可挖层 · 镐子与钻机"), TEXT("/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo")},
        {TEXT("回声 → 卡牌 → 逃生"), TEXT("黑暗迷宫 · 巨人牌室 · 涨水 · 挖穿电视"), TEXT("/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1")},
        {TEXT("文思博物馆"), TEXT("交换属性词 · 压力与导电机关"), TEXT("/Game/WenSi/Maps/Lvl_WenSi_ScreenBridge")},
        {TEXT("办公室"), TEXT("两台电脑 · 两台打印机 · 任务加速与喷发"), TEXT("/Game/PopupClose/Maps/Lvl_PopupClose")},
        {TEXT("细胞宇宙结局"), TEXT("十一段尺度旅程 · 挖掘与飞行"), TEXT("/Game/CellUniverse/Maps/Lvl_CellUniverse_Demo")},
        {TEXT("收集图鉴"), TEXT("四类藏品 · 环境线索 · 三维检视"), TEXT("/Game/CollectionPrototype/Maps/Lvl_Collection_V03")},
        {TEXT("三维矿道"), TEXT("连续岩体 · 统一基岩 · 三维隧道样例"), TEXT("/Game/MiningPrototype/Maps/Lvl_MiningSpatial_Demo")},
        {TEXT("钢笔矿道"), TEXT("手绘矿道 · 空洞与支线样例"), TEXT("/Game/MiningPrototype/Maps/Lvl_MiningManual_Demo")},
        {TEXT("多矿区串联"), TEXT("主房间 · 独立矿区 · 隐藏洞室"), TEXT("/Game/MiningPrototype/Maps/Lvl_MiningHollowChain_Demo")}
    };

    class FShortcuts : public IInputProcessor
    {
    public:
        explicit FShortcuts(UMiningExperienceSubsystem* Owner) : Subsystem(Owner) {}
        virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
        virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
        {
            auto* Owner = Subsystem.Get();
            if (!Owner || Event.IsRepeat()) return false;
            if (Event.GetKey() == EKeys::F6)
                if (auto* Hall = AMiningExhibitHall::Find(Owner->GetWorld())) { Hall->ReturnToLobby(); return true; }
            if (Event.GetKey() == EKeys::F4) return Owner->ToggleMenu();
            if (Event.GetKey() == EKeys::Escape && Owner->IsMenuOpen()) { Owner->CloseMenu(); return true; }
            return false;
        }
    private:
        TWeakObjectPtr<UMiningExperienceSubsystem> Subsystem;
    };
}

void UMiningExperienceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (FSlateApplication::IsInitialized())
    {
        Shortcuts = MakeShared<MiningExperiences::FShortcuts>(this);
        FSlateApplication::Get().RegisterInputPreProcessor(Shortcuts, 0);
    }
}

void UMiningExperienceSubsystem::Deinitialize()
{
    CloseMenu();
    if (Shortcuts && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(Shortcuts);
    Shortcuts.Reset();
    Super::Deinitialize();
}

TArray<FString> UMiningExperienceSubsystem::MapPaths()
{
    TArray<FString> Paths;
    for (const auto& Entry : MiningExperiences::Entries) Paths.Add(Entry.Map);
    return Paths;
}

bool UMiningExperienceSubsystem::ToggleMenu()
{
    if (auto* Hall = AMiningExhibitHall::Find(GetWorld())) return Hall->ToggleMenu();
    if (Menu) { CloseMenu(); return true; }
    UWorld* World = GetWorld();
    auto* Controller = World ? World->GetFirstPlayerController() : nullptr;
    auto* Viewport = GetGameInstance()->GetGameViewportClient();
    if (!Controller || !Controller->IsLocalController() || !Viewport || Controller->IsPaused() ||
        Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored() || Controller->bShowMouseCursor) return false;
    MenuController = Controller;
    if (APawn* Pawn = Controller->GetPawn())
    {
        MenuMining = Pawn->FindComponentByClass<UMiningComponent>();
        if (MenuMining.IsValid())
        {
            bPreviousMiningSuspended = MenuMining->IsInputSuspended();
            MenuMining->SetInputSuspended(true);
        }
        if (auto* Movement = Pawn->FindComponentByClass<UMiningCharacterMovementComponent>()) Movement->ResetFlightInput();
    }
    auto Choices = SNew(SVerticalBox);
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(MiningExperiences::Entries); ++Index)
    {
        const auto& Entry = MiningExperiences::Entries[Index];
        Choices->AddSlot().AutoHeight().Padding(0, 3)
        [SNew(SButton).ContentPadding(FMargin(18, 9)).OnClicked_Lambda([this, Index]() { OpenExperience(Index); return FReply::Handled(); })
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                    [SNew(STextBlock).Text(FText::FromString(Entry.Title)).Font(FCoreStyle::GetDefaultFontStyle("Bold", 20)).ColorAndOpacity(FLinearColor::White)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,3,0,0)
                    [SNew(STextBlock).Text(FText::FromString(Entry.Detail)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 14)).ColorAndOpacity(FLinearColor(.75f,.83f,.86f)).AutoWrapText(true)]]];
    }
    Menu = SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(.01f,.018f,.025f,.97f)).Padding(24).HAlign(HAlign_Center).VAlign(VAlign_Fill)
        [SNew(SBox).WidthOverride(860).MaxDesiredHeight(850)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(12)
                    [SNew(STextBlock).Text(FText::FromString(TEXT("BrainMining · 全部玩法"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 26)).ColorAndOpacity(FLinearColor::White)]
                + SVerticalBox::Slot().AutoHeight().Padding(12,0,12,10)
                    [SNew(STextBlock).Text(FText::FromString(TEXT("切换关卡会重开试玩，本轮进度不跨地图保存。\nF4 / Esc 关闭并继续当前试玩。"))).Font(FCoreStyle::GetDefaultFontStyle("Regular", 15)).ColorAndOpacity(FLinearColor(.75f,.83f,.86f)).AutoWrapText(true)]
                + SVerticalBox::Slot().FillHeight(1)
                    [SNew(SScrollBox) + SScrollBox::Slot()[Choices]]
                + SVerticalBox::Slot().AutoHeight().Padding(0,8)
                    [SNew(SButton).HAlign(HAlign_Center).ContentPadding(12)
                        .OnClicked_Lambda([this]() { CloseMenu(); return FReply::Handled(); })
                        [SNew(STextBlock).Text(FText::FromString(TEXT("继续当前试玩"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 18)).ColorAndOpacity(FLinearColor::White)]]]];
    Controller->SetIgnoreMoveInput(true);
    Controller->SetIgnoreLookInput(true);
    bPausedByMenu = Controller->SetPause(true);
    Viewport->AddViewportWidgetContent(Menu.ToSharedRef(), 1000);
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(Choices->GetChildren()->GetChildAt(0));
    Controller->SetInputMode(Mode);
    Controller->bShowMouseCursor = true;
    return true;
}

void UMiningExperienceSubsystem::CloseMenu()
{
    if (auto* Hall = AMiningExhibitHall::Find(GetWorld())) Hall->CloseMenu();
    if (!Menu) return;
    if (auto* Viewport = GetGameInstance()->GetGameViewportClient()) Viewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
    if (auto* Controller = MenuController.Get())
    {
        if (bPausedByMenu) Controller->SetPause(false);
        Controller->SetIgnoreMoveInput(false);
        Controller->SetIgnoreLookInput(false);
        Controller->SetInputMode(FInputModeGameOnly());
        Controller->bShowMouseCursor = false;
    }
    Menu.Reset();
    if (MenuMining.IsValid()) MenuMining->SetInputSuspended(bPreviousMiningSuspended);
    MenuMining.Reset();
    bPausedByMenu = false;
    MenuController.Reset();
}

bool UMiningExperienceSubsystem::OpenExperience(int32 Index)
{
    if (AMiningExhibitHall::Find(GetWorld())) return false;
    if (Index < 0 || Index >= UE_ARRAY_COUNT(MiningExperiences::Entries) || !GetWorld()) return false;
    const FString Map = MiningExperiences::Entries[Index].Map;
    if (!FPackageName::DoesPackageExist(Map)) return false;
    CloseMenu();
    UGameplayStatics::OpenLevel(GetWorld(), FName(*Map));
    return true;
}

bool UMiningExperienceSubsystem::IsMenuOpen() const
{
    if (auto* Hall = AMiningExhibitHall::Find(GetWorld())) return Hall->IsMenuOpen();
    return Menu.IsValid();
}
