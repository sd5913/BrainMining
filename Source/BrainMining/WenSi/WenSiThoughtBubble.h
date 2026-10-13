#pragma once

#include "CoreMinimal.h"
#include "Layout/ArrangedChildren.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

namespace WenSiThoughtBubble
{
 // The supplied PNG has transparent margins. Sample its original alpha bounds;
 // mirror only the brush so Chinese text and input remain left-to-right.
 inline FSlateBrush MakeBrush(UObject* Texture, int32 Side)
 {
  FSlateBrush Brush;
  Brush.SetResourceObject(Texture);
  Brush.DrawAs=ESlateBrushDrawType::Image;
  Brush.ImageSize=FVector2D(440,328.19);
  Brush.SetUVRegion(FBox2f(FVector2f(1081.f/4800.f,617.f/3584.f),FVector2f(4119.f/4800.f,2883.f/3584.f)));
  Brush.Mirroring=Side==0 ? ESlateBrushMirrorType::Horizontal : ESlateBrushMirrorType::NoMirror;
  return Brush;
 }

 // Homography fitted to the straight interior edges of the supplied bubble.
 // Each character uses its local Jacobian; Slate receives the same transformed
 // geometry for painting and hit testing (including the draggable red word).
 class SPerspectiveRow : public SHorizontalBox
 {
 public:
  int32 Side=1;
  FVector2D Project(double X,double Y) const
  {
   const double U=Side==0 ? 1.-X/360. : X/360.;
   const double V=Y/240.;
   constexpr double G=156./224.-1.;
   const double D=1.+G*U;
   const double PX=( (400.*(1.+G)-28.)*U+28.)/D;
   const double PY=( (18.*(1.+G)-46.)*U+156.*V+46.)/D;
   return FVector2D(Side==0 ? 440.-PX : PX,PY);
  }
  virtual void OnArrangeChildren(const FGeometry& Geometry,FArrangedChildren& Arranged) const override
  {
   FArrangedChildren Flat(EVisibility::Visible);
   SHorizontalBox::OnArrangeChildren(Geometry.MakeChild(GetDesiredSize(),FSlateLayoutTransform()),Flat);
   const double Fit=FMath::Min(1.,330./FMath::Max(1.,GetDesiredSize().X));
   const double Start=(360.-GetDesiredSize().X*Fit)*.5;
   for (int32 I=0;I<Flat.Num();++I)
   {
    const auto& Child=Flat[I];
    const FVector2D Size=Child.Geometry.GetLocalSize();
    const FVector2D Offset=Geometry.AbsoluteToLocal(Child.Geometry.LocalToAbsolute(FVector2D::ZeroVector));
    const double X=Start+Offset.X*Fit,Y=120.+(Offset.Y-GetDesiredSize().Y*.5)*Fit;
    const FVector2D P=Project(X,Y);
    const FVector2D DX=(Project(X+.01,Y)-P)*(Fit/.01);
    const FVector2D DY=(Project(X,Y+.01)-P)*(Fit/.01);
    const FSlateRenderTransform Transform(FMatrix2x2(DX.X,DX.Y,DY.X,DY.Y));
    Arranged.AddWidget(FArrangedWidget(Child.Widget,Geometry.MakeChild(Size,FSlateLayoutTransform(P),Transform,FVector2D::ZeroVector)));
   }
  }
 };

 inline void AddText(const TSharedPtr<SHorizontalBox>& Row,const FString& Text,const FSlateFontInfo& Font)
 {
  for (TCHAR Character:Text)
   Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
    [SNew(STextBlock).Text(FText::FromString(FString::Chr(Character))).Font(Font).ColorAndOpacity(FLinearColor(.035,.035,.04))];
 }

 inline TSharedRef<SWidget> MakeCard(int32 Side,const FSlateBrush* Brush,TSharedPtr<SHorizontalBox>& Row,FOnClicked Close)
 {
  TSharedRef<SPerspectiveRow> Perspective=SNew(SPerspectiveRow);
  Perspective->Side=Side;Row=Perspective;
  return SNew(SBox).WidthOverride(440).HeightOverride(328.19)
   [SNew(SOverlay)
    +SOverlay::Slot()[SNew(SImage).Image(Brush).Visibility(EVisibility::HitTestInvisible)]
    +SOverlay::Slot()[Perspective]
    +SOverlay::Slot().HAlign(Side==0 ? HAlign_Left:HAlign_Right).VAlign(VAlign_Top).Padding(Side==0 ? FMargin(37,33,0,0):FMargin(0,33,37,0))
     [SNew(SButton).IsFocusable(false).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).ContentPadding(6).OnClicked(Close)
      [SNew(STextBlock).Text(FText::FromString(TEXT("×"))).Font(FCoreStyle::GetDefaultFontStyle("Bold",20)).ColorAndOpacity(FLinearColor(.08,.08,.09))]]
   ];
 }
}
