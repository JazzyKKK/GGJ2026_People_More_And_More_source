"""Run with UE 5.6 PythonScript commandlet. Creates NEW Crowd assets; never replaces a map.

UnrealEditor-Cmd.exe <project> -run=pythonscript -script=<this file>
  -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -nullrhi
"""
import unreal

ROOT = "/Game/GGJ/Crowd"
MAP = ROOT + "/Maps/L_Crowd_PhysicsTest"
assets = unreal.AssetToolsHelpers.get_asset_tools()
editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

if editor_assets.does_asset_exist(MAP):
    raise RuntimeError("Map already exists; refusing to overwrite: " + MAP)


def material(name, rgb):
    path = ROOT + "/Materials/" + name
    existing = unreal.load_asset(path) if editor_assets.does_asset_exist(path) else None
    if existing:
        return existing
    result = assets.create_asset(name, ROOT + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    result.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    color = unreal.MaterialEditingLibrary.create_material_expression(result, unreal.MaterialExpressionConstant3Vector, -200, 0)
    color.set_editor_property("constant", unreal.LinearColor(*rgb, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(result, unreal.MaterialExpressionConstant, -200, 160)
    roughness.set_editor_property("r", 0.65)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(result)
    editor_assets.save_loaded_asset(result)
    return result


def blueprint(name, folder, parent):
    path = ROOT + "/Blueprints/" + folder
    existing = unreal.load_asset(path + "/" + name) if editor_assets.does_asset_exist(path + "/" + name) else None
    if existing:
        return existing
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    result = assets.create_asset(name, path, unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(result)
    editor_assets.save_loaded_asset(result)
    return result


floor_mat = material("M_CrowdFloor", (0.025, 0.040, 0.065))
wall_mat = material("M_CrowdWall", (0.16, 0.24, 0.32))
body_mat = material("M_CrowdBody", (0.06, 0.80, 0.67))
head_mat = material("M_CrowdHead", (0.85, 0.97, 1.0))

agent_bp = blueprint("BP_CrowdAgent", "Agents", unreal.CrowdAgent)
agent_defaults = unreal.get_default_object(agent_bp.generated_class())
agent_defaults.set_editor_property("body_material", body_mat)
agent_defaults.set_editor_property("head_material", head_mat)
agent_defaults.get_editor_property("visual_mesh").set_editor_property("override_materials", [body_mat])
agent_defaults.get_editor_property("head_mesh").set_editor_property("override_materials", [head_mat])
editor_assets.save_loaded_asset(agent_bp, only_if_is_dirty=False)

camera_bp = blueprint("BP_CrowdCamera", "Framework", unreal.CrowdCameraPawn)
lighting_bp = blueprint("BP_CrowdLightingRig", "Framework", unreal.CrowdLightingRig)
controller_bp = blueprint("BP_CrowdController", "Framework", unreal.CrowdPlayerController)
mode_bp = blueprint("BP_CrowdGameMode", "Framework", unreal.CrowdGameMode)
mode_defaults = unreal.get_default_object(mode_bp.generated_class())
mode_defaults.set_editor_property("default_pawn_class", camera_bp.generated_class())
mode_defaults.set_editor_property("player_controller_class", controller_bp.generated_class())
editor_assets.save_loaded_asset(mode_bp, only_if_is_dirty=False)
director_bp = blueprint("BP_CrowdLevelDirector", "Framework", unreal.CrowdLevelDirector)
director_defaults = unreal.get_default_object(director_bp.generated_class())
director_defaults.set_editor_property("agent_class", agent_bp.generated_class())
editor_assets.save_loaded_asset(director_bp, only_if_is_dirty=False)

data_path = ROOT + "/Data/DA_CrowdPhysics_Default"
physics_data = unreal.load_asset(data_path) if editor_assets.does_asset_exist(data_path) else None
if not physics_data:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.CrowdAgentPhysicsData)
    physics_data = assets.create_asset("DA_CrowdPhysics_Default", ROOT + "/Data", unreal.CrowdAgentPhysicsData, factory)
    editor_assets.save_loaded_asset(physics_data)

if not levels.new_level(MAP):
    raise RuntimeError("Could not create " + MAP)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", mode_bp.generated_class())


def block(label, xyz, size, mesh_path="/Engine/BasicShapes/Cube.Cube", mat=wall_mat):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*xyz))
    actor.set_actor_label(label)
    component = actor.static_mesh_component
    component.set_static_mesh(unreal.load_asset(mesh_path))
    component.set_material(0, mat)
    component.set_collision_profile_name("BlockAll")
    actor.set_actor_scale3d(unreal.Vector(*(value / 100.0 for value in size)))
    return actor


block("Floor", (0, 0, -25), (2020, 1220, 50), mat=floor_mat)
block("Boundary_Left", (-1000, 0, 60), (40, 1220, 120))
block("Boundary_Right", (1000, 0, 60), (40, 1220, 120))
block("Boundary_Top", (0, 600, 60), (2020, 40, 120))
block("Boundary_Bottom", (0, -600, 60), (2020, 40, 120))
# 100 cm gap: one 60 cm body fits; two cannot pass side-by-side.
block("Divider_Upper", (-80, 250, 60), (70, 400, 120))
block("Divider_Lower", (-80, -250, 60), (70, 400, 120))
block("Pillar_Upper", (380, 180, 60), (150, 150, 120), "/Engine/BasicShapes/Cylinder.Cylinder")
block("Pillar_Lower", (570, -170, 60), (150, 150, 120), "/Engine/BasicShapes/Cylinder.Cylinder")
actors.spawn_actor_from_class(lighting_bp.generated_class(), unreal.Vector(0, 0, 200))

director = actors.spawn_actor_from_class(director_bp.generated_class(), unreal.Vector(-620, 0, 0))
director.set_actor_label("Crowd_LevelDirector (edit initial count here)")
director.set_editor_property("camera_center_offset", unreal.Vector(620, 0, 0))
director.set_editor_property("physics_data", physics_data)
director.set_editor_property("agent_class", agent_bp.generated_class())
director.set_editor_property("initial_count", 12)

if not levels.save_current_level():
    raise RuntimeError("Map save failed")
unreal.log("CROWD_PROTOTYPE_CREATED: " + MAP)
