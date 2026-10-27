#include "MiningExperienceSubsystem.h"
#include "MiningAchievementSubsystem.h"
#include "MiningLocalizationSubsystem.h"
#include "MiningUISounds.h"
#include "MiningSaveSubsystem.h"
#include "BrainMiningPlayerController.h"
#include "EndingCreditsInput.h"
#include "BrainMiningFonts.h"
#include "Showcase/MiningExhibitHall.h"
#include "MiningComponent.h"
#include "MiningProgressionComponent.h"
#include "MiningWalletSubsystem.h"
#include "MiningBombToolComponent.h"
#include "StartRoom/MiningStartRoom.h"
#include "MiningCollection.h"
#include "EngineUtils.h"
#include "MiningCharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
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
        {TEXT("镜像博物馆 · 独立测试"), TEXT("当前主游戏使用的镜像谜题房间"), TEXT("/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum")},
        {TEXT("涌现结尾 · 独立测试"), TEXT("新结尾房的独立演示"), TEXT("/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo")}
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
            if(Event.GetKey()==EKeys::C || Event.GetKey()==EKeys::Escape)
            {
                auto* World=Owner->GetWorld();
                auto* PC=World?World->GetFirstPlayerController():nullptr;
                auto* Viewport=World?World->GetGameViewport():nullptr;
                auto Widget=Viewport?Viewport->GetGameViewportWidget():nullptr;
                if(Widget&&(Widget->HasKeyboardFocus()||Widget->HasFocusedDescendants()))
                    if(EndingCreditsInput::Handle(PC,Event.GetKey()))return true;
            }
            if (Event.GetKey() == EKeys::Escape)
            {
                // Close settings before opening another modal. Consuming this press
                // prevents a hidden settings panel from surviving underneath pause.
                auto* PC=Owner->GetWorld()?Cast<ABrainMiningPlayerController>(Owner->GetWorld()->GetFirstPlayerController()):nullptr;
                if(PC&&PC->IsAudioSettingsOpen()){PC->ToggleAudioSettings();return true;}
                if(Owner->IsMenuOpen()) { if(Owner->bQuitConfirmation)Owner->CancelQuitConfirmation();else Owner->CloseMenu();return true; }
                // Handle the UMG catalogue before editor/game viewport shortcuts
                // so Escape consistently returns to gameplay, including in PIE.
                if(Owner->GetWorld())for(TActorIterator<AMiningCollectionField> Field(Owner->GetWorld());Field;++Field)
                    if(Field->bCatalogueOpen){Field->ToggleCatalogue();return true;}
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
    Collection.InitializeDependency<UMiningLocalizationSubsystem>();
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

bool UMiningExperienceSubsystem::DeveloperGrantTool(int32 ToolIndex)
{
    auto* PC=MenuController.Get();
    if(!Menu || !bDeveloperMenu || !PC || !PC->GetPawn() || ToolIndex<0 || ToolIndex>2)return false;
    GetGameInstance()->GetSubsystem<UMiningAchievementSubsystem>()->DisableForDebug();
    APawn* Pawn=PC->GetPawn();
    if(ToolIndex==0)
    {
        auto* Progress=Pawn->FindComponentByClass<UMiningProgressionComponent>();
        auto* Wallet=GetGameInstance()->GetSubsystem<UMiningWalletSubsystem>();
        if(!Progress || !Wallet || !Progress->InitializeProgression())return false;
        if(Wallet->UpgradeLevels.Num()<4)Wallet->UpgradeLevels.SetNum(4);
        Wallet->UpgradeLevels[0]=FMath::Clamp(Wallet->UpgradeLevels[0]+1,1,4);
        Progress->ApplyUpgrades();
        if(auto* Room=AMiningStartRoom::Find(GetWorld()))Room->UnlockDeveloperPickaxe();
        MenuError=FString::Printf(TEXT("矿镐已升级至等级 %d%s"),Wallet->UpgradeLevels[0],Wallet->UpgradeLevels[0]==4?TEXT(" · 钻石大矿镐"):TEXT(""));
    }
    else if(ToolIndex==1)
    {
        auto* Bomb=Pawn->FindComponentByClass<UMiningBombToolComponent>();if(!Bomb)return false;
        Bomb->UnlockSkill();Bomb->BombCount=Bomb->MaxBombs;MenuError=TEXT("炸弹已解锁");
    }
    else
    {
        auto* Drill=Pawn->FindComponentByClass<UMiningComponent>();if(!Drill)return false;
        Drill->UnlockDrill();Drill->DrillEnergy=Drill->MaxDrillEnergy;MenuError=TEXT("钻机已解锁，按 V 切换");
    }
    return true;
}

bool UMiningExperienceSubsystem::DeveloperUnlockMineral(int32 ItemId)
{
    if(!Menu || !bDeveloperMenu || !MenuController.IsValid() ||
        (ItemId!=INDEX_NONE && !AMiningCollectible::IsMineralReward(ItemId)))return false;
    GetGameInstance()->GetSubsystem<UMiningAchievementSubsystem>()->DisableForDebug();
    for(TActorIterator<AMiningCollectionField> Field(GetWorld());Field;++Field)
        if(!Field->bCatalogueOnly)
        {
            int32 Added=0;
            for(int32 Id=12;Id<16;++Id)
                if(ItemId==INDEX_NONE || ItemId==Id)Added+=Field->GrantMineralReward(Id)?1:0;
            if(Added>0)CloseMenu();
            else MenuError=TEXT("所选藏品已解锁，不重复发放。");
            return true;
        }
    MenuError=TEXT("当前场景没有可用的藏品系统，请先进入主游戏。");
    return false;
}

bool UMiningExperienceSubsystem::BuildMenu(bool bDeveloper)
{
    UWorld* World = GetWorld();
    auto* Controller = World ? World->GetFirstPlayerController() : nullptr;
    auto* Viewport = GetGameInstance()->GetGameViewportClient();
    if (!Controller || !Controller->IsLocalController() || !Viewport || Controller->IsPaused() ||
        Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored() || Controller->bShowMouseCursor) return false;
    MenuController = Controller;
    CaptureMenuInput(Controller);
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
        Choices->AddSlot().AutoHeight().Padding(0,6)[SNew(STextBlock)
            .Text(FText::FromString(TEXT("主流程 · 通路与关卡跳转"))).Font(BrainMiningFonts::Style("Bold",18))];
        const TCHAR* Destinations[]={TEXT("第1段通路 · 硅质岩区"),TEXT("电脑房"),TEXT("第2段通路 · 溶洞岩区"),
            TEXT("盲人通道 → 卡牌房"),TEXT("第3段通路"),TEXT("博物馆")};
        const int32 AreaCount=Hall?Hall->Areas.Num():UE_ARRAY_COUNT(Destinations);
        for(int32 Index=0;Index<AreaCount;++Index)
        {
            const FString Label=Index<UE_ARRAY_COUNT(Destinations)?FString(Destinations[Index]):Hall->Areas[Index].Title.ToString();
            Choices->AddSlot().AutoHeight().Padding(0,2)[SNew(SButton).ButtonStyle(MiningUISounds::DefaultButton()).ContentPadding(FMargin(12,7))
                .Tag(FName(*FString::Printf(TEXT("GM_Area_%d"),Index))).IsEnabled(!Hall || Hall->bReady)
                .OnClicked_Lambda([this,Index](){JumpToDeveloperArea(Index);return FReply::Handled();})
                [SNew(STextBlock).Text(FText::FromString(TEXT("跳转 · ")+Label)).Font(BrainMiningFonts::Body(18))]];
        }
        if(!Hall)Choices->AddSlot().AutoHeight().Padding(0,4)[SNew(STextBlock)
            .Text(FText::FromString(TEXT("选择后返回主流程，恢复已有存档，再跳到指定入口。"))).Font(BrainMiningFonts::Body(14))];
        Choices->AddSlot().AutoHeight().Padding(0,3)[SNew(SButton).ButtonStyle(MiningUISounds::DefaultButton()).ContentPadding(12)
            .Tag(TEXT("GM_ToggleUI"))
            .OnClicked_Lambda([this](){CloseMenu();ToggleAllUI();return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(bAllUIHidden?TEXT("恢复全部 UI"):TEXT("隐藏全部 UI（录制模式）"))).Font(BrainMiningFonts::Body(18))]];
        if(Hall)
        {
            Choices->AddSlot().AutoHeight().Padding(0,3)[SNew(SButton).ButtonStyle(MiningUISounds::DefaultButton()).ContentPadding(12)
                .Tag(TEXT("GM_ReturnToStart"))
                .OnClicked_Lambda([this](){if(auto* H=AMiningExhibitHall::Find(GetWorld()))H->ReturnToLobby();CloseMenu();return FReply::Handled();})
                [SNew(STextBlock).Text(FText::FromString(TEXT("返回当前通路／关卡起点"))).Font(BrainMiningFonts::Body(18))]];
            Choices->AddSlot().AutoHeight().Padding(0,3)[SNew(SButton).ButtonStyle(MiningUISounds::DefaultButton()).ContentPadding(12)
                .Tag(TEXT("GM_SkipRoom"))
                .OnClicked_Lambda([this](){if(auto* H=AMiningExhibitHall::Find(GetWorld());H && H->SkipCurrentRoom())CloseMenu();else MenuError=TEXT("当前区域没有可以跳过的房间。");return FReply::Handled();})
                [SNew(STextBlock).Text(FText::FromString(TEXT("跳过当前玩法房间"))).Font(BrainMiningFonts::Body(18))]];
        }
        Choices->AddSlot().AutoHeight().Padding(0,10)[SNew(STextBlock).Text(FText::FromString(TEXT("GM · 装备"))).Font(BrainMiningFonts::Style("Bold",18))];
        const TCHAR* Tools[]={TEXT("升级矿镐一级（最高：钻石大矿镐）"),TEXT("解锁炸弹"),TEXT("解锁钻机")};
        for(int32 Tool=0;Tool<3;++Tool)
            Choices->AddSlot().AutoHeight().Padding(0,3)[SNew(SButton).ButtonStyle(MiningUISounds::DefaultButton()).ContentPadding(12)
                .OnClicked_Lambda([this,Tool](){DeveloperGrantTool(Tool);return FReply::Handled();})
                [SNew(STextBlock).Text(FText::FromString(Tools[Tool])).Font(BrainMiningFonts::Body(18))]];
        Choices->AddSlot().AutoHeight().Padding(0,10)[SNew(STextBlock)
            .Text(FText::FromString(TEXT("GM · 矿石藏品（直接解锁，不增加矿石）"))).Font(BrainMiningFonts::Style("Bold",18))];
        for(int32 Id=12;Id<=16;++Id)
        {
            const int32 RewardId=Id==16?INDEX_NONE:Id;
            Choices->AddSlot().AutoHeight().Padding(0,3)[SNew(SButton).ButtonStyle(MiningUISounds::DefaultButton()).ContentPadding(12)
                .Tag(FName(*FString::Printf(TEXT("GM_UnlockMineral_%d"),RewardId)))
                .OnClicked_Lambda([this,RewardId](){DeveloperUnlockMineral(RewardId);return FReply::Handled();})
                [SNew(STextBlock).Text(FText::FromString(Id==16?TEXT("一键解锁四件矿石藏品"):
                    FString::Printf(TEXT("解锁 · %s"),*AMiningCollectible::ItemName(Id)))).Font(BrainMiningFonts::Body(18))]];
        }
        if(Hall && !Hall->bReady)MenuError=TEXT("房间仍在加载，稍后重新打开面板即可跳转。");
        Choices->AddSlot().AutoHeight().Padding(0,12)[SNew(STextBlock).Text(FText::FromString(TEXT("GM · 收集物附近"))).Font(BrainMiningFonts::Style("Bold",18))];
        for(int32 Id=0;Id<AMiningCollectible::ItemTypeCount;++Id)
            if(!AMiningCollectible::IsMineralReward(Id))Choices->AddSlot().AutoHeight().Padding(0,2)[SNew(SButton).ButtonStyle(MiningUISounds::DefaultButton()).ContentPadding(10)
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
        [SNew(SButton).ButtonStyle(MiningUISounds::DefaultButton()).IsEnabled(bAvailable).ContentPadding(FMargin(18, 9)).OnClicked_Lambda([this, Index, bDeveloper]()
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
                        : FString(TEXT("切换关卡会重开试玩，本轮进度不跨地图保存。\nEsc 关闭并继续当前试玩。"))))
                        .Font(BrainMiningFonts::Style("Regular", 15)).ColorAndOpacity(FLinearColor(.75f,.83f,.86f)).AutoWrapText(true)]
                + SVerticalBox::Slot().FillHeight(1)
                    [SNew(SScrollBox).Tag(TEXT("GM_Scroll")) + SScrollBox::Slot()[Choices]]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 6)
                    [SNew(STextBlock).Font(BrainMiningFonts::Body(12)).Text_Lambda([this]() { return FText::FromString(MenuError); })
                        .ColorAndOpacity(FLinearColor(1.f, .55f, .3f)).AutoWrapText(true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,8)
                    [SAssignNew(CloseButton, SButton).ButtonStyle(MiningUISounds::DefaultButton()).HAlign(HAlign_Center).ContentPadding(12)
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

void UMiningExperienceSubsystem::CaptureMenuInput(APlayerController* Controller)
{
    auto* Viewport = GetGameInstance()->GetGameViewportClient();
    MenuPawn = Controller->GetPawn();
    bPreviousMenuCursor = Controller->bShowMouseCursor;
    bPreviousMenuUIOnly = Viewport->IgnoreInput();
    PreviousMenuCapture = Viewport->GetMouseCaptureMode();
    PreviousMenuLock = Viewport->GetMouseLockMode();
    bPreviousMenuHideCursor = Viewport->HideCursorDuringCapture();
    bPreviousMenuGameAndUI = !bPreviousMenuUIOnly && !Controller->ShouldFlushKeysWhenViewportFocusChanges();
    PreviousMenuFocus.Reset();
    if (FSlateApplication::IsInitialized() && Controller->GetLocalPlayer())
        PreviousMenuFocus = FSlateApplication::Get().GetUserFocusedWidget(Controller->GetLocalPlayer()->GetControllerId());
}

void UMiningExperienceSubsystem::RestoreMenuInput(APlayerController* Controller)
{
    auto* Viewport = GetGameInstance()->GetGameViewportClient();
    const bool bSamePawn = MenuPawn.IsValid() && Controller->GetPawn() == MenuPawn.Get();
    const auto Focus = PreviousMenuFocus.Pin();
    if (bSamePawn && bPreviousMenuUIOnly)
    {
        FInputModeUIOnly Mode;
        Mode.SetWidgetToFocus(Focus).SetLockMouseToViewportBehavior(PreviousMenuLock);
        Controller->SetInputMode(Mode);
    }
    else if (bSamePawn && bPreviousMenuGameAndUI)
    {
        FInputModeGameAndUI Mode;
        Mode.SetWidgetToFocus(Focus).SetLockMouseToViewportBehavior(PreviousMenuLock).SetHideCursorDuringCapture(bPreviousMenuHideCursor);
        Controller->SetInputMode(Mode);
    }
    else Controller->SetInputMode(FInputModeGameOnly());
    Controller->bShowMouseCursor = bSamePawn && bPreviousMenuCursor;
    if (Viewport && bSamePawn)
    {
        Viewport->SetMouseCaptureMode(PreviousMenuCapture);
        Viewport->SetMouseLockMode(PreviousMenuLock);
        Viewport->SetHideCursorDuringCapture(bPreviousMenuHideCursor);
    }
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
        RestoreMenuInput(Controller);
    }
    Menu.Reset();
    ResumeButton.Reset();
    CancelQuitButton.Reset();
    PreviousMenuFocus.Reset();
    MenuPawn.Reset();
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
    if (!Menu || !bDeveloperMenu) return false;
    GetGameInstance()->GetSubsystem<UMiningAchievementSubsystem>()->DisableForDebug();
    if(!Hall)
    {
        if(Index<0 || Index>=6)return false;
        PendingDeveloperArea=Index;
        if(OpenDeveloperExperience(0))return true;
        PendingDeveloperArea=INDEX_NONE;return false;
    }
    if (!Hall->EnterDeveloperArea(Index, MenuError)) return false;
    CloseMenu();
    return true;
}

void UMiningExperienceSubsystem::FinishDeveloperTravel()
{
    if(PendingDeveloperArea==INDEX_NONE)return;
    auto* Hall=AMiningExhibitHall::Find(GetWorld());if(!Hall || !Hall->bReady)return;
    const int32 Index=PendingDeveloperArea;PendingDeveloperArea=INDEX_NONE;
    FString Error;
    if(!Hall->EnterDeveloperArea(Index,Error)){BuildMenu(true);MenuError=Error;}
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
    GetGameInstance()->GetSubsystem<UMiningAchievementSubsystem>()->DisableForDebug();
    CloseMenu();
    GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>()->EndSession();
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
    GetGameInstance()->GetSubsystem<UMiningSaveSubsystem>()->EndSession();
    UGameplayStatics::OpenLevel(GetWorld(), FName(*Map));
    return true;
}

bool UMiningExperienceSubsystem::IsMenuOpen() const
{
    return Menu.IsValid();
}

void UMiningExperienceSubsystem::ToggleAllUI()
{
    bAllUIHidden=!bAllUIHidden;
}
