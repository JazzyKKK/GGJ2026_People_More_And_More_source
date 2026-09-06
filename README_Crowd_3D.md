# 众众：3D 人群原型与蓝图搭建指南

当前版本实现：初始人群、同步移动、Chaos 刚体碰撞、透视斜视角、Q/E 绕场地中心旋转 90°，以及可编辑的动态主光与环境补光。尚未加入复制、按钮、传送门和通关。

C++ 已按文件、类、属性、函数和关键算法加入中文注释。需要系统阅读时请打开根目录的 `SOURCE_CODE_GUIDE_CN.md`；其中包含全部源文件索引、运行链路、UE 宏解释和后续扩展入口。

## 运行

打开 `/Game/GGJ/Crowd/Maps/L_Crowd_PhysicsTest` 后点击 **Play**：

- WASD / 方向键 / 手柄左摇杆：整群移动，方向相对当前镜头。
- Q：向左切换一个观察面，每次 90°。
- E：向右切换一个观察面，每次 90°。
- R：把人群恢复到本关初始位置。

Q/E 的旋转使用 Ease In Out：开始加速、中段最快、结束减速。切换过程中再次按 Q/E 会被忽略，不会排队或突然反向。切镜时仍可移动，移动方向每帧跟随当前镜头。

## 自己搭建关卡：最短流程

1. 新建普通 **Basic Level**，自己摆地板、墙、坡面外观和其它场景模型。
2. World Settings → GameMode Override 选择 `BP_CrowdGameMode`。
3. 场景内只放一个 `BP_CrowdLevelDirector`。
4. Director 的 Agent Class 选择 `BP_CrowdAgent`，Physics Data 选择 `DA_CrowdPhysics_Default`。
5. 放一个空 Actor 或 TargetPoint 到场地视觉中心，将它拖给 Director 的 Camera Target。
6. 放一个 `BP_CrowdLightingRig`，调它的 KeyLight / FillLight；也可以完全不用它，改用自己的灯光。
7. 确认地板与墙有简单碰撞，且能 Block `PhysicsBody`。点击 Play 测试。

不要手工摆 CameraPawn，也不要给场景 Pawn 设置 Auto Possess Player 0；GameMode 会生成并占有唯一 `BP_CrowdCamera`。GameMode、Director 和 LightingRig 都不会创建、删除或修改你的场地几何。

## 你可以修改的地方

### BP_CrowdCamera：镜头手感

打开 `/Game/GGJ/Crowd/Blueprints/Framework/BP_CrowdCamera`，在 Class Defaults → Crowd / Camera 修改 Orbit Settings：

| 参数 | 作用 | 默认值 |
|---|---|---:|
| Orbit Distance | 镜头到场地中心的距离；越大看到的范围越广 | 3200 cm |
| Pitch | 俯视角；接近 -85 更俯视，接近 -15 更贴地 | -50° |
| Initial Yaw | 开局从场地的哪个方向看 | -45° |
| Field Of View | 透视视野；小值更像长焦，大值透视更强 | 50° |
| Turn Duration | 每次 90° 切镜所需时间 | 0.65 s |
| Ease Exponent | 加减速感；越大，两端越慢、中段越快 | 2.5 |

Camera Target 是旋转中心，不是相机位置。需要每关不同镜头时，在 Director 勾选 `Override Camera Settings`，修改 `Camera Settings Override`；它只覆盖当前关卡。运行时蓝图也可调用：

- `Configure View(Center)`：换旋转中心并回到初始角度。
- `Apply Settings(Settings)`：应用参数并重置镜头。
- `Request Quarter Turn(Direction)`：负数相当于 Q，正数相当于 E；返回 false 表示正在旋转或方向为 0。
- `Reset View`、`Is Rotating`、`Get Current Yaw`。
- `On Rotation Started / Finished`：可在蓝图里接音效、UI 或转场特效。

相机使用 Perspective，不要在 Camera 组件里改成 Orthographic；运行时 C++ 仍会强制回 Perspective。相机使用局部固定曝光，避免继承其它地图的自动曝光导致开局忽明忽暗。

### BP_CrowdLevelDirector：人数、出生和每关镜头

| 参数 | 作用 |
|---|---|
| Agent Class | 生成的小人蓝图 |
| Physics Data | 全体共用的物理参数 |
| Initial Count | 没有 Spawn Points 时的初始人数，1～64 |
| Grid Columns / Grid Spacing | 自动出生阵列的列数与间距 |
| Spawn Points | 非空时，一项生成一人，并覆盖 Initial Count |
| Camera Target | 推荐：场地观察中心 Actor / TargetPoint |
| Camera Center Offset | Camera Target 为空时，相对 Director 的备用中心 |
| Override Camera Settings | 是否为本关覆盖 BP_CrowdCamera 默认参数 |

