import unreal


# [PIXEL_WIND_VFX_ASSET] 只创建约定的蓝图子类；存在时拒绝覆盖。
ASSET_DIRECTORY = "/Game/GGJ/Gameplay/Effects/PixelWind"
ASSET_NAME = "BP_PixelWindVFX"
ASSET_PATH = f"{ASSET_DIRECTORY}/{ASSET_NAME}"


if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
    unreal.log(f"[PIXEL_WIND_VFX_ASSET] 已存在，未覆盖：{ASSET_PATH}")
else:
    parent_class = unreal.load_class(None, "/Script/GGJ_gamedemo.GGJPixelWindVFX")
    if not parent_class:
        raise RuntimeError("无法加载 AGGJPixelWindVFX；请先编译 GGJ_gamedemoEditor。")

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    blueprint = asset_tools.create_asset(
        ASSET_NAME,
        ASSET_DIRECTORY,
        unreal.Blueprint,
        factory,
    )
    if not blueprint:
        raise RuntimeError(f"创建蓝图失败：{ASSET_PATH}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
        raise RuntimeError(f"保存蓝图失败：{ASSET_PATH}")
    unreal.log(f"[PIXEL_WIND_VFX_ASSET] 创建成功：{ASSET_PATH}")
