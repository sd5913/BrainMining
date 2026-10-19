#include "MiningInteractionHUDWidget.h"
#include "MainGameDoor.h"
#include "MiningGamepadSubsystem.h"
#include "BrainMiningFonts.h"
#include "BrainMiningPlayerController.h"
#include "MiningGUIWidgets.h"
#include "MiningCollection.h"
#include "WenSi/WenSiMirrorPuzzle.h"
#include "WenSi/MouseMuseumPortal.h"
#include "MiningComponent.h"
#include "DiggableRock.h"
#include "MiningWalletSubsystem.h"
#include "Showcase/MiningExhibitHall.h"
#include "Showcase/MiningTreasure.h"
#include "PopupClose/PopupCloseStage.h"
#include "WenSi/WenSiPuzzle.h"
#include "CardRoomPlayer.h"
#include "CardRoomStage.h"
#include "EchoPassagePlayer.h"
#include "StartRoom/MiningStartRoom.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"

bool UMiningInteractionHUDWidget::IsActiveFor(const APlayerController* Controller)
{
    const auto* MiningController = Cast<ABrainMiningPlayerController>(Controller);
    return MiningController && MiningController->InteractionHUD && MiningController->InteractionHUD->IsInViewport();
}

TSharedRef<SWidget> UMiningInteractionHUDWidget::RebuildWidget()
{
    return SNew(SBox);
}

void UMiningInteractionHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    if (auto* Instance = GetGameInstance())
    {
        Wallet = Instance->GetSubsystem<UMiningWalletSubsystem>();
        if (Wallet.IsValid())
        {
            Wallet->OnCoinsAdded.Remove(CoinsAddedHandle);
            CoinsAddedHandle = Wallet->OnCoinsAdded.AddUObject(this, &ThisClass::ShowCoinGain);
        }
    }
    if (auto* Controller = GetOwningPlayer())
    {
        if (auto* Cards = Controller->FindComponentByClass<UCardRoomInteractionComponent>()) Cards->bUseSharedInteractionHUD = true;
        if (auto* Echo = Controller->FindComponentByClass<UEchoPassageInteractionComponent>()) Echo->bUseSharedInteractionHUD = true;
    }
}

void UMiningInteractionHUDWidget::NativeDestruct()
{
    if (Wallet.IsValid()) Wallet->OnCoinsAdded.Remove(CoinsAddedHandle);
    CoinsAddedHandle.Reset();
    if (auto* Controller = GetOwningPlayer())
    {
        if (auto* Cards = Controller->FindComponentByClass<UCardRoomInteractionComponent>()) Cards->bUseSharedInteractionHUD = false;
        if (auto* Echo = Controller->FindComponentByClass<UEchoPassageInteractionComponent>()) Echo->bUseSharedInteractionHUD = false;
    }
    Super::NativeDestruct();
}

void UMiningInteractionHUDWidget::ShowCoinGain(int64 Amount)
{
    CoinFeedback.Add(Amount);
    DisplayedCoinGain = CoinFeedback.Amount;
    bCoinGainVisible = CoinFeedback.IsVisible();
}