Director.Z 是公共移动平面高度。小人中心生成在 `Director.Z + Radius + 3 cm`；SpawnPoint 只采用 XY。当前人群锁定 Z 和旋转，所以 3D 指场景、角色模型、透视和光影，不包含跳跃、台阶或沿斜坡移动。若关卡需要高低层，应先扩展角色物理方案，不能仅把坡面摆进场景。

### BP_CrowdAgent：角色模型与外观

- `Body`：唯一物理碰撞球。不要缩放 Actor 或 Body。
- `VisualRoot`：纯表现挂点，可以整体调整角色模型的高度、朝向和缩放。
- `VisualMesh / HeadMesh`：当前占位模型，可换成自己的 Static Mesh；必须保持 NoCollision、不开启 Simulate Physics。
- `Body Material / Head Material`：非空时覆盖对应模型第 0 材质槽。完整人物模型自带材质时，清空覆盖参数，并可隐藏 HeadMesh。

视觉网格默认 Cast Shadow。碰撞半径不会自动缩放模型，需要同时调整 VisualRoot。

### DA_CrowdPhysics_Default：移动和挤压

| 参数 | 作用 |
|---|---|
| Radius | 球形碰撞半径，也是阵列安全间距依据 |
| Mass Kg | 碰撞质量；移动驱动力使用加速度，不因质量改变 |
| Drive Acceleration | 输入驱动强度 |
| Linear Damping | 松键后的减速速度 |
| Soft Speed Limit | 正常驱动的软速度上限，不硬裁剪碰撞速度 |

### BP_CrowdLightingRig：光影

- `KeyLight`：Movable Directional Light，负责方向、明暗和动态阴影。常调 Rotation、Intensity、Light Color、Source Angle、Shadow Bias。
- `FillLight`：Movable Sky Light，避免背光面死黑。常调 Intensity、Light Color、Cubemap。

LightingRig 是可选便利蓝图。场景已有太阳、SkyLight、HDRI 或室内灯时，不要重复放置；删除它不会影响玩法。材质 `M_CrowdBody / Head / Floor / Wall` 已切换为 Default Lit，会响应你自己的灯光。

### BP_CrowdController：自定义输入资产

不配置任何输入资产时，C++ 会自动创建 WASD、方向键、手柄、R、Q、E 映射，可直接运行。

如果换成自己的 Enhanced Input 资产，请在 BP_CrowdController 同时正确配置 Input Mapping、Move Action、Restart Action、Rotate Left Action、Rotate Right Action，并在映射中加入 Q/E。Move 必须是 Axis2D，其余是 Bool。只替换一半资产会造成映射不完整。

## 模块边界

| 类 | 职责 |
|---|---|
| CrowdGameMode | 指定唯一 Controller 与 CameraPawn |
| CrowdPlayerController | Enhanced Input、Q/E 请求、按镜头转换移动方向 |
| CrowdCameraPawn | 透视轨道位置、90° 状态机和缓动 |
| CrowdLevelDirector | 初始化人群、每关相机中心/覆盖配置、R 重置 |
| CrowdLightingRig | 可选可编辑灯光，不持有玩法状态 |
| CrowdPopulationSubsystem | 当前 World 的人口注册、出生入口与共享输入 |
| CrowdAgent | 物理球、3D 表现挂点、材质与阴影 |
| CrowdPhysicsMovementComponent | PrePhysics 读取共享输入并向刚体施力 |

## 删除与验证

旧的 RootGrowthPawn C++、BP_RootGrowthPawn、两份 TestDemo 地图和 ProceduralMeshComponent 依赖已经删除。删除前的可恢复备份保存在 `Backups/BeforeCrowd3D_20260905_015104.zip`。

UE 5.6.1 Editor Development 编译通过。自动化测试报告位于 `Saved/Automation/Crowd3D/index.json`，覆盖人口、碰撞、平面锁、蓝图实例、30/60/120 FPS 移动、透视相机、四个观察面、缓入缓出、跨 180° 和 Q/E 输入链路。

`Scripts/UpgradeCrowdTo3D.py` 是本次一次性资源迁移脚本；它只升级 Crowd 材质、创建灯光蓝图并在测试图加入灯光，不重建场地几何。`Scripts/ValidateCrowdPrototype.py` 是只读资源验证。`Scripts/CreateCrowdPrototype.py` 仅用于开发者从零重建物理测试图，不是自建关卡必需步骤。
