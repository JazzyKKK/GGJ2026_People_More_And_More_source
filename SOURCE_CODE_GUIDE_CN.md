# GGJ 原型：C++ 源码阅读指南

这份文档配合 `Source` 目录内的中文注释使用。它回答三个问题：每个文件管什么、代码运行时怎样连起来、以后应该从哪里扩展。

当前项目同时保留两套互不覆盖的原型：基础人群移动、Chaos 碰撞、斜视透视相机与 Q/E 四面转镜；以及使用美术骨骼和 Physical Animation 的单人第三人称可操控布娃娃。复制区、按钮、传送门及关卡胜负尚未实现。

## 1. 推荐阅读顺序

第一次阅读建议按下列顺序，不必从模块入口逐行向下看：

1. `CrowdAgentPhysicsData.h`：先认识小人的可调物理数据。
2. `CrowdPopulationSubsystem.h/.cpp`：理解“一个 World 一份人口与共享输入”。
3. `CrowdAgent.h/.cpp`：理解物理球和视觉模型为什么分开。
4. `CrowdPhysicsMovementComponent.h/.cpp`：看共享方向怎样变成 Chaos 加速度。
5. `CrowdCameraSettings.h`、`CrowdCameraPawn.h/.cpp`：看 Q/E 轨道相机与屏幕方向换算。
6. `CrowdPlayerController.h/.cpp`：把 Enhanced Input、相机和共享输入串起来。
7. `CrowdLevelDirector.h/.cpp`：看每张关卡如何配置相机并初始化人口。
8. `CrowdGameMode.h/.cpp`：看 Controller 和 CameraPawn 如何成为关卡默认框架类。
9. `CrowdLightingRig.h/.cpp`：可选表现层，不影响玩法。
10. `CrowdPhysicsTests.cpp`：最后用测试理解系统必须维持的不变量。

## 2. Source 目录的层次

```text
Source/
├─ GGJ_gamedemo.Target.cs             游戏构建目标
├─ GGJ_gamedemoEditor.Target.cs       编辑器构建目标
└─ GGJ_gamedemo/
   ├─ GGJ_gamedemo.Build.cs           模块依赖
   ├─ GGJ_gamedemo.h/.cpp             模块入口
   ├─ Public/Character/                可操控物理动画角色与专用 GameMode 声明
   ├─ Private/Character/               角色实现和自动化测试
   ├─ Public/Crowd/                    可被其它模块包含的类型声明
   │  ├─ Data/                         纯配置数据
   │  ├─ Core/                         World 级人口状态
   │  ├─ Agents/                       单个小人与出生标记
   │  └─ Framework/                    GameMode、Controller、相机、关卡入口、灯光
   └─ Private/Crowd/                   上述类型的实现与自动化测试
```

`.h` 主要声明“外部能看到什么”，`.cpp` 主要实现“具体怎样做”。`Public` 不等于“蓝图可见”；蓝图是否可见由 `Blueprintable`、`BlueprintType`、`UFUNCTION` 和 `UPROPERTY` 的标记决定。

## 3. 三条核心运行链路

### 3.1 开始关卡

```text
地图选择 BP_CrowdGameMode
  → GameMode 生成 BP_CrowdController 与 BP_CrowdCamera
  → 场景中的 BP_CrowdLevelDirector::BeginPlay
  → 延迟到下一帧，等待 Pawn 和场景碰撞初始化
  → ConfigureCamera：设置关卡观察中心/可选覆盖参数
  → ResetCrowd：计算出生位置
  → CrowdPopulationSubsystem::SpawnAgent
  → Deferred Spawn 完成蓝图构造
  → CrowdAgent::InitializePhysics
  → 开启 CrowdPhysicsMovementComponent Tick
```

Director 延迟一帧不是视觉特效，而是初始化时序保护。如果立刻生成，关卡碰撞体或默认 CameraPawn 可能还没准备好。

### 3.2 每帧同步移动

```text
WASD / 方向键 / 手柄
  → Enhanced Input 计算 MoveAction
  → CrowdPlayerController::HandleMove 暂存屏幕 XY
  → CrowdPlayerController::PostProcessInput
  → CameraPawn::AdvanceOrbit 先更新本帧镜头
  → CameraPawn::ScreenInputToWorld 转为世界 XY
  → PopulationSubsystem::SetSharedInput 限制到单位圆
  → 每个 MovementComponent 在 TG_PrePhysics 读取同一输入
  → Body::AddForce(..., bAccelChange=true)
  → Chaos 分别解决每个球与小人/墙壁之间的碰撞和挤压
```

