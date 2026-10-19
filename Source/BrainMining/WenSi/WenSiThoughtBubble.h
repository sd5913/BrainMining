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
 // Animate the complete card, so its perspective text and Slate hit geometry
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
  const float Side=Delta.X>0.f ? -1.f:1.f;
  const float Yaw=FMath::DegreesToRadians(Side*36.f*FMath::Sin(PI*Travel));
  const float Pitch=FMath::DegreesToRadians(22.f*(1.f-Travel));
  const float Roll=FMath::DegreesToRadians(Side*7.f*FMath::Sin(PI*Travel));
  const FVector2D DX(FMath::Cos(Yaw)*FMath::Cos(Roll)*Scale,FMath::Cos(Yaw)*FMath::Sin(Roll)*Scale);
  const FVector2D DY((FMath::Sin(Yaw)*FMath::Sin(Pitch)*FMath::Cos(Roll)-FMath::Cos(Pitch)*FMath::Sin(Roll))*Scale,(FMath::Sin(Yaw)*FMath::Sin(Pitch)*FMath::Sin(Roll)+FMath::Cos(Pitch)*FMath::Cos(Roll))*Scale);
  Card->SetRenderTransformPivot(FVector2D(.5,.5));
  Card->SetRenderTransform(FSlateRenderTransform(FMatrix2x2(DX.X,DX.Y,DY.X,DY.Y),Offset));
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

 // Homography fitted to the straight interior edges of the supplied bubble.
 // Each character uses its local Jacobian; Slate receives the same transformed
 // geometry for painting and hit testing (including the draggable red word).
 class SPerspectiveRow : public SHorizontalBox
 {
 public:
  int32 Side=1;
  FVector2D Project(double X,double Y) const
  {
   const double U=Side==1 ? 1.-X/360. : X/360.;
   const double V=Y/240.;
   constexpr double G=156./224.-1.;
   const double D=1.+G*U;
   const double PX=( (400.*(1.+G)-28.)*U+28.)/D;
   const double PY=( (18.*(1.+G)-46.)*U+156.*V+46.)/D;
   return FVector2D(Side==1 ? 440.-PX : PX,PY);
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
    const double X=Start+Offset.X*Fit,Y=150.+(Offset.Y-GetDesiredSize().Y*.5)*Fit;
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
  return SNew(SBox).WidthOverride(440).HeightOverride(369.75)
   [SNew(SOverlay)
    +SOverlay::Slot()[SNew(SImage).Image(Brush).Visibility(EVisibility::HitTestInvisible)]
    +SOverlay::Slot()[Perspective]

   ];
 }
}
