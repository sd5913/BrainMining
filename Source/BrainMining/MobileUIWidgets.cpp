#include "MobileUIWidgets.h"
#include "BrainMiningFonts.h"
#include "BrainMiningPlayerController.h"
#include "MiningAudioComponent.h"
#include "MiningAudioSettings.h"
#include "MiningCharacterMovementComponent.h"
#include "MiningComponent.h"
#include "MiningExperienceSubsystem.h"
#include "MiningHapticsSubsystem.h"
#include "MiningPickaxeSettings.h"
#include "MiningProgressionComponent.h"
#include "MiningWalletSubsystem.h"
#include "StartRoom/MiningOpening.h"
#include "Brushes/SlateImageBrush.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace MiningMobileUI
{
    const FSlateBrush* Brush(FName Name)
    {
        static TMap<FName, TSharedPtr<FSlateVectorImageBrush>> Brushes;
        auto& Result = Brushes.FindOrAdd(Name);
        if (!Result) Result = MakeShared<FSlateVectorImageBrush>(FPaths::ProjectContentDir() / TEXT("UI/Mobile") / (Name.ToString() + TEXT(".svg")), FVector2D(192));
        return Result.Get();
    }

    FLinearColor Ink() { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("282A26"))); }
    FLinearColor Cream() { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("F4EBD6"))); }
    FLinearColor Gold() { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E9BB58"))); }
    FLinearColor Teal() { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8EB7AA"))); }
    float Scale(FVector2D Size) { return FMath::Max(.35f, FMath::Min(Size.Y / 720.f, Size.X / 1280.f)); }
}

namespace
{
    using namespace MiningMobileUI;

    float WidgetScale(const SWidget& Widget)
    {
        const FVector2D Size = Widget.GetCachedGeometry().GetLocalSize();
        return Size.X > 0 && Size.Y > 0 ? Scale(Size) : 1.f;
    }

    const FButtonStyle* MobileButtonStyle()
    {
        static FButtonStyle Style;
        static bool bReady = false;
        if (!bReady)
        {
            FSlateBrush Pressed = *Brush(TEXT("panel"));
            Pressed.TintColor = FLinearColor(.72f, .72f, .72f, 1.f);
            Style.SetNormal(*Brush(TEXT("panel"))).SetHovered(*Brush(TEXT("panel"))).SetPressed(Pressed)
                .SetNormalPadding(FMargin(12)).SetPressedPadding(FMargin(14));
            bReady = true;
        }
        return &Style;
    }

    TSharedRef<SWidget> Glyph(FName Name, float Extent = 52, FLinearColor Color = Cream())
    {
        return SNew(SBox).WidthOverride(Extent).HeightOverride(Extent)
            [SNew(SImage).Image(Brush(Name)).ColorAndOpacity(Color)];
    }

    TSharedRef<SButton> IconButton(FName Name, TFunction<void()> Action, bool bPrimary = false)
    {
        return SNew(SButton).ButtonStyle(MobileButtonStyle()).HAlign(HAlign_Center).VAlign(VAlign_Center)
            .ButtonColorAndOpacity(bPrimary ? Gold() : Ink())
            .OnClicked_Lambda([Action]() { Action(); return FReply::Handled(); })
            [Glyph(Name, 52, bPrimary ? Ink() : Cream())];
    }

    enum class EMobileMenuPage : uint8 { Pause, Upgrade, Sound, Controls, Help, Quit };

    class SMobileGameMenu : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SMobileGameMenu) {}
            SLATE_ARGUMENT(TWeakObjectPtr<ABrainMiningPlayerController>, Controller)
            SLATE_ARGUMENT(TWeakObjectPtr<UMiningExperienceSubsystem>, Owner)
            SLATE_ARGUMENT(bool, Upgrade)
        SLATE_END_ARGS()

        void Construct(const FArguments& Args)
        {
            Controller = Args._Controller;
            Owner = Args._Owner;
            ChangePage(Args._Upgrade ? EMobileMenuPage::Upgrade : EMobileMenuPage::Pause);
        }

        virtual ~SMobileGameMenu() override
        {
            if (bSettingsChanged && Controller.IsValid())
            {
                Controller->SaveConfig();
                if (auto* Settings = Controller->GetGameInstance()->GetSubsystem<UMiningAudioSettings>()) Settings->Save();
            }
        }

        virtual void Tick(const FGeometry& Geometry, double Time, float Delta) override
        {
            SCompoundWidget::Tick(Geometry, Time, Delta);
            if (Page == EMobileMenuPage::Quit && Owner.IsValid() && !Owner->bQuitConfirmation) ChangePage(EMobileMenuPage::Pause);
        }

        TSharedPtr<SButton> FirstButton;

    private:
        void Close()
        {
            if (Owner.IsValid()) Owner->CloseMenu();
        }

        TSharedRef<SWidget> RowButton(FName Name, TFunction<void()> Action, bool bPrimary = false)
        {
            auto Button = IconButton(Name, Action, bPrimary);
            if (!FirstButton) FirstButton = Button;
            return SNew(SBox).HeightOverride(96)[Button];
        }

        void ChangePage(EMobileMenuPage Next)
        {
            Page = Next;
            FirstButton.Reset();
            if (Owner.IsValid()) Owner->bQuitConfirmation = Page == EMobileMenuPage::Quit;
            auto Rows = SNew(SVerticalBox);
            auto CloseButton = IconButton(TEXT("close"), [this]() { if (Page == EMobileMenuPage::Pause || Page == EMobileMenuPage::Upgrade) Close(); else ChangePage(EMobileMenuPage::Pause); });
            Rows->AddSlot().AutoHeight().HAlign(HAlign_Right).Padding(0, 0, 0, 16)
                [SNew(SBox).WidthOverride(80).HeightOverride(80)[CloseButton]];
            if (Page == EMobileMenuPage::Pause)
            {
                Rows->AddSlot().AutoHeight().Padding(0, 0, 0, 24)[RowButton(TEXT("play"), [this]() { Close(); }, true)];
                Rows->AddSlot().AutoHeight().Padding(0, 0, 0, 24)
                    [SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 12, 0)[RowButton(TEXT("sound"), [this]() { ChangePage(EMobileMenuPage::Sound); })]
                        + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 12, 0)[RowButton(TEXT("sliders"), [this]() { ChangePage(EMobileMenuPage::Controls); })]
                        + SHorizontalBox::Slot().FillWidth(1)[RowButton(TEXT("help"), [this]() { ChangePage(EMobileMenuPage::Help); })]];
                Rows->AddSlot().AutoHeight()[RowButton(TEXT("door"), [this]() { ChangePage(EMobileMenuPage::Quit); })];
            }
            else if (Page == EMobileMenuPage::Quit)
            {
                Rows->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 12, 0, 32)[Glyph(TEXT("door"), 84, Ink())];
                Rows->AddSlot().AutoHeight().Padding(0, 0, 0, 24)[RowButton(TEXT("check"), [this]()
                {
                    auto* Player = Controller.Get();
                    Close();
                    if (Player) UKismetSystemLibrary::QuitGame(Player, Player, EQuitPreference::Quit, false);
                }, true)];
                Rows->AddSlot().AutoHeight()[RowButton(TEXT("back"), [this]() { ChangePage(EMobileMenuPage::Pause); })];
            }
            else if (Page == EMobileMenuPage::Upgrade) BuildUpgrade(Rows);
            else if (Page == EMobileMenuPage::Sound) BuildSound(Rows);
            else if (Page == EMobileMenuPage::Controls)
            {
                Rows->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 20, 0, 36)[Glyph(TEXT("sliders"), 64, Ink())];
                Rows->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(80)
                    [SNew(SSlider).Value_Lambda([this]() { return Controller.IsValid() ? (Controller->MobileLookSensitivity - .03f) / .15f : .4f; })
                        .OnValueChanged_Lambda([this](float Value) { if (Controller.IsValid()) { Controller->MobileLookSensitivity = .03f + .15f * Value; bSettingsChanged = true; } })]];
            }
            else
            {
                for (const auto Pair : {TPair<FName, FName>(TEXT("pick"), TEXT("drill")), TPair<FName, FName>(TEXT("jet"), TEXT("bomb")), TPair<FName, FName>(TEXT("hand"), TEXT("knock"))})
                    Rows->AddSlot().AutoHeight().Padding(0, 12)
                        [SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Center)[Glyph(Pair.Key, 72, Ink())]
                            + SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Center)[Glyph(Pair.Value, 72, Ink())]];
            }
            if (!FirstButton) FirstButton = CloseButton;
            ChildSlot
                [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink().CopyWithNewOpacity(.72f))
                    [SNew(SSafeZone).Padding(24)
                        [SNew(SDPIScaler).DPIScale_Lambda([this]() { return WidgetScale(*this); })
                        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                            [SNew(SBox).WidthOverride(512).HeightOverride(600)
                                [SNew(SBorder).BorderImage(Brush(TEXT("panel"))).BorderBackgroundColor(Cream()).Padding(40)
                                    [SNew(SScrollBox) + SScrollBox::Slot()[Rows]]]]]]]];
            if (Owner.IsValid() && Owner->IsMenuOpen() && FSlateApplication::IsInitialized())
                FSlateApplication::Get().SetKeyboardFocus(FirstButton, EFocusCause::SetDirectly);
        }

        void BuildUpgrade(TSharedRef<SVerticalBox> Rows)
        {
            auto* Progression = Controller.IsValid() && Controller->GetPawn() ? Controller->GetPawn()->FindComponentByClass<UMiningProgressionComponent>() : nullptr;
            if (!Progression || (!Progression->bInitialized && !Progression->InitializeProgression())) return;
            const int32 Level = Progression->GetLevel(0);
            Rows->AddSlot().AutoHeight().Padding(0, 0, 0, 24)
                [SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[Glyph(TEXT("pick"), 96, Ink())]
                    + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).HAlign(HAlign_Right)
                    [SNew(STextBlock).Font(BrainMiningFonts::Style("Bold", 36)).ColorAndOpacity(Ink())
                        .Text(FText::FromString(Level >= 4 ? FString::FromInt(Level) : FString::Printf(TEXT("%d → %d"), Level, Level + 1)))]];
            if (const auto* Wallet = Progression->GetWallet(); Wallet && Level < 4)
            {
                if (Wallet->UsesMineralInventory())
                {
                    for (const auto& Cost : UMiningPickaxeSettings::ResolveLevel(Level + 1).MaterialCosts)
                    {
                        const int32 Owned = Wallet->GetMineralCount(Cost.Mineral);
                        const FLinearColor Colors[] = {Gold(), FLinearColor(.45f, .45f, .4f), Teal(), FLinearColor(.4f, .7f, .8f)};
                        const int32 Index = Cost.Mineral == EMiningMineral::Copper ? 0 : Cost.Mineral == EMiningMineral::Iron ? 1 : Cost.Mineral == EMiningMineral::Malachite ? 2 : 3;
                        Rows->AddSlot().AutoHeight().Padding(0, 12)
                            [SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[Glyph(TEXT("gem"), 42, Colors[Index])]
                                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).HAlign(HAlign_Right)
                                    [SNew(STextBlock).Font(BrainMiningFonts::Style("Bold", 26)).ColorAndOpacity(Owned >= Cost.Count ? Ink() : FLinearColor(.65f,.12f,.06f))
                                        .Text(FText::FromString(FString::Printf(TEXT("%d / %d"), Owned, Cost.Count)))]];
                    }
                }
                else Rows->AddSlot().AutoHeight().Padding(0, 20)[SNew(STextBlock).Font(BrainMiningFonts::Style("Bold", 26)).ColorAndOpacity(Ink())
                    .Text(FText::FromString(FString::Printf(TEXT("%lld / %lld"), Wallet->Balance, Progression->GetNextPrice(0))))];
            }
            auto Buy = RowButton(Level >= 4 ? TEXT("check") : TEXT("up"), [this]()
            {
                auto* Progression = Controller.IsValid() && Controller->GetPawn() ? Controller->GetPawn()->FindComponentByClass<UMiningProgressionComponent>() : nullptr;
                const bool bPurchased = Progression && Progression->Purchase(0, Progression->GetLevel(0) + 1);
                if (auto* Audio = Controller.IsValid() && Controller->GetPawn() ? Controller->GetPawn()->FindComponentByClass<UMiningAudioComponent>() : nullptr) Audio->PlayPurchase(bPurchased);
                UMiningHapticsSubsystem::Emit(Controller.Get(), bPurchased ? EMiningHapticCue::UpgradeSuccess : EMiningHapticCue::UpgradeRejected);
                ChangePage(EMobileMenuPage::Upgrade);
            }, true);
            Buy->SetEnabled(Level < 4);
            Rows->AddSlot().AutoHeight().Padding(0, 24, 0, 0)[Buy];
        }

        void BuildSound(TSharedRef<SVerticalBox> Rows)
        {
            auto* Settings = Controller.IsValid() ? Controller->GetGameInstance()->GetSubsystem<UMiningAudioSettings>() : nullptr;
            if (!Settings) return;
            const FName Names[] = {TEXT("sound"), TEXT("music"), TEXT("bomb"), TEXT("ambient"), TEXT("voice")};
            TWeakObjectPtr<UMiningAudioSettings> WeakSettings(Settings);
            for (int32 Channel = 0; Channel < 5; ++Channel)
                Rows->AddSlot().AutoHeight().Padding(0, 6)
                    [SNew(SBox).HeightOverride(60)[SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 24, 0)[Glyph(Names[Channel], 36, Ink())]
                        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SSlider)
                            .Value_Lambda([WeakSettings, Channel]() { return WeakSettings.IsValid() ? WeakSettings->GetVolume(Channel) : 0.f; })
                            .OnValueChanged_Lambda([this, WeakSettings, Channel](float Value) { if (WeakSettings.IsValid()) { WeakSettings->SetVolume(Channel, Value); bSettingsChanged = true; } })]]];
            if (auto* Haptics = UMiningHapticsSubsystem::Find(Controller.Get()))
            {
                TWeakObjectPtr<UMiningHapticsSubsystem> WeakHaptics(Haptics);
                Rows->AddSlot().AutoHeight().Padding(0, 12)
                    [SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 24, 0)[Glyph(TEXT("vibration"), 36, Ink())]
                        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SCheckBox)
                            .IsChecked_Lambda([WeakHaptics]() { return WeakHaptics.IsValid() && WeakHaptics->bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                            .OnCheckStateChanged_Lambda([WeakHaptics](ECheckBoxState State) { if (WeakHaptics.IsValid()) WeakHaptics->SetEnabled(State == ECheckBoxState::Checked); })]];
            }
        }

        TWeakObjectPtr<ABrainMiningPlayerController> Controller;
        TWeakObjectPtr<UMiningExperienceSubsystem> Owner;
        EMobileMenuPage Page = EMobileMenuPage::Pause;
        bool bSettingsChanged = false;
    };
}

