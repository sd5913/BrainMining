"""Use the supplied, unmodified movie and import its exact first frame."""
from pathlib import Path
import unreal,json,shutil
root=Path(unreal.Paths.project_dir());EA=unreal.EditorAssetLibrary;ML=unreal.MaterialEditingLibrary
out=root/'Saved/Validation/TraversalFeedback';paths=['/Game/EndingEmergence/PaperFinale/MS_EndingFinale','/Game/EndingEmergence/PaperFinale/T_EndingFinaleFirstFrame','/Game/EndingEmergence/Materials/M_EE_VideoPortal']
for p in paths:
    f=root/'Content'/(p[6:]+'.uasset');b=out/'Before'/f.relative_to(root)
    if not b.exists():b.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,b)
exec(compile((root/'Scripts/reimport_ending_first_frame.py').read_text(encoding='utf-8-sig'),'first_frame','exec'),{'__name__':'__main__'})
source=unreal.load_asset(paths[0]);source.set_file_path(str(root/'Content/Movies/EndingEmergence/EndingFinale.mp4'))
source.set_editor_property('precache_file',True);assert EA.save_loaded_asset(source,False)
mat=unreal.load_asset(paths[2]);found=False
for n in ML.get_material_expressions(mat):
    if isinstance(n,unreal.MaterialExpressionScalarParameter) and str(n.get_editor_property('parameter_name'))=='VideoAspect':
        n.set_editor_property('default_value',1280/720);found=True
assert found;ML.recompile_material(mat);assert EA.save_loaded_asset(mat,False)
(out/'movie_asset.json').write_text(json.dumps({'source':source.get_editor_property('file_path'),'width':1280,'height':720,'duration':12.1,'original_bytes_preserved':True},indent=2))
unreal.log('ENDING_MOVIE_ASSET_UPDATED')
