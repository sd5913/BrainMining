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
  int32 Art=1;
 };

 inline const FWorkPopup& Get(int32 Index)
 {
  static const FWorkPopup Introduction[] = {
   {TEXT("Game Jam · 主题公布"),TEXT("21天gamejam游戏创作主题公布！"),TEXT("GAME JAM STARTS"),FLinearColor(.91f,.95f,1.f),FVector2D(720,440)},
   {TEXT("21天 GAME JAM · 主题发布页"),TEXT("【涌现】"),TEXT("EMERGENCE"),FLinearColor(.96f,.96f,.91f),FVector2D(900,550)}
  };
  if(Index>=0 && Index<2)return Introduction[Index];
  static const FWorkPopup Copies[] = {
   {TEXT("组队群 · 2条新消息"),TEXT("题目出来了。\n「涌现」……你们有什么想法？"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),4},
   {TEXT("Git · 新建仓库"),TEXT("仓库 gamejam-21 已建立。\n邀请已发送给另外3位成员。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),1},
   {TEXT("会议提醒"),TEXT("主题讨论会将在5分钟后开始。\n3位成员已加入。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),0},
   {TEXT("企划草案.docx"),TEXT("检测到较新的云端版本。\n是否加载其他成员的修改？"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),1},
   {TEXT("Git · 正在克隆"),TEXT("正在克隆 gamejam-21…"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),5},
   {TEXT("账号验证"),TEXT("请完成验证后继续登录。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),3},
   {TEXT("共享文档 · 有人提到了你"),TEXT("@你 核心循环那页改好了。\n这一版先不做多人模式了。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),0},
   {TEXT("会议时间已更新"),TEXT("玩法讨论会延长30分钟。\n下一场：分工与排期。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),1},
   {TEXT("策划案_v07.docx"),TEXT("有2条批注尚未解决。\n「这里能不能再简单一点？」"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),0},
   {TEXT("任务已分配给你"),TEXT("任务「角色移动」已指派给你。\n截止时间：明天18:00。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),1},
   {TEXT("To-do List · 今日到期"),TEXT("角色移动、物品交互、新手引导。\n还有3项任务未完成。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),0},
   {TEXT("会议提醒"),TEXT("进度同步会已开始。\n主持人正在等待你加入。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),1},
   {TEXT("资源导入"),TEXT("正在导入场景素材…"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),5},
   {TEXT("组队群 · 新消息"),TEXT("先把灰盒跑通，素材还在改。\n今晚再同步一下进度。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),4},
   {TEXT("引擎崩溃报告"),TEXT("游戏引擎意外退出。\n是否恢复上次自动保存的项目？"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),2},
   {TEXT("版本控制 · 合并失败"),TEXT("关卡文件存在冲突。\n请解决冲突后重试。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),2},
   {TEXT("连接已断开"),TEXT("正在重新连接5/5"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),1},
   {TEXT("组队群 · @你"),TEXT("音效发群文件了。\n按钮和脚步声记得接一下。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),0},
   {TEXT("资源同步完成"),TEXT("场景素材.zip 已下载。\n新增24个文件，替换6个文件。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),1},
   {TEXT("着色器编译"),TEXT("正在编译着色器…"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),5},
   {TEXT("试玩群 · 新消息"),TEXT("我进游戏了。\n开头这里是要往哪边走？"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),4},
   {TEXT("收到新的反馈"),TEXT("「跳过对话以后动不了了。」\n附件：录屏_02.mp4"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),0},
   {TEXT("问题追踪 · 新增3项"),TEXT("存档无法读取、角色穿墙、音效丢失。\n其中2项被标记为阻塞。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),2},
   {TEXT("组队群 · @你"),TEXT("跳跃参数调过了，再试下？\n我把新包传上去了。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),1},
   {TEXT("性能分析 · 警告"),TEXT("检测到帧耗时峰值：86 ms。\n记录已保存至性能报告。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),2},
   {TEXT("回归测试提醒"),TEXT("今天修复了12个问题。\n还有3项等待确认。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),0},
   {TEXT("打包失败"),TEXT("缺少资源：开始按钮音效。\n构建已终止，查看日志。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),2},
   {TEXT("测试群 · 新消息"),TEXT("新包打开是黑屏。\n等等……好了，能进了。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),4},
   {TEXT("Game Jam · 草稿已保存"),TEXT("作品页面尚未发布。\n待填写：操作说明、封面。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),1},
   {TEXT("组队群 · @所有人"),TEXT("最后这个包谁测过了？\n没问题我就上传了。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),0},
   {TEXT("上传中"),TEXT("game_final_final.zip — 99%\n请勿关闭此窗口。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),5},
   {TEXT("提交验证"),TEXT("请完成验证后提交作品。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),3},
   {TEXT("Game Jam · 提交提醒"),TEXT("距离截止还有10分钟。\n你的作品仍处于草稿状态。"),TEXT("GAME JAM"),FLinearColor(.91f,.95f,1.f),FVector2D(420,257),2}
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
 constexpr int32 DesktopWidth=1280;
 constexpr int32 DesktopHeight=800;
}
