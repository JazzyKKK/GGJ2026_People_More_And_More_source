"""Create designer-facing Blueprint children for the three Physical Character gameplay zones."""

import unreal


ZONE_DIR = "/Game/GGJ/Gameplay/Zones"
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()


def create_or_load(name, parent_class):
    path = ZONE_DIR + "/" + name
    if assets.does_asset_exist(path):
        result = unreal.load_asset(path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        result = tools.create_asset(name, ZONE_DIR, unreal.Blueprint, factory)
    if not result:
        raise RuntimeError("Could not create " + name)
    return result


spawn_bp = create_or_load("BP_SpawnZone", unreal.GGJSpawnZone)
spawn_defaults = unreal.get_default_object(spawn_bp.generated_class())
spawn_defaults.set_editor_property("spawn_order", 0)
spawn_defaults.set_editor_property("spawn_height_offset", 0.0)
spawn_defaults.set_editor_property("enabled", True)

destroy_bp = create_or_load("BP_DestroyZone", unreal.GGJDestroyZone)
destroy_defaults = unreal.get_default_object(destroy_bp.generated_class())
destroy_defaults.set_editor_property("enabled", True)
destroy_defaults.set_editor_property("can_destroy_leader", True)

count_bp = create_or_load("BP_CountZone", unreal.GGJCountZone)
count_defaults = unreal.get_default_object(count_bp.generated_class())
count_defaults.set_editor_property("target_count", 10)
count_defaults.set_editor_property("comparison", unreal.GGJCountComparison.GREATER_OR_EQUAL)
count_defaults.set_editor_property("text_height", 260.0)
count_defaults.set_editor_property("text_world_size", 120.0)
count_defaults.set_editor_property("success_pulse_amplitude", 0.12)
count_defaults.set_editor_property("success_pulse_speed", 4.0)

for blueprint in (spawn_bp, destroy_bp, count_bp):
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assets.save_loaded_asset(blueprint, only_if_is_dirty=False)

unreal.log("GAMEPLAY_ZONE_BLUEPRINTS_CREATED=" + ",".join(
    blueprint.get_path_name() for blueprint in (spawn_bp, destroy_bp, count_bp)))
