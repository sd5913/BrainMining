# 卡牌房机器人

`character.fbx` 为用户在 2026-10-18 提供的原始文件，保留未修改副本。它包含静态硬表面分件，没有骨骼、动画或表情形变；引用的外部木纹法线图片未随文件提供。

`Scripts/prepare_card_robot.py` 保留原几何，归一到脚底为原点、140 cm 高、朝向 -X，拆分双眼并为头、眉毛、手臂和脚设置机械转轴。`Prepared/parts.json` 记录转轴；FBX 导出供 UE 导入。运行时 `ACardRoomRobot` 用刚性关节插值和眼睛缩放驱动动作，不对硬壳进行柔性蒙皮。

UE 资源位于 `/Game/Rooms/CardDuel/Robot`，使用独立的暖色壳体与深色金属材质。P2 的 `bUseRobotOpponent` 启用对手机器人；地面摆件使用同一类，保留轻微待机转头和眨眼。不要重跑旧兔子安装脚本覆盖新对手。
