#include "MiningExperienceSubsystem.h"
#include "BrainMiningFonts.h"
#include "Showcase/MiningExhibitHall.h"
#include "MiningComponent.h"
#include "MiningCollection.h"
#include "EngineUtils.h"
#include "MiningCharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
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
#include "Widgets/SViewport.h"

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

    const FEntry DeveloperEntries[] = {
        {TEXT("主游戏 · MainGame"), TEXT("重新载入三矿区主流程，再用上方房间列表跳转"), TEXT("/Game/MainGame/Maps/Lvl_MainGame")},
        {TEXT("办公室 · 独立测试"), TEXT("电脑弹窗、打印机、炸弹与撤离，从房间初始状态开始"), TEXT("/Game/PopupClose/Maps/Lvl_PopupClose")},
        {TEXT("回声 → 卡牌 → 电视逃生 · 独立测试"), TEXT("完整联合玩法，从回声通道入口开始"), TEXT("/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1")},
        {TEXT("卡牌房 · 独立测试"), TEXT("直接进入卡牌房，无需先走回声通道"), TEXT("/Game/Rooms/CardDuel/Maps/Lvl_CardRoom_P1")},
        {TEXT("镜像博物馆 · 独立测试"), TEXT("当前主游戏使用的镜像谜题房间"), TEXT("/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum")},
        {TEXT("涌现结尾 · 独立测试"), TEXT("新结尾房的独立演示"), TEXT("/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo")},
        {TEXT("旧主流程 · FullJourney"), TEXT("重新载入旧主流程，可继续使用房间跳转"), TEXT("/Game/Campaign/Maps/Lvl_FullJourney")}
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
            if (Event.GetKey() == EKeys::F10 && Event.IsControlDown())
            {
                auto* Viewport = Owner->GetWorld() ? Owner->GetWorld()->GetGameViewport() : nullptr;
                const auto ViewportWidget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
                if (ViewportWidget && (ViewportWidget->HasKeyboardFocus() || ViewportWidget->HasFocusedDescendants() || Owner->IsMenuOpen()))
                    return Owner->ToggleDeveloperMenu();
                return false;
            }
            if (Event.GetKey() == EKeys::F6 && !Owner->IsMenuOpen())
                if (auto* Hall = AMiningExhibitHall::Find(Owner->GetWorld())) { Hall->ReturnToLobby(); return true; }
            if (Event.GetKey() == EKeys::F4) return Owner->ToggleMenu();
            if (Event.GetKey() == EKeys::Escape)
            {
                if(Owner->IsMenuOpen()) { if(Owner->bQuitConfirmation)Owner->CancelQuitConfirmation();else Owner->CloseMenu();return true; }
                auto* Viewport=Owner->GetWorld()?Owner->GetWorld()->GetGameViewport():nullptr;
                auto Widget=Viewport?Viewport->GetGameViewportWidget():nullptr;
                if(Widget&&(Widget->HasKeyboardFocus()||Widget->HasFocusedDescendants()))return Owner->TogglePauseMenu();
            }
            return false;
        }
    private:
        TWeakObjectPtr<UMiningExperienceSubsystem> Subsystem;
    };
}

void UMiningExperienceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::OnWorldCleanup);
    if (FSlateApplication::IsInitialized())
    {
        Shortcuts = MakeShared<MiningExperiences::FShortcuts>(this);
        FSlateApplication::Get().RegisterInputPreProcessor(Shortcuts, 0);
    }
}

void UMiningExperienceSubsystem::Deinitialize()
{
    CloseMenu();
    FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
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
    if (Menu) { CloseMenu(); return true; }
    if (AMiningExhibitHall::Find(GetWorld())) return false;
    return BuildMenu(false);
}

bool UMiningExperienceSubsystem::ToggleDeveloperMenu()
{
    if (Menu) { CloseMenu(); return true; }
    if (auto* Hall = AMiningExhibitHall::Find(GetWorld()))
    {
        if (Hall->IsMapOpen()) Hall->CloseMap();
        if (Hall->IsInteractionOpen()) return false;
    }
    return BuildMenu(true);
}

