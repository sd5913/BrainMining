import unreal
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
folder='/Game/WenSi/Museum/Refresh/Materials/'
for name in ['MI_ScreenLacquer','MI_ScreenPaper','MI_ScreenLeaves','MI_ScreenBlossom']:
 m=unreal.load_asset(folder+name);overrides=m.get_editor_property('base_property_overrides')
 overrides.set_editor_property('override_two_sided',True);overrides.set_editor_property('two_sided',True);m.set_editor_property('base_property_overrides',overrides)
 unreal.MaterialEditingLibrary.update_material_instance(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 if isinstance(a,unreal.WenSiMirrorExhibit) and a.get_editor_property('kind')==5:
  a.modify();a.refresh_art_model()
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
print('SCREEN_TWO_SIDED_SAVED')