“同输入”不等于“同位置”。每个小人施加相同主动加速度，但拥有独立刚体状态，所以地形和人群密度会自然把它们分开。

### 3.3 Q/E 转镜

```text
Q 或 E Action Started
  → Controller::RotateLeft / RotateRight
  → CameraPawn::RequestQuarterTurn(-1 / +1)
  → 记录 StartYaw 与未归一化 TargetYaw
  → 后续 PostProcessInput 每帧 AdvanceOrbit
  → InterpEaseInOut 计算加速—减速角度
  → ApplyOrbitPose 更新透视 Camera 的相对位置/旋转
  → 完成时归一化角度并广播 OnRotationFinished
```

转镜过程中再次按 Q/E 会返回 `false`，不会排队。使用未归一化目标角是为了从 170° 转到 260° 时仍按指定方向走 90°，而不是被 ±180° 表示边界误导。

## 4. 每个文件的职责与代码块

### 4.1 构建与模块入口（5 个文件）

| 文件 | 是什么 | 关键代码 |
|---|---|---|
| `Source/GGJ_gamedemo.Target.cs` | 独立游戏/打包目标 | `TargetType.Game`；将 `GGJ_gamedemo` 加入游戏构建 |
| `Source/GGJ_gamedemoEditor.Target.cs` | UE 编辑器目标 | `TargetType.Editor`；Development Editor 使用它 |
| `Source/GGJ_gamedemo/GGJ_gamedemo.Build.cs` | 主模块依赖清单 | `Core`、`Engine`、`EnhancedInput`、`PhysicsCore` 等依赖；新增 UMG 等模块时在此添加 |
| `Source/GGJ_gamedemo/GGJ_gamedemo.h` | 主模块公共头 | 当前只含 `CoreMinimal.h`，玩法接口都在 Crowd 子目录 |
| `Source/GGJ_gamedemo/GGJ_gamedemo.cpp` | 主模块注册入口 | `IMPLEMENT_PRIMARY_GAME_MODULE` 让 UE 能加载项目模块 |

这几个文件通常不参与玩法迭代。只有新增模块依赖、改 Target 类型或需要模块启动/关闭回调时才修改。

### 4.2 Data：蓝图可调的纯数据（3 个文件）

#### `Public/Crowd/Data/CrowdAgentPhysicsData.h`

- `FCrowdPhysicsSettings`：一名小人的物理参数结构体。
- `Radius`：唯一玩法碰撞球半径，也参与生成防重叠和网格间距计算。
- `MassKg`：质量影响互相碰撞时谁更容易被推走。
- `DriveAcceleration`：输入驱动强度；采用加速度模式，不因质量改变主动手感。
- `LinearDamping`：松键后的物理阻尼。
- `SoftSpeedLimit`：只停止继续顺向加速，不裁掉碰撞带来的速度。
- `UCrowdAgentPhysicsData`：把结构体包装成 Data Asset，供多个关卡复用。

#### `Private/Crowd/Data/CrowdAgentPhysicsData.cpp`

- `Sanitized()` 复制原参数，先用 `FMath::IsFinite` 排除 NaN/无穷，再限制到安全范围。
- 返回副本而非修改资产，因此 PIE 运行不会把编辑器里的原值写脏。

#### `Public/Crowd/Data/CrowdCameraSettings.h`

- `FCrowdCameraSettings`：镜头距离、俯角、初始水平角、透视 FOV、转动时长和缓动指数。
- `Sanitized()` 写在头文件内部，是一个很短的内联数据校验函数。
- `InitialYaw` 使用 `UnwindDegrees` 归一化；其它量使用合理的上下限。

### 4.3 Core：一个 World 一份的共享状态（2 个文件）

#### `Public/Crowd/Core/CrowdPopulationSubsystem.h`

- `UWorldSubsystem` 让每个 Game/PIE World 自动拥有独立实例，无需场景手动放置。
- `SharedInput` 是全体小人的世界 XY 输入。
- `Agents` 使用弱引用：登记小人但不拥有小人生命周期。
- 蓝图只开放人口数量和共享输入查询；生成、销毁和写输入保持为 C++ 接口，避免所有权混乱。

