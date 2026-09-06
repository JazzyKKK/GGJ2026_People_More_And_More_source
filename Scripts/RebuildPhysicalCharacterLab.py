"""Rebuild only the isolated Physical Character test level from existing assets."""

import unreal


MAP = "/Game/GGJ/Character/Maps/L_PhysicalCharacterLab"
FLOOR_MATERIAL = "/Game/GGJ/Character/Materials/M_PhysicalLabFloor"
WALL_MATERIAL = "/Game/GGJ/Character/Materials/M_PhysicalLabWall"
PROP_MATERIAL = "/Game/GGJ/Character/Materials/M_PhysicalLabProp"
GAME_MODE = "/Game/GGJ/Character/Blueprints/BP_PhysicalAnimationGameMode"

editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

if editor_assets.does_asset_exist(MAP):
    if not editor_assets.delete_asset(MAP):
        raise RuntimeError("Could not delete the incomplete test map")
if not levels.new_level(MAP):
    raise RuntimeError("Could not create test map: " + MAP)

# Reacquire the world-facing subsystem after switching maps. This is important for unattended runs.
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
mode_bp = unreal.load_asset(GAME_MODE)
floor_mat = unreal.load_asset(FLOOR_MATERIAL)
wall_mat = unreal.load_asset(WALL_MATERIAL)
prop_mat = unreal.load_asset(PROP_MATERIAL)
if not all((world, mode_bp, floor_mat, wall_mat, prop_mat)):
    raise RuntimeError("Missing world or Physical Character lab assets")
world.get_world_settings().set_editor_property("default_game_mode", mode_bp.generated_class())


def spawn(actor_class, location, rotation=None, label=""):
    actor = actors.spawn_actor_from_class(
        actor_class, unreal.Vector(*location), rotation or unreal.Rotator())
    if not actor:
        raise RuntimeError("Could not spawn " + (label or str(actor_class)))
    if label:
        actor.set_actor_label(label)
    return actor


def block(label, xyz, size, material_asset, simulate=False):
    actor = spawn(unreal.StaticMeshActor, xyz, label=label)
    component = actor.static_mesh_component
    component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube.Cube"))
    component.set_material(0, material_asset)
    component.set_collision_profile_name("PhysicsActor" if simulate else "BlockAll")
    actor.set_actor_scale3d(unreal.Vector(*(value / 100.0 for value in size)))
    if simulate:
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_simulate_physics(True)
        component.set_mass_override_in_kg("None", 18.0, True)
    return actor


block("Floor", (0, 0, -25), (1800, 1400, 50), floor_mat)
block("Backstop", (620, 0, 100), (50, 800, 200), wall_mat)
block("SideWall_Left", (0, -680, 80), (1800, 40, 160), wall_mat)
block("SideWall_Right", (0, 680, 80), (1800, 40, 160), wall_mat)
block("PhysicsBox_A", (120, -140, 70), (95, 95, 95), prop_mat, True)
block("PhysicsBox_B", (220, 20, 55), (75, 75, 75), prop_mat, True)
block("LowObstacle", (410, 180, 35), (150, 260, 70), wall_mat)

spawn(unreal.PlayerStart, (-520, 0, 110), label="PhysicalCharacter_PlayerStart")

sun = spawn(unreal.DirectionalLight, (0, 0, 500), unreal.Rotator(-42, -35, 0),
            "PhysicalLab_DirectionalLight")
sun_component = sun.get_component_by_class(unreal.DirectionalLightComponent)
sun_component.set_editor_property("intensity", 5.0)
sun_component.set_editor_property("cast_shadows", True)

sky = spawn(unreal.SkyLight, (0, 0, 300), label="PhysicalLab_SkyLight")
sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
sky_component.set_editor_property("intensity", 0.8)
sky_component.set_mobility(unreal.ComponentMobility.MOVABLE)

created = actors.get_all_level_actors()
if len(created) < 10:
    raise RuntimeError("Expected at least 10 actors before save, found " + str(len(created)))
if not levels.save_current_level():
    raise RuntimeError("Failed to save test map")

unreal.log("PHYSICAL_CHARACTER_MAP_REBUILT=" + MAP)
unreal.log("PHYSICAL_CHARACTER_MAP_ACTOR_COUNT=" + str(len(created)))
