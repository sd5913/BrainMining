#include "MiningDesignerCatalog.h"
#include "MiningTunnelGenerator.h"
#include "MiningLayoutRoom.h"
#include "MiningHollowRegion.h"
#include "MiningHollowChamber.h"
#include "MiningLayoutSpace.h"
#include "MiningTunnelRoute.h"
#include "MiningTunnelShapeSection.h"
#include "UObject/UnrealType.h"

namespace MiningDesigner
{
UClass* Family(const UObject* Object)
{
	if (!Object) return nullptr;
	for (UClass* Type : {AMiningTunnelGenerator::StaticClass(), AMiningLayoutRoom::StaticClass(), AMiningHollowRegion::StaticClass(), AMiningHollowChamber::StaticClass(), AMiningLayoutSpace::StaticClass(), AMiningTunnelRoute::StaticClass(), AMiningTunnelShapeSection::StaticClass()})
	{
		if (Object->IsA(Type)) return Type;
	}
	return nullptr;
}

TArray<FName> Fields(const UObject* Object, EMiningDesignerSection Section)
{
	if (!Family(Object)) return {};
	if (Object->IsA<AMiningTunnelGenerator>())
	{
		switch (Section)
		{
		case EMiningDesignerSection::Gameplay: return {"PlayerScaleSettings", "FlightSettings"};
		case EMiningDesignerSection::Mining: return {"DiggingSettings"};
		case EMiningDesignerSection::Visual: return {"bImproveMiningVisibility", "HeadlampIntensity", "FillLightIntensity", "DeveloperMapSettings", "PreviewMode", "SectionAxis", "SectionPosition", "bSectionKeepPositive", "bShowBedrockInEditor"};
		case EMiningDesignerSection::Advanced: return {"SegmentTriangleBudget", "bAutomaticChunks", "ChunkLength", "bSimplifyChunks", "SimplificationTolerance", "RoomEntryInset", "RoomEntryHoldTime", "CompletedSegmentPolicy", "FrozenSegmentsToKeep", "RecycleHiddenTime"};
		case EMiningDesignerSection::Terrain: return {"bAutoRebuild", "bHollowRegionPrototype", "HollowRegion", "bDirectRoomFaces", "BedrockThickness", "TunnelWidth", "TunnelHeight", "MaxSlopeDegrees", "SearchDistance", "DoorMargin", "bRequireDescending", "BedrockMaterial", "TunnelMaterial"};
		}
	}
	if (Section != EMiningDesignerSection::Terrain) return {};
	if (Object->IsA<AMiningLayoutRoom>()) return {"ConnectionOrder", "RoomSize", "LayoutId"};
	if (Object->IsA<AMiningHollowRegion>()) return {"FromRoom", "ToRoom", "RegionWidth", "RockAbove", "RockBelow", "PassageRadius", "MaximumSlope", "Plugs"};
	if (Object->IsA<AMiningHollowChamber>()) return {"Region", "bEnabled", "ChamberSize", "ConnectionPosition", "BranchRadius", "bHiddenEntrance", "EntranceThickness", "bManualBranchPath"};
	if (Object->IsA<AMiningTunnelShapeSection>()) return {"Tunnel", "bEnabled", "Width", "Height"};
	if (Object->IsA<AMiningTunnelRoute>()) return {"StartWall", "EndWall"};
	if (Object->IsA<AMiningLayoutSpace>()) return {"bEnabled", "SpaceName", "FromRoom", "Shape", "SpaceSize", "Contents", "Connection", "Entrance", "BranchWidth", "BranchHeight", "bAlignBranchFloors", "EntranceRockThickness", "LayoutId"};
	return {};
}

bool Transferable(const FProperty* Property)
{
	if (!Property || !Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_EditConst | CPF_Transient)) return false;
	static const TSet<FName> Identity = {"ConnectionOrder", "LayoutId", "SpaceName", "bManualBranchPath", "bHollowRegionPrototype", "bDirectRoomFaces", "bAutoRebuild"};
	if (Identity.Contains(Property->GetFName())) return false;
	if (CastField<FObjectPropertyBase>(Property) || CastField<FSoftObjectProperty>(Property)) return false;
	return CastField<FNumericProperty>(Property) || CastField<FBoolProperty>(Property) || CastField<FEnumProperty>(Property) || CastField<FStructProperty>(Property) || CastField<FArrayProperty>(Property);
}

