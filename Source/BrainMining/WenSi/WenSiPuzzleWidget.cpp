#include "WenSi/WenSiPuzzle.h"
#include "BrainMiningFonts.h"
#include "WenSi/WenSiThoughtBubble.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Engine/Texture2D.h"
#include "MiningInteractionHUDWidget.h"
#include "Input/DragAndDrop.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"

namespace
{
 FSlateFontInfo PuzzleThoughtFont(float Size)
 {
        return BrainMiningFonts::Body(Size);
    }
 class FPuzzleThoughtDrag : public FDragDropOperation
 {
 public:
  DRAG_DROP_OPERATOR_TYPE(FPuzzleThoughtDrag,FDragDropOperation)
  int32 ThoughtIndex=0;
  FString Word;
  TWeakObjectPtr<AWenSiObject> Source;
  TSharedPtr<SWidget> Decorator;
  void InitializeDecorator()
  {
   bCreateNewWindow=true;
   Decorator=SNew(SBorder).Padding(FMargin(8,4)).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(FLinearColor(.22f,.045f,.03f,.95f)).Visibility(EVisibility::HitTestInvisible)
    [SNew(STextBlock).Text(FText::FromString(Word)).Font(PuzzleThoughtFont(23)).ColorAndOpacity(FLinearColor(1.f,.22f,.18f))];
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
 class SPuzzleThoughtWord : public SBorder
 {
 public:
  SLATE_BEGIN_ARGS(SPuzzleThoughtWord) {}
   SLATE_ARGUMENT(int32,ThoughtIndex)
   SLATE_ARGUMENT(FString,Word)
   SLATE_ARGUMENT(TWeakObjectPtr<AWenSiPuzzle>,Puzzle)
  SLATE_END_ARGS()
  int32 Index=0;
  TWeakObjectPtr<AWenSiPuzzle> Puzzle;
  FString Word;
  void Construct(const FArguments& Args)
  {
   Index=Args._ThoughtIndex; Puzzle=Args._Puzzle; Word=Args._Word;
   SBorder::Construct(SBorder::FArguments().Padding(FMargin(5,2)).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(FLinearColor(.22,.045,.03,.9))
    [ SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Word)).Font(PuzzleThoughtFont(26)).ColorAndOpacity(FLinearColor(1,.28,.22))]
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
   auto Drag=MakeShared<FPuzzleThoughtDrag>(); Drag->ThoughtIndex=Index; Drag->Word=Word; Drag->Source=Puzzle->Thoughts[Index]; Drag->InitializeDecorator();
   Drag->OnDragged(FDragDropEvent(Event,Drag));
   return FReply::Handled().BeginDragDrop(Drag);
  }
  virtual FReply OnDrop(const FGeometry&,const FDragDropEvent& E) override
  {
   auto Drag=E.GetOperationAs<FPuzzleThoughtDrag>();
   UE_LOG(LogTemp,Verbose,TEXT("WenSi word drop: index=%d payload=%d"),Index,Drag.IsValid());
   if (Drag && Puzzle.IsValid() && Drag->ThoughtIndex!=Index && Drag->Source==Puzzle->Thoughts[Drag->ThoughtIndex])
   { Puzzle->SwapRules(Drag->ThoughtIndex,Index); return FReply::Handled(); }
   return FReply::Unhandled();
  }
 };
}

TSharedRef<SWidget> UWenSiPuzzleWidget::RebuildWidget()
{
 SetIsFocusable(true);
 ThoughtTexture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/WenSi/UI/T_ThoughtBubbleRight.T_ThoughtBubbleRight"));
 for(int32 I=0;I<2;++I)ThoughtBrushes[I]=WenSiThoughtBubble::MakeBrush(ThoughtTexture,I);
 auto MakeCard=[this](int32 Index)->TSharedRef<SWidget>
 {
  return WenSiThoughtBubble::MakeCard(Index,&ThoughtBrushes[Index],Rows[Index],FOnClicked::CreateLambda([this,Index](){if(Puzzle.IsValid())Puzzle->CloseThought(Index);return FReply::Handled();}));
 };
 auto Overlay=SNew(SOverlay)
  +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(24,0)[SAssignNew(Cards[0],SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBrush")))[MakeCard(0)]]
  +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(24,0)[SAssignNew(Cards[1],SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBrush")))[MakeCard(1)]]
  +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(TEXT("·"))).Font(PuzzleThoughtFont(24)).Visibility_Lambda([this](){return !UMiningInteractionHUDWidget::IsActiveFor(GetOwningPlayer()) && Puzzle.IsValid() && !Puzzle->bEditing && (!Puzzle->bComplete || Puzzle->ResetButton) ? EVisibility::HitTestInvisible:EVisibility::Collapsed;})]
  +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,40)[SAssignNew(Hint,STextBlock).Font(PuzzleThoughtFont(18)).ColorAndOpacity(FLinearColor::White).ShadowOffset(FVector2D(1,1))];
 Signature.Empty();
 return SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
  [SNew(SBox).WidthOverride(1600).HeightOverride(900)[Overlay]];
}
FReply UWenSiPuzzleWidget::NativeOnKeyDown(const FGeometry& G,const FKeyEvent& E)
{
 if (Puzzle.IsValid() && !E.IsRepeat())
 {
  if (E.GetKey()==EKeys::G || E.GetKey()==EKeys::Escape) {Puzzle->ToggleEdit();return FReply::Handled();}
  if (E.GetKey()==EKeys::Q) {Puzzle->CloseThought(0);return FReply::Handled();}
  if (E.GetKey()==EKeys::R) {Puzzle->CloseThought(1);return FReply::Handled();}
 }
 return Super::NativeOnKeyDown(G,E);
}
void UWenSiPuzzleWidget::NativeTick(const FGeometry& MyGeometry,float InDeltaTime)
{
 Super::NativeTick(MyGeometry,InDeltaTime);
 for(int32 I=0;I<2;++I)WenSiThoughtBubble::AdvanceFlight(Cards[I],MyGeometry,InDeltaTime,FlightAge[I]);
}
void UWenSiPuzzleWidget::Refresh()
{
 if (!Puzzle.IsValid() || !Hint.IsValid()) return;
 FString NewSignature=Puzzle->bEditing ? TEXT("edit") : TEXT("walk");
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
   Rows[I]->ClearChildren(); if (!Obj) continue;
   WenSiThoughtBubble::AddText(Rows[I],TEXT("这是一个"),PuzzleThoughtFont(26));
   Rows[I]->AddSlot().AutoWidth().VAlign(VAlign_Center)[SNew(SPuzzleThoughtWord).ThoughtIndex(I).Word(Obj->Adjective()).Puzzle(Puzzle)];
   WenSiThoughtBubble::AddText(Rows[I],TEXT("的")+Obj->Noun(),PuzzleThoughtFont(26));
  }
 }
 FString Message;
 const bool SharedHUD = UMiningInteractionHUDWidget::IsActiveFor(GetOwningPlayer());
 if(Puzzle->bAimReset && !Puzzle->bEditing) { if (!SharedHUD) Message=TEXT("E 重置谜题"); }
 else if (!Puzzle->bComplete)
 {
  if (Puzzle->bEditing) Message=TEXT("拖动红词交换    G 返回    Q 放下思考");
  else
  {
   if (Puzzle->Aim!=INDEX_NONE && !SharedHUD) Message=TEXT("E 思考    ");
   if(Puzzle->Thoughts[0].IsValid() || Puzzle->Thoughts[1].IsValid()) Message+=TEXT("G 重组    Q 放下左侧    R 放下右侧");
  }
 }
 Hint->SetText(FText::FromString(Message));
}
