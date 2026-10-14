#pragma once

#include "CoreMinimal.h"

namespace PopupCloseContent
{
 struct FWorkPopup
 {
  const TCHAR* Title;
  const TCHAR* Body;
  const TCHAR* PhysicalTitle;
  FLinearColor Color;
  FVector2D Size;
 };

 inline const FWorkPopup& Get(int32 Index)
 {
  static const FWorkPopup Copies[] = {
   {TEXT("DDL 提醒"),TEXT("今日 18:00 前提交项目汇报。\n附件、数据和演示稿仍待确认。"),TEXT("DDL 18:00"),FLinearColor(1.f,.88f,.82f),FVector2D(420,230)},
   {TEXT("有新的工作消息"),TEXT("项目群：刚才的方案需要再调整一下。\n请尽快回复并同步最新进度。"),TEXT("NEW MESSAGE"),FLinearColor(.85f,.93f,1.f),FVector2D(350,190)},
   {TEXT("新增待办"),TEXT("你收到一项新的紧急任务。\n优先级：高。交付时间：今天。"),TEXT("URGENT TASK"),FLinearColor(1.f,.95f,.79f),FVector2D(330,200)},
   {TEXT("会议即将开始"),TEXT("需求评审将在 5 分钟后开始。\n请准备好最新版本和会议纪要。"),TEXT("MEETING IN 5 MIN"),FLinearColor(.88f,.94f,.87f),FVector2D(390,210)},
   {TEXT("方案修改意见"),TEXT("反馈已更新：\n1. 补充预算和排期\n2. 更新所有图表\n3. 今天下班前重新提交"),TEXT("REVISION REQUIRED"),FLinearColor(.94f,.88f,1.f),FVector2D(460,280)},
   {TEXT("汇报材料待审核"),TEXT("周报、工时表和项目总结尚未完成审核。\n请核对附件并补齐缺失的内容。"),TEXT("REPORT REVIEW"),FLinearColor(.89f,.93f,.96f),FVector2D(500,245)},
   {TEXT("工作邮件 · 需要回复"),TEXT("客户：请提供最新进度、交付清单和风险说明。\n抄送：项目负责人、协作团队。"),TEXT("REPLY REQUESTED"),FLinearColor(.91f,.96f,.96f),FVector2D(470,220)},
   {TEXT("排期变更"),TEXT("交付时间已提前。\n请重新安排手头的工作，并更新项目排期。"),TEXT("SCHEDULE CHANGED"),FLinearColor(1.f,.9f,.87f),FVector2D(360,225)}
  };
  return Copies[(Index%8+8)%8];
 }
 constexpr int32 Count=8;
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