FString SectionName(EMiningDesignerSection Section)
{
	switch (Section)
	{
	case EMiningDesignerSection::Gameplay: return TEXT("角色与飞行");
	case EMiningDesignerSection::Mining: return TEXT("挖掘手感");
	case EMiningDesignerSection::Terrain: return TEXT("场景与空间");
	case EMiningDesignerSection::Visual: return TEXT("画面与调试");
	case EMiningDesignerSection::Advanced: return TEXT("高级与性能");
	}
	return TEXT("未知分类");
}

FString Effect(EMiningDesignerSection Section)
{
	switch (Section)
	{
	case EMiningDesignerSection::Terrain: return TEXT("修改后重新生成；自动生成开启时会延迟更新。失败保留上次成功地形，Ctrl+Z 可撤销参数。");
	case EMiningDesignerSection::Visual: return TEXT("剖面和基岩显示：编辑预览生效。角色照明与开发者地图：下次试玩生效。");
	case EMiningDesignerSection::Advanced: return TEXT("性能参数需重新生成，推进参数下次试玩生效。每块预算上限仍为 30000，默认锁定。");
	default: return TEXT("下次试玩生效，不需要重新生成地形。保存关卡后配置才会写入磁盘。");
	}
}

FString Caption(FName Name)
{
	static const TMap<FName, FString> Labels = {
		{"PlayerScaleSettings", TEXT("人物比例")}, {"FlightSettings", TEXT("按住上升、松开下落")}, {"DiggingSettings", TEXT("关卡挖掘设置")}, {"DeveloperMapSettings", TEXT("开发者小地图")},
		{"bEnabled", TEXT("启用")}, {"Scale", TEXT("人物缩放")}, {"MovementSpeedMultiplier", TEXT("水平速度倍率")}, {"JumpHeightMultiplier", TEXT("普通跳跃高度倍率")}, {"StepHeightMultiplier", TEXT("跨台阶高度倍率")}, {"MiningRadiusMultiplier", TEXT("挖掘半径倍率")}, {"MiningReachMultiplier", TEXT("挖掘距离倍率")}, {"MiningDownwardOffsetMultiplier", TEXT("挖掘下偏倍率")},
		{"AscentSpeed", TEXT("上升速度上限")}, {"MaxFallSpeed", TEXT("下落速度上限")}, {"AirMoveSpeed", TEXT("空中水平速度")}, {"LiftAcceleration", TEXT("上升加速度")}, {"FallGravityMultiplier", TEXT("下落重力倍率")},
		{"bOverrideCharacter", TEXT("启用关卡挖掘设置")}, {"Reach", TEXT("基础挖掘距离")}, {"Radius", TEXT("基础挖掘半径")}, {"Interval", TEXT("连续挖掘间隔")}, {"InsetFraction", TEXT("挖掘中心内嵌比例")}, {"DownwardOffset", TEXT("挖掘中心向下偏移")}, {"FallResetDistance", TEXT("坠落重置距离")},
		{"RoomSize", TEXT("房间尺寸")}, {"ConnectionOrder", TEXT("主房间连接顺序")}, {"LayoutId", TEXT("布局归属标识")}, {"FromRoom", TEXT("起始房间／所属通道")}, {"ToRoom", TEXT("终点房间")}, {"Region", TEXT("所属矿区")}, {"HollowRegion", TEXT("空心矿区对象")}, {"Tunnel", TEXT("所属主通道")},
		{"RegionWidth", TEXT("矿区左右总宽度")}, {"RockAbove", TEXT("上方普通岩层厚度")}, {"RockBelow", TEXT("下方普通岩层厚度")}, {"PassageRadius", TEXT("主矿道半径")}, {"MaximumSlope", TEXT("允许的最大坡度")}, {"Plugs", TEXT("主矿道岩塞列表")}, {"Position", TEXT("沿矿道的位置比例")}, {"Thickness", TEXT("岩塞厚度")},
		{"ChamberSize", TEXT("洞室尺寸")}, {"ConnectionPosition", TEXT("支路入口位置比例")}, {"BranchRadius", TEXT("支路半径")}, {"bHiddenEntrance", TEXT("隐藏入口（需挖开）")}, {"EntranceThickness", TEXT("隐藏封口厚度")}, {"bManualBranchPath", TEXT("手动编辑支路路径")},
		{"bAutoRebuild", TEXT("自动重新生成")}, {"bHollowRegionPrototype", TEXT("使用空心矿区方案")}, {"bDirectRoomFaces", TEXT("房间整面直连")}, {"BedrockThickness", TEXT("不可破坏基岩厚度")}, {"TunnelWidth", TEXT("旧版通道宽度")}, {"TunnelHeight", TEXT("旧版通道高度")}, {"MaxSlopeDegrees", TEXT("旧版路径最大坡度")}, {"SearchDistance", TEXT("旧版寻路范围")}, {"DoorMargin", TEXT("旧版洞口余量")}, {"bRequireDescending", TEXT("强制通道向下")}, {"BedrockMaterial", TEXT("基岩材质")}, {"TunnelMaterial", TEXT("可挖岩石材质")},
		{"Width", TEXT("截面宽度")}, {"Height", TEXT("截面高度")}, {"StartWall", TEXT("起始连接面")}, {"EndWall", TEXT("终点连接面")}, {"SpaceName", TEXT("空间名称")}, {"Shape", TEXT("空间形状")}, {"SpaceSize", TEXT("空间尺寸")}, {"Contents", TEXT("空间填充内容")}, {"Connection", TEXT("连接方式")}, {"Entrance", TEXT("入口类型")}, {"BranchWidth", TEXT("支路宽度")}, {"BranchHeight", TEXT("支路高度")}, {"bAlignBranchFloors", TEXT("对齐支路地板")}, {"EntranceRockThickness", TEXT("入口岩墙厚度")},
		{"bImproveMiningVisibility", TEXT("启用挖掘照明")}, {"HeadlampIntensity", TEXT("头灯亮度")}, {"FillLightIntensity", TEXT("角色周围补光")}, {"PreviewMode", TEXT("编辑器预览模式")}, {"SectionAxis", TEXT("剖切轴（0=X / 1=Y / 2=Z）")}, {"SectionPosition", TEXT("剖切位置")}, {"bSectionKeepPositive", TEXT("保留坐标正方向")}, {"bShowBedrockInEditor", TEXT("编辑器显示基岩")},
		{"bShowOnStart", TEXT("试玩开始时显示")}, {"PanelWidth", TEXT("小地图面板宽度")}, {"PanelHeight", TEXT("小地图面板高度")}, {"FollowWidth", TEXT("跟随模式世界宽度")}, {"BackgroundOpacity", TEXT("小地图背景不透明度")},
		{"SegmentTriangleBudget", TEXT("每块三角形预算（最多30000）")}, {"bAutomaticChunks", TEXT("启用自动分块")}, {"ChunkLength", TEXT("分块长度")}, {"bSimplifyChunks", TEXT("保守简化挖掘网格")}, {"SimplificationTolerance", TEXT("简化容差")}, {"RoomEntryInset", TEXT("抵达房间所需进入深度")}, {"RoomEntryHoldTime", TEXT("抵达房间确认时间")}, {"CompletedSegmentPolicy", TEXT("已完成矿区处理策略")}, {"FrozenSegmentsToKeep", TEXT("保留的已完成矿区数量")}, {"RecycleHiddenTime", TEXT("回收前的遮挡等待时间")}
	};
	const FString* Label = Labels.Find(Name);
	return Label ? *Label : Name.ToString();
}

