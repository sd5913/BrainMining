#include "WenSi/WenSiMirrorPuzzle.h"
#include "MiningLocalizationSubsystem.h"
#include "BrainMiningFonts.h"
#include "Internationalization/TextLocalizationManager.h"
#include "WenSi/MouseMuseumPortal.h"
#include "WenSi/WenSiThoughtBubble.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Engine/Texture2D.h"
#include "Input/DragAndDrop.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
 FSlateFontInfo ThoughtFont(float Size)
 {
        return BrainMiningFonts::Body(Size);
    }
 class SThoughtExchange : public SLeafWidget
 {
 public:
  SLATE_BEGIN_ARGS(SThoughtExchange) {}
   SLATE_ARGUMENT(TWeakObjectPtr<AWenSiMirrorPuzzle>,Puzzle)
   SLATE_ARGUMENT(TFunction<FGeometry(int32)>,WordGeometry)
  SLATE_END_ARGS()
  void Construct(const FArguments& Args)
  {
   Puzzle=Args._Puzzle;WordGeometry=Args._WordGeometry;SetVisibility(EVisibility::HitTestInvisible);
  }
  virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
  virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Cull,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool Enabled) const override
  {
   if(!Puzzle.IsValid() || !Puzzle->IsSwapping() || Puzzle->HasSwapCommitted())return Layer;
   FVector2D Centers[2];
   for(int32 Index=0;Index<2;++Index)
   {
    const FGeometry Word=WordGeometry(Index);
    if(Word.GetLocalSize().X<1.f)return Layer;
    Centers[Index]=Geometry.AbsoluteToLocal(Word.LocalToAbsolute(Word.GetLocalSize()*.5));
   }
   const float Progress=FMath::SmoothStep(0.f,1.f,Puzzle->GetSwapProgress());
   const float Lift=FMath::Sin(PI*Progress);
   const auto Font=ThoughtFont(26);
   const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
   for(int32 Index=0;Index<2;++Index)
   {
    const FString& Word=Puzzle->GetSwapWord(Index);
    const FVector2D Size=Measure->Measure(Word,Font)+FVector2D(16,8);
    const FVector2D Center=FMath::Lerp(Centers[Index],Centers[1-Index],Progress)+FVector2D(0,(Index==0?-75.f:75.f)*Lift);
    const float Scale=1.f+.18f*Lift;
    const FVector2D Position=Center-Size*Scale*.5;
    const auto Transform=FSlateLayoutTransform(Scale,FVector2f(Position));
    FSlateDrawElement::MakeBox(Elements,++Layer,Geometry.ToPaintGeometry(FVector2f(Size),Transform),FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),ESlateDrawEffect::None,FLinearColor(.22f,.045f,.03f,.95f));
    FSlateDrawElement::MakeText(Elements,++Layer,Geometry.ToPaintGeometry(FVector2f(Size),FSlateLayoutTransform(Scale,FVector2f(Position+FVector2D(8,4)*Scale))),Word,Font,ESlateDrawEffect::None,FLinearColor(1.f,.28f,.22f));
   }
   return Layer;
  }
 private:
  TWeakObjectPtr<AWenSiMirrorPuzzle> Puzzle;
  TFunction<FGeometry(int32)> WordGeometry;
 };
 class FThoughtDrag : public FDragDropOperation
 {
 public:
  DRAG_DROP_OPERATOR_TYPE(FThoughtDrag,FDragDropOperation)
  int32 ThoughtIndex=0;
  FString Word;
  TWeakObjectPtr<AWenSiMirrorExhibit> Source;
  TSharedPtr<SWidget> Decorator;
  void InitializeDecorator()
  {
   bCreateNewWindow=true;
   Decorator=SNew(SBorder).Padding(FMargin(8,4)).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(FLinearColor(.22f,.045f,.03f,.95f)).Visibility(EVisibility::HitTestInvisible)
    [SNew(STextBlock).Text(FText::FromString(Word)).Font(ThoughtFont(23)).ColorAndOpacity(FLinearColor(1.f,.22f,.18f))];
   Construct();
  }
  virtual void OnDragged(const FDragDropEvent& Event) override
  {
   FDragDropOperation::OnDragged(Event);
   if (CursorDecoratorWindow.IsValid())
    UE_LOG(LogTemp,Verbose,TEXT("WenSi drag visual: visible=%d cursor=%s window=%s"),CursorDecoratorWindow->IsVisible(),*Event.GetScreenSpacePosition().ToString(),*CursorDecoratorWindow->GetPositionInScreen().ToString());
  }
  virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
  { return Decorator; }
 };
 class SThoughtWord : public SBorder
 {
 public:
  SLATE_BEGIN_ARGS(SThoughtWord) {}
   SLATE_ARGUMENT(int32,ThoughtIndex)
   SLATE_ARGUMENT(FString,Word)
   SLATE_ARGUMENT(TWeakObjectPtr<AWenSiMirrorPuzzle>,Puzzle)
  SLATE_END_ARGS()
  int32 Index=0;
  TWeakObjectPtr<AWenSiMirrorPuzzle> Puzzle;
  FString Word;
  void Construct(const FArguments& Args)
  {
   Index=Args._ThoughtIndex; Puzzle=Args._Puzzle; Word=Args._Word;
   SBorder::Construct(SBorder::FArguments().Padding(FMargin(5,2)).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(FLinearColor(.22,.045,.03,.9))
    [ SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Word)).Font(ThoughtFont(26)).ColorAndOpacity(FLinearColor(1,.28,.22))]
      +SVerticalBox::Slot().AutoHeight()[SNew(SBorder).Padding(FMargin(0,1)).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(FLinearColor(1,.28,.22))]
    ]);
   SetVisibility(EVisibility::Visible);
  }
  virtual FReply OnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& E) override
  {
   UE_LOG(LogTemp,Verbose,TEXT("WenSi word mouse-down: index=%d editing=%d"),Index,Puzzle.IsValid() && Puzzle->bEditing);
   if (Puzzle.IsValid() && Puzzle->bEditing && E.GetEffectingButton()==EKeys::LeftMouseButton)
    return OnDragDetected(Geometry,E);
   return FReply::Unhandled();
  }
  virtual FReply OnDragDetected(const FGeometry&,const FPointerEvent& Event) override
  {
   if (!Puzzle.IsValid() || !Puzzle->bEditing || !Puzzle->Thoughts[Index].IsValid()) return FReply::Unhandled();
   auto Drag=MakeShared<FThoughtDrag>(); Drag->ThoughtIndex=Index; Drag->Word=Word; Drag->Source=Puzzle->Thoughts[Index]; Drag->InitializeDecorator();
   Drag->OnDragged(FDragDropEvent(Event,Drag));
   return FReply::Handled().BeginDragDrop(Drag);
  }
  virtual FReply OnDrop(const FGeometry&,const FDragDropEvent& E) override
  {
   auto Drag=E.GetOperationAs<FThoughtDrag>();
   UE_LOG(LogTemp,Verbose,TEXT("WenSi word drop: index=%d payload=%d"),Index,Drag.IsValid());
   if (Drag && Puzzle.IsValid() && Drag->ThoughtIndex!=Index && Drag->Source==Puzzle->Thoughts[Drag->ThoughtIndex])
   { Puzzle->SwapWords(Drag->ThoughtIndex,Index); return FReply::Handled(); }
   return FReply::Unhandled();
  }
 };
}

