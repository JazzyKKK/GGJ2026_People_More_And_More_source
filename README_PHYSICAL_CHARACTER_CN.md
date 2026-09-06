# 可操控物理动画角色：使用与维护

本版本把 `Test_Lab2/BP_ThirdPersonCharacter` 中的 `phyAni` BeginPlay 节点迁到了 C++，并在 `GGJ_gamedemo` 中接好美术模型、共享骨架、三段移动动画和 Physics Asset。蓝图不再需要复制物理初始化节点，只负责选资源和调参数。

## 1. 直接运行

在内容浏览器打开：

`/Game/GGJ/Character/Maps/L_PhysicalCharacterLab`

点击 Play。操作方式：

- `WASD` / 方向键 / 手柄左摇杆：按镜头方向移动。
- 鼠标 / 手柄右摇杆：自由环绕第三人称镜头。
- `Space` / 手柄下方按钮：跳跃。

测试图自己的 World Settings 已指定 `BP_PhysicalAnimationGameMode`，不会改动原来 Crowd 原型的默认地图和 GameMode。场景里的橙色方块开启了物理，可以用来检查人物身体的碰撞、摇摆与受力。

## 2. 已接好的资源关系

```text
SK_GGJCharacter（人物网格）
├─ SKEL_GGJCharacter（唯一共享 Skeleton）
├─ PHYS_GGJCharacter（身体刚体与关节约束）
└─ BP_PhysicalAnimationCharacter / Mesh

SKEL_GGJCharacter
├─ A_GGJ_HappyIdle
├─ A_GGJ_Walk
└─ A_GGJ_Run
```

资源目录：`/Game/GGJ/Character/`

- `Art/Model/SK_GGJCharacter`：从 `Asset/base_tp.fbx` 导入的 Skeletal Mesh。
- `Art/Model/SKEL_GGJCharacter`：模型的骨架。三段动画必须引用这一份骨架。
- `Art/Model/PHYS_GGJCharacter`：自动生成的 Physics Asset，是布娃娃碰撞体和关节所在位置。
- `Art/Animations/`：Idle、Walking、Running，均作为 `AnimSequence` 导入，不是第二份人物模型。
- `Blueprints/BP_PhysicalAnimationCharacter`：最终给关卡使用的角色蓝图。
- `Blueprints/BP_CharacterGroupManager`：初始人数、人数上限、队形间距和未来复制接口。
- `Blueprints/BP_PhysicalAnimationGameMode`：让关卡生成上述蓝图角色。

FBX 原始骨骼名是 `mixamorig:Hips`，UE 5.6 Interchange 导入后去掉命名空间，资产里的实际名称是 `Hips`。因此当前 `Driven Bone Name` 必须填 `Hips`。

这批 FBX 的单位元数据声明为米，但模型数值按厘米制作；导入脚本已对模型和三段动画统一使用 `Import Uniform Scale = 0.01`。如果以后手动重新导入同一批文件，也要保留这个值，否则人物会被放大 100 倍。

## 3. Test_Lab2 蓝图逻辑迁移结果

原蓝图 BeginPlay 的执行顺序为：

```text
PhysicalAnimation.SetSkeletalMeshComponent(Mesh)
→ ApplyPhysicalAnimationSettingsBelow(Bone, Data, IncludeSelf=true)
→ Mesh.SetAllBodiesBelowSimulatePhysics(Bone, true, IncludeSelf=false)
```

现在对应到 `AGGJPhysicalAnimationCharacter::InitializePhysicalAnimation()`。保留的原参数是：

| 参数 | 当前值 | 作用 |
|---|---:|---|
| Driven Bone Name | `Hips` | 从髋部开始应用肌肉和子骨骼模拟 |
| Include Driven Bone In Simulation | `false` | 髋部仍跟随 Character 胶囊，下面的身体参与物理 |
| Is Local Simulation | `true` | 使用局部空间物理驱动 |
| Orientation Strength | `100` | 把骨骼旋转拉回动画姿势 |
| Angular Velocity Strength | `100` | 抑制角速度偏差和持续乱甩 |
| Position Strength | `100` | 把位置拉回动画姿势 |
| Velocity Strength | `100` | 抑制线速度偏差 |
| Max Linear / Angular Force | `0` | UE 的无限制默认值 |
| Muscle Strength | `5` | PhysicalAnimation 的总强度倍率 |