FString UMiningInteractionHUDWidget::ResolveInteractionAction() const
{
    auto* Controller = GetOwningPlayer();
    if (!Controller || !Controller->GetPawn()) return FString();
    if(AMainGameDoor::FindInteractionTarget(Controller))return TEXT("开门");
    if (auto* Cards = Controller->FindComponentByClass<UCardRoomInteractionComponent>();
        Cards && Cards->bRoomActive && Cards->Stage && Cards->Stage->Run.Phase == ECardRoomPhase::Choosing)
    {
        const auto& Run = Cards->Stage->Run;
        if (Run.Round == 4 && !Run.CanConfirm()) return TEXT("全选");
        return Run.CanConfirm() ? TEXT("出牌") : FString();
    }
    if (auto* Room = AMiningStartRoom::Find(GetWorld()))
    {
        const FString Action = Room->GetInteractionAction(Controller);
        if (!Action.IsEmpty()) return Action;
    }
    for (TActorIterator<APopupCloseStage> Office(GetWorld()); Office; ++Office)
        if (Office->bAreaActive)
        {
            const FString Action = Office->GetInteractionAction(Controller);
            if (!Action.IsEmpty()) return Action;
        }
    for (TActorIterator<AWenSiPuzzle> Puzzle(GetWorld()); Puzzle; ++Puzzle)
        if (Puzzle->bAreaActive && !Puzzle->bEditing)
        {
            if (Puzzle->bAimReset) return TEXT("重置谜题");
            if (!Puzzle->bComplete && Puzzle->Objects.IsValidIndex(Puzzle->Aim))
            {
                auto* Object = Puzzle->Objects[Puzzle->Aim].Get();
                if (Puzzle->CanThinkAbout(Object) && Puzzle->Thoughts[0] != Object && Puzzle->Thoughts[1] != Object
                    && (!Puzzle->Thoughts[0].IsValid() || !Puzzle->Thoughts[1].IsValid()))
                    return TEXT("思考 · ") + Object->Noun();
            }
        }
    for (TActorIterator<AWenSiMirrorPuzzle> Mirror(GetWorld()); Mirror; ++Mirror)
        if(Mirror->bAreaActive && Mirror->MousePortal && Mirror->MousePortal->IsFocused())return TEXT("进入画中的圆环");
    for (TActorIterator<AMiningCollectionField> Collection(GetWorld()); Collection; ++Collection)
        if (Collection->IsAreaActive() && !Collection->bCatalogueOpen && IsValid(Collection->Target)
            && !Collection->Target->bCollected && Collection->Target->bReleased)
            return AMiningCollectible::ItemName(Collection->Target->ItemId);
    for (TActorIterator<AMiningTreasureField> Treasure(GetWorld()); Treasure; ++Treasure)
        if (Treasure->IsAreaActive() && IsValid(Treasure->Target) && !Treasure->Target->bCollected
            && AMiningTreasure::IsExposureEligible(Treasure->Target->Exposure))
            return Treasure->Target->TreasureName.ToString();
    if (auto* Reward = Controller->FindComponentByClass<UCardRoomInteractionComponent>();
        Reward && Reward->bRoomActive && IsValid(Reward->Stage) && Reward->Stage->Run.Phase == ECardRoomPhase::Victory)
        return TEXT("钻机");
    if (auto* Cards = Controller->FindComponentByClass<UCardRoomInteractionComponent>();
        Cards && Cards->bRoomActive && IsValid(Cards->Stage) && Cards->Stage->Run.Phase == ECardRoomPhase::Exploring
        && Cards->Stage->IsPlayerNear(Controller->GetPawn()->GetActorLocation()))
        return TEXT("入座");
    if (auto* Hall = AMiningExhibitHall::Find(GetWorld()); Hall && Hall->bReady && Hall->ActiveArea == INDEX_NONE && !Hall->IsInteractionOpen())
        for (int32 Index = 0; Index < Hall->Areas.Num(); ++Index)
        {
            const FVector Portal = Hall->LobbyEntry.GetLocation() + FVector(550 + (Index / 3) * 650, ((Index % 3) - 1) * 650, -90);
            if (FVector::Dist2D(Controller->GetPawn()->GetActorLocation(), Portal) < 280)
                return TEXT("进入 · ") + Hall->Areas[Index].Title.ToString();
        }
    return FString();
}

void UMiningInteractionHUDWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry, DeltaSeconds);
    CoinFeedback.Advance(DeltaSeconds);
    bCoinGainVisible = CoinFeedback.IsVisible();
    const auto* Controller = GetOwningPlayer();
    bExplorationVisible = Controller && Controller->GetPawn() && !Controller->IsPaused()
        && !Controller->bShowMouseCursor && !Controller->IsLookInputIgnored()
        && Controller->GetViewTarget() == Controller->GetPawn();
    const auto* Cards = Controller ? Controller->FindComponentByClass<UCardRoomInteractionComponent>() : nullptr;
    const bool bCardReward = Cards && Cards->bRoomActive && IsValid(Cards->Stage) && Cards->Stage->Run.Phase == ECardRoomPhase::Victory && !Controller->IsPaused();
    bExplorationVisible |= bCardReward;
    const bool bCardChoosing = Cards && Cards->bRoomActive && Cards->Stage && Cards->Stage->Run.Phase == ECardRoomPhase::Choosing && !Controller->IsPaused();
    bExplorationVisible |= bCardChoosing;
    InteractionAction = bExplorationVisible ? ResolveInteractionAction() : FString();
}