#### `Private/Crowd/Core/CrowdPopulationSubsystem.cpp`

- `DoesSupportWorldType`：只支持真实游戏和 PIE，忽略资源预览 World。
- `Deinitialize`：World 结束时清理自身缓存。
- `GetPopulationCount`：逐项判断弱引用有效性，而不是直接返回数组长度。
- `SetSharedInput`：非法输入归零，长度超过 1 才归一化，因此手柄模拟量不会丢失。
- `SpawnAgent`：验证参数 → 球形阻挡查询 → Deferred Spawn → 蓝图构造完成 → 应用权威物理参数 → 注册 → 开启移动。
- `UnregisterAgent`：小人销毁时移除自己和其它失效项。
- `ClearPopulation`：先把数组 `MoveTemp` 到局部变量，再销毁 Actor，避免 `EndPlay` 回调一边遍历一边修改原数组。

### 4.4 Agents：单个刚体及其表现（6 个文件）

#### `Public/Crowd/Agents/CrowdAgent.h`

- `ACrowdAgent` 是普通 `AActor`，不是 `ACharacter` 或被占有的 `APawn`。
- `Body`：根组件、球形碰撞、唯一开启 Physics 的组件。
- `VisualRoot`：蓝图整体调整外观的挂点。
- `VisualMesh` / `HeadMesh`：占位 3D 模型，必须保持 NoCollision。
- `BodyMaterial` / `HeadMaterial`：可选的第 0 材质槽覆盖。
- `Movement`：专用刚体驱动组件。
- `RuntimeMaterial`：运行时临时物理材质，不保存进资产。

#### `Private/Crowd/Agents/CrowdAgent.cpp`

- 构造函数创建默认球体、圆柱身体、球形头和移动组件。
- Body 关闭重力，锁定 Z 位移和三个旋转轴，打开 CCD 并提高求解迭代次数。
- `OnConstruction` / `ApplyVisualOverrides` 让材质在编辑器预览和运行时都正确。
- `InitializePhysics` 在蓝图构造后强制恢复权威半径、碰撞、阻尼、约束、低摩擦零弹性材质、质量与模拟状态。
- `ActivateMovement` 延后开启 Tick，保证移动不会早于注册和物理初始化。
- `EndPlay` 先停 Tick，再从 PopulationSubsystem 注销。

#### `Public/Crowd/Agents/CrowdPhysicsMovementComponent.h`

- 保存 Body 和 Population 的弱引用，以及 Spawn 时复制的物理参数。
- 对蓝图而言一般不需要直接操作；玩法手感应改 Data Asset。

#### `Private/Crowd/Agents/CrowdPhysicsMovementComponent.cpp`

- Tick Group 是 `TG_PrePhysics`，所以本帧力会进入本帧 Chaos 模拟。
- `Initialize` 绑定依赖，并要求自身在 PlayerController Tick 之后运行。
- `TickComponent` 把二维输入变成 XY 加速度。
- 达到软上限后，仅移除与当前速度同方向的正加速度，保留侧移、反向制动和碰撞速度。
- `AddForce(..., true)` 的 `true` 是 `bAccelChange`，表示按加速度施加、忽略质量；不要乘 `DeltaTime`。

#### `Public/Crowd/Agents/CrowdSpawnPoint.h` 与 `Private/.../CrowdSpawnPoint.cpp`

- 这是可由设计师摆放的青色箭头标记，不含 Spawn 逻辑。
- Director 只读取标记的 X/Y；所有人的 Z 仍按 Director 公共地面高度计算。
- 当前不读取箭头朝向，未来若要小人出生朝向或分组可在这里扩展数据。

### 4.5 Framework：输入、镜头和关卡初始化（10 个文件）

#### `Public/Crowd/Framework/CrowdGameMode.h` 与 `Private/.../CrowdGameMode.cpp`

- `ACrowdGameMode` 只把 PlayerControllerClass 设为 CrowdController、DefaultPawnClass 设为 CrowdCameraPawn。
- 它不生成 Director、灯光或场地；这些必须由关卡设计师放置/搭建。

#### `Public/Crowd/Framework/CrowdCameraPawn.h`

