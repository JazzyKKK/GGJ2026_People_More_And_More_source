"""Read-only checks for saved Crowd Blueprint defaults and map contents."""
import unreal

ROOT = "/Game/GGJ/Crowd"
editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
for obsolete in ["/Game/GGJ/BP/BP_RootGrowthPawn", "/Game/GGJ/Map/TestDemo", "/Game/TestDemo"]:
    assert not editor_assets.does_asset_exist(obsolete), obsolete
agent = unreal.get_default_object(unreal.load_class(None, ROOT + "/Blueprints/Agents/BP_CrowdAgent.BP_CrowdAgent_C"))
for property_name, material_name in [("visual_mesh", "M_CrowdBody"), ("head_mesh", "M_CrowdHead")]:
    mesh = agent.get_editor_property(property_name)
    actual = mesh.get_material(0)
    assert actual and actual.get_name() == material_name, (property_name, actual)

mode = unreal.get_default_object(unreal.load_class(None, ROOT + "/Blueprints/Framework/BP_CrowdGameMode.BP_CrowdGameMode_C"))
assert mode.get_editor_property("default_pawn_class").get_name() == "BP_CrowdCamera_C"
assert mode.get_editor_property("player_controller_class").get_name() == "BP_CrowdController_C"
camera = unreal.get_default_object(unreal.load_class(None, ROOT + "/Blueprints/Framework/BP_CrowdCamera.BP_CrowdCamera_C"))
assert camera.get_editor_property("camera").get_editor_property("projection_mode") == unreal.CameraProjectionMode.PERSPECTIVE
for name in ["M_CrowdBody", "M_CrowdHead", "M_CrowdFloor", "M_CrowdWall"]:
    assert unreal.load_asset(ROOT + "/Materials/" + name).get_editor_property("shading_model") == unreal.MaterialShadingModel.MSM_DEFAULT_LIT
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level(ROOT + "/Maps/L_Crowd_PhysicsTest")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
directors = [actor for actor in actors if isinstance(actor, unreal.CrowdLevelDirector)]
assert len(directors) == 1
assert directors[0].get_editor_property("initial_count") == 12
assert directors[0].get_editor_property("agent_class").get_name() == "BP_CrowdAgent_C", directors[0].get_editor_property("agent_class").get_name()
assert directors[0].get_editor_property("physics_data") is not None
assert len([actor for actor in actors if isinstance(actor, unreal.StaticMeshActor)]) == 9
assert len([actor for actor in actors if isinstance(actor, unreal.CrowdLightingRig)]) == 1
unreal.log("CROWD_ASSETS_VALIDATED: perspective camera, lit materials, one light rig, one director")