`ACharacter` 的胶囊和 `CharacterMovement` 负责可靠移动、爬坡和跳跃；骨骼刚体负责身体摇摆、碰撞和受挤压；Physical Animation 像肌肉一样把身体拉回正在播放的动画姿势。这种分层可以做到“持续可控的布娃娃”，而不是死亡后完全失控的 Ragdoll。

## 4. 蓝图里怎样调

打开 `BP_PhysicalAnimationCharacter`，点击 `Class Defaults`，主要看以下分类：

### Physical Character | Physics

- `Muscle Strength`：最直观。建议先在 `0.5～8` 之间试。越小越软、越容易被撞倒；越大越努力追随动画。
- `Drive Settings / Orientation Strength`：站姿和四肢保持能力。
- `Drive Settings / Angular Velocity Strength`：减少四肢甩动。过大可能显得僵硬或抖动。
- `Drive Settings / Position Strength`：位置跟随力度。对线性自由度被锁定的 Physics Asset，效果可能不明显。
- `Driven Bone Name`：必须是 Skeleton Tree 中真实存在、且 Physics Asset 有 Body 的骨骼。
- `Include Driven Bone In Simulation`：当前保持关闭。打开后 Hips 也会完全参与模拟，角色可能与胶囊脱节或摔倒，需要另做骨盆跟随控制。
- `Enable Physical Animation On Begin Play`：关闭后只剩正常第三人称角色，便于做 A/B 对比。

运行时蓝图还可以调用：

- `Set Muscle Strength`：例如进入冰面时降低，获得更无力的感觉。
- `Add Bone Impulse`：机关撞击或其它角色挤压时给具体骨骼冲量。
- `Reset Physical Animation`：角色卡住或关卡重置时清速度并重建物理动画。
- `Initialize Physical Animation`：运行时更换 Mesh / Physics Asset 后重新初始化。

### Physical Character | Animation

- `Idle Animation`、`Walk Animation`、`Run Animation`：三段动画资源。
- `Idle Speed Threshold`：低于该速度进入 Idle。
- `Run Speed Threshold`：高于该速度进入 Run，中间为 Walk。

当前使用 Single Node 播放，优点是不需要先学习 Anim Blueprint，替换同骨架动画很快；切换是直接切换，不带状态机混合。后续需要更自然的转身、过渡和上半身分层时，再建立 Anim Blueprint，把这三个资源接入 Locomotion State Machine。Physical Animation 的 C++ 部分不需要因此重写。

### Mesh 与 Camera Boom

- `Mesh`：可修改 Skeletal Mesh、Physics Asset Override、材质和相对位置/旋转。
- `Camera Boom / Target Arm Length`：镜头距离，当前为官方第三人称模板的 `400`。
- `Camera Boom / Socket Offset`：镜头观察高度。
- `Follow Camera / Field Of View`：透视强弱。

## 5. 换下一版人物模型

如果美术给的是“同一副骨架、仅模型外观更新”：

1. 导入新 Skeletal Mesh 时，在 Import Options 的 `Skeleton` 选择 `SKEL_GGJCharacter`。
2. 不要生成第二份 Skeleton。
3. 在 `BP_PhysicalAnimationCharacter > Mesh` 替换 Skeletal Mesh。
4. 打开动画检查模型是否正常播放；打开 Physics Asset 检查身体碰撞体是否仍贴合。

如果美术给的是“全新骨架”：

1. 第一次导入模型时让 UE 创建新的 Skeleton。
2. 在 Skeletal Mesh 编辑器中创建或指定新的 Physics Asset。
3. 后续所有动画导入时，明确选择这份新 Skeleton；不要勾成 Skeletal Mesh 导入。
4. 在角色蓝图 Mesh 中同时替换 Skeletal Mesh 和 Physics Asset Override。
5. 在 Skeleton Tree 查找骨盆骨骼真实名称，并修改 `Driven Bone Name`。
6. 打开 Physics Asset，确认骨盆、躯干、头、上下臂、上下腿至少都有合理 Body，并逐个检查 Constraint 角度。
7. 先关闭 Physical Animation 预览普通动画；确认无拉伸后再开启并从低 `Muscle Strength` 往上调。