- Pawn 的世界位置就是轨道圆心，Camera 是相对圆心摆放的子组件。
- `ConfigureView` 改圆心；`ApplySettings` 改参数；两者都会重置镜头。
- `RequestQuarterTurn` 只启动状态机，`AdvanceOrbit` 才按 DeltaTime 推进。
- `ScreenInputToWorld` 将“屏幕右/上”投影到地面 XY。
- `OnRotationStarted/Finished` 是蓝图可绑定事件，可接声音、UI、粒子或场景动画。

#### `Private/Crowd/Framework/CrowdCameraPawn.cpp`

- 构造时强制 Perspective、16:9，并用相机局部手动曝光保持光照稳定。
- Bloom、Motion Blur、Vignette 默认归零，保证原型画面和镜头运动清晰。
- `ResetView` 同步 Current/Start/Target 三个角并停止过渡。
- `RequestQuarterTurn` 以 `StartYaw ± 90` 生成未归一化目标。
- `AdvanceOrbit` 使用 `InterpEaseInOut`，到达终点后才归一化角度。
- `ApplyOrbitPose` 用 `-ViewRotation.Vector() * OrbitDistance` 求相机相对位置；同一个旋转同时使 Camera 正对圆心。
- `ScreenInputToWorld` 分别单位化相机 Right/Up 的 XY 投影，避免俯角造成纵向输入变慢。

#### `Public/Crowd/Framework/CrowdPlayerController.h`

- 可配置 `InputMapping`、Move、Restart、RotateLeft、RotateRight 五类资产。
- 如果没有配置，运行时会创建瞬态 Context 和 Action，因此 C++ 原型开箱可玩。
- `CurrentScreenInput` 只是本帧缓存，绝不是持续状态。

#### `Private/Crowd/Framework/CrowdPlayerController.cpp`

- `EnsureInputAssets` 不覆盖完整的用户移动映射；Q/E 缺失时使用独立兜底 Context。
- 键盘方向通过 Negate 和 Swizzle 组合成 Axis2D；手柄使用 DeadZone。
- `SetupInputComponent` 绑定 Triggered/Completed/Canceled/Started。
- `BeginPlay` 将 Context 加入 LocalPlayer 的 Enhanced Input Subsystem。
- `PostProcessInput` 是关键汇合点：先推进相机，再转换方向，再发布共享输入，最后清本帧缓存。
- 暂停、输入被忽略、没有正确 CameraPawn 时一律发布零输入。
- `Restart` 找到 Director 调用 `ResetCrowd`；Q/E 只向 CameraPawn 发请求。
- `EndPlay` 归零输入并移除本类添加的 Context。

#### `Public/Crowd/Framework/CrowdLevelDirector.h`

- 每张玩法地图必须恰好放一个 Director。
- `AgentClass`、`PhysicsData`、初始人数/阵列和 SpawnPoints 决定人口初始化。
- `CameraTarget` 优先定义观察中心；为空时使用 Director 位置加 `CameraCenterOffset`。
- `bOverrideCameraSettings` 允许不同关卡覆盖 Camera 蓝图默认手感。
- `bInitialized` 代表最近一次完整重建是否成功。

#### `Private/Crowd/Framework/CrowdLevelDirector.cpp`

- `BeginPlay` 延迟一帧调用相机配置和人口重置。
- `ConfigureCamera` 检查 Controller 当前 Pawn 必须是 CrowdCameraPawn。
- `ResetCrowd` 检查唯一 Director、人口子系统和 AgentClass。
- SpawnPoints 非空时一项一人；否则以 Director 为中心生成规则网格。
- 间距至少为 `2 × Radius + 6 cm`，球心高度为 `Director.Z + Radius + 3 cm`。
- 任一空 SpawnPoint 或出生阻挡都会清除已生成的部分人口，保持原子初始化。
- `ReportError` 同时写 Output Log 和屏幕红字。

#### `Public/Crowd/Framework/CrowdLightingRig.h` 与 `Private/.../CrowdLightingRig.cpp`

- 可选 Actor，包含一盏可移动 Directional Light 和一盏可移动 Sky Light。
- 主光产生方向与动态阴影；Sky Light 使用引擎日光 Cubemap 填充暗部。
- 场景已有完整灯光时可以不放它，删除它不会破坏任何玩法引用。

### 4.6 Tests：自动化保护（1 个文件）

#### `Private/Crowd/Tests/CrowdPhysicsTests.cpp`