int32 UMiningInteractionHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
    const FSlateRect& CullingRect, FSlateWindowElementList& DrawElements, int32 LayerId,
    const FWidgetStyle& WidgetStyle, bool bParentEnabled) const
{
    int32 Layer = Super::NativePaint(Args, Geometry, CullingRect, DrawElements, LayerId, WidgetStyle, bParentEnabled);
    if (!bExplorationVisible && !CoinFeedback.IsVisible()) return Layer;
    const FVector2D Size = Geometry.GetLocalSize();
    const FVector2D Center = Size * .5f;
    const float Scale = FMath::Max(.5f, FMath::Min(Size.X / 1920.f, Size.Y / 1080.f));
    static const FSlateRoundedBoxBrush Circle(FLinearColor::White);
    static const FSlateRoundedBoxBrush Key(FLinearColor::White, 4.f);
    const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    auto Box = [&](FVector2D Position, FVector2D Extent, const FSlateBrush& Brush, FLinearColor Color)
    {
        FSlateDrawElement::MakeBox(DrawElements, ++Layer,
            Geometry.ToPaintGeometry(FVector2f(Extent * Scale), FSlateLayoutTransform(FVector2f(Center + Position * Scale))),
            &Brush, ESlateDrawEffect::None, Color * WidgetStyle.GetColorAndOpacityTint());
    };
    auto Text = [&](const FString& Label, FVector2D Position, int32 FontSize, FLinearColor Color, bool bCentered)
    {
        const auto Font = BrainMiningFonts::Style("Medium", FMath::Max(8, FMath::RoundToInt(FontSize * Scale)));
        const FVector2D TextSize = FontMeasure->Measure(Label, Font);
        FVector2D Offset = Center + Position * Scale;
        if (bCentered) Offset.X -= TextSize.X * .5f;
        FSlateDrawElement::MakeText(DrawElements, ++Layer,
            Geometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f(Offset))), Label, Font,
            ESlateDrawEffect::None, Color * WidgetStyle.GetColorAndOpacityTint());
    };
    bool bToolReticleVisible = false;
    if (const auto* MiningController = GetOwningPlayer<ABrainMiningPlayerController>();
        MiningController && MiningController->MiningGUI && MiningController->MiningGUI->IsVisible())
    {
        for (const FName Name : {FName(TEXT("ReticlePickaxe")), FName(TEXT("ReticleDrill")), FName(TEXT("ReticleBomb"))})
            if (const auto* Reticle = MiningController->MiningGUI->GetWidgetFromName(Name); Reticle && Reticle->IsVisible())
                bToolReticleVisible = true;
    }
    if (bExplorationVisible && !bToolReticleVisible)
    {
        Box(FVector2D(-3, -3), FVector2D(6, 6), Circle, FLinearColor(.015f, .02f, .025f, .85f));
        Box(FVector2D(-2, -2), FVector2D(4, 4), Circle, FLinearColor::White);
    }
    if (!InteractionAction.IsEmpty())
    {
        const auto* Cards = GetOwningPlayer()->FindComponentByClass<UCardRoomInteractionComponent>();
        const bool bPlayCard = Cards && Cards->bRoomActive && Cards->Stage && Cards->Stage->Run.Phase == ECardRoomPhase::Choosing;
        const bool bGamepad = UMiningGamepadSubsystem::UsesGamepad(GetOwningPlayer());
        const float HalfKey = bPlayCard && !bGamepad ? 35.f : 17.f;
        Text(InteractionAction, FVector2D(1, 39), 20, FLinearColor(0, 0, 0, .85f), true);
        Text(InteractionAction, FVector2D(0, 38), 20, FLinearColor::White, true);
        Box(FVector2D(-HalfKey, 73), FVector2D(HalfKey*2, 34), Key, FLinearColor(.015f, .02f, .025f, .85f));
        Box(FVector2D(-HalfKey+2, 74), FVector2D(HalfKey*2-4, 30), Key, FLinearColor(.96f, .95f, .89f));
        Text(bGamepad ? TEXT("X") : (bPlayCard ? TEXT("空格") : TEXT("E")), FVector2D(0, 77), 19, FLinearColor(.04f, .045f, .05f), true);
    }
    if (CoinFeedback.IsVisible())
    {
        const float Fade = FMath::Clamp((FMiningCoinFeedback::Duration - CoinFeedback.Age) / .35f, 0.f, 1.f);
        const float Pop = 1.f + .16f * FMath::Sin(FMath::Clamp(CoinFeedback.Age / .18f, 0.f, 1.f) * PI);
        const FString Amount = TEXT("+") + FText::AsNumber(CoinFeedback.Amount).ToString();
        const auto Font = BrainMiningFonts::Style("Medium", FMath::Max(8, FMath::RoundToInt(22 * Scale)));
        const float Width = 34.f + FontMeasure->Measure(Amount, Font).X / Scale;
        const float Left = -Width * .5f;
        const float Top = (InteractionAction.IsEmpty() ? 60.f : 142.f) - CoinFeedback.Age * 19.f;
        const FVector2D CoinCenter(Left + 12, Top + 14);
        auto CoinLayer = [&](float Radius, FLinearColor Color)
        {
            Color.A *= Fade;
            Box(CoinCenter - FVector2D(Radius, Radius) * Pop, FVector2D(Radius * 2, Radius * 2) * Pop, Circle, Color);
        };
        CoinLayer(13.f, FLinearColor(.1f, .055f, .01f, .9f));
        CoinLayer(11.f, FLinearColor(1.f, .68f, .12f));
        CoinLayer(8.f, FLinearColor(.63f, .30f, .025f));
        CoinLayer(6.5f, FLinearColor(1.f, .76f, .22f));
        Box(CoinCenter + FVector2D(-1.5f, -4.5f), FVector2D(3, 9), Circle, FLinearColor(1.f, .94f, .64f, Fade));
        Text(Amount, FVector2D(Left + 35, Top + 1), 22, FLinearColor(0, 0, 0, Fade * .85f), false);
        Text(Amount, FVector2D(Left + 34, Top), 22, FLinearColor(1.f, .83f, .36f, Fade), false);
    }
    return Layer;
}
