"""Create the designer-facing group manager Blueprint and connect it to the existing GameMode."""

import unreal


MANAGER_PATH = "/Game/GGJ/Character/Blueprints/BP_CharacterGroupManager"
GAME_MODE_PATH = "/Game/GGJ/Character/Blueprints/BP_PhysicalAnimationGameMode"

assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()

if assets.does_asset_exist(MANAGER_PATH):
    manager_bp = unreal.load_asset(MANAGER_PATH)
else:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.GGJCharacterGroupManager)
    manager_bp = tools.create_asset(
        "BP_CharacterGroupManager", "/Game/GGJ/Character/Blueprints", unreal.Blueprint, factory)
if not manager_bp:
    raise RuntimeError("Could not create BP_CharacterGroupManager")

manager_defaults = unreal.get_default_object(manager_bp.generated_class())
manager_defaults.set_editor_property("initial_population", 3)
manager_defaults.set_editor_property("max_population", 30)
manager_defaults.set_editor_property("formation_spacing", 380.0)
unreal.BlueprintEditorLibrary.compile_blueprint(manager_bp)
assets.save_loaded_asset(manager_bp, only_if_is_dirty=False)

game_mode_bp = unreal.load_asset(GAME_MODE_PATH)
if not game_mode_bp:
    raise RuntimeError("Missing BP_PhysicalAnimationGameMode")
game_mode_defaults = unreal.get_default_object(game_mode_bp.generated_class())
game_mode_defaults.set_editor_property("group_manager_class", manager_bp.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(game_mode_bp)
assets.save_loaded_asset(game_mode_bp, only_if_is_dirty=False)

unreal.log("CHARACTER_GROUP_BLUEPRINT_CREATED=" + manager_bp.get_path_name())