bool UMiningExperienceSubsystem::BuildMenu(bool bDeveloper)
{
    UWorld* World = GetWorld();
    auto* Controller = World ? World->GetFirstPlayerController() : nullptr;
    auto* Viewport = GetGameInstance()->GetGameViewportClient();
    if (!Controller || !Controller->IsLocalController() || !Viewport || Controller->IsPaused() ||
        Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored() || Controller->bShowMouseCursor) return false;
    MenuController = Controller;
    MenuWorld = World;
    bDeveloperMenu = bDeveloper;
    MenuError.Empty();
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
    if (bDeveloper)
    {
        auto* Hall = AMiningExhibitHall::Find(World);
        Choices->AddSlot().AutoHeight().Padding(0, 10)
            [SNew(STextBlock).Text(FText::FromString(Hall ? TEXT("当前地图 · 跳转到区域入口（保留房间进度）") : TEXT("当前地图无主流程区域，请使用下方独立玩法入口。")))
                .Font(BrainMiningFonts::Style("Bold", 18)).AutoWrapText(true)];
        if (Hall)
        {
            for (int32 Index = 0; Index < Hall->Areas.Num(); ++Index)
            {
                Choices->AddSlot().AutoHeight().Padding(0, 3)
                    [SNew(SButton).IsEnabled(Hall->bReady).ContentPadding(FMargin(18, 9))
                        .OnClicked_Lambda([this, Index]() { JumpToDeveloperArea(Index); return FReply::Handled(); })
                        [SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("跳转 · %s%s"), *Hall->Areas[Index].Title.ToString(),
                            Index == Hall->ActiveArea ? TEXT("（当前）") : TEXT("")))).Font(BrainMiningFonts::Body(18))]];
            }
            if (!Hall->bReady) MenuError = TEXT("房间仍在加载，稍后重新打开面板即可跳转。");
        }
        Choices->AddSlot().AutoHeight().Padding(0,12)[SNew(STextBlock).Text(FText::FromString(TEXT("GM · 收集物附近"))).Font(BrainMiningFonts::Style("Bold",18))];
        for(int32 Id=0;Id<AMiningCollectible::ItemTypeCount;++Id)
            Choices->AddSlot().AutoHeight().Padding(0,2)[SNew(SButton).ContentPadding(10)
                .OnClicked_Lambda([this,Id](){JumpToCollectible(Id);return FReply::Handled();})
                [SNew(STextBlock).Font(BrainMiningFonts::Body(12)).Text(FText::FromString(AMiningCollectible::ItemName(Id)))]];
        Choices->AddSlot().AutoHeight().Padding(0, 16, 0, 6)
            [SNew(STextBlock).Text(FText::FromString(TEXT("独立玩法 · 切换地图并从头测试房间"))).Font(BrainMiningFonts::Style("Bold", 18))];
    }
    const MiningExperiences::FEntry* MenuEntries = MiningExperiences::Entries;
    int32 EntryCount = UE_ARRAY_COUNT(MiningExperiences::Entries);
    if (bDeveloper) { MenuEntries = MiningExperiences::DeveloperEntries; EntryCount = UE_ARRAY_COUNT(MiningExperiences::DeveloperEntries); }
    for (int32 Index = 0; Index < EntryCount; ++Index)
    {
        const auto& Entry = MenuEntries[Index];
        const bool bAvailable = FPackageName::DoesPackageExist(Entry.Map);
        Choices->AddSlot().AutoHeight().Padding(0, 3)
        [SNew(SButton).IsEnabled(bAvailable).ContentPadding(FMargin(18, 9)).OnClicked_Lambda([this, Index, bDeveloper]()
            { if (bDeveloper) OpenDeveloperExperience(Index); else OpenExperience(Index); return FReply::Handled(); })
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                    [SNew(STextBlock).Text(FText::FromString(Entry.Title)).Font(BrainMiningFonts::Style("Bold", 20)).ColorAndOpacity(FLinearColor::White)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,3,0,0)
                    [SNew(STextBlock).Text(FText::FromString(bAvailable ? Entry.Detail : TEXT("当前安装或构建未包含此关卡"))).Font(BrainMiningFonts::Style("Regular", 14)).ColorAndOpacity(FLinearColor(.75f,.83f,.86f)).AutoWrapText(true)]]];
    }
    TSharedPtr<SButton> CloseButton;
    Menu = SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(.01f,.018f,.025f,.97f)).Padding(24).HAlign(HAlign_Center).VAlign(VAlign_Fill)
        [SNew(SBox).WidthOverride(860).MaxDesiredHeight(850)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(12)
                    [SNew(STextBlock).Text(FText::FromString(bDeveloper ? TEXT("开发者 · 房间跳转与玩法测试") : TEXT("BrainMining · 全部玩法"))).Font(BrainMiningFonts::Style("Bold", 26)).ColorAndOpacity(FLinearColor::White)]
                + SVerticalBox::Slot().AutoHeight().Padding(12,0,12,10)
                    [SNew(STextBlock).Text(FText::FromString(bDeveloper
                        ? FString::Printf(TEXT("当前地图：%s\n跳转可进入未解锁区域，并补齐开局装备；不会自动通关。\n独立测试会离开当前地图，当前房间进度不保留。Ctrl + F10 / Esc 关闭。"), *UGameplayStatics::GetCurrentLevelName(World))
                        : FString(TEXT("切换关卡会重开试玩，本轮进度不跨地图保存。\nF4 / Esc 关闭并继续当前试玩。"))))
                        .Font(BrainMiningFonts::Style("Regular", 15)).ColorAndOpacity(FLinearColor(.75f,.83f,.86f)).AutoWrapText(true)]
                + SVerticalBox::Slot().FillHeight(1)
                    [SNew(SScrollBox) + SScrollBox::Slot()[Choices]]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 6)
                    [SNew(STextBlock).Font(BrainMiningFonts::Body(12)).Text_Lambda([this]() { return FText::FromString(MenuError); })
                        .ColorAndOpacity(FLinearColor(1.f, .55f, .3f)).AutoWrapText(true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,8)
                    [SAssignNew(CloseButton, SButton).HAlign(HAlign_Center).ContentPadding(12)
                        .OnClicked_Lambda([this]() { CloseMenu(); return FReply::Handled(); })
                        [SNew(STextBlock).Text(FText::FromString(TEXT("继续当前试玩"))).Font(BrainMiningFonts::Body(18)).ColorAndOpacity(FLinearColor::White)]]]];
    Controller->SetIgnoreMoveInput(true);
    Controller->SetIgnoreLookInput(true);
    bPausedByMenu = Controller->SetPause(true);
    Viewport->AddViewportWidgetContent(Menu.ToSharedRef(), 1000);
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(CloseButton);
    Controller->SetInputMode(Mode);
    Controller->bShowMouseCursor = true;
    return true;
}