文件只在开发自动化测试构建中参与编译。`FTestWorld` 为每项测试创建有物理、无渲染依赖的瞬态 Game World，并在析构时彻底清理。

当前自动化测试分别保护：

1. `Crowd.M0.PopulationAndInput`：输入限制、出生防重叠、人口登记。
2. `Crowd.M0.SharedMovementAndBraking`：同步移动、软限速、松键停止、镜头坐标换算。
3. `Crowd.M0.CollisionAndPlaneLocks`：互推、墙阻挡、Z/旋转锁。
4. `Crowd.M0.InitialGridAndReset`：默认 12 人阵列和原子重置。
5. `Crowd.M0.EnhancedInputPipeline`：Enhanced Action 到共享输入的完整链路。
6. `Crowd.M0.FrameRateConsistency`：30/60/120 FPS 下两秒位移一致性。
7. `Crowd.M0.BlueprintSpawnAppearance`：蓝图类生成与材质覆盖。
8. `Crowd.Camera.PerspectiveAndFourViews`：透视、斜视、恒定轨道和四面闭环。
9. `Crowd.Camera.EaseInOutAndBusyInput`：缓动、忙碌拒绝和无过冲。
10. `Crowd.Camera.WrapAndFrameRate`：跨 ±180° 及不同帧率角度一致性。
11. `Crowd.Camera.EnhancedInputAndMovementBasis`：Q/E、移动基准与暂停行为。
修改核心不变量后应同步修改或新增测试；仅仅为了让失败测试变绿而放宽断言，会掩盖真实回归。

### 4.7 Character：第三人称可操控布娃娃（5 个文件）

- `Public/Character/GGJPhysicalAnimationCharacter.h`：公开给蓝图的模型动画、肌肉和驱动骨骼参数，以及运行时调节/复位接口。
- `Private/Character/GGJPhysicalAnimationCharacter.cpp`：官方第三人称模板式移动和镜头、Enhanced Input 兜底、三段单节点动画、Physical Animation 初始化顺序。
- `Public/Character/GGJCharacterGroupManager.h` 与对应 `.cpp`：人物名册、初始三人阵列、统一世界方向输入，以及未来人数增减的唯一入口。
- `Public/Gameplay/Zones/GGJSpawnZone.h` 与对应 `.cpp`：多出生区的初始人数均分和区域内网格位置计算。
- `Public/Gameplay/Zones/GGJDestroyZone.h` 与对应 `.cpp`：重叠销毁成员，并通过 Manager 安全处理 Leader 移交。
- `Public/Gameplay/Zones/GGJCountZone.h` 与对应 `.cpp`：区域/场景人数、可覆写条件、触发状态事件和场景 Text Render 可视化。
- `Public/Character/GGJPhysicalAnimationGameMode.h` 与对应 `.cpp`：让独立测试关卡生成物理角色蓝图，不改变 Crowd 默认框架。
- `Private/Character/Tests/GGJPhysicalAnimationCharacterTests.cpp`：验证 Test_Lab2 参数、共享 Skeleton/Physics Asset 和运行时 Chaos 模拟。

资源与蓝图的实际使用、换骨架流程和调参说明见项目根目录 `README_PHYSICAL_CHARACTER_CN.md`。

## 5. C++ 与蓝图的边界

| C++ 保证 | 蓝图/关卡负责 |
|---|---|
| 人口注册、共享输入所有权 | 选择 Agent 蓝图和 Physics Data |
| 出生防重叠与失败回滚 | 摆 Director 或 SpawnPoint |
| 物理球、平面锁、刚体施力 | 替换 VisualRoot 下的模型和材质 |
| 输入兜底与镜头相对方向 | 可选配置正式 Enhanced Input 资产 |
| Q/E 状态机、缓动和跨角度边界 | 调相机距离、角度、FOV、时长、指数 |
| GameMode 默认 Controller/Camera | 每张图指定 BP_CrowdGameMode |
| LightingRig 的基础组件结构 | 决定是否放置，并调整灯光表现 |

原则是：会破坏碰撞和全局状态的不变量放 C++；关卡差异与美术表现留给蓝图。蓝图模型必须挂在 `VisualRoot` 下并保持 NoCollision、关闭 Simulate Physics。

## 6. 常见 UE C++ 语法

