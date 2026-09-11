# 同屏双人控制实验使用说明

这套功能只在关卡选择 `GGJPartyGameMode`（或它的蓝图子类）时运行，不会改变原来的
`GGJPhysicalAnimationGameMode`、`GGJCharacterGroupManager` 和普通 `GGJSpawnZone` 流程。

## 推荐的蓝图目录

在内容浏览器中新建：

```text
Content/GGJ/PartyExperiment/
├─ GameMode/
├─ Camera/
├─ Players/
└─ Zones/
```

建议创建四个蓝图：

1. `BP_PartyGameMode`，父类选择 `GGJPartyGameMode`。
2. `BP_PartyCamera`，父类选择 `GGJPartyCameraActor`。
3. `BP_PartyPopulationGroup`，父类选择 `GGJPartyPopulationGroup`，用于统一调整掉落间隔和高度。
4. `BP_PartySpawnZone`，父类选择 `GGJPartySpawnZone`。

## 最小测试关卡搭建

1. 新建一张独立测试关卡，不要修改正式关卡。
2. 在“世界设置 → GameMode Override”选择 `BP_PartyGameMode`。
3. 打开 `BP_PartyGameMode` 的 Class Defaults：
   - `Default Member Class` 选择当前带模型、骨骼、动画和 Physical Animation 的人物蓝图。
   - `Party Camera Class` 选择 `BP_PartyCamera`。
   - `Population Group Class` 选择 `BP_PartyPopulationGroup`。
   - `Player Setups[0]` 保持 `PlayerIndex=0`、`InitialPopulation=3`。
   - `Player Setups[1]` 保持 `PlayerIndex=1`、`InitialPopulation=3`。
4. 向关卡放两个 `BP_PartySpawnZone`：
   - 第一个 `PlayerIndex=0`，属于玩家一。
   - 第二个 `PlayerIndex=1`，属于玩家二。
5. Play：
   - P1：W/A/S/D 移动，Space 跳跃。
   - P2：键盘方向键移动，Right Shift 跳跃。
   - 共享视角：Q/E，每次平滑旋转 90 度。
   - 暂停菜单：ESC；暂停时再次按 ESC 可以回到游戏。

同一玩家可以放多个 SpawnZone。只要它们的 `PlayerIndex` 相同，开局人数就会按照
`SpawnOrder` 自动平均分配。

## 蓝图人口接口

从 `Get Game Mode` 转成 `GGJPartyGameMode` 后可以调用：

- `Get Player Group(PlayerIndex)`：取得某名玩家的人口组。
- `Get Player Population Count(PlayerIndex)`：取得指定玩家当前人数。
- `Get Total Population Count`：取得场景内所有玩家总人数。
- `Get Owning Player Index(Character)`：判断一个人物属于哪名玩家；找不到返回 -1。
- `Add Population For Player`：在指定世界位置逐个掉落新增人物。
- `Add Population At Spawn Zone`：从指定玩家出生区逐个掉落新增人物。
- `Request Population Delta For Player`：正数增长，负数减少。

例如给玩家二增加 5 人：

```text
Get Game Mode
→ Cast To GGJPartyGameMode
→ Add Population For Player
   PlayerIndex = 1
   Amount = 5
   SpawnCenter = 机关位置
```

人物会由对应 `GGJPartyPopulationGroup` 按 `RuntimeSpawnInterval` 逐个生成，并从
`RuntimeSpawnDropHeight` 高度掉下。人口组的这两个参数可以在其蓝图子类或手放实例上调整。

## 输入和暂停菜单配置

双人移动、跳跃键位位于 `GGJPartyPlayerController.KeyboardControlSchemes`。默认配置为：

- `PlayerIndex=0`：W/A/S/D，`JumpKey=Space Bar`。
- `PlayerIndex=1`：方向键，`JumpKey=Right Shift`。

如果要在蓝图中改键，创建 `GGJPartyPlayerController` 的蓝图子类，修改上述数组，然后在
`BP_PartyGameMode → Player Controller Class` 选择这个控制器蓝图。旧控制器蓝图中新增的
`JumpKey` 即使显示为 None，玩家0和玩家1仍会分别回退到 Space 和 Right Shift。

`GGJPartyGameMode` 会自动创建 `GGJPauseMenuManager`，因此关卡不需要额外放置 Actor。
如需使用自定义菜单蓝图，在 `BP_PartyGameMode → Party Experiment | Pause Menu →
Pause Menu Manager Class` 中替换；如果关卡已经手动放了一个管理器实例，则优先复用该实例。

## 人物互相推挤

派对人口组默认开启 `Character Pushing`。它用于弥补 UE `ACharacter` 胶囊不会像真正刚体
一样互相传递动量的问题，只对派对模式生成的人物生效：

- `Character Push Force`：推动强度，默认 350000；感觉推不动可逐步提高。
- `Character Push Padding`：胶囊接触前的额外检测距离。
- `Character Push Max Horizontal Speed`：被多人挤压时的最大水平速度。
- `Push Own Group`：同一玩家的人物能否互相挤。
- `Push Other Players`：能否推动另一名玩家的人物。

这是保留 `CharacterMovement` 稳定行走和楼梯能力的“游戏化推挤”，不是完全刚体仿真。
如果后续决定把角色改成真正以骨盆刚体驱动的 Pawn，需要重新设计角色根组件、站立平衡和移动。

## 扩展更多玩家

增加第三名键盘玩家时：

1. 在 `BP_PartyGameMode.PlayerSetups` 增加 `PlayerIndex=2`。
2. 创建 `GGJPartyPlayerController` 的蓝图子类，在 `KeyboardControlSchemes` 增加
   `PlayerIndex=2`，例如使用 I/J/K/L，并设置一个不冲突的 `JumpKey`。
3. 在 `BP_PartyGameMode` 把 `PlayerControllerClass` 换成该控制器蓝图。
4. 放置 `PlayerIndex=2` 的 PartySpawnZone。

手柄输入以后直接调用 `Apply Player Screen Movement`、`Apply Player Jump Start` 和
`Apply Player Jump End` 即可，人口组、相机和增长接口都不需要修改。

## 当前隔离限制

旧版 `GGJCountZone` 和 `GGJDestroyZone` 只认识旧单人人口管理器，因此本实验暂时不复用它们。
后续应新增派对版计数/销毁区，或者在确认玩法方向后抽象统一的人口接口；这样可以避免本次
实验影响已经完成的正式关卡。
