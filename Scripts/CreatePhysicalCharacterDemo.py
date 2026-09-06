"""Import the artist FBX files correctly and build a standalone Physical Animation demo.

This script is intentionally create-only. It refuses to overwrite /Game/GGJ/Character so an
accidental rerun cannot destroy hand edits made later in the Blueprint or test map.
"""

import unreal


ROOT = "/Game/GGJ/Character"
MODEL_DIR = ROOT + "/Art/Model"
ANIM_DIR = ROOT + "/Art/Animations"
MATERIAL_DIR = ROOT + "/Materials"
BLUEPRINT_DIR = ROOT + "/Blueprints"
MAP = ROOT + "/Maps/L_PhysicalCharacterLab"
SOURCE_DIR = r"D:\Desktop\GGJ\Asset"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

if editor_assets.does_directory_exist(ROOT):
    # A previous unattended import may have stopped after creating only this known mesh/skeleton pair.
    # Delete that exact partial state so the create-only script can retry; refuse every broader state.
    existing = set(editor_assets.list_assets(ROOT, recursive=True, include_folder=False))
    allowed_partial = {
        MODEL_DIR + "/SK_GGJCharacter.SK_GGJCharacter",
        MODEL_DIR + "/SK_GGJCharacter_Skeleton.SK_GGJCharacter_Skeleton",
    }
    if existing and existing.issubset(allowed_partial):
        if not editor_assets.delete_directory(ROOT):
            raise RuntimeError("Could not remove the known partial character import")
    else:
        raise RuntimeError("Refusing to overwrite existing character content: " + str(sorted(existing)))


def all_loaded_assets(path):
    result = []
    for object_path in editor_assets.list_assets(path, recursive=True, include_folder=False):
        asset = unreal.load_asset(object_path)
        if asset:
            result.append(asset)
    return result


def require_one(objects, asset_class, label):
    matches = [obj for obj in objects if isinstance(obj, asset_class)]
    if len(matches) != 1:
        raise RuntimeError("Expected one {}, found {}: {}".format(
            label, len(matches), [obj.get_path_name() for obj in matches]))
    return matches[0]


def import_model():
    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("import_materials", True)
    options.set_editor_property("import_textures", True)
    options.set_editor_property("create_physics_asset", True)
    options.set_editor_property("override_full_name", True)

    mesh_data = options.get_editor_property("skeletal_mesh_import_data")
    # 美术 FBX 声明 UnitScaleFactor=100（米），但顶点数值实际按厘米导出；
    # 不修正会被 UE 再放大 100 倍，角色高度达到约 275 米。
    mesh_data.set_editor_property("import_uniform_scale", 0.01)
    mesh_data.set_editor_property("import_morph_targets", False)
    mesh_data.set_editor_property("update_skeleton_reference_pose", False)
    mesh_data.set_editor_property("use_t0_as_ref_pose", False)
    mesh_data.set_editor_property("preserve_smoothing_groups", True)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", SOURCE_DIR + r"\base_tp.fbx")
    task.set_editor_property("destination_path", MODEL_DIR)
    task.set_editor_property("destination_name", "SK_GGJCharacter")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("save", True)
    task.set_editor_property("options", options)
    asset_tools.import_asset_tasks([task])

    imported = all_loaded_assets(MODEL_DIR)
    mesh = require_one(imported, unreal.SkeletalMesh, "SkeletalMesh")
    skeleton = mesh.get_editor_property("skeleton")
    physics_asset = mesh.get_editor_property("physics_asset")
    if not skeleton:
        raise RuntimeError("Imported mesh has no Skeleton: " + mesh.get_path_name())
    if not physics_asset:
        # UE 5.6 Interchange can ignore create_physics_asset in automated imports. The dedicated
        # factory uses the same editor auto-generation path but is deterministic in commandlets.
        skeletal_mesh_editor = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
        physics_asset = skeletal_mesh_editor.create_physics_asset(mesh)
        if not physics_asset:
            raise RuntimeError("PhysicsAssetFactory failed for " + mesh.get_path_name())

    rename_targets = (
        (mesh, MODEL_DIR + "/SK_GGJCharacter"),
        (skeleton, MODEL_DIR + "/SKEL_GGJCharacter"),
        (physics_asset, MODEL_DIR + "/PHYS_GGJCharacter"),
    )
    for asset, target in rename_targets:
        source = asset.get_path_name().split(".")[0]
        if source != target and not editor_assets.rename_asset(source, target):
            raise RuntimeError("Failed to rename {} to {}".format(source, target))

    mesh = unreal.load_asset(MODEL_DIR + "/SK_GGJCharacter")
    skeleton = unreal.load_asset(MODEL_DIR + "/SKEL_GGJCharacter")
    physics_asset = unreal.load_asset(MODEL_DIR + "/PHYS_GGJCharacter")
    for asset in (mesh, skeleton, physics_asset):
        editor_assets.save_loaded_asset(asset, only_if_is_dirty=False)
    return mesh, skeleton, physics_asset


