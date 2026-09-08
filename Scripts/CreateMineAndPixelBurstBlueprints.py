import unreal


# [MINE_ASSETS] 只创建以下两个蓝图；已存在时拒绝覆盖用户资产。
BLUEPRINTS = (
    (
        "/Game/GGJ/Gameplay/Hazards",
        "BP_RadialMine",
        "/Script/GGJ_gamedemo.GGJRadialMineActor",
    ),
    (
        "/Game/GGJ/Gameplay/Effects/PixelBurst",
        "BP_PixelBurstVFX",
        "/Script/GGJ_gamedemo.GGJPixelBurstVFX",
    ),
)


asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
for directory, asset_name, parent_class_path in BLUEPRINTS:
    asset_path = f"{directory}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log(f"[MINE_ASSETS] 已存在，未覆盖：{asset_path}")
        continue

    parent_class = unreal.load_class(None, parent_class_path)
    if not parent_class:
        raise RuntimeError(f"无法加载父类：{parent_class_path}")

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    blueprint = asset_tools.create_asset(
        asset_name,
        directory,
        unreal.Blueprint,
        factory,
    )
    if not blueprint:
        raise RuntimeError(f"创建蓝图失败：{asset_path}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
        raise RuntimeError(f"保存蓝图失败：{asset_path}")
    unreal.log(f"[MINE_ASSETS] 创建成功：{asset_path}")