TSharedRef<SWidget> MiningMobileUI::MakeToolbar(ABrainMiningPlayerController* Controller)
{
    TWeakObjectPtr<ABrainMiningPlayerController> Player(Controller);
    auto Back = IconButton(TEXT("back"), [Player]()
    {
        if (!Player.IsValid()) return;
        if (Player->GetMobileActionLabel() == TEXT("CLICK")) Player->HandleMobileInteract();
        else if (Player->IsMobileThoughtVisible() && Player->bShowMouseCursor) Player->HandleMobileEdit();
        else Player->HandleMobileBack();
    });
    Back->SetVisibility(TAttribute<EVisibility>::CreateLambda([Player]()
    {
        return Player.IsValid() && (Player->IsMobileBackVisible() || Player->GetMobileActionLabel() == TEXT("CLICK") || (Player->IsMobileThoughtVisible() && Player->bShowMouseCursor)) ? EVisibility::Visible : EVisibility::Collapsed;
    }));
    auto Upgrade = IconButton(TEXT("up"), [Player]() { if (Player.IsValid()) Player->HandleMobileUpgrade(); });
    Upgrade->SetVisibility(TAttribute<EVisibility>::CreateLambda([Player]() { return Player.IsValid() && Player->IsMobileUpgradeVisible() && !Player->IsMoveInputIgnored() && !Player->IsLookInputIgnored() ? EVisibility::Visible : EVisibility::Collapsed; }));
    TSharedRef<SOverlay> Toolbar = SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible);
    TWeakPtr<SOverlay> WeakToolbar(Toolbar);
    Toolbar->AddSlot().HAlign(HAlign_Right).VAlign(VAlign_Top)
        [SNew(SSafeZone).Visibility(EVisibility::SelfHitTestInvisible)
            [SNew(SDPIScaler).DPIScale_Lambda([WeakToolbar]() { const auto Widget = WeakToolbar.Pin(); return Widget ? WidgetScale(*Widget) : 1.f; })
                [SNew(SBox).Padding(32).Visibility_Lambda([Player]()
                {
                    if (!Player.IsValid() || AMiningOpening::IsOpening(Player->GetWorld()) || Player->IsAudioSettingsOpen()) return EVisibility::Collapsed;
                    const auto* Menu = Player->GetGameInstance()->GetSubsystem<UMiningExperienceSubsystem>();
                    return Menu && Menu->IsMenuOpen() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible;
                })[SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 32, 0)[SNew(SBox).WidthOverride(100).HeightOverride(96)[Back]]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 32, 0)[SNew(SBox).WidthOverride(100).HeightOverride(96)[Upgrade]]
                    + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(100).HeightOverride(96)
                        [IconButton(TEXT("pause"), [Player]() { if (Player.IsValid()) Player->HandleMobilePause(); }, true)]]]]]];
    return Toolbar;
}

