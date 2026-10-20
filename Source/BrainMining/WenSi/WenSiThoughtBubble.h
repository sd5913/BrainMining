#pragma once

#include "CoreMinimal.h"
#include "Layout/ArrangedChildren.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Text/STextBlock.h"

namespace WenSiThoughtBubble
{
 // Animate the complete card, so its flat text and Slate hit geometry
 // travel with the artwork. The origin is relative to the actual game viewport,
 // including PIE window placement and DPI, rather than the desktop resolution.
 inline void AdvanceFlight(const TSharedPtr<SWidget>& Card,const FGeometry& Viewport,float Dt,float& Age)
 {
  if (!Card.IsValid() || Age<0.f || !Card->GetVisibility().IsVisible()) return;
  const FGeometry& CardGeometry=Card->GetCachedGeometry();
  if (CardGeometry.GetLocalSize().X<1.f) return;
  Age=FMath::Min(Age+Dt,.85f);
  const float T=Age/.85f;
  // First recede away from the camera, then move sideways into the HUD.
  const float Recede=FMath::SmoothStep(0.f,.65f,T);
  const float Travel=FMath::SmoothStep(.20f,1.f,T);
  const auto Layout=CardGeometry.GetAccumulatedLayoutTransform();
  const FVector2D Origin=Viewport.LocalToAbsolute(Viewport.GetLocalSize()*FVector2D(.5,.56));
  const FVector2D Delta=(Origin-Layout.GetTranslation())/Layout.GetScale()-CardGeometry.GetLocalSize()*.5;
  const FVector2D Offset=Delta*(1.f-Travel)+FVector2D(0,-42.f*FMath::Sin(PI*Travel));
  const float Scale=600.f/FMath::Lerp(360.f,600.f,Recede);
  Card->SetRenderTransformPivot(FVector2D(.5,.5));
  Card->SetRenderTransform(FSlateRenderTransform(FScale2D(Scale),Offset));
  Card->SetRenderOpacity(FMath::SmoothStep(0.f,.22f,T));
  if(T>=1.f)
  {
   Card->SetRenderTransform(FSlateRenderTransform());Card->SetRenderOpacity(1.f);Age=-1.f;
  }
 }
 // The supplied PNG has transparent margins. Sample its alpha bounds and keep
 // the artwork at its natural cloud-like aspect ratio. The source artwork is
 // the left-hand bubble (tail on the lower-left); mirror only the right brush
 // so Chinese text and input remain left-to-right.
 inline FSlateBrush MakeBrush(UObject* Texture, int32 Side)
 {
  FSlateBrush Brush;
  Brush.SetResourceObject(Texture);
  Brush.DrawAs=ESlateBrushDrawType::Image;
  Brush.ImageSize=FVector2D(440,369.75);
  Brush.SetUVRegion(FBox2f(FVector2f(500.f/4800.f,44.f/3584.f),FVector2f(4654.f/4800.f,3536.f/3584.f)));
  Brush.Mirroring=Side==1 ? ESlateBrushMirrorType::Horizontal : ESlateBrushMirrorType::NoMirror;
  return Brush;
 }

 inline void AddText(const TSharedPtr<SHorizontalBox>& Row,const FString& Text,const FSlateFontInfo& Font)
 {
  for (TCHAR Character:Text)
   Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
    [SNew(STextBlock).Text(FText::FromString(FString::Chr(Character))).Font(Font).ColorAndOpacity(FLinearColor(.035,.035,.04))];
 }

 inline TSharedRef<SWidget> MakeCard(int32 Side,const FSlateBrush* Brush,TSharedPtr<SHorizontalBox>& Row,FOnClicked Close)
 {
  Row=SNew(SHorizontalBox);
  return SNew(SBox).WidthOverride(440).HeightOverride(369.75)
   [SNew(SOverlay)
    +SOverlay::Slot()[SNew(SImage).Image(Brush).Visibility(EVisibility::HitTestInvisible)]
    +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(42,115,42,0))
      [SNew(SBox).WidthOverride(356).HeightOverride(70)
       [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)[Row.ToSharedRef()]]]

   ];
 }
}