void UMiningExperienceSubsystem::CloseMenu()
{
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
    MenuWorld.Reset();
    bDeveloperMenu = false;bPauseMenu=false;bQuitConfirmation=false;
}

void UMiningExperienceSubsystem::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
    if (MenuWorld.Get() == World) CloseMenu();
}

TArray<FString> UMiningExperienceSubsystem::DeveloperMapPaths()
{
    TArray<FString> Paths;
    for (const auto& Entry : MiningExperiences::DeveloperEntries) Paths.Add(Entry.Map);
    return Paths;
}

bool UMiningExperienceSubsystem::JumpToDeveloperArea(int32 Index)
{
    auto* Hall = AMiningExhibitHall::Find(GetWorld());
    if (!Menu || !bDeveloperMenu || !Hall) return false;
    if (!Hall->EnterDeveloperArea(Index, MenuError)) return false;
    CloseMenu();
    return true;
}

bool UMiningExperienceSubsystem::OpenDeveloperExperience(int32 Index)
{
    if (!Menu || !bDeveloperMenu || !GetWorld()) return false;
    const auto Paths = DeveloperMapPaths();
    if (!Paths.IsValidIndex(Index) || !FPackageName::DoesPackageExist(Paths[Index]))
    {
        MenuError = TEXT("找不到目标关卡，请检查关卡是否已包含在当前构建中。");
        return false;
    }
    const FName Map(*Paths[Index]);
    CloseMenu();
    UGameplayStatics::OpenLevel(GetWorld(), Map);
    return true;
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
    return Menu.IsValid();
}
