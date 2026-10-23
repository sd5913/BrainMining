#include "MiningGUIWidgets.h"
#include "MiningCollection.h"
#include "BrainMiningFonts.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"

void UMiningGUIHUDWidget::QueueCollectionPickup(AMiningCollectionField* Field,int32 Id)
{
    if(IsValid(Field) && Id>=0 && Id<AMiningCollectible::ItemTypeCount)PickupQueue.Emplace(Field,Id);
}

void UMiningGUIHUDWidget::TickCollectionPickup(const FGeometry& Geometry,float Dt)
{
    if(!CollectionShortcut)return;
    // Pausing/catalogue inspection suspends the flight; a queued reward is
    // already safely in the inventory, so closing a screen can never lose it.
    if(!GetOwningPlayer() || GetOwningPlayer()->IsPaused())return;
    CollectionArrivalAge+=Dt;
    const float Pulse=CollectionArrivalAge<.65f?FMath::Sin(CollectionArrivalAge/.65f*PI):0.f;
    CollectionShortcut->SetRenderScale(FVector2D(1+.16f*Pulse));
    CollectionShortcut->SetColorAndOpacity(FMath::Lerp(FLinearColor(.94f,.95f,.92f),FLinearColor(1,.77f,.25f),Pulse));
    if(ActivePickupId==INDEX_NONE)
    {
        while(!PickupQueue.IsEmpty() && !PickupQueue[0].Key.IsValid())PickupQueue.RemoveAt(0);
        if(PickupQueue.IsEmpty())return;
        PickupField=PickupQueue[0].Key;ActivePickupId=PickupQueue[0].Value;PickupQueue.RemoveAt(0);
        PickupAge=0;PickupCaptureAge=0;PickupField->BeginPickupPreview(ActivePickupId);
        if(!PickupImage)
        {
            auto* Root=Cast<UCanvasPanel>(WidgetTree->RootWidget);if(!Root){ActivePickupId=INDEX_NONE;return;}
            PickupImage=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("CollectionPickupFlight"));
            auto* FlightSlot=Root->AddChildToCanvas(PickupImage);FlightSlot->SetZOrder(90);FlightSlot->SetAlignment(FVector2D(.5));
            PickupLabel=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("CollectionPickupName"));
            PickupLabel->SetFont(BrainMiningFonts::Body(23));PickupLabel->SetJustification(ETextJustify::Center);
            PickupLabel->SetColorAndOpacity(FLinearColor(1,.88f,.54f));PickupLabel->SetShadowOffset(FVector2D(1,2));
            PickupLabel->SetShadowColorAndOpacity(FLinearColor(0,0,0,.85));
            auto* LabelSlot=Root->AddChildToCanvas(PickupLabel);LabelSlot->SetZOrder(91);LabelSlot->SetAlignment(FVector2D(.5,0));LabelSlot->SetSize(FVector2D(540,40));
        }
        PickupImage->SetBrushFromMaterial(PickupField->PickupMaterial);
        PickupImage->SetVisibility(ESlateVisibility::HitTestInvisible);PickupLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
        PickupLabel->SetText(FText::FromString((AMiningCollectible::IsMineralReward(ActivePickupId)?TEXT("灵感涌现 · "):TEXT("已收集 · "))+AMiningCollectible::ItemName(ActivePickupId)));
    }
    if(!PickupField.IsValid())
    {ActivePickupId=INDEX_NONE;PickupImage->SetVisibility(ESlateVisibility::Collapsed);PickupLabel->SetVisibility(ESlateVisibility::Collapsed);return;}
    PickupAge+=Dt;PickupCaptureAge+=Dt;
    if(PickupAge<1.15f && PickupCaptureAge>=1.f/24.f)
    {PickupField->UpdatePickupPreview(PickupAge);PickupCaptureAge=0;}
    const FVector2D Start=Geometry.GetLocalSize()*FVector2D(.5,.43);
    const auto& Destination=CollectionShortcut->GetCachedGeometry();
    const FVector2D End=Geometry.AbsoluteToLocal(Destination.LocalToAbsolute(FVector2D(12,14)));
    const float Flight=FMath::Clamp((PickupAge-1.15f)/.8f,0.f,1.f);
    const float T=Flight*Flight*(3-2*Flight),S=1-T;
    const FVector2D Control=(Start+End)*.5+FVector2D(-55,-130);
    const FVector2D Position=S*S*Start+2*S*T*Control+T*T*End;
    const float Reveal=FMath::Clamp(PickupAge/.22f,0.f,1.f);
    const float Scale=(.78f+.22f*FMath::Sin(Reveal*PI*.5f))*FMath::Lerp(1.f,.07f,T);
    auto* FlightSlot=CastChecked<UCanvasPanelSlot>(PickupImage->Slot);FlightSlot->SetPosition(Position);FlightSlot->SetSize(FVector2D(320,240)*Scale);
    PickupImage->SetRenderOpacity(Reveal*FMath::Clamp((1-Flight)/.15f,0.f,1.f));
    PickupImage->SetRenderTransformAngle(Flight*Flight*-16.f);
    CastChecked<UCanvasPanelSlot>(PickupLabel->Slot)->SetPosition(Start+FVector2D(0,125));
    PickupLabel->SetRenderOpacity(Reveal*(1-FMath::Clamp(Flight*4,0.f,1.f)));
    if(Flight>=1)
    {
        PickupField->EndPickupPreview();ActivePickupId=INDEX_NONE;CollectionArrivalAge=0;++CompletedPickupFlights;
        PickupImage->SetVisibility(ESlateVisibility::Collapsed);PickupLabel->SetVisibility(ESlateVisibility::Collapsed);
    }
}
