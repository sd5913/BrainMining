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
 };

 inline const FWorkPopup& Get(int32 Index)
 {
  static const FWorkPopup Introduction[] = {
   {MiningText::Get(TEXT("Office.Popup.00.Title")),MiningText::Get(TEXT("Office.Popup.00.Body")),TEXT("GAME JAM STARTS"),FLinearColor(.91f,.95f,1.f),FVector2D(720,440)},
   {MiningText::Get(TEXT("Office.Popup.01.Title")),MiningText::Get(TEXT("Office.Popup.01.Body")),TEXT("EMERGENCE"),FLinearColor(.96f,.96f,.91f),FVector2D(900,550)}
  };
  if(Index>=0 && Index<2)return Introduction[Index];
  static const FWorkPopup Copies[] = {
   {MiningText::Get(TEXT("Office.Popup.02.Title")),MiningText::Get(TEXT("Office.Popup.02.Body")),TEXT("READ THE THEME"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.03.Title")),MiningText::Get(TEXT("Office.Popup.03.Body")),TEXT("BRAINSTORM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.04.Title")),MiningText::Get(TEXT("Office.Popup.04.Body")),TEXT("CORE GAME LOOP"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.05.Title")),MiningText::Get(TEXT("Office.Popup.05.Body")),TEXT("PROTOTYPE REVIEW"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.06.Title")),MiningText::Get(TEXT("Office.Popup.06.Body")),TEXT("SCOPE CHECK"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.07.Title")),MiningText::Get(TEXT("Office.Popup.07.Body")),TEXT("TEAM ASSIGNMENTS"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.08.Title")),MiningText::Get(TEXT("Office.Popup.08.Body")),TEXT("TODAY TO-DO"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.09.Title")),MiningText::Get(TEXT("Office.Popup.09.Body")),TEXT("DAILY MEETING"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.10.Title")),MiningText::Get(TEXT("Office.Popup.10.Body")),TEXT("ART ASSETS READY"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.11.Title")),MiningText::Get(TEXT("Office.Popup.11.Body")),TEXT("AUDIO TODO"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.12.Title")),MiningText::Get(TEXT("Office.Popup.12.Body")),TEXT("ENGINE CRASHED"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.13.Title")),MiningText::Get(TEXT("Office.Popup.13.Body")),TEXT("MERGE CONFLICT"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.14.Title")),MiningText::Get(TEXT("Office.Popup.14.Body")),TEXT("RECONNECTING 5/5"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.15.Title")),MiningText::Get(TEXT("Office.Popup.15.Body")),TEXT("FIRST PLAYTEST"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.16.Title")),MiningText::Get(TEXT("Office.Popup.16.Body")),TEXT("PLAYTEST FEEDBACK"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.17.Title")),MiningText::Get(TEXT("Office.Popup.17.Body")),TEXT("BUG TRIAGE"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.18.Title")),MiningText::Get(TEXT("Office.Popup.18.Body")),TEXT("GAME FEEL"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.19.Title")),MiningText::Get(TEXT("Office.Popup.19.Body")),TEXT("PERFORMANCE CHECK"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.20.Title")),MiningText::Get(TEXT("Office.Popup.20.Body")),TEXT("FINAL REVIEW"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.21.Title")),MiningText::Get(TEXT("Office.Popup.21.Body")),TEXT("BUILD FAILED"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.22.Title")),MiningText::Get(TEXT("Office.Popup.22.Body")),TEXT("CLEAN MACHINE TEST"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.23.Title")),MiningText::Get(TEXT("Office.Popup.23.Body")),TEXT("SUBMISSION PAGE"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.24.Title")),MiningText::Get(TEXT("Office.Popup.24.Body")),TEXT("RELEASE CHECK"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.25.Title")),MiningText::Get(TEXT("Office.Popup.25.Body")),TEXT("DEADLINE APPROACHING"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)},
   {MiningText::Get(TEXT("Office.Popup.26.Title")),MiningText::Get(TEXT("Office.Popup.26.Body")),TEXT("READY TO SUBMIT"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257)}
  };
  return Copies[FMath::Clamp(Index-2,0,int32(UE_ARRAY_COUNT(Copies))-1)];
 }
 constexpr int32 Count=27;
 inline bool IsIntroduction(int32 Index){return Index>=0 && Index<2;}
 // Development advances with the office round, independently of click speed.
 // The final phase stays at submission; it never loops back to design.
 inline int32 ForProgress(float Seconds,int32 Ordinal)
 {
  const int32 Start=Seconds<20.f?2:Seconds<40.f?7:Seconds<55.f?15:21;
  const int32 Length=Seconds<20.f?5:Seconds<40.f?7:6;
  return Start+FMath::Max(0,Ordinal)%Length;
 }
 inline int32 PhaseForContent(int32 Index){return Index<2?-1:Index<7?0:Index<15?1:Index<21?2:3;}
 inline int32 ArtVariant(int32 Index){return IsIntroduction(Index)?1:(Index==12 || Index==21)?2:Index%2;}
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
 constexpr int32 DesktopWidth=1280;
 constexpr int32 DesktopHeight=800;
}