FString Help(FName Name)
{
	static const TMap<FName, FString> HelpText = {
		{"Scale", TEXT("0.7 表示原尺寸的70%。胶囊碰撞、角色和挖掘一起缩放，不改变地图尺寸。")},
		{"bOverrideCharacter", TEXT("关闭时保持角色蓝图原值；打开后，本关卡使用下列基础参数，再乘人物缩放与挖掘倍率。")},
		{"Radius", TEXT("单次球形挖掘半径；最终值还会乘人物缩放和半径倍率。越大不代表预算越高。")},
		{"Reach", TEXT("从视角向前检测的最大距离；最终值还会乘人物缩放与距离倍率。")},
		{"Interval", TEXT("按住左键的两次开挖间隔。单位秒，越小挖得越频繁。")},
		{"LiftAcceleration", TEXT("越大，越快刹住下落并转为上升。是抵消重力后的净向上加速度。")},
		{"FallResetDistance", TEXT("0关闭坠落重置。正数会被系统提高到地图安全深度，避免到达下层房间时误重置。")},
		{"ConnectionOrder", TEXT("主房间顺序须唯一，从0开始。预设不会复制顺序，以免产生重复房间编号。")},
		{"ChamberSize", TEXT("完整内部尺寸，单位厘米。位置锚点是地板中心；修改尺寸不会移动地板。超过几何限制时保留旧地形。")},
		{"ConnectionPosition", TEXT("0到1之间的主矿道长度比例。移动洞室不会改变所属矿区，入口跟随这个比例。")},
		{"EntranceThickness", TEXT("主矿道净空之外至少保留的普通岩石厚度。不是额外的基岩墙。")},
		{"bManualBranchPath", TEXT("手动模式可以在视口拖动支路内部样条点，首尾仍自动跟随。首次使用可点“直线支路控制点”。")},
		{"Region", TEXT("开放和隐藏洞室都属于这个矿区，不会自己触发主房间推进。")},
		{"Plugs", TEXT("使用列表右侧添加或删除按钮。位置沿实际矿道长度计，厚度单位厘米；过近或越界会提示生成失败。")},
		{"PreviewMode", TEXT("只影响编辑视图。Layout=布局，Section=剖面，Game=游戏几何；不会把剖切应用到试玩碰撞。")},
		{"CompletedSegmentPolicy", TEXT("KeepFrozen保留已挖地形；SealAndRecycle先封闭旧入口再回收。不会仅因转动相机就删除地形。")},
		{"SegmentTriangleBudget", TEXT("独立分块预算，不是全地图共享预算。编辑器和预设导入都禁止高于30000。")},
		{"ChunkLength", TEXT("越小单次碰撞更新可能更轻，但组件更多。需重新生成；不会拓宽通道。")}
	};
	const FString* Text = HelpText.Find(Name);
	return Text ? *Text : TEXT("直接修改现有对象；支持撤销。单位及合法范围以输入框为准。");
}

FString Status(const UObject* Object)
{
	if (const auto* Actor = Cast<AMiningTunnelGenerator>(Object)) return FString::Printf(TEXT("生成版本 %d ｜ %s\n%s"), Actor->BuildRevision, *Actor->LastBuildStatus, *Actor->LayoutWarnings);
	if (const auto* Actor = Cast<AMiningHollowRegion>(Object)) return Actor->RegionStatus;
	if (const auto* Actor = Cast<AMiningHollowChamber>(Object)) return Actor->ChamberStatus;
	if (const auto* Actor = Cast<AMiningLayoutSpace>(Object)) return Actor->SpaceStatus;
	if (const auto* Actor = Cast<AMiningTunnelRoute>(Object)) return Actor->RouteStatus;
	if (const auto* Actor = Cast<AMiningTunnelShapeSection>(Object)) return Actor->SectionStatus;
	return TEXT("房间位置为地板中心。拖动编辑保留；检查连接顺序是否唯一。");
}
}