- `UCLASS(Blueprintable)`：该 C++ 类能创建蓝图子类。
- `USTRUCT(BlueprintType)`：结构体能作为蓝图变量/引脚类型。
- `GENERATED_BODY()`：插入 UE 反射、序列化和蓝图桥接所需生成代码；不要删除。
- `GGJ_GAMEDEMO_API`：导出符号，使其它模块可链接该类型。
- `UPROPERTY(...)`：让 UE 知道成员，参与编辑器显示、序列化或垃圾回收引用追踪。
- `VisibleAnywhere`：组件引用可见但不能被替换；组件自身属性仍可展开修改。
- `EditAnywhere`：类默认值和关卡实例均可改；`EditDefaultsOnly` 只改类/蓝图默认值；`EditInstanceOnly` 只改关卡实例。
- `BlueprintReadOnly`：蓝图可读取，但不能直接给成员赋值。
- `UFUNCTION(BlueprintCallable)`：蓝图能主动调用；`BlueprintPure` 表示只查询、没有显式执行引脚。
- `TObjectPtr<T>`：受 UE 垃圾回收追踪的 UObject 强引用，适合组件和配置资产。
- `TWeakObjectPtr<T>`：不拥有对象的弱引用；对象销毁后 `IsValid()` 自动为 false。
- `TSubclassOf<T>`：只允许选择继承自 T 的类/蓝图类。
- `UWorldSubsystem`：由每个 World 自动创建和销毁的一份服务对象。
- `CreateDefaultSubobject`：构造类默认组件；只应在构造函数中使用。
- `NewObject`：运行时创建 UObject，这里用于瞬态 Action、Context 和物理材质。
- `SpawnActorDeferred`：先创建 Actor，再让外部在 FinishSpawning 前后控制初始化时序。
- `ConstructorHelpers::FObjectFinder`：构造默认对象时按资源路径寻找引擎资产。
- `TickGroup = TG_PrePhysics`：组件 Tick 在物理模拟前执行。
- `SCENE_QUERY_STAT(...)`：给碰撞查询命名，便于性能分析工具识别。
- `CDO`（Class Default Object）：每个 UE 类的一份默认对象；蓝图 Class Defaults 最终来自它及蓝图生成类默认值。

## 7. 常见修改应该去哪里

| 想修改的内容 | 首选位置 |
|---|---|
| 人数、阵列、出生点 | 关卡内 `BP_CrowdLevelDirector` |
| 半径、质量、加速度、阻尼、软限速 | `DA_CrowdPhysics_Default` 或 Director 的 DefaultPhysics |
| 人物模型、材质、视觉偏移 | `BP_CrowdAgent` 的 VisualRoot/VisualMesh/HeadMesh |
| 默认镜头距离、俯角、FOV、缓动 | `BP_CrowdCamera` 的 OrbitSettings |
| 单关专用镜头 | Director 的 Override Camera Settings |
| 正式键位资产 | `BP_CrowdController` 的 Input 属性 |
| 主光、补光、阴影 | 场景中的 `BP_CrowdLightingRig` 或自建灯光 |
| 共享移动算法 | `CrowdPhysicsMovementComponent.cpp` |
| 人口生成/销毁总入口 | `CrowdPopulationSubsystem.cpp` |
| 每关开始、重置、以后胜负规则 | `CrowdLevelDirector` 或新增独立关卡规则组件 |

## 8. 后续扩展建议

复制玩法不要直接塞进 MovementComponent。建议保持层次：

1. 新建 `ACrowdReplicationZone`，用 Box Trigger 检测区域中的 Agent。
2. 新建 `FCrowdGrowthRule` 或 Data Asset 表达 `n+1`、`n²`、`n³` 和自定义函数。
3. 区域只计算“输入人数 → 输出人数差”，实际新增仍调用 `PopulationSubsystem::SpawnAgent`。
4. 为批量出生增加位置采样/容量校验，任何生成失败应定义清楚是回滚、部分成功还是等待空间。
5. 按钮和出口只查询人口/区域占用，不应直接持有强引用控制每个小人。
6. 每增加一个规则，先写对应自动化测试，再让蓝图配置数据与美术表现。

如果未来加入台阶、跳跃或多层场地，需要重新设计当前的 Z Translation Lock；只在蓝图里放斜坡并不能让现有二维物理移动自动变成三维移动。