TSharedRef<SWidget> UWenSiMirrorWidget::RebuildWidget()
{
 SetIsFocusable(true);
 ThoughtTexture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/WenSi/UI/T_ThoughtBubbleRight.T_ThoughtBubbleRight"));
 for(int32 I=0;I<2;++I)ThoughtBrushes[I]=WenSiThoughtBubble::MakeBrush(ThoughtTexture,I);
 auto MakeCard=[this](int32 Index)->TSharedRef<SWidget>
 {
  return WenSiThoughtBubble::MakeCard(Index,&ThoughtBrushes[Index],Rows[Index],FOnClicked::CreateLambda([this,Index](){if(Puzzle.IsValid())Puzzle->CloseThought(Index);return FReply::Handled();}));
 };
 auto Overlay=SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
  +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(24,0)[SAssignNew(Cards[0],SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBrush")))[MakeCard(0)]]
  +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(24,0)[SAssignNew(Cards[1],SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBrush")))[MakeCard(1)]]
  +SOverlay::Slot()[SNew(SThoughtExchange).Puzzle(Puzzle).WordGeometry([this](int32 Index){return WordWidgets[Index].IsValid()?WordWidgets[Index]->GetCachedGeometry():FGeometry();})]
  +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(TEXT("·"))).Font(ThoughtFont(24)).Visibility_Lambda([this](){return Puzzle.IsValid() && !Puzzle->bEditing && (!Puzzle->bComplete || true) ? EVisibility::HitTestInvisible:EVisibility::Collapsed;})]
  +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,40)[SAssignNew(Hint,STextBlock).Visibility(EVisibility::HitTestInvisible).Font(ThoughtFont(18)).ColorAndOpacity(FLinearColor::White).ShadowOffset(FVector2D(1,1))];
 Signature.Empty();
 return SNew(SScaleBox).Visibility(EVisibility::SelfHitTestInvisible).Stretch(EStretch::ScaleToFit)
  [SNew(SBox).Visibility(EVisibility::SelfHitTestInvisible).WidthOverride(1600).HeightOverride(900)[Overlay]];
}
FReply UWenSiMirrorWidget::NativeOnKeyDown(const FGeometry& G,const FKeyEvent& E)
{
 if (Puzzle.IsValid() && !E.IsRepeat())
 {
  if (E.GetKey()==EKeys::R) {Puzzle->SwapThoughts();return FReply::Handled();}
  if (E.GetKey()==EKeys::Q) {Puzzle->CloseThoughts();return FReply::Handled();}
  if (E.GetKey()==EKeys::Escape && Puzzle->bEditing) {Puzzle->ToggleEdit();return FReply::Handled();}
 }
 return Super::NativeOnKeyDown(G,E);
}
void UWenSiMirrorWidget::NativeTick(const FGeometry& MyGeometry,float InDeltaTime)
{
 Super::NativeTick(MyGeometry,InDeltaTime);
 if(!Puzzle.IsValid() || !Puzzle->IsSwapping())
  for(int32 I=0;I<2;++I)WenSiThoughtBubble::AdvanceFlight(Cards[I],MyGeometry,InDeltaTime,FlightAge[I]);
}
void UWenSiMirrorWidget::Refresh()
{
 if (!Puzzle.IsValid() || !Hint.IsValid()) return;
 FString NewSignature=FString::FromInt(FTextLocalizationManager::Get().GetTextRevision())+(Puzzle->bEditing ? TEXT("edit") : TEXT("walk"));
 NewSignature+=Puzzle->IsSwapping()?(Puzzle->HasSwapCommitted()?TEXT("landed"):TEXT("swapping")):TEXT("idle");
 for (int32 I=0;I<2;++I)
 {
  auto Obj=Puzzle->Thoughts[I].Get();
  NewSignature+=Obj ? Obj->Noun()+Obj->Adjective() : TEXT("empty");
 }
 if (NewSignature!=Signature)
 {
  Signature=NewSignature;
  for (int32 I=0;I<2;++I)
  {
   auto Obj=Puzzle->Thoughts[I].Get();
   if(PresentedThoughts[I].Get()!=Obj)
   {
    PresentedThoughts[I]=Obj;FlightAge[I]=Obj ? 0.f:-1.f;
    Cards[I]->SetRenderOpacity(Obj ? 0.f:1.f);
    Cards[I]->SetRenderTransform(FSlateRenderTransform());
   }
   Cards[I]->SetVisibility(Obj ? (Puzzle->bEditing ? EVisibility::Visible:EVisibility::HitTestInvisible) : EVisibility::Collapsed);
   Rows[I]->ClearChildren();WordWidgets[I].Reset(); if (!Obj) continue;
   WenSiThoughtBubble::AddStatement(Rows[I],Obj->Noun(),
    SAssignNew(WordWidgets[I],SThoughtWord).ThoughtIndex(I).Word(Obj->Adjective()).Puzzle(Puzzle),ThoughtFont(26));
  }
 }
 for(int32 Index=0;Index<2;++Index)
 {
  if(Puzzle->IsSwapping())
  {
   FlightAge[Index]=-1.f;Cards[Index]->SetRenderTransform(FSlateRenderTransform());
   Cards[Index]->SetRenderOpacity(Puzzle->GetSwapOpacity());
  }
  else if(FlightAge[Index]<0.f)Cards[Index]->SetRenderOpacity(1.f);
  if(WordWidgets[Index].IsValid())WordWidgets[Index]->SetRenderOpacity(Puzzle->IsSwapping() && !Puzzle->HasSwapCommitted()?0.f:1.f);
 }
 FString Message;
 if(Puzzle->MousePortal && Puzzle->MousePortal->IsEntering()) Message=TEXT("");
 else if(Puzzle->MousePortal && Puzzle->MousePortal->IsFocused()) Message=FString();
 else if(Puzzle->bAimReset && !Puzzle->bEditing) Message=MiningText::Get(TEXT("WenSi.Hint.Reset")).ToString();
 else if (!Puzzle->bComplete)
 {
  Message=MiningText::Get(TEXT("WenSi.Hint.Swap")).ToString();
 }
 Hint->SetText(FText::FromString(Message));
}
