"""Create the designer-facing adaptive group camera and connect it to the Physical GameMode."""

import unreal


CAMERA_PATH = "/Game/GGJ/Character/Blueprints/BP_GroupCamera"
GAME_MODE_PATH = "/Game/GGJ/Character/Blueprints/BP_PhysicalAnimationGameMode"

assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()

if assets.does_asset_exist(CAMERA_PATH):
    camera_bp = unreal.load_asset(CAMERA_PATH)
else:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.GGJGroupCameraActor)
    camera_bp = tools.create_asset(
        "BP_GroupCamera", "/Game/GGJ/Character/Blueprints", unreal.Blueprint, factory)
if not camera_bp:
    raise RuntimeError("Could not create BP_GroupCamera")

camera_defaults = unreal.get_default_object(camera_bp.generated_class())
camera_defaults.set_editor_property("pitch", -50.0)
camera_defaults.set_editor_property("initial_yaw", -45.0)
camera_defaults.set_editor_property("field_of_view", 50.0)
camera_defaults.set_editor_property("min_distance", 1800.0)
camera_defaults.set_editor_property("max_distance", 9000.0)
camera_defaults.set_editor_property("member_bounds_radius", 140.0)
camera_defaults.set_editor_property("framing_padding", 320.0)
camera_defaults.set_editor_property("focus_height_offset", 55.0)
camera_defaults.set_editor_property("focus_spring_stiffness", 55.0)
camera_defaults.set_editor_property("zoom_spring_stiffness", 45.0)
camera_defaults.set_editor_property("critical_damping", 1.0)
camera_defaults.set_editor_property("quarter_turn_duration", 0.65)
camera_defaults.set_editor_property("rotation_ease_exponent", 2.5)
unreal.BlueprintEditorLibrary.compile_blueprint(camera_bp)
assets.save_loaded_asset(camera_bp, only_if_is_dirty=False)

game_mode_bp = unreal.load_asset(GAME_MODE_PATH)
if not game_mode_bp:
    raise RuntimeError("Missing BP_PhysicalAnimationGameMode")
game_mode_defaults = unreal.get_default_object(game_mode_bp.generated_class())
game_mode_defaults.set_editor_property("group_camera_class", camera_bp.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(game_mode_bp)
assets.save_loaded_asset(game_mode_bp, only_if_is_dirty=False)

unreal.log("GROUP_CAMERA_BLUEPRINT_CREATED=" + camera_bp.get_path_name())