def import_animation(source_filename, destination_name, skeleton):
    before = {obj.get_path_name() for obj in all_loaded_assets(ANIM_DIR)}
    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("import_mesh", False)
    options.set_editor_property("import_animations", True)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("create_physics_asset", False)
    options.set_editor_property("skeleton", skeleton)
    options.set_editor_property("override_full_name", True)

    anim_data = options.get_editor_property("anim_sequence_import_data")
    # 动画必须与模型使用同一单位修正，否则根骨位移和 Skeleton 尺寸不一致。
    anim_data.set_editor_property("import_uniform_scale", 0.01)
    anim_data.set_editor_property("import_meshes_in_bone_hierarchy", False)
    anim_data.set_editor_property("remove_redundant_keys", True)
    anim_data.set_editor_property("do_not_import_curve_with_zero", True)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", SOURCE_DIR + "\\" + source_filename)
    task.set_editor_property("destination_path", ANIM_DIR)
    task.set_editor_property("destination_name", destination_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("save", True)
    task.set_editor_property("options", options)
    asset_tools.import_asset_tasks([task])

    new_assets = [obj for obj in all_loaded_assets(ANIM_DIR) if obj.get_path_name() not in before]
    animation = require_one(new_assets, unreal.AnimSequence, destination_name + " AnimSequence")
    target = ANIM_DIR + "/" + destination_name
    source = animation.get_path_name().split(".")[0]
    if source != target and not editor_assets.rename_asset(source, target):
        raise RuntimeError("Failed to rename {} to {}".format(source, target))
    animation = unreal.load_asset(target)
    editor_assets.save_loaded_asset(animation, only_if_is_dirty=False)
    return animation


def material(name, rgb, roughness_value):
    result = asset_tools.create_asset(name, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    result.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    color = unreal.MaterialEditingLibrary.create_material_expression(
        result, unreal.MaterialExpressionConstant3Vector, -220, 0)
    color.set_editor_property("constant", unreal.LinearColor(*rgb, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        result, unreal.MaterialExpressionConstant, -220, 150)
    roughness.set_editor_property("r", roughness_value)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(result)
    editor_assets.save_loaded_asset(result)
    return result


def blueprint(name, parent):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    result = asset_tools.create_asset(name, BLUEPRINT_DIR, unreal.Blueprint, factory)
    if not result:
        raise RuntimeError("Could not create Blueprint " + name)
    unreal.BlueprintEditorLibrary.compile_blueprint(result)
    return result


skeletal_mesh, shared_skeleton, physics_asset = import_model()
idle = import_animation("Happy Idle.fbx", "A_GGJ_HappyIdle", shared_skeleton)
walk = import_animation("Walking.fbx", "A_GGJ_Walk", shared_skeleton)
run = import_animation("Running.fbx", "A_GGJ_Run", shared_skeleton)

floor_mat = material("M_PhysicalLabFloor", (0.035, 0.055, 0.085), 0.72)
wall_mat = material("M_PhysicalLabWall", (0.12, 0.22, 0.31), 0.58)
prop_mat = material("M_PhysicalLabProp", (0.92, 0.36, 0.12), 0.42)

character_bp = blueprint("BP_PhysicalAnimationCharacter", unreal.GGJPhysicalAnimationCharacter)
character_defaults = unreal.get_default_object(character_bp.generated_class())
mesh_component = character_defaults.get_editor_property("mesh")
mesh_component.set_editor_property("skeletal_mesh_asset", skeletal_mesh)
mesh_component.set_editor_property("physics_asset_override", physics_asset)
mesh_component.set_collision_profile_name("Pawn")
mesh_component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
character_defaults.set_editor_property("idle_animation", idle)
character_defaults.set_editor_property("walk_animation", walk)
character_defaults.set_editor_property("run_animation", run)
character_defaults.set_editor_property("driven_bone_name", "Hips")
character_defaults.set_editor_property("muscle_strength", 5.0)
character_defaults.set_editor_property("include_driven_bone_in_simulation", False)
unreal.BlueprintEditorLibrary.compile_blueprint(character_bp)
editor_assets.save_loaded_asset(character_bp, only_if_is_dirty=False)

manager_bp = blueprint("BP_CharacterGroupManager", unreal.GGJCharacterGroupManager)
manager_defaults = unreal.get_default_object(manager_bp.generated_class())
manager_defaults.set_editor_property("initial_population", 3)
manager_defaults.set_editor_property("max_population", 30)
manager_defaults.set_editor_property("formation_spacing", 380.0)
unreal.BlueprintEditorLibrary.compile_blueprint(manager_bp)
editor_assets.save_loaded_asset(manager_bp, only_if_is_dirty=False)

mode_bp = blueprint("BP_PhysicalAnimationGameMode", unreal.GGJPhysicalAnimationGameMode)
mode_defaults = unreal.get_default_object(mode_bp.generated_class())
mode_defaults.set_editor_property("default_pawn_class", character_bp.generated_class())
mode_defaults.set_editor_property("group_manager_class", manager_bp.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(mode_bp)
editor_assets.save_loaded_asset(mode_bp, only_if_is_dirty=False)

if not levels.new_level(MAP):
    raise RuntimeError("Could not create test map: " + MAP)
# 切换 World 后重新取得面向关卡的子系统；无界面批处理时旧引用可能仍指向上一张地图。
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", mode_bp.generated_class())


def block(label, xyz, size, material_asset, simulate=False):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*xyz))
    if not actor:
        raise RuntimeError("Could not spawn block: " + label)
    actor.set_actor_label(label)
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

player_start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-520, 0, 110))
player_start.set_actor_label("PhysicalCharacter_PlayerStart")

sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500),
                                    unreal.Rotator(-42, -35, 0))
sun.set_actor_label("PhysicalLab_DirectionalLight")
sun_component = sun.get_component_by_class(unreal.DirectionalLightComponent)
sun_component.set_editor_property("intensity", 5.0)
sun_component.set_editor_property("cast_shadows", True)

sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300))
sky.set_actor_label("PhysicalLab_SkyLight")
sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
sky_component.set_editor_property("intensity", 0.8)
sky_component.set_mobility(unreal.ComponentMobility.MOVABLE)

created_actors = actors.get_all_level_actors()
if len(created_actors) < 10:
    raise RuntimeError("Expected at least 10 test-map actors, found " + str(len(created_actors)))
if not levels.save_current_level():
    raise RuntimeError("Failed to save test map")

unreal.log("PHYSICAL_CHARACTER_CREATED=" + character_bp.get_path_name())
unreal.log("PHYSICAL_CHARACTER_MESH=" + skeletal_mesh.get_path_name())
unreal.log("PHYSICAL_CHARACTER_SKELETON=" + shared_skeleton.get_path_name())
unreal.log("PHYSICAL_CHARACTER_PHYSICS=" + physics_asset.get_path_name())
unreal.log("PHYSICAL_CHARACTER_MAP=" + MAP)
unreal.log("PHYSICAL_CHARACTER_MAP_ACTOR_COUNT=" + str(len(created_actors)))
