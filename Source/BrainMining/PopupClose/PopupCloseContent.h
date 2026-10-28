#pragma once

#include "CoreMinimal.h"
#include "MiningLocalizationSubsystem.h"

namespace PopupCloseContent
{
 struct FWorkPopup
 {
  FText Title;
  FText Body;
  const TCHAR* PhysicalTitle;
  FLinearColor Color;
  FVector2D Size;
  int32 Art=1;
 };

 inline const FWorkPopup& Get(int32 Index)
 {
  static const FWorkPopup Introduction[] = {
   {MiningText::Get(TEXT("Office.Popup.00.Title")),MiningText::Get(TEXT("Office.Popup.00.Body")),TEXT("GAME JAM STARTS"),FLinearColor(.91f,.95f,1.f),FVector2D(720,440)},
   {MiningText::Get(TEXT("Office.Popup.01.Title")),MiningText::Get(TEXT("Office.Popup.01.Body")),TEXT("EMERGENCE"),FLinearColor(.96f,.96f,.91f),FVector2D(900,550)}
  };
  if(Index>=0 && Index<2)return Introduction[Index];
  static const FWorkPopup Copies[] = {
   {MiningText::Get(TEXT("Office.Popup.02.Title")),MiningText::Get(TEXT("Office.Popup.02.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),4},
   {MiningText::Get(TEXT("Office.Popup.Repository.Title")),MiningText::Get(TEXT("Office.Popup.Repository.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),1},
   {MiningText::Get(TEXT("Office.Popup.09.Title")),MiningText::Get(TEXT("Office.Popup.04.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),0},
   {MiningText::Get(TEXT("Office.Popup.05.Title")),MiningText::Get(TEXT("Office.Popup.05.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),1},
   {MiningText::Get(TEXT("Office.Popup.Clone.Title")),MiningText::Get(TEXT("Office.Popup.Clone.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),5},
   {MiningText::Get(TEXT("Office.Popup.Verification.Title")),MiningText::Get(TEXT("Office.Popup.Verification.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),3},
   {MiningText::Get(TEXT("Office.Popup.03.Title")),MiningText::Get(TEXT("Office.Popup.CoreLoop.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),0},
   {MiningText::Get(TEXT("Office.Popup.MeetingUpdate.Title")),MiningText::Get(TEXT("Office.Popup.MeetingUpdate.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),1},
   {MiningText::Get(TEXT("Office.Popup.DesignRevision.Title")),MiningText::Get(TEXT("Office.Popup.DesignRevision.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),0},
   {MiningText::Get(TEXT("Office.Popup.07.Title")),MiningText::Get(TEXT("Office.Popup.07.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),1},
   {MiningText::Get(TEXT("Office.Popup.08.Title")),MiningText::Get(TEXT("Office.Popup.08.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),0},
   {MiningText::Get(TEXT("Office.Popup.09.Title")),MiningText::Get(TEXT("Office.Popup.09.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),1},
   {MiningText::Get(TEXT("Office.Popup.AssetImport.Title")),MiningText::Get(TEXT("Office.Popup.AssetImport.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),5},
   {MiningText::Get(TEXT("Office.Popup.11.Title")),MiningText::Get(TEXT("Office.Popup.Greybox.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),4},
   {MiningText::Get(TEXT("Office.Popup.12.Title")),MiningText::Get(TEXT("Office.Popup.12.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),2},
   {MiningText::Get(TEXT("Office.Popup.13.Title")),MiningText::Get(TEXT("Office.Popup.13.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),2},
   {MiningText::Get(TEXT("Office.Popup.14.Title")),MiningText::Get(TEXT("Office.Popup.14.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),1},
   {MiningText::Get(TEXT("Office.Popup.18.Title")),MiningText::Get(TEXT("Office.Popup.11.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),0},
   {MiningText::Get(TEXT("Office.Popup.10.Title")),MiningText::Get(TEXT("Office.Popup.10.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),1},
   {MiningText::Get(TEXT("Office.Popup.Shaders.Title")),MiningText::Get(TEXT("Office.Popup.Shaders.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),5},
   {MiningText::Get(TEXT("Office.Popup.15.Title")),MiningText::Get(TEXT("Office.Popup.15.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),4},
   {MiningText::Get(TEXT("Office.Popup.16.Title")),MiningText::Get(TEXT("Office.Popup.16.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),0},
   {MiningText::Get(TEXT("Office.Popup.17.Title")),MiningText::Get(TEXT("Office.Popup.17.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),2},
   {MiningText::Get(TEXT("Office.Popup.18.Title")),MiningText::Get(TEXT("Office.Popup.18.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),1},
   {MiningText::Get(TEXT("Office.Popup.19.Title")),MiningText::Get(TEXT("Office.Popup.19.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),2},
   {MiningText::Get(TEXT("Office.Popup.20.Title")),MiningText::Get(TEXT("Office.Popup.20.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),0},
   {MiningText::Get(TEXT("Office.Popup.21.Title")),MiningText::Get(TEXT("Office.Popup.21.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),2},
   {MiningText::Get(TEXT("Office.Popup.22.Title")),MiningText::Get(TEXT("Office.Popup.22.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),4},
   {MiningText::Get(TEXT("Office.Popup.23.Title")),MiningText::Get(TEXT("Office.Popup.23.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),1},
   {MiningText::Get(TEXT("Office.Popup.24.Title")),MiningText::Get(TEXT("Office.Popup.24.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),0},
   {MiningText::Get(TEXT("Office.Popup.25.Title")),MiningText::Get(TEXT("Office.Popup.25.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),5},
   {MiningText::Get(TEXT("Office.Popup.SubmissionVerification.Title")),MiningText::Get(TEXT("Office.Popup.SubmissionVerification.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),3},
   {MiningText::Get(TEXT("Office.Popup.26.Title")),MiningText::Get(TEXT("Office.Popup.26.Body")),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),2}
  };
  return Copies[FMath::Clamp(Index-2,0,int32(UE_ARRAY_COUNT(Copies))-1)];
 }
 constexpr int32 Count=35;
 constexpr int32 ReconnectIndex=18;
 inline bool IsIntroduction(int32 Index){return Index>=0 && Index<2;}
 // Development advances with the office round, independently of click speed.
 // The final phase stays at submission; it never loops back to design.
 inline int32 ForProgress(float Seconds,int32 Ordinal)
 {
  // The sparse opening still gets its essential setup messages in order.
  if(Seconds<45.f && Ordinal>=0 && Ordinal<4)return 2+Ordinal;
  const int32 Start=Seconds<30.f?2:Seconds<48.f?12:Seconds<60.f?22:28;
  const int32 Length=Seconds<30.f?10:Seconds<48.f?10:Seconds<60.f?6:7;
  const int32 Candidate=Start+FMath::Max(0,Ordinal)%Length;
  // The initial design document appears once in the sparse opening; later
  // planning traffic uses meetings and revisions instead of repeating it.
  if(Candidate==5)return 9;
  // Asset completion belongs to late production, after planning and setup.
  if(Candidate==ReconnectIndex)return 13; // the widget schedules this interruption once
  return Candidate==20 && Seconds<40.f?14:Candidate;
 }
 inline int32 PhaseForContent(int32 Index){return Index<2?-1:Index<12?0:Index<22?1:Index<28?2:3;}
 inline int32 ArtVariant(int32 Index){return Get(Index).Art;}
 inline int32 UnreadCount(float Seconds,int32 PopupId,int32 ContentIndex)
 {
  // Keep each alert stable after creation; later alerts reflect the growing backlog.
  const int32 Phase=Seconds<0?FMath::Max(0,PhaseForContent(ContentIndex)):Seconds<30?0:Seconds<48?1:Seconds<60?2:3;
  static constexpr int32 Counts[][3]={{1,3,9},{12,28,66},{99,128,256},{999,999,999}};
  return Counts[Phase][FMath::Max(0,PopupId-3)%3];
 }

 struct FPopupArt
 {
  const TCHAR* AssetName;
  FVector2D Size;
  // Normalized hit area on the supplied artwork: x, y, width, height.
  FVector4 CloseRect;
  FVector4 TextRect;
  bool bUseWorkText;
 };
 constexpr int32 ArtCount=6;
 inline const FPopupArt& GetArt(int32 Index)
 {
  static const FPopupArt Skins[]={
   {TEXT("Window"),FVector2D(300,302),FVector4(.365,.725,.28,.17),FVector4(.07,.25,.86,.45),true},
   {TEXT("SystemMessage"),FVector2D(420,257),FVector4(.176,.725,.303,.15),FVector4(.065,.24,.87,.47),true},
   {TEXT("SystemError"),FVector2D(520,177),FVector4(.251,.713,.231,.151),FVector4(.18,.22,.76,.44),true},
   {TEXT("Robot"),FVector2D(420,116.5),FVector4(.057,.32,.097,.35),FVector4(0,0,0,0),false},
   {TEXT("Message"),FVector2D(340,283),FVector4(.37,.783,.275,.094),FVector4(0,0,0,0),false},
   {TEXT("Loading"),FVector2D(440,274),FVector4(.54,.745,.305,.125),FVector4(0,0,0,0),false}
  };
  return Skins[(Index%ArtCount+ArtCount)%ArtCount];
 }
 // One warning per twelve emitted windows, previously two per twelve.
 inline int32 FloodArtVariant(int32 Ordinal)
 {
  static constexpr int32 Sequence[]={0,1,2,3,4,5,0,1,4,3,4,5};
  return Sequence[FMath::Max(0,Ordinal)%UE_ARRAY_COUNT(Sequence)];
 }
 constexpr int32 DesktopWidth=1280;
 constexpr int32 DesktopHeight=800;
 // Keep complete frames inside the curved CRT aperture, clear of its bezel.
 constexpr float PopupMarginX=16.f;
 constexpr float PopupMarginTop=16.f;
 constexpr float PopupMarginBottom=24.f;
 inline FVector2D FitPopupPosition(FVector2D Position,FVector2D Size)
 {
  return FVector2D(FMath::Clamp(Position.X,PopupMarginX,DesktopWidth-Size.X-PopupMarginX),
   FMath::Clamp(Position.Y,PopupMarginTop,DesktopHeight-Size.Y-PopupMarginBottom));
 }
}
