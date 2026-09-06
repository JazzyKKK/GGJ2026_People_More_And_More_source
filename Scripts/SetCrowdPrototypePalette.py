"""Explicit reset of BP_CrowdAgent's prototype palette; does not change the map or meshes."""
import unreal

root = "/Game/GGJ/Crowd"
asset = unreal.load_asset(root + "/Blueprints/Agents/BP_CrowdAgent")
defaults = unreal.get_default_object(asset.generated_class())
defaults.set_editor_property("body_material", unreal.load_asset(root + "/Materials/M_CrowdBody"))
defaults.set_editor_property("head_material", unreal.load_asset(root + "/Materials/M_CrowdHead"))
unreal.get_editor_subsystem(unreal.EditorAssetSubsystem).save_loaded_asset(asset, only_if_is_dirty=False)
unreal.log("CROWD_PALETTE_SAVED")
