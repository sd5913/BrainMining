import unreal


for name in ["M_EchoContour", "M_WarmWood", "M_WarmGlow"]:
    material = unreal.load_asset("/Game/Passages/Echo/Materials/" + name)
    if not material:
        raise RuntimeError("Missing echo material: " + name)
    unreal.MaterialEditingLibrary.set_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not levels.load_level("/Game/Showcase/Maps/Lvl_FeatureShowcase"):
    raise RuntimeError("Missing showcase map")
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(actor, unreal.TextRenderActor):
        actor.set_actor_rotation(unreal.Rotator(pitch=0, yaw=180, roll=0), False)
for path in unreal.EditorAssetLibrary.list_assets("/Game/Showcase/Materials"):
    material = unreal.load_asset(path)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    if "M_ShowcaseFloor" in path or "M_ShowcasePlinth" in path:
        color = (0.025, 0.045, 0.07) if "M_ShowcaseFloor" in path else (0.055, 0.085, 0.115)
        unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
        node = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector)
        node.set_editor_property("constant", unreal.LinearColor(*color, 1))
        unreal.MaterialEditingLibrary.connect_material_property(node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
if not levels.save_current_level():
    raise RuntimeError("Showcase save failed")
unreal.log("SHOWCASE_FINALIZED: ISM materials and native lobby components")
