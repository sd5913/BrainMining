import unreal,json,pathlib,shutil
out=pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/MuseumOutline/Unified';out.mkdir(parents=True,exist_ok=True)
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
src=pathlib.Path(unreal.Paths.project_content_dir())/'WenSi/Maps/Lvl_WenSi_MirrorMuseum1.umap'
if not (out/src.name).exists():shutil.copy2(src,out/src.name)
paths=['/Game/NiagaraExamples/Materials/MI_SmokePuffLight_8x8','/Game/NiagaraExamples/Materials/MasterMaterials/M_SmokeAndFire_Sprites']
report=[]
for p in paths:
 a=unreal.load_asset(p)
 report.append({'path':p,'class':a.get_class().get_name(),'parent':str(a.get_editor_property('parent')) if isinstance(a,unreal.MaterialInstanceConstant) else None,'blend':str(a.get_editor_property('blend_mode')) if isinstance(a,unreal.Material) else None,'custom_depth':a.get_editor_property('allow_translucent_custom_depth_writes') if isinstance(a,unreal.Material) else None,'clip':a.get_editor_property('opacity_mask_clip_value') if isinstance(a,unreal.Material) else None})
(out/'smoke_before.json').write_text(json.dumps(report,indent=2));print('SMOKE_OUTLINE_INSPECTED',report)