Skeleton 是“骨骼名称和层级”；Skeletal Mesh 是“皮肤顶点怎样跟随骨骼”；Animation Sequence 是“这些骨骼随时间怎样动”；Physics Asset 是“哪些骨骼拥有刚体，以及刚体之间怎样受关节约束”。动画、模型和物理要稳定协作，四者的引用关系必须正确。

## 6. Physics Asset 首轮人工检查

自动生成只能作为起点。双击 `PHYS_GGJCharacter` 后建议按这个顺序检查：

1. 胶囊/盒体不要大面积穿出人物，也不要互相严重重叠。
2. 手臂、腿的 Body 不应覆盖到相邻肢体，否则会不断自碰撞抖动。
3. 肩、肘、髋、膝的 Constraint 旋转轴要符合动作方向。
4. 膝和肘限制不要完全自由；躯干可以稍软，但不要 360 度翻折。
5. 运行测试图，让物理方块撞击角色，再逐步调 Constraint 和 Muscle Strength。

若出现“身体飞散”，优先查重叠 Body 和 Constraint；若出现“模型留在原地、胶囊走远”，确认 `Hips` 没有被设为模拟；若完全没有摇摆，确认 Physics Asset 已赋值、身体 Body 存在且 BeginPlay 日志显示 Physical Animation 已启用。

## 7. C++ 文件分工

- `Public/Character/GGJPhysicalAnimationCharacter.h`：蓝图能看到的组件、参数和函数接口。
- `Private/Character/GGJPhysicalAnimationCharacter.cpp`：第三人称移动、相机、输入兜底、动画选择和 Physical Animation 初始化。
- `Public/Character/GGJCharacterGroupManager.h` 与对应 `.cpp`：统一保存成员弱引用、初始化三人、广播同一移动输入，并提供未来人数增减接口。
- `Public/Character/GGJPhysicalAnimationGameMode.h` 与对应 `.cpp`：测试关卡默认 Pawn 的最小 GameMode。
- `Private/Character/Tests/GGJPhysicalAnimationCharacterTests.cpp`：参数、资源兼容和真实 Chaos 运行时测试。
- `Scripts/CreatePhysicalCharacterDemo.py`：本轮可重复参考的资源导入/蓝图/测试关卡生成脚本；它是 create-only，检测到现有目录后会拒绝覆盖，避免误删你后续的蓝图修改。
- `Scripts/RebuildPhysicalCharacterLab.py`：只重建独立测试关卡，不改模型、动画、Physics Asset 或角色蓝图。

需要改手感时优先改蓝图默认值；需要改变初始化顺序、输入规则或运行时能力时再改 C++。关卡可以完全由你自己搭建，只需在 World Settings 中把 GameMode Override 指向 `BP_PhysicalAnimationGameMode`，或手动放置并占有 `BP_PhysicalAnimationCharacter`。

## 8. 三人统一控制

使用 `BP_PhysicalAnimationGameMode` 的关卡会自动创建 `BP_CharacterGroupManager`。它等待玩家角色被占有后，把该角色登记为 Leader，再在左右两侧生成两个同蓝图类成员。只有 Leader 拥有 PlayerController；Manager 把 Leader 算出的同一世界方向以及跳跃按下/松开强制发送给所有人，所以另外两人无需 AIController 也能移动和跳跃。

在 `BP_CharacterGroupManager > Class Defaults` 中可修改：

- `Initial Population`：当前为 `3`。
- `Max Population`：当前为 `30`，以后限制函数增长。
- `Formation Spacing`：当前为 `380 cm`，按这套人物张开双臂后的宽度留出安全间距。
- `Member Class`：默认留空，运行时自动采用 Leader 的 `BP_PhysicalAnimationCharacter` 类。

已预留但当前没有复制区调用的蓝图接口：`Add Population`、`Remove Population`、`Request Population Delta`、`Get Population Count` 和 `Get Members`。复制区以后只需计算人数变化并调用 `Request Population Delta`，不需要改统一控制结构。

### 当前动画兼容提醒

你现在指定的 `Walking_1__Anim` 和 `running_1__Anim` 使用的是 `running_1__Skeleton`，而角色模型使用 `SKEL_GGJCharacter`。两者不能直接混用。C++ 已做安全保护：不兼容时会保持 Idle，不会因为三个人同时切换动画而崩溃。要恢复走路和跑步动画，请在编辑器中把这两段动画重定向到 `SKEL_GGJCharacter`，然后在 `BP_PhysicalAnimationCharacter > Class Defaults` 中重新指定重定向后的动画资源。

