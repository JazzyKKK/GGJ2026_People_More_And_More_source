"""Upgrade the existing crowd materials and add an optional lighting rig; never rebuild geometry."""
import unreal

root = "/Game/GGJ/Crowd"
editor = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
editing = unreal.MaterialEditingLibrary
for name in ["M_CrowdBody", "M_CrowdHead", "M_CrowdFloor", "M_CrowdWall"]:
    material = unreal.load_asset(root + "/Materials/" + name)
    if not material:
        raise RuntimeError("Missing expected material: " + name)
    if material.get_editor_property("shading_model") == unreal.MaterialShadingModel.MSM_UNLIT:
        color = editing.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        if not color:
            raise RuntimeError("Unexpected material graph; refusing to replace it: " + name)
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        editing.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
        zero = editing.create_material_expression(material, unreal.MaterialExpressionConstant, -200, 160)
        zero.set_editor_property("r", 0.0)
        editing.connect_material_property(zero, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        roughness = editing.create_material_expression(material, unreal.MaterialExpressionConstant, -200, 280)
        roughness.set_editor_property("r", 0.65)
        editing.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
        editing.recompile_material(material)
        editor.save_loaded_asset(material, only_if_is_dirty=False)

rig_path = root + "/Blueprints/Framework/BP_CrowdLightingRig"
if editor.does_asset_exist(rig_path):
    rig = unreal.load_asset(rig_path)
else:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.CrowdLightingRig)
    rig = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "BP_CrowdLightingRig", root + "/Blueprints/Framework", unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(rig)
    editor.save_loaded_asset(rig, only_if_is_dirty=False)

camera_bp = unreal.load_asset(root + "/Blueprints/Framework/BP_CrowdCamera")
camera_defaults = unreal.get_default_object(camera_bp.generated_class())
camera_defaults.get_editor_property("camera").set_editor_property(
    "projection_mode", unreal.CameraProjectionMode.PERSPECTIVE)
editor.save_loaded_asset(camera_bp, only_if_is_dirty=False)

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not levels.load_level(root + "/Maps/L_Crowd_PhysicsTest"):
    raise RuntimeError("Could not load the existing test map")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
before = actors.get_all_level_actors()
geometry = [(actor.get_actor_label(), actor.get_actor_transform()) for actor in before if isinstance(actor, unreal.StaticMeshActor)]
if not any(isinstance(actor, (unreal.CrowdLightingRig, unreal.DirectionalLight)) for actor in before):
    light = actors.spawn_actor_from_class(rig.generated_class(), unreal.Vector(0, 0, 200))
    light.set_actor_label("Crowd_LightingRig (optional - edit lights or replace)")
levels.save_current_level()
after_geometry = [(actor.get_actor_label(), actor.get_actor_transform()) for actor in actors.get_all_level_actors() if isinstance(actor, unreal.StaticMeshActor)]
assert len(geometry) == len(after_geometry), "Geometry count changed unexpectedly"
unreal.log("CROWD_3D_UPGRADED: lit materials + optional lighting; preserved " + str(len(geometry)) + " existing static meshes")