bool UMiningExperienceSubsystem::ToggleMobileMenu(bool bUpgrade)
{
    if (Menu) { CloseMenu(); return true; }
    auto* Player = GetWorld() ? Cast<ABrainMiningPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
    auto* Viewport = GetGameInstance()->GetGameViewportClient();
    if (!Player || !Player->IsLocalController() || !Player->GetPawn() || !Viewport || AMiningOpening::IsOpening(GetWorld())) return false;
    Player->ResetMobileInput();
    CaptureMenuInput(Player);
    MenuController = Player;
    MenuWorld = GetWorld();
    bPauseMenu = true;
    bQuitConfirmation = false;
    MenuMining = Player->GetPawn()->FindComponentByClass<UMiningComponent>();
    if (MenuMining.IsValid()) { bPreviousMiningSuspended = MenuMining->IsInputSuspended(); MenuMining->SetInputSuspended(true); }
    if (auto* Movement = Player->GetPawn()->FindComponentByClass<UMiningCharacterMovementComponent>()) { Movement->ResetFlightInput(); Movement->StopMovementImmediately(); }
    auto Panel = SNew(SMobileGameMenu).Controller(Player).Owner(this).Upgrade(bUpgrade);
    Menu = Panel;
    ResumeButton = Panel->FirstButton;
    Player->SetIgnoreMoveInput(true);
    Player->SetIgnoreLookInput(true);
    bPausedByMenu = !Player->IsPaused() && Player->SetPause(true);
    Viewport->AddViewportWidgetContent(Menu.ToSharedRef(), 2500);
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(Panel->FirstButton);
    Player->SetInputMode(Mode);
    Player->bShowMouseCursor = true;
    return true;
}