## 9. 群体自适应相机

`BP_PhysicalAnimationGameMode` 会自动生成 `BP_GroupCamera`，PlayerController 仍占有 Leader，但最终画面使用独立群体相机。镜头焦点取所有有效成员 Actor Location 的平均值，并根据成员离质心的最大距离、角色半径、留白和 FOV 自动计算取景距离。Q/E 分别请求向左/向右旋转 90°；旋转期间不叠加新请求。

在 `BP_GroupCamera > Class Defaults` 中可修改：

- `Pitch / Initial Yaw / Field Of View`：斜俯视角、开局观察面和透视视野。
- `Min Distance / Max Distance`：自动缩放范围。
- `Member Bounds Radius / Framing Padding`：人物占屏安全半径和边缘留白。
- `Focus Height Offset`：相机中心相对人物原点的高度。
- `Focus Spring Stiffness / Zoom Spring Stiffness`：质心跟随和远近调节速度。
- `Critical Damping`：默认 `1` 为临界阻尼，平滑加速减速且不振荡。
- `Quarter Turn Duration / Rotation Ease Exponent`：Q/E 转镜用时和渐入渐出曲线。

一般关卡无需手动放相机。若需要某一关单独调参，可以把 `BP_GroupCamera` 放进场景，GameMode 会优先复用场景中的实例，而不是再自动生成。

## 10. 玩法区域

蓝图统一位于 `/Game/GGJ/Gameplay/Zones`。三个区域都可以直接拖进关卡并缩放 Box；出生区和销毁区没有运行时模型，美术可以在蓝图子类或关卡中自行添加装饰，但不要让装饰组件替代原有 Box。

### BP_SpawnZone

关卡没有出生区时沿用 PlayerStart 和旧阵列逻辑；只要存在至少一个启用的出生区，初始人数就全部改由出生区分配。假设 `Initial Population=10`：两个区域分成 5/5，三个区域分成 4/3/3。余数按照 `Spawn Order` 从小到大分配，顺序相同时按 Actor 名称稳定排序。

- `Spawn Bounds`：出生范围；多人会在范围内按居中网格排开。
- `Spawn Order`：分配顺序。
- `Spawn Height Offset`：角色 Actor 原点相对 Box 中心的高度修正。
- `Enabled`：关闭后该区域不参与初始分配。

### BP_DestroyZone

角色与 `Destroy Bounds` 重叠时，立即从 Manager 注销并销毁。`Can Destroy Leader` 开启时，Leader 进入也会消失；若还有成员，Manager 自动选择一个成员成为新 Leader、移交 PlayerController，并恢复群体相机 ViewTarget。可以监听 `On Member Destroyed` 播放粒子、声音或场景反馈。

销毁区和计数区同时使用 Box 局部空间几何检测，不依赖角色 Collision Profile 或 `Generate Overlap Events`。`Detection Padding` 是 Box 边缘的额外容差；销毁区的 `Detection Interval` 默认每 `0.05 s` 扫描一次。

### BP_CountZone

运行时每帧更新以下蓝图只读值：

- `Inside Population` / `Get Inside Population`：当前位于区域中的有效群体成员数。
- `Scene Population` / `Get Scene Population`：当前场景整个群体的人数。
- `Is Triggered` / `Is Triggered()`：当前判定是否达成，供门、灯、机关等其他蓝图读取。
- `On Count Updated`：区域人数或场景人数变化时广播。
- `On Triggered Changed`：达成状态发生变化时广播，开门逻辑优先绑定这个事件，不必每帧查询。

简单规则直接设置 `Comparison`：`=、!=、>、>=、<、<=`，并填写 `Target Count`。复杂规则在 `BP_CountZone` 的子蓝图中 Override `Evaluate Count Condition`，使用传入的 `Inside Count、Total Count、Target` 计算并返回 Bool；将 `Comparison` 设为 `Custom`，再用 `Custom Comparison Label` 指定场景文字中显示的短符号。

默认场景显示为：区域无人时显示 `?`；有人但未达成时以橙色显示例如 `6>=10`；达成时变为绿色并持续轻微缩放脉冲。`Status Text` 是 Text Render 组件，会自动朝向玩家相机。可在 `Visual` 分类修改文字高度、尺寸、三种颜色及脉冲幅度/速度，以匹配简约低多边形美术。
