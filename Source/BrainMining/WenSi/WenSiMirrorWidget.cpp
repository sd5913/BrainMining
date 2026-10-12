#include "WenSi/WenSiMirrorPuzzle.h"
#include "Fonts/CompositeFont.h"
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
 FSlateFontInfo ThoughtFont(float Size)
 {
  static TSharedPtr<const FCompositeFont> Font;
  if (!Font)
  {
   FString Path=TEXT("C:/Windows/Fonts/simhei.ttf");
   if (FPaths::FileExists(Path)) Font=MakeShared<FStandaloneCompositeFont>(NAME_None,Path,EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
  }
  return Font ? FSlateFontInfo(Font,Size) : FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),Size);
 }
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
      +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Word)).Font(ThoughtFont(22)).ColorAndOpacity(FLinearColor(1,.28,.22))]
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
 auto MakeCard=[this](int32 Index)->TSharedRef<SWidget>
 {
  return SNew(SBorder).Padding(16).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(FLinearColor(.025,.035,.045,.85))
  [SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
   [SNew(SButton).IsFocusable(false).Text(FText::FromString(TEXT("×"))).OnClicked_Lambda([this,Index](){if(Puzzle.IsValid())Puzzle->CloseThought(Index);return FReply::Handled();})]
   +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(Rows[Index],SHorizontalBox)]
  ];
 };
 auto Overlay=SNew(SOverlay)
  +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(24,0)[SAssignNew(Cards[0],SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBrush")))[MakeCard(0)]]
  +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(24,0)[SAssignNew(Cards[1],SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBrush")))[MakeCard(1)]]
  +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(TEXT("·"))).Font(ThoughtFont(24)).Visibility_Lambda([this](){return Puzzle.IsValid() && !Puzzle->bEditing && (!Puzzle->bComplete || true) ? EVisibility::HitTestInvisible:EVisibility::Collapsed;})]
  +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,40)[SAssignNew(Hint,STextBlock).Font(ThoughtFont(18)).ColorAndOpacity(FLinearColor::White).ShadowOffset(FVector2D(1,1))];
 Signature.Empty();
 return Overlay;
}
FReply UWenSiMirrorWidget::NativeOnKeyDown(const FGeometry& G,const FKeyEvent& E)
{
 if (Puzzle.IsValid() && !E.IsRepeat())
 {
  if (E.GetKey()==EKeys::G || E.GetKey()==EKeys::Escape) {Puzzle->ToggleEdit();return FReply::Handled();}
  if (E.GetKey()==EKeys::Q) {Puzzle->CloseThought(0);return FReply::Handled();}
  if (E.GetKey()==EKeys::R) {Puzzle->CloseThought(1);return FReply::Handled();}
 }
 return Super::NativeOnKeyDown(G,E);
}
void UWenSiMirrorWidget::Refresh()
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
   Cards[I]->SetVisibility(Obj ? (Puzzle->bEditing ? EVisibility::Visible:EVisibility::HitTestInvisible) : EVisibility::Collapsed);
   Rows[I]->ClearChildren(); if (!Obj) continue;
   Rows[I]->AddSlot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(TEXT("这是一个"))).Font(ThoughtFont(22))];
   Rows[I]->AddSlot().AutoWidth().VAlign(VAlign_Center)[SNew(SThoughtWord).ThoughtIndex(I).Word(Obj->Adjective()).Puzzle(Puzzle)];
   Rows[I]->AddSlot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(TEXT("的")+Obj->Noun()+TEXT("。"))).Font(ThoughtFont(22))];
  }
 }
 FString Message;
 if(Puzzle->bAimReset && !Puzzle->bEditing) Message=TEXT("E 重置谜题");
 else if (!Puzzle->bComplete)
 {
  if (Puzzle->bEditing) Message=TEXT("拖动红词交换    G 返回    × 放下思考");
  else
  {
   if (Puzzle->Aim.IsValid()) Message=TEXT("E 思考    ");
   if(Puzzle->Thoughts[0].IsValid() || Puzzle->Thoughts[1].IsValid()) Message+=TEXT("G 重组    Q 放下左侧    R 放下右侧");
  }
 }
 Hint->SetText(FText::FromString(Message));
}
